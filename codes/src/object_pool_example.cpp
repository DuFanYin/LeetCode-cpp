#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

#include "../includes/object_pool.h"

struct Widget {
    int a;
    double b;
    Widget(int x, double y) : a(x), b(y) {}
};

int main() {
    // Number of Widget instances created in each variant.
    constexpr std::size_t N = 500000;

    // Baseline: allocate Widgets via std::make_unique.
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::unique_ptr<Widget>> v;
        v.reserve(N);
        for (std::size_t i = 0; i < N; ++i) {
            v.push_back(std::make_unique<Widget>(int(i), double(i)));
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "unique_ptr: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << " ms\n";
    }

    // Compare: allocate Widgets from ObjectPool.
    {
        auto start = std::chrono::high_resolution_clock::now();
        ObjectPool<Widget> pool;
        std::vector<Widget*> v;
        v.reserve(N);
        for (std::size_t i = 0; i < N; ++i) {
            v.push_back(pool.create(int(i), double(i)));
        }
        for (auto* p : v) {
            pool.destroy(p);
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "ObjectPool: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << " ms\n";
    }
}


