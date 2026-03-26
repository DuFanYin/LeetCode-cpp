# Learning Materials

A growing C++ and systems-programming notebook: LeetCode solutions, deeper notes, and small runnable experiments that evolve over time as I learn.

## Notes

### foundations

- [RAII: mechanisms, ideas, and tools](notes/foundations/raii-mechanics-tools.md)
- [Value semantics and object lifetime system](notes/foundations/value-semantics-system.md)
- [Language fundamentals and memory model](notes/foundations/fundamental-memory-model.md)
- [Type system and polymorphism](notes/foundations/type-system-polymorphism.md)
- [OOP overall concepts](notes/foundations/oop-overall.md)
- [Static vs dynamic polymorphism](notes/foundations/static-vs-dynamic-polymorphism.md)

### build-runtime

- [C++ compile and build pipeline](notes/build-runtime/cpp-build-pipeline.md)
- [From source code to CPU: a compiler-centric view](notes/build-runtime/compiler-to-cpu-mental-model.md)
- [C++ Program Lifecycle: From Executable to Page Fault](notes/build-runtime/cpp-program-lifecycle.md)

### concurrency

- [Concurrency and systems programming](notes/concurrency/concurrency-systems.md)
- [Practical concurrency: threads and synchronization](notes/concurrency/threads-synchronization.md)
- [Four layers of concurrency knowledge](notes/concurrency/concurrency-layers-overview.md)
- [C++ memory order and reordering](notes/concurrency/memory-order.md)
- [Classic synchronization problems](notes/concurrency/classic-synchronization-problems.md)
- [Common concurrency challenges (checklist)](notes/concurrency/common-concurrency-challenges.md)

### stl-generic

- [Deep understanding of STL](notes/stl-generic/stl-deep-understanding.md)
- [Toy STL vs standard STL: how far are we from `std::`?](notes/stl-generic/stl-toy-vs-std.md)

### infra-performance

- [MemoryPool: small-object pool in practice](notes/infra-performance/memory-pool.md)
- [ObjectPool: object lifetime management on a pool](notes/infra-performance/object-pool.md)
- [MemoryPool vs ObjectPool: design, responsibilities, and safety](notes/infra-performance/mempool-vs-objectpool-safety.md)
- [ThreadPool: fixed thread pool and task submission](notes/infra-performance/thread-pool.md)
- [SPSC Ring Buffer: single-producer single-consumer queue](notes/infra-performance/spsc-ring-buffer.md)
- [Ring buffer vs mutex queue (in-depth)](notes/infra-performance/ring-buffer-vs-mutex-queue.md)
- [CPU cache efficiency for HFT (latency context)](notes/infra-performance/cpu-cache-efficiency-hft.md)
- [Cache organization: L1/L2/L3 and set-associative mapping](notes/infra-performance/cache-organization-set-associative.md)

### operating-system

- [Study plan](notes/operating-system/study-plan.md)
- [Week 1: Virtual Memory](notes/operating-system/week1-virtual-memory.md)
- [Week 2: Syscall Trap](notes/operating-system/week2-syscall-trap.md)
- [Week 3: Process Scheduling](notes/operating-system/week3-process-scheduling.md)
- [Week 4: Locking & IO](notes/operating-system/week4-locking-io.md)

### distributed-systems

- [gRPC: concepts, architecture, and usage](notes/distributed-systems/grpc-overview.md)

## Codes

```text
codes/
├── includes/
│   ├── memory_pool.h
│   ├── memory_pool_safe.h
│   ├── object_pool.h
│   ├── object_pool_safe.h
│   ├── thread_pool.h
│   ├── spsc_ring_buffer.h
│   ├── vector.h
│   ├── hash_table.h
│   └── lru_cache.h
└── src/
    ├── infra/
    │   ├── memory_pool_example.cpp     # new/delete vs MemoryPool
    │   ├── object_pool_example.cpp     # std::make_unique vs ObjectPool
    │   ├── thread_pool_example.cpp     # per-task threads vs ThreadPool
    │   └── spsc_example.cpp            # mutex queue vs SpscRingBuffer
    ├── perf/
    │   └── cache_alignment_example.cpp # non-aligned vs cache-aligned counters
    ├── ds/
    │   ├── vector_example.cpp          # toy Vector<T> usage
    │   ├── hash_table_example.cpp      # toy HashTable<K, V> usage
    │   └── lru_cache_example.cpp       # toy LRUCache<K, V> usage
    ├── concurrency/
    │   └── memory_order_example.cpp    # memory_order semantics demo
    └── oop/
        ├── dynamic_virtual_example.cpp       # virtual dispatch / runtime polymorphism
        └── static_polymorphism_example.cpp   # template-based static polymorphism
```

## Solved Questions

### Easy

- Q1. Two Sum
- Q20. Valid Parentheses
- Q21. Merge Two Sorted Lists
- Q121. Best Time to Buy and Sell Stock
- Q125. Valid Palindrome
- Q226. Invert Binary Tree
- Q242. Valid Anagram
- Q704. Binary Search
- Q733. Flood Fill
- Q141. Linked List Cycle

### Medium

- _TBD_

### Hard

- _TBD_