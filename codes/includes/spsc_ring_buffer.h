#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

// Single-producer single-consumer ring buffer.
// - Lock-free for the common case.
// - Template parameter N must be power of two for mask optimization.
template <typename T, std::size_t N>
class SpscRingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be power of two");

public:
    // Head == consumer index, tail == producer index.
    SpscRingBuffer() : head_(0), tail_(0) {}

    // Enqueue; returns false if buffer is full.
    bool push(const T& value) {
        return emplace(value);
    }

    bool push(T&& value) {
        return emplace(std::move(value));
    }

    // Dequeue; returns empty optional if buffer is empty.
    std::optional<T> pop() {
        std::size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt;  // empty
        }
        T value = std::move(buffer_[head & mask()]);
        head_.store(head + 1, std::memory_order_release);
        return value;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    bool full() const noexcept {
        return size() == N;
    }

    std::size_t size() const noexcept {
        std::size_t head = head_.load(std::memory_order_acquire);
        std::size_t tail = tail_.load(std::memory_order_acquire);
        return tail - head;
    }

private:
    constexpr std::size_t mask() const noexcept { return N - 1; }

    template <typename U>
    bool emplace(U&& value) {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        std::size_t nextTail = tail + 1;
        if (nextTail - head_.load(std::memory_order_acquire) > N) {
            return false;  // full
        }
        buffer_[tail & mask()] = std::forward<U>(value);
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    T buffer_[N];
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;
};

