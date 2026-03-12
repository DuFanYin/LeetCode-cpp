#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include "memory_pool.h"

// Simple object pool built on top of MemoryPool.
// - T must be destructible.
// - Not thread-safe.
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(std::size_t blocksPerChunk = 1024)
        : pool_(sizeof(T), blocksPerChunk) {}

    template <typename... Args>
    // Construct T in-place and return a raw pointer owned by the pool.
    T* create(Args&&... args) {
        void* mem = pool_.allocate();
        return new (mem) T(std::forward<Args>(args)...);
    }

    // Call destructor and release storage back to the underlying pool.
    void destroy(T* obj) noexcept {
        if (!obj) return;
        obj->~T();
        pool_.deallocate(obj);
    }

    std::size_t blockSize() const noexcept { return pool_.blockSize(); }

private:
    MemoryPool pool_;
};

