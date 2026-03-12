#include <chrono>
#include <iostream>
#include <future>
#include <thread>
#include <vector>

#include "../includes/thread_pool.h"

int main() {
    // CPU-bound toy workload to keep threads busy.
    auto work = [](int n) {
        volatile int x = 0;
        for (int i = 0; i < n; ++i) {
            x += i;
        }
    };

    constexpr int TASKS = 100;    // number of logical tasks
    constexpr int N = 100000;     // work per task

    // Baseline: spawn one std::thread per task.
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads;
        threads.reserve(TASKS);
        for (int i = 0; i < TASKS; ++i) {
            threads.emplace_back(work, N);
        }
        for (auto& t : threads) {
            t.join();
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "Per-task thread: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << " ms\n";
    }

    // Compare: reuse a fixed ThreadPool for the same tasks.
    {
        auto start = std::chrono::high_resolution_clock::now();
        ThreadPool pool;
        std::vector<std::future<void>> futs;
        futs.reserve(TASKS);
        for (int i = 0; i < TASKS; ++i) {
            futs.push_back(pool.submit(work, N));
        }
        for (auto& f : futs) {
            f.get();
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "ThreadPool: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << " ms\n";
    }
}

