#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <vector>

// Simple fixed-size block memory pool.
// - Not thread-safe: wrap with external synchronization if needed.
// - Intended to replace many small heap allocations of equal size.
class MemoryPool {
public:
    MemoryPool(std::size_t blockSize, std::size_t blocksPerChunk = 1024)
        : blockSize_(alignBlockSize(blockSize)),
          blocksPerChunk_(blocksPerChunk),
          freeList_(nullptr) {
        if (blockSize_ == 0 || blocksPerChunk_ == 0) {
            throw std::invalid_argument("MemoryPool: blockSize and blocksPerChunk must be > 0");
        }
    }

    ~MemoryPool() {
        for (void* chunk : chunks_) {
            ::operator delete(chunk);
        }
    }

    // Get one fixed-size block.
    void* allocate() {
        if (!freeList_) {
            allocateChunk();
        }
        Node* node = freeList_;
        freeList_ = freeList_->next;
        return node;
    }

    // Return a block previously obtained from allocate().
    void deallocate(void* p) noexcept {
        if (!p) return;
        auto* node = static_cast<Node*>(p);
        node->next = freeList_;
        freeList_ = node;
    }

    std::size_t blockSize() const noexcept { return blockSize_; }

private:
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

    void allocateChunk() {
        const std::size_t chunkSize = blockSize_ * blocksPerChunk_;
        void* raw = ::operator new(chunkSize);
        chunks_.push_back(raw);

        std::uint8_t* start = static_cast<std::uint8_t*>(raw);
        for (std::size_t i = 0; i < blocksPerChunk_; ++i) {
            auto* node = reinterpret_cast<Node*>(start + i * blockSize_);
            node->next = freeList_;
            freeList_ = node;
        }
    }

    std::size_t blockSize_;
    std::size_t blocksPerChunk_;
    std::vector<void*> chunks_;
    Node* freeList_;
};
