#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include "../includes/spsc_ring_buffer.h"

int main() {
    // Total messages to send from producer to consumer.
    constexpr std::size_t N = 10000000;

    // Baseline: std::queue guarded by a single mutex.
    {
        std::queue<int> q;
        std::mutex m;
        std::atomic<bool> done{false};

        auto start = std::chrono::high_resolution_clock::now();

        std::thread prod([&] {
            for (std::size_t i = 0; i < N; ++i) {
                std::lock_guard<std::mutex> lk(m);
                q.push(int(i));
            }
            done.store(true, std::memory_order_release);
        });

        std::thread cons([&] {
            std::size_t count = 0;
            for (;;) {
                {
                    std::lock_guard<std::mutex> lk(m);
                    if (!q.empty()) {
                        (void)q.front();
                        q.pop();
                        ++count;
                    } else if (done.load(std::memory_order_acquire) && q.empty()) {
                        break;
                    }
                }
            }
        });

        prod.join();
        cons.join();

        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "mutex queue: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << " ms\n";
    }

    // Compare: lock-free SPSC ring buffer with the same traffic.
    {
        SpscRingBuffer<int, 1 << 16> rb;
        std::atomic<bool> done{false};

        auto start = std::chrono::high_resolution_clock::now();

        std::thread prod([&] {
            for (std::size_t i = 0; i < N; ++i) {
                while (!rb.push(int(i))) {
                    // busy-wait; in real systems consider backoff/parking
                }
            }
            done.store(true, std::memory_order_release);
        });

        std::thread cons([&] {
            std::size_t count = 0;
            for (;;) {
                if (auto v = rb.pop()) {
                    ++count;
                } else if (done.load(std::memory_order_acquire) && rb.empty()) {
                    break;
                }
            }
        });

        prod.join();
        cons.join();

        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "SPSC ring buffer: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << " ms\n";
    }
}

