//
// Created by ganler-Mac on 2019/7/21.
//
#pragma once

#include <cstddef>
#include <thread>
#include <future>
#include <array>
#include <queue>
#include <algorithm>
#include <mutex>
#include <memory>
#include <condition_variable>

#include "util.hpp"

namespace thread_pools
{

template <std::size_t PoolSize> // TODO: Set better default value according to the architecture.
class spool final
{// Ref: https://github.com/progschj/ThreadPool/blob/master/ThreadPool.h
public:
    spool();
    ~spool();
    template <typename Func, typename ... Args>
    auto enqueue(Func &&f, Args &&... args) // For Cpp14+ -> decltype(auto).
        -> std::future<typename std::result_of<Func(Args...)>::type>;
private:
    using task_type = std::function<void()>;
    // Use xx::function<> wrapper is not zero overhead.(See the link below)
    // https://www.boost.org/doc/libs/1_45_0/doc/html/function/misc.html#id1285061
    // https://www.boost.org/doc/libs/1_45_0/doc/html/function/faq.html#id1284915
    using thread_container = std::array<std::thread, PoolSize>;
    thread_container                       m_workers;
    std::mutex                             m_mu;
    std::condition_variable                m_cv;
    std::queue<task_type>                  m_task_queue;
    bool                                   m_shutdown    {false};
};

// Implementation:
template <std::size_t Sz> inline spool<Sz>::spool()
{
    for(auto && w : m_workers)
        w = std::thread{[this]{
            while(true)
            {
                std::unique_lock<std::mutex> lock(m_mu);
                m_cv.wait(lock, [this] { return m_shutdown or !m_task_queue.empty(); });
                if (m_shutdown and m_task_queue.empty()) return; // Conditions to let the thread go.
                auto task = std::move(m_task_queue.front());
                m_task_queue.pop();
                lock.unlock();
                task();
            }}};
}

template <std::size_t Sz> template <typename Func, typename ... Args>
auto spool<Sz>::enqueue(Func &&f, Args &&... args) -> std::future<typename std::result_of<Func(Args...)>::type>
{
    using return_type = typename std::result_of<Func(Args...)>::type;
// >>> [Original std::shared_ptr version]. Actually in Cpp14 new standard we can use std::unique_ptr.
// But some new compilers are still using old libc++ version.  So I used raw ptr to make my code more compatible.
//    auto task = std::make_shared< std::packaged_task<return_type()> >(
//      std::bind(std::forward<Func>(f), std::forward<Args>(args)...)); // Just use raw ptr if you don't care exception.
    std::packaged_task<return_type()>* task = nullptr;
    try_allocate(task, std::forward<Func>(f), std::forward<Args>(args)...);
    auto result = task->get_future();
    std::unique_lock<std::mutex> lock(m_mu);
    m_task_queue.emplace( [task](){ (*task)(); delete task; } ); // The benefit of lambda: get more chance to inline.
    m_cv.notify_one();
    return result;
}

template <std::size_t Sz> inline spool<Sz>::~spool()
{
    std::unique_lock<std::mutex> lock(m_mu);
    m_shutdown = true;
    lock.unlock();
    m_cv.notify_all();
    std::for_each(m_workers.begin(), m_workers.end(), std::mem_fn(&std::thread::join));
}

}