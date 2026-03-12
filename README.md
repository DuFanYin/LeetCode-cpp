# LeetCode-cpp

## Solved Questions

### Easy

- Q1. Two Sum
- Q20. Valid Parentheses
- Q21. Merge Two Sorted Lists
- Q121. Best Time to Buy and Sell Stock

### Medium

- _TBD_

### Hard

- _TBD_

## C++ Notes

- [RAII：机制、理念与工具](cpp-notes/raii_mechanics_tools.md)
- [对象语义与生命周期体系](cpp-notes/value_semantics_system.md)
- [语言基础与内存模型](cpp-notes/fundamental_and_memory_model.md)
- [类型系统与多态体系](cpp-notes/type_system_and_polymorphism.md)
- [C++ 编译与构建流程](cpp-notes/cpp_build_pipeline.md)
- [STL 深度理解](cpp-notes/STL/stl_deep_understanding.md)
- [Toy STL vs 标准库 STL：我们离 `std::` 还有多远？](cpp-notes/STL/stl_toy_vs_std.md)

### Concurrency Notes

- [并发与系统级编程](cpp-notes/currency/concurrency_and_systems.md)
- [并发实践：线程与同步细节](cpp-notes/currency/currency_specific.md)
- [并发知识的四个层次概览](cpp-notes/currency/concurrency_layers_overview.md)
- [并发经典同步问题](cpp-notes/currency/concurrency_classic_problems.md)

### Infra / Performance Components

- **Code & headers**
  - [`MemoryPool` 头文件](codes/includes/memory_pool.h)
  - [`ObjectPool` 头文件](codes/includes/object_pool.h)
  - [`MemoryPoolSafe`（带 Debug 检查）](codes/includes/memory_pool_safe.h)
  - [`ObjectPoolSafe`（带 Debug 检查 + RAII handle）](codes/includes/object_pool_safe.h)
  - [`ThreadPool` 头文件](codes/includes/thread_pool.h)
  - [`SpscRingBuffer` 头文件](codes/includes/spsc_ring_buffer.h)
  - [性能对比示例与编译命令总览](codes/README.md)

- **Notes**
  - [MemoryPool：小对象内存池的工程用法](cpp-notes/infra/memory_pool.md)
  - [ObjectPool：基于内存池的对象生命周期管理](cpp-notes/infra/object_pool.md)
  - [ThreadPool：固定线程池与任务提交模型](cpp-notes/infra/thread_pool.md)
  - [SPSC Ring Buffer：单生产者单消费者环形队列](cpp-notes/infra/spsc_ring_buffer.md)
  - [MemoryPool vs ObjectPool：理念、职责与内存安全](cpp-notes/infra/mempool_vs_objectpool_safety.md)

### Data Structures (toy implementations)

- **Code & examples**
  - [`Vector<T>`](codes/includes/vector.h) + [example](codes/src/vector_example.cpp)
  - [`HashTable<K, V>`](codes/includes/hash_table.h) + [example](codes/src/hash_table_example.cpp)
  - [`LRUCache<K, V>`](codes/includes/lru_cache.h) + [example](codes/src/lru_cache_example.cpp)