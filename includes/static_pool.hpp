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
    std::mutex                             m_queue_mu;
    std::mutex                             m_end_mu;
    std::condition_variable                m_cv;
    std::condition_variable                m_cv_end;
    std::queue<task_type>                  m_task_queue;
    std::atomic<int>                       m_live        {  0  };
    bool                                   m_shutdown    {false};
};

// Implementation:
inline spool::spool(std::size_t sz)
{
    m_live = sz;
    for(int i=0; i<sz; ++i)
        std::thread([this] {
            while (true) {
                std::unique_lock<std::mutex> lock(m_queue_mu);
                m_cv.wait(lock, [this] { return m_shutdown or !m_task_queue.empty(); });
                if (m_shutdown and m_task_queue.empty()) {
                    lock.unlock();
                    if (--m_live == 0)
                        m_cv_end.notify_one();
                    return; // Conditions to let the thread go.
                }
                auto task = std::move(m_task_queue.front());
                m_task_queue.pop();
                lock.unlock();
                task();
            }
        }).detach();
}

template <typename Func, typename ... Args>
auto spool::enqueue(Func &&f, Args &&... args) -> std::future<typename std::result_of<Func(Args...)>::type>
{
    using return_type = typename std::result_of<Func(Args...)>::type;
// >>> [Original std::shared_ptr version]. Actually in Cpp14 new standard we can use std::unique_ptr.
// But some new compilers are still using old libc++ version.  So I used raw ptr to make my code more compatible.
//    auto task = std::make_shared< std::packaged_task<return_type()> >(
//      std::bind(std::forward<Func>(f), std::forward<Args>(args)...)); // Just use raw ptr if you don't care exception.
    MAKE_TASK()
}

inline spool::~spool()
{
    m_shutdown = true;
    m_cv.notify_all();
    std::unique_lock<std::mutex> lock(m_end_mu); // lock-free waiting is slower...
    m_cv_end.wait(lock, [this] { return m_live.load() == 0; });
}

}