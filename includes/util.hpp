//
// Created by ganler-Mac on 2019/7/23.
//
#pragma once

//#define GANLER_DEBUG

#ifdef GANLER_DEBUG
#include <iostream>
#define GANLER_DEBUG_DETAIL(X) { std::cout << "line: "<< __LINE__ << "\t " << (#X) << " is " << (X) << '\n'; }
#else
#define GANLER_DEBUG_DETAIL(X) {}
#endif


#include <type_traits>

namespace thread_pools
{

template <typename Type, typename Func, typename ... Args>
inline void try_allocate(Type& task, Func&& f, Args&& ... args)
{
    try{
        task = new typename std::remove_pointer<Type>::type(std::bind(std::forward<Func>(f), std::forward<Args>(args)...));
    } catch(const std::bad_alloc& e)
    {
        std::cerr << "Bad allocation occured when newing a std::packaged_task: " << e.what() << '\n';
    } catch (const std::exception& e)
    {
        if(task != nullptr)
            delete task;
        std::cerr << "This exception has nothing to do with std::bad_alloc: " << e.what() << '\n';
    }
}

}