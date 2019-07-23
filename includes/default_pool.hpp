//
// Created by ganler-Mac on 2019/7/21.
//
#pragma once

#include <cstddef>
#include <thread>
#include <future>
#include <vector>
#include <queue>
#include <algorithm>
#include <mutex>
#include <condition_variable>

#include "util.hpp"

namespace thread_pools
{

class pool final
{
public:
    pool(std::size_t);
    ~pool();
    template <typename Func, typename ... Args>
    auto enqueue(Func&& f, Args&& ... args) -> std::future<typename std::result_of<Func(Args...)>::type>;
private:
    using task_type        = std::function<void()>;
    using thread_container = std::vector<std::thread>;
    std::mutex                   m_mu;
    std::condition_variable      m_cv;
    std::queue<task_type>        m_task_queue;
    thread_container             m_workers;
    bool                         m_shutdown {false};
};

// Implementation.
inline pool::pool(std::size_t thread_num)
{
    m_workers = thread_container(thread_num);
    for(auto && w : m_workers)
        w = std::thread{[this]() {
            while(true)
            {
                std::unique_lock<std::mutex> lock(m_mu);
                m_cv.wait(lock, [this](){ return m_shutdown or !m_task_queue.empty(); });
                if(m_shutdown and m_task_queue.empty()) return;
                auto task = std::move(m_task_queue.front());
                m_task_queue.pop();
                lock.unlock();
                task();
            }}};
}

template <typename Func, typename ... Args>
auto pool::enqueue(Func &&f, Args &&... args) -> std::future<typename std::result_of<Func(Args...)>::type>
{
    using return_type = typename std::result_of<Func(Args...)>::type;
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<Func>(f), std::forward<Args>(args)...));
    auto result = task->get_future();
    std::unique_lock<std::mutex> lock(m_mu);
    m_task_queue.emplace([task](){ (*task)(); });
    m_cv.notify_one();
    return result;
}

inline pool::~pool()
{
    std::unique_lock<std::mutex> lock(m_mu);
    m_shutdown = true;
    lock.unlock();
    m_cv.notify_all();
    std::for_each(m_workers.begin(), m_workers.end(), std::mem_fn(&std::thread::join));
}

}