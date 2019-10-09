//
// Created by ganler-Mac on 2019/7/21.
//
#pragma once

#include <cstddef>
#include <thread>
#include <future>
#include <array>
#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <algorithm>
#include <condition_variable>

#include "util.hpp"

namespace thread_pools
{

class spool final
{// Ref: https://github.com/progschj/ThreadPool/blob/master/ThreadPool.h
public:
    spool(std::size_t);
    ~spool();
    template <typename Func, typename ... Args>
    auto enqueue(Func &&f, Args &&... args) // For Cpp14+ -> decltype(auto).
        -> std::future<typename std::result_of<Func(Args...)>::type>;
private:
    using task_type = std::function<void()>;
    // Use xx::function<> wrapper is not zero overhead.(See the link below)
    // https://www.boost.org/doc/libs/1_45_0/doc/html/function/misc.html#id1285061
    // https://www.boost.org/doc/libs/1_45_0/doc/html/function/faq.html#id1284915
    struct pool_src
    {
        std::mutex                             m_queue_mu; //
        std::condition_variable                m_cv;
        std::queue<task_type>                  m_task_queue;          //
        bool                                   m_shutdown    {false}; //
    };
    std::shared_ptr<pool_src>                  shared_src;
};

// Implementation:
inline spool::spool(std::size_t sz) : shared_src(std::make_shared<pool_src>())
{
    for(int i=0; i<sz; ++i)
        std::thread([](std::shared_ptr<pool_src> ptr2src) {
            while (true) {
                std::unique_lock<std::mutex> lock(ptr2src->m_queue_mu);
                ptr2src->m_cv.wait(lock, [&] { return ptr2src->m_shutdown or !ptr2src->m_task_queue.empty(); });
                if (ptr2src->m_shutdown and ptr2src->m_task_queue.empty())
                    return; // Conditions to let the thread go.
                auto task = std::move(ptr2src->m_task_queue.front());
                ptr2src->m_task_queue.pop();
                lock.unlock();
                task();
            }
        }, shared_src).detach();
}

template <typename Func, typename ... Args>
auto spool::enqueue(Func &&f, Args &&... args) -> std::future<typename std::result_of<Func(Args...)>::type>
{
    using return_type = typename std::result_of<Func(Args...)>::type;
// >>> [Original std::shared_ptr version]. Actually in Cpp14 new standard we can use std::unique_ptr.
// But some new compilers are still using old libc++ version.  So I used raw ptr to make my code more compatible.
//    auto task = std::make_shared< std::packaged_task<return_type()> >(
//      std::bind(std::forward<Func>(f), std::forward<Args>(args)...)); // Just use raw ptr if you don't care exception.
//    MAKE_TASK()
    std::packaged_task<return_type()>* task = nullptr;/* Why raw pointer? See comments in static_pool.hpp's enqueue().*/
    try_allocate(task, std::forward<Func>(f), std::forward<Args>(args)...);
    auto result = task->get_future();
    {
        std::lock_guard<std::mutex> lock(shared_src->m_queue_mu);
        shared_src->m_task_queue.emplace([task](){ (*task)(); delete task; });
    }
    shared_src->m_cv.notify_one();
    return result;
}

inline spool::~spool()
{
    shared_src->m_shutdown = true;
    std::atomic_signal_fence(std::memory_order_seq_cst);
    shared_src->m_cv.notify_all();
}

}