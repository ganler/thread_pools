#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <thread_pools.hpp>

#define out              std::cout
#define PY_IMPORT_SCRIPT "import matplotlib.pyplot as plt\n\n"
#define PY_PLOT_SCRIPT   "plt.xlabel(\'task num\')\nplt.ylabel(\'time/milliseconds\')\n"\
                         "plt.xscale(\'log\')\nplt.grid()\nplt.savefig(\"benchmark.png\")\nplt.show()\n"
#define GEN_BENCH(X)     { \
                          out << #X << " = [";\
                          for(auto && x : (X))\
                             out << x << ',';\
                          out << "\b]\n"; }
#define PYPLOT(X,Y)      {out << "plt.plot(" << #X << ", " << #Y << ")\n"; }
#define ADD_PYLINE(X)    {out << X << '\n';}

int main() {
    // x*y = 10000
    // x/y = 10
    // proportion = mission_sz / psize ~ 10^{-2, -1, 0, 1, 2}
    // total_amount = mission_sz * psize
    // mission_sz = √proportion * total_amount
    // psize = √total_amount / proportion

    constexpr std::size_t psize = 1000;
//    constexpr std::size_t mission_sz = 1;    // 10, 100, 1000, 10000, 100000
    constexpr std::size_t execute_time = 1; // milli.
    constexpr std::size_t scale = 6;

    std::vector<double> answers[3];

    auto bench = [&](std::size_t mission_sz){
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        std::cout << ">>> Bench : mission size = " << mission_sz << '\n';
        {   // TEST for thread_pools::spool
            auto beg = std::chrono::steady_clock::now();
            {
                thread_pools::spool<psize> pool;
                for (int i = 0; i < mission_sz; ++i)
                    pool.enqueue([execute_time](){ std::this_thread::sleep_for(std::chrono::milliseconds(execute_time)); });
            }
            answers[0].push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - beg).count());
            std::cout << "[STATIC POOL TEST] "  << psize << " threads execute time: " << answers[0].back() << " ms\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        {   // TEST for thread_pools::pool
            auto beg = std::chrono::steady_clock::now();
            {
                thread_pools::pool pool(psize);
                for (int i = 0; i < mission_sz; ++i)
                    pool.enqueue([execute_time](){ std::this_thread::sleep_for(std::chrono::milliseconds(execute_time)); });
            }
            answers[1].push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - beg).count());
            std::cout << "[DEFAULT POOL TEST] "  << psize << " threads execute time: " << answers[1].back() << " ms\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        {   // TEST for thread_pools::dpool
            auto beg = std::chrono::steady_clock::now();
            {
                thread_pools::dpool pool(psize); // max_pool_size
                for (int i = 0; i < mission_sz; ++i)
                    pool.enqueue([execute_time](){ std::this_thread::sleep_for(std::chrono::milliseconds(execute_time)); });
            }
            answers[2].push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - beg).count());
            std::cout << "[DYNAMIC POOL TEST] "  << psize << " threads execute time: " << answers[2].back() << " ms\n";
        }
    };

    std::vector<int> x;
    for (int i = 0; i < scale; ++i)
    {
        x.push_back(std::pow(10, i));
        bench(x.back());
    }

    std::cout << ">>> Generating python code\n";


    // Generate code.
    auto& spool = answers[0];
    auto& pool = answers[1];
    auto& dpool = answers[2];

    out << PY_IMPORT_SCRIPT;
    GEN_BENCH(x)
    GEN_BENCH(spool)
    PYPLOT(x, spool)
    GEN_BENCH(pool)
    PYPLOT(x, pool)
    GEN_BENCH(dpool)
    PYPLOT(x, dpool)
    ADD_PYLINE("plt.legend(['spool', 'pool', 'dpool'])")

    out << PY_PLOT_SCRIPT;
}