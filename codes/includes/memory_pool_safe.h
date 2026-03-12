#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <set>
#include <vector>

// Memory pool with optional debug-time safety checks.
// - Template parameter Debug: when true, enables poisoning, double-free
//   and cross-pool deallocate detection; when false, behavior and
//   layout match the unsafe MemoryPool (zero extra cost).
// - Not thread-safe.
class MemoryPoolSafeBase {
public:
    MemoryPoolSafeBase(std::size_t blockSize, std::size_t blocksPerChunk, bool debug)
        : blockSize_(alignBlockSize(blockSize)),
          blocksPerChunk_(blocksPerChunk),
          freeList_(nullptr),
          debug_(debug) {
        if (blockSize_ == 0 || blocksPerChunk_ == 0) {
            throw std::invalid_argument("MemoryPoolSafe: blockSize and blocksPerChunk must be > 0");
        }
    }

    ~MemoryPoolSafeBase() {
        for (void* chunk : chunks_) {
            ::operator delete(chunk);
        }
    }

    void* allocate() {
        if (!freeList_) {
            allocateChunk();
        }
        Node* node = freeList_;
        freeList_ = freeList_->next;
        if (debug_) {
            allocated_.insert(node);
        }
        return node;
    }

    void deallocate(void* p) noexcept(false) {
        if (!p) return;
        auto* node = static_cast<Node*>(p);
        if (debug_) {
            if (allocated_.count(node) == 0) {
                if (belongsToPool(node)) {
                    // Double free: pointer was already deallocated.
                    throw std::logic_error("MemoryPoolSafe: double free detected");
                } else {
                    throw std::invalid_argument("MemoryPoolSafe: pointer not from this pool");
                }
            }
            allocated_.erase(node);
            // Poison freed memory so use-after-free is more visible.
            std::memset(node, 0xDD, blockSize_);
        }
        node->next = freeList_;
        freeList_ = node;
    }

    std::size_t blockSize() const noexcept { return blockSize_; }
    bool debug() const noexcept { return debug_; }

protected:
    struct Node {
        Node* next;
    };

    static std::size_t alignBlockSize(std::size_t sz) {
        if (sz < sizeof(Node)) {
            sz = sizeof(Node);
        }
        constexpr std::size_t align = alignof(std::max_align_t);
        return (sz + align - 1) & ~(align - 1);
    }

    bool belongsToPool(void* p) const noexcept {
        const std::uint8_t* ptr = static_cast<std::uint8_t*>(p);
        for (void* chunk : chunks_) {
            std::uint8_t* start = static_cast<std::uint8_t*>(chunk);
            std::uint8_t* end = start + blockSize_ * blocksPerChunk_;
            if (ptr >= start && ptr < end) {
                return true;
            }
        }
        return false;
    }

    void allocateChunk() {
        const std::size_t chunkSize = blockSize_ * blocksPerChunk_;
        void* raw = ::operator new(chunkSize);
        chunks_.push_back(raw);

        std::uint8_t* start = static_cast<std::uint8_t*>(raw);
        for (std::size_t i = 0; i < blocksPerChunk_; ++i) {
            auto* n = reinterpret_cast<Node*>(start + i * blockSize_);
            n->next = freeList_;
            freeList_ = n;
        }
    }

    std::size_t blockSize_;
    std::size_t blocksPerChunk_;
    std::vector<void*> chunks_;
    Node* freeList_;
    bool debug_;
    std::set<void*> allocated_;
};

// Debug build: safety checks on. Release build: no checks, same as MemoryPool.
class MemoryPoolSafe : public MemoryPoolSafeBase {
public:
    MemoryPoolSafe(std::size_t blockSize, std::size_t blocksPerChunk = 1024)
        : MemoryPoolSafeBase(blockSize, blocksPerChunk,
#ifndef NDEBUG
                            true
#else
                            false
#endif
        ) {}
};
