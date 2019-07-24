//
// Created by ganler-Mac on 2019/7/20.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <stack>
#include <condition_variable>
#include <exception>
#include <unordered_map>
#include <mutex>
#include <list>
#include <queue>
#include <thread>
#include <algorithm>

#include "util.hpp"

namespace thread_pools
{

class dpool final
{
public:
    dpool(std::size_t = no_input, std::size_t = no_input, std::size_t = std::thread::hardware_concurrency());
    ~dpool();
    template <std::size_t Sz, typename Func, typename ... Args>
    auto enqueue(Func&& f, Args&& ... args) -> std::future<typename std::result_of<Func(Args...)>::type>;
    template <typename Func, typename ... Args>
    auto enqueue(Func&& f, Args&& ... args) -> std::future<typename std::result_of<Func(Args...)>::type>;
    std::size_t current_threads() noexcept;
private:
    static constexpr std::size_t no_input        = std::numeric_limits<std::size_t>::max();
    using                        task_type       = std::function<void()>;
    using                        thread_index    = std::thread::id;
    using                        thread_map      = std::unordered_map<thread_index, std::thread>;

    const std::size_t            m_max_size;
    const std::size_t            m_max_idle_size;
    std::atomic<std::size_t>     m_idle_num;
    std::mutex                   m_mu;
    std::condition_variable      m_cv;
    thread_map                   m_workers;
    std::queue<task_type>        m_task_queue;
    bool                         m_shutdown      = false;

    // Helper
    void make_worker();                          // Called by locked function.
    void destroy_worker(thread_index);           // Called by locked function.
};


// Implementation
inline std::size_t dpool::current_threads() noexcept
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_workers.size();
}

inline dpool::dpool(std::size_t pre_alloc, std::size_t max_idle_sz, std::size_t max_sz)
: m_max_size(max_sz), m_max_idle_size( max_idle_sz == no_input ? max_sz / 2 : max_idle_sz)
{
    pre_alloc = (pre_alloc == no_input) ? m_max_idle_size : pre_alloc;
    if(pre_alloc > m_max_idle_size || m_max_idle_size >= m_max_size || m_max_size == 0)
        throw std::logic_error("Please make sure: pre_alloc <= max_idle_sz && max_idle_sz < max_size && max_sz > 0\n");
    m_workers.reserve(pre_alloc);
    for (int i = 0; i < pre_alloc; ++i)
        make_worker();
    m_idle_num = pre_alloc;
}

inline void dpool::destroy_worker(thread_index index)
{
    m_task_queue.push([this, index](){
        GANLER_DEBUG_DETAIL(m_workers[index].joinable())
        if(!m_shutdown)
        {
            m_workers[index].join();
            m_workers.erase(index);
        }
    });
}

inline void dpool::make_worker()
{
    auto thread = std::thread{[this](){
        while(true)
        {
            std::unique_lock<std::mutex> lock(m_mu);
            m_cv.wait(lock, [this](){ return m_shutdown or !m_task_queue.empty() or m_idle_num > m_max_idle_size; });
            if(m_shutdown and m_task_queue.empty()) { return; } // Game over.
            if(m_idle_num > m_max_idle_size and !m_shutdown)
            {// Cut the idle threads.
                destroy_worker(std::this_thread::get_id());
                return;
            }
            --m_idle_num;
            auto task = std::move(m_task_queue.front());
            m_task_queue.pop();
            lock.unlock(); // LOCK IS OVER.
            task();
            ++m_idle_num;  // ATOMIC
        }
    }};
    ++m_idle_num;
    m_workers[thread.get_id()] = std::move(thread);
}

inline dpool::~dpool()
{
    std::unique_lock<std::mutex> lock(m_mu);
    m_shutdown = true;
    lock.unlock();
    m_cv.notify_all(); // TODO: If I add this code, I found it sometimes become 3x slower. Why?
    std::for_each(m_workers.begin(), m_workers.end(), [](thread_map::value_type& x){ (x.second).join(); });
}

template <std::size_t Sz, typename Func, typename ... Args>
auto dpool::enqueue(Func &&f, Args &&... args) -> std::future<typename std::result_of<Func(Args...)>::type>
{
    using return_type = typename std::result_of<Func(Args...)>::type;
    std::packaged_task<return_type()>* task = nullptr; // Why raw pointer? See comments in static_pool.hpp's enqueue().
    try_allocate(task, std::forward<Func>(f), std::forward<Args>(args)...);
    auto result = task->get_future();
    if(m_idle_num == 0)
        for (int i = 0; i < std::min(Sz, m_max_idle_size); ++i)
            make_worker();
    std::unique_lock<std::mutex> lock(m_mu);
    m_task_queue.emplace([task](){ (*task)(); delete task; });
    m_cv.notify_one();
    return result;
}

template <typename Func, typename ... Args>
auto dpool::enqueue(Func &&f, Args &&... args) -> std::future<typename std::result_of<Func(Args...)>::type>
{
    using return_type = typename std::result_of<Func(Args...)>::type;
    static std::size_t queue_size = 0;
    std::packaged_task<return_type()>* task = nullptr; // Why raw pointer? See comments in static_pool.hpp's enqueue().
    try_allocate(task, std::forward<Func>(f), std::forward<Args>(args)...);
    auto result = task->get_future();
    GANLER_DEBUG_DETAIL(current_threads())
    if(m_idle_num == 0)
        for (int i = 0; i < std::min(std::max(1ul, queue_size), m_max_idle_size); ++i)
            make_worker();
    std::unique_lock<std::mutex> lock(m_mu);
    m_task_queue.emplace([task](){ (*task)(); delete task; });
    queue_size = m_task_queue.size();
    m_cv.notify_one();
    return result;
}

}
