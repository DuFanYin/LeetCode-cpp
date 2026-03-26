## MemoryPool：小对象内存池的工程用法

这个 `MemoryPool` 更偏工程基础设施，而不是“业务数据结构”。它解决的核心问题是：

> 当有大量**大小相同、生命周期类似的小对象**需要频繁分配 / 释放时，  
> 直接用 `new/delete` 容易导致堆碎片和分配开销过大。

可以把它理解为：**针对固定大小 block 的“专用堆”**。

---

### 一、接口轮廓与关键约束

对应代码：`codes/includes/memory_pool.h`

核心接口非常小：

```cpp
class MemoryPool {
public:
    MemoryPool(std::size_t blockSize,
               std::size_t blocksPerChunk = 1024);

    void* allocate();              // 获取一块固定大小的内存
    void  deallocate(void* p) noexcept;  // 归还这块内存

    std::size_t blockSize() const noexcept;
};
```

- **blockSize**：每个 block 的大小，会对齐到 `std::max_align_t`。  
- **blocksPerChunk**：每次向系统申请的 block 数量（一次性大块，再拆成小块挂到 free list）。  
- **不做线程安全**：调用方需要自己加锁或使用 TLS 等手段。

---

### 二、内部结构：chunk + free list

结构可以抽象成两层：

- **chunks_**：`std::vector<void*>`，记录向系统申请的每一大块内存（chunk）。  
- **freeList_**：单向链表，每个节点就是一个可用的 block。

关键点：

- 每次 `allocateChunk()`：
  - 调用一次 `::operator new(chunkSize)`。
  - 把这块大内存切成 `blocksPerChunk_` 个等大小 block。
  - 把这些 block 串成单向链表，挂到 `freeList_` 上。
- `allocate()`：
  - 如果 `freeList_` 为空，先扩容（再从系统要一块新 chunk）。  
  - 弹出链表头结点，返回它的地址给调用方。
- `deallocate(p)`：
  - 把 `p` 强转成 `Node*`，头插回 `freeList_`。

这一套结构保证了：

- **从第二次分配开始**，大部分 `allocate()` / `deallocate()` 都只是在链表头上做 O(1) 操作。  
- 减少了频繁触发全局堆管理器的次数。

---

### 三、和直接 `new/delete` 的对比场景

在 `codes/src/memory_pool_example.cpp` 中，对比了两种路径：

```cpp
// Baseline: raw new/delete allocation.
for (...) {
    ptrs.push_back(new Foo{...});
}
for (auto* p : ptrs) {
    delete p;
}

// Compare: same workload served from MemoryPool.
MemoryPool pool(sizeof(Foo));
for (...) {
    void* mem = pool.allocate();
    ptrs.push_back(new (mem) Foo{...});
}
for (auto* p : ptrs) {
    p->~Foo();
    pool.deallocate(p);
}
```

典型适用场景：

- 大量短生命周期的小对象（如游戏中的粒子、临时组件）。  
- 对象大小固定，且类型单一或有限。  
- 频繁分配 / 回收，标准堆分配器带来的碎片与 metadata 开销明显。

---

### 四、设计取舍与注意事项

- **不负责调用构造 / 析构**：  
  - `MemoryPool` 返回的是原始内存，真正构造 / 析构由上层负责（示例中用 placement new + 手动析构）。  
- **不记录所有权关系**：  
  - 谁分配谁释放，没有引用计数或 GC。  
- **不做调试安全检查**：  
  - 不检测 double free / 跨池释放等 UB，默认调用方严格自律。  
- **面向“固定大小”的设计**：  
  - block 大小一旦确定，整个生命周期都不再变化。

这使得 `MemoryPool` 更适合作为**底层构件**，由上层封装更安全的接口（例如 `ObjectPool<T>`）。

---

### 五、小结：原理 & 使用场景

- **原理一句话**：  
  - 预先按固定大小从系统申请大块内存 → 切成很多小 block → 用单向 free list 管理可用块，之后 `allocate/deallocate` 只在链表头 O(1) 操作。
- **典型使用场景**：  
  - 大量同尺寸小对象、频繁分配 / 回收、对性能和碎片比较敏感的模块（游戏对象、临时组件、短生命周期 handle 等）。

---

## 附：内存安全版本（Debug 检查）做了什么？

对应代码：`codes/includes/memory_pool_safe.h`（`MemoryPoolSafe` / `MemoryPoolSafeBase`）

这一版的目标不是“彻底保证内存安全”（那需要更强的抽象），而是在 **Debug 构建** 下尽可能让常见错误“更早、更明确地暴露”。

### 1）做了哪些 effort（Debug 下启用）

- **double-free 检测**
  - 思路：维护一个“当前已分配块集合”（`allocated_`）。
  - `allocate()`：把返回的 block 记入 `allocated_`。
  - `deallocate(p)`：如果 `p` 不在 `allocated_`，则说明不是“当前已分配块”，可能是 double free 或跨池释放。

- **跨池释放检测（wrong-pool deallocate）**
  - 当 `p` 不在 `allocated_` 时，再检查 `p` 是否落在本 pool 的任一 chunk 地址范围内（`belongsToPool`）。
  - 若不属于本 pool 的地址范围，则抛出 “pointer not from this pool”。

- **poison（释放后填充特殊字节）**
  - `deallocate(p)` 时用 `0xDD` 填充整个 block。
  - 目的：把 use-after-free 从“悄悄读到旧数据”变成“更容易观察到异常值/更快触发崩溃或断言”（配合 sanitizer 效果更明显）。

### 2）原理解释：为什么这些检查能抓住问题

- **double free** 的本质：同一块内存被“归还两次”，会破坏 free list 结构，后续分配可能重复返回同一块内存。
  - `allocated_` 把“是否处于借出状态”显式记录下来，所以第二次归还能被立刻识别。

- **跨池释放** 的本质：把不属于本 pool 的指针塞进 free list，后续 `allocate()` 会返回一块“不在自己管理范围内”的内存，直接进入 UB。
  - `belongsToPool` 通过 chunk 地址范围判断，把这类错误提前变成异常。

- **poison** 并不能阻止 UAF，但能让 UAF 的后果更显性：
  - 例如对象字段被 0xDD 覆盖后更容易在日志/断言/条件判断里暴露异常路径。

### 3）Release 下的取舍

`MemoryPoolSafe` 在 `NDEBUG`（Release）下会关闭上述检查：

- 不再维护 `allocated_`（避免 O(log n) 集合操作开销）
- 不做 poison（避免额外 `memset`）

因此它仍然是“更快但更危险”的 infra：  
**Debug 追求尽早暴露错误，Release 追求最短 fast path。**

