#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include "memory_pool_safe.h"

template <typename T>
class ObjectPoolSafe;

// RAII handle: when destroyed, automatically returns the object to the pool.
// Move-only; copying is disabled.
template <typename T>
class PooledPtr {
public:
    PooledPtr() noexcept : pool_(nullptr), ptr_(nullptr) {}

    PooledPtr(ObjectPoolSafe<T>* pool, T* ptr) noexcept : pool_(pool), ptr_(ptr) {}

    ~PooledPtr() {
        if (pool_ && ptr_) {
            pool_->destroy(ptr_);
            ptr_ = nullptr;
        }
    }

    PooledPtr(PooledPtr&& other) noexcept
        : pool_(other.pool_), ptr_(other.ptr_) {
        other.pool_ = nullptr;
        other.ptr_ = nullptr;
    }

    PooledPtr& operator=(PooledPtr&& other) noexcept {
        if (this != &other) {
            if (pool_ && ptr_) {
                pool_->destroy(ptr_);
            }
            pool_ = other.pool_;
            ptr_ = other.ptr_;
            other.pool_ = nullptr;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    PooledPtr(const PooledPtr&) = delete;
    PooledPtr& operator=(const PooledPtr&) = delete;

    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    ObjectPoolSafe<T>* pool_;
    T* ptr_;
};

// Object pool with debug-time safety: built on MemoryPoolSafe so that
// double free, use-after-free (poison), and wrong-pool deallocate are
// detected when built in Debug (NDEBUG not defined).
// - T must be destructible.
// - Not thread-safe.
// - Use create_handle() for RAII ownership.
template <typename T>
class ObjectPoolSafe {
public:
    explicit ObjectPoolSafe(std::size_t blocksPerChunk = 1024)
        : pool_(sizeof(T), blocksPerChunk) {}

    template <typename... Args>
    T* create(Args&&... args) {
        void* mem = pool_.allocate();
        return new (mem) T(std::forward<Args>(args)...);
    }

    /// Returns a RAII handle that calls destroy in its destructor.
    template <typename... Args>
    PooledPtr<T> create_handle(Args&&... args) {
        return PooledPtr<T>(this, create(std::forward<Args>(args)...));
    }

    // Call destructor and release storage. In debug build may throw on
    // double destroy or pointer not from this pool.
    void destroy(T* obj) {
        if (!obj) return;
        obj->~T();
        pool_.deallocate(obj);
    }

    std::size_t blockSize() const noexcept { return pool_.blockSize(); }
    bool debug() const noexcept { return pool_.debug(); }

private:
    MemoryPoolSafe pool_;
};
