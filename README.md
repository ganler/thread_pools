# Thread Pool Library

## Quick Start

```c++
#include <thread_pools.hpp>
#include <iostream>

int main()
{
    thread_pools::spool<10> pool;
    // thread_pools::pool pool(10);  // default pool
    // thread_pools::dpool dpool;    // dynamic pool

    auto result = pool.enqueue([]() { return 2333; });
    std::cout << result.get() << '\n';
}
```

## Introduction

In the last 2 days, I implemented 3 kinds of thread pools:
- **thread_pools::spool<Sz>**: Static pool which holds fixed number of threads in std::array<std::thread, Sz>.
- **thread_pools::pool**: Default pool which holds fixed number of threads in std::vector<std::thread>.
- **thread_pools::dpool**: Dynamic pool which holds unfixed number of threads in std::unordered_map<thread_index, std::thread>(This is very efficient).

These kinds of pools can be qualified with different tasks according to user's specific situations.

The `spool` and `pool` are nearly the same, as their only difference is that they use different containers.

However, I myself design the growing strategy of the dynamic pool. For a `dpool(J, I, K)`:

- Dynamically adapt to the task size;
- The total threads are no more than K(Set by the user);
- The number of idle threads are no more than I(Set by the user);
- Create J threads when constructing.(Please make sure: J <= I && I < K && K > 0, or there will be a `std::logic_error`)

## Benchmark

```shell
mkdir build && cd build
cmake .. && make -j4 && ./thread_pools
```

Copy the output python script and run it. You can see the results.
(Note that each task cost 10ms. For each pool, it can at most create `psize` threads to finish `mission_sz` tasks.)
![](images/benchmark.png)

## How I designed the dynamic pool

![](images/thread_theory.png)

## TODOS

- Thread pool that holds fixed kind of functions. (Reduce dynamic allocation when std::function<void()> SOO cannot work)
- Replace shared_ptr<std::packaged_task<...>> with exception-safety wrapped raw pointer.
- Lock-free thread pool.
