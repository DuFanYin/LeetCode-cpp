## Toy STL vs 标准库 STL：我们离 `std::` 还有多远？

`codes/includes/` 里这三份“玩具实现”：

- `Vector<T>`：`codes/includes/vector.h`
- `HashTable<K, V>`：`codes/includes/hash_table.h`
- `LRUCache<K, V>`：`codes/includes/lru_cache.h`

它们的定位是：**帮助你把 STL 容器背后的关键思想“写出来、跑起来、踩过坑”**。  
但它们不等价于标准库容器，差距主要体现在：**接口完整性、复杂度保证、异常保证、迭代器体系、allocator 与内存模型、以及大量边角行为**。

下面按“教学视角”把差距列清楚：你在 toy 版里学到什么？要达到 `std::` 需要补哪些能力？

---

### 0. 一句话总览：`std::` 的价值是什么

标准库容器的价值不只是“能存数据”，而是：

- **接口合同很清晰**：复杂度、异常保证、迭代器失效规则都有规范。
- **对泛型/类型系统非常友好**：iterator + allocator + traits 体系。
- **可移植且高度优化**：针对小对象、移动、分支、cache、平台 ABI 都做了工程级打磨。

toy 版通常只覆盖“核心数据结构”，把上面这些“合同层”省略了。

---

## 一、`Vector<T>` vs `std::vector<T>`

对应实现：`codes/includes/vector.h`

### 1.1 你在 toy `Vector` 里学到的

- 连续内存（contiguous storage）与随机访问 O(1)
- `size` / `capacity` / 扩容（reallocate + move）
- 元素构造/析构与异常路径（基本保证）

### 1.2 和 `std::vector` 的关键差距

- **allocator 模型不完整**
  - `std::vector` 是 `std::vector<T, Alloc>`，allocator 影响分配、传播、`max_size` 等。
  - toy 版固定使用 `std::allocator<T>`，没有把 allocator 作为模板参数，也没有处理传播规则。

- **接口面太窄**
  - `std::vector` 还有 `insert/erase`, `assign`, `shrink_to_fit`, `swap`, `front/back`, `capacity` 控制、`reserve` 的更多边界、`emplace`、比较运算、`data`/迭代器类别等。
  - toy 版只做了最核心的 push/pop/reserve/resize/at。

- **迭代器与失效规则“没写成合同”**
  - `std::vector` 有标准化的 iterator / const_iterator / reverse_iterator，且失效规则是 API 合同的一部分。
  - toy 版用裸指针当迭代器，但没有把“哪些操作会 invalidate”写成契约，也没有调试检测。

- **异常保证与强保证差距**
  - `std::vector` 在很多操作上提供更强的异常保证（依赖类型 traits），并在标准里写清楚。
  - toy 版整体只有“尽量不泄漏”的 basic guarantee，细节不完整。

### 1.3 要逼近 `std::vector`，该补哪些点（路线图）

- 让 allocator 成为模板参数：`template<class T, class Alloc=std::allocator<T>>`
- 引入 iterator 类型与 traits（至少要有 `iterator_category` / `difference_type`）
- 实现 `insert/erase`，并明确写出 iterator 失效规则
- 用 `std::is_nothrow_move_constructible_v<T>` 等 traits 做更细的异常保证分支

---

## 二、`HashTable<K, V>` vs `std::unordered_map<K, V>`

对应实现：`codes/includes/hash_table.h`（开放寻址 + 线性探测）

### 2.1 你在 toy `HashTable` 里学到的

- hash table 的核心：bucket / probe / load factor / rehash
- tombstone（删除标记）对性能与 rehash 的影响
- “插入/查找/删除平均 O(1)”背后的代价：空间 + rehash

### 2.2 和 `std::unordered_map` 的关键差距

- **数据模型不一样**
  - `std::unordered_map` 是“bucket + 链式/节点式”的模型（常见实现是节点分配 + bucket 数组）。
  - toy 版是开放寻址，bucket 内存连续，删除用 tombstone。
  - 这会直接影响：迭代器稳定性、内存碎片、rehash 的成本、erase 行为。

- **哈希与等价的合同不足**
  - 标准库对 Hash/Eq 有严格要求（相等则 hash 必须相等、透明比较、heterogeneous lookup 等）。
  - toy 版默认 `DefaultHash` 是为了绕开某些 toolchain 的链接问题（`std::__1::__hash_memory`），它的“泛型 fallback（按对象字节 hash）”并不适合泛型生产代码。

- **缺少 `reserve`, `rehash`, `max_load_factor` 等控制接口**
  - `std::unordered_map` 给你明确的负载因子控制与 bucket 管理接口。
  - toy 版只有内部阈值（0.7）与简单扩容策略，属于“写死策略”。

- **迭代器与引用稳定性**
  - 标准库的节点式实现通常保证：元素节点地址稳定（直到 erase/rehash）。
  - 开放寻址在 rehash 时会搬运元素，且 bucket 本身会变动；迭代器模型完全不同。

### 2.3 要逼近 `std::unordered_map`，该补哪些点

- 让 bucket 数量始终为 2 的幂（目前是），并把 `rehash/reserve/max_load_factor` 变成公开 API
- 引入 iterator（至少 forward iterator），并定义 rehash/erase 下的失效规则
- 正确实现透明查找（`is_transparent` + `find` 支持 `string_view` 等）
- 提供 `operator[]`, `try_emplace`, `emplace`, `contains` 等常用接口

---

## 三、`LRUCache<K, V>` vs “工程级 LRU”

对应实现：`codes/includes/lru_cache.h`

### 3.1 你在 toy `LRUCache` 里学到的

- LRU 的经典结构：`list`（维护新旧顺序）+ “索引表”（key -> list iterator）
- `touch` 用 `splice` O(1) 把节点挪到头部
- 满容量时淘汰尾部（least recently used）

### 3.2 和工程级 LRU 的关键差距

- **线程安全与并发性能**
  - 工程里常见需求：多线程 get/put，分片锁（shard），或者读写锁。
  - toy 版明确不做线程安全。

- **容量语义与内存语义**
  - 工程 LRU 常按“字节大小/权重”淘汰，而不是“条目数”。
  - 还会处理 value 的构造成本、move、以及回调（eviction callback）。

- **接口与命中率优化**
  - 常见接口：`get_or_compute`, `peek`, `promote`, `stats`, `try_get`（避免复制）。
  - toy 版 `get` 返回 `std::optional<V>`（会复制/移动），更像教学接口。

- **索引表实现**
  - 常见用 `unordered_map`，这里为了兼容你的 toolchain，把索引改成了 `HashTable<K, ListIt>`。
  - 这让 LRU 的正确性依赖 toy 哈希表的 hash/eq 合同与 rehash 行为；工程实现会更谨慎地选用成熟容器。

---

## 四、总结：toy 实现的“价值”与“边界”

- **价值（你应该从 toy 实现里带走什么）**
  - 数据结构的骨架：连续数组、开放寻址、LRU 的 list+index
  - 性能直觉：cache、rehash、tombstone、move vs copy

- **边界（你不应该从 toy 实现里得到什么错觉）**
  - “接口像就等价于 std”：不成立  
  - “平均 O(1) 就一定快”：不成立（常数、缓存、分配策略决定一切）
  - “能跑就能上生产”：不成立（合同、测试、UB、防御性、并发模型都缺）

把这三份代码当成“把 STL 原理写出来的练习题”就对了：  
**写得越小，越适合教学；写到能替代 `std::`，才是真正的工程。**

