#include <chrono>
#include <iostream>
#include <vector>

#include "../includes/memory_pool.h"

struct Foo {
    int x, y, z;
};

int main() {
    // Total objects to allocate in each run.
    constexpr std::size_t N = 1'000'000;

    // Baseline: raw new/delete allocation.
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<Foo*> ptrs;
        ptrs.reserve(N);
        for (std::size_t i = 0; i < N; ++i) {
            ptrs.push_back(new Foo{int(i), int(i), int(i)});
        }
        for (auto* p : ptrs) {
            delete p;
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "new/delete: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << " ms\n";
    }

    // Compare: same workload served from MemoryPool.
    {
        auto start = std::chrono::high_resolution_clock::now();
        MemoryPool pool(sizeof(Foo));
        std::vector<Foo*> ptrs;
        ptrs.reserve(N);
        for (std::size_t i = 0; i < N; ++i) {
            void* mem = pool.allocate();
            ptrs.push_back(new (mem) Foo{int(i), int(i), int(i)});
        }
        for (auto* p : ptrs) {
            p->~Foo();
            pool.deallocate(p);
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "MemoryPool: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << " ms\n";
    }
}

