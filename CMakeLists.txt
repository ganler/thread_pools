cmake_minimum_required(VERSION 3.5.1)
project(thread_pools)

set(CMAKE_CXX_STANDARD 11)
include_directories(includes)

add_executable(thread_pools benchmark.cpp includes/thread_pools.hpp includes/dynamic_pool.hpp includes/static_pool.hpp includes/default_pool.hpp includes/util.hpp)