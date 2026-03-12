## ObjectPool：基于内存池的对象生命周期管理

`ObjectPool<T>` 是在 `MemoryPool` 之上的一层封装，目标是：

> 把“裸内存块”的管理，升级成对具体类型 `T` 的**构造 / 析构**管理。

它属于“资源管理工具”，而不是传统意义上的“容器”。

---

### 一、接口轮廓与角色定位

对应代码：`codes/includes/object_pool.h`

核心接口：

```cpp
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(std::size_t blocksPerChunk = 1024);

    template <typename... Args>
    T* create(Args&&... args);   // 在池内构造一个 T

    void destroy(T* obj) noexcept;  // 调用析构并归还内存

    std::size_t blockSize() const noexcept;
};
```

可以简单理解为：

- **内部持有一个 `MemoryPool`**，负责 raw memory。  
- 对外暴露“以 `T` 为中心”的接口：`create` / `destroy`。

---

### 二、内部实现：把 RAII 应用于“对象批量复用”

核心字段只有一个：

```cpp
MemoryPool pool_;
```

实现要点：

- `create`：

  ```cpp
  void* mem = pool_.allocate();
  return new (mem) T(std::forward<Args>(args)...);
  ```

  - 向底层池要一块 block；
  - 在这块内存上用 placement new 构造一个 `T`。

- `destroy`：

  ```cpp
  if (!obj) return;
  obj->~T();
  pool_.deallocate(obj);
  ```

  - 显式调用 `T` 的析构函数；
  - 把内存块归还给底层 `MemoryPool`。

可以看到，`ObjectPool` 本身并不维护“容器语义”：

- 不记录当前有哪些指针在外面；  
- 不关心对象的逻辑索引；  
- 只负责：**这块内存正在被某个 `T` 使用，还是已经回到 free list**。

---

### 三、使用示例与对比点

示例文件：`codes/src/object_pool_example.cpp`

对比的是：

- 使用 `std::make_unique<Widget>` 的普通堆分配；
- 使用 `ObjectPool<Widget>` 的池化分配。

关键片段：

```cpp
// Baseline: allocate Widgets via std::make_unique.
std::vector<std::unique_ptr<Widget>> v;
for (...) {
    v.push_back(std::make_unique<Widget>(...));
}

// Compare: allocate Widgets from ObjectPool.
ObjectPool<Widget> pool;
std::vector<Widget*> v;
for (...) {
    v.push_back(pool.create(...));
}
for (auto* p : v) {
    pool.destroy(p);
}
```

观察点：

- 分配 / 释放的次数相同；  
- `ObjectPool` 把一部分分配开销摊在 `MemoryPool` 的 chunk 分配上；  
- 指针所有权从“智能指针”变成“池管理 + 原始指针”，需要更小心。

---

### 四、适用场景与设计注意

适合的情况：

- 对象类型 `T` 很清晰，创建 / 销毁模式固定；  
- 存在**大量短生命周期对象**，频繁进出；  
- 更关注性能 / cache 友好性，而不是复杂的所有权关系。

不适合的情况：

- 需要复杂的生命周期管理（共享所有权、跨线程共享等）；  
- 对象大小变化大，或必须支持多种不相关的类型。

设计注意：

- **线程安全**：当前实现没有锁，多个线程同时使用同一个 `ObjectPool<T>` 会产生数据竞争。  
- **异常安全**：`create` 里如果构造函数抛异常，底层 `MemoryPool` 中这块内存会被视作“未使用”，下一次 `allocate` 仍会返回它。  
- **删除方式**：只能用 `destroy` 归还；`delete obj;` 会导致 UB。

从分层角度看：

- `MemoryPool` 管理“原始内存块”；  
- `ObjectPool<T>` 把“对象生命周期 + 内存复用”绑定在一起；  
- 上层业务可以在此之上再包一层类型安全 / 所有权更明确的封装。

---

### 五、小结：原理 & 使用场景

- **原理一句话**：  
  - 用底层 `MemoryPool` 管理 raw memory，`create` 在池内 placement new 出一个 `T`，`destroy` 调用析构并把这块内存归还池子，实现“对象批量复用”。  
- **典型使用场景**：  
  - 明确的对象类型、频繁创建 / 销毁、数量大但结构简单的对象群（如游戏实体、连接对象、任务节点等），并且可以接受“池管理 + 原始指针”的使用约束。

---

## 附：内存安全版本（Debug 检查 + RAII）做了什么？

对应代码：`codes/includes/object_pool_safe.h`（`ObjectPoolSafe<T>` / `PooledPtr<T>`）  
依赖：`codes/includes/memory_pool_safe.h`（`MemoryPoolSafe`）

这一版的核心思路是两层：

1) **底层内存块层面**：用 `MemoryPoolSafe` 在 Debug 下做 double-free / 跨池释放 / poison；  
2) **对象生命周期层面**：用 RAII handle（`PooledPtr<T>`）减少“忘记归还/重复归还”的人为错误空间。

### 1）做了哪些 effort

- **继承底层 mempool 的 Debug 检查**
  - `ObjectPoolSafe<T>::destroy(obj)` 最终会走到底层 `pool_.deallocate(obj)`。
  - 因此：  
    - double destroy（同一对象 destroy 两次）会被当作 double free 检测到；  
    - 把非本池对象传入 destroy，能更早以异常暴露（wrong-pool）；  
    - destroy 后内存会被 poison，UAF 更容易显性化。

- **提供 RAII 句柄：`PooledPtr<T>`**
  - `PooledPtr<T>` 在析构时自动调用 `pool_->destroy(ptr_)`。
  - 这把“归还对象”从“依赖人记住调用 destroy”变成“依赖作用域结束自动执行”。

- **move-only，避免复制导致的重复归还**
  - `PooledPtr` 禁止拷贝、只允许移动：
    - 拷贝会引入“两个 handle 指向同一对象” → 析构两次 → double destroy
    - move-only 把所有权语义做得更明确：同一时刻只有一个 handle 持有该对象

- **提供 `create_handle()` 作为推荐入口**
  - 相比 `create()` 返回裸 `T*`，`create_handle()` 直接返回 `PooledPtr<T>`，让“正确使用方式”成为默认路径。

### 2）原理解释：它把哪些风险从“运行时 UB”变成“更早暴露”

- **忘记归还（泄漏/耗尽池）**
  - 原版 `ObjectPool`：如果忘记 `destroy`，池内 block 永远不会回到 free list。
  - `PooledPtr`：离开作用域自动归还，减少“忘记调用”的概率。

- **重复归还（double destroy）**
  - 原版 `ObjectPool`：重复 destroy 直接进入 UB（free list 腐蚀）。
  - 安全版：  
    - `PooledPtr` 的 move-only 设计减少“同一对象被多个 owner 管理”的机会；  
    - 即便发生，底层 `MemoryPoolSafe` 在 Debug 下会检测到并抛异常。

- **use-after-free / use-after-release**
  - RAII 不能阻止别人把 `T*` 另存一份然后继续用，但：
    - poison 会让释放后的内容更快呈现异常值；
    - 搭配 sanitizer 更容易定位。

### 3）边界与现实取舍

- **它仍然不等价于“完全内存安全”**
  - 仍可能发生：越界写、UAF、跨线程并发访问等问题。
- **Debug/Release 的策略差异**
  - Debug 下靠检查尽早暴露；Release 下关闭检查以走快路径。
- **线程安全仍由上层保证**
  - 当前实现不加锁，多线程共享同一个 pool 会有数据竞争。

