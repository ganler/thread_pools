cmake_minimum_required(VERSION 3.14)
project(thread_pools)

set(CMAKE_CXX_STANDARD 14)

include_directories(includes)

add_executable(${PROJECT_NAME} benchmark.cpp)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads)