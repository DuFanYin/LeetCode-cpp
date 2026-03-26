## STL 深度理解：不仅仅是会用（STL Deep Understanding）

这一份只讲 STL 容器背后的**设计思路、底层结构与使用陷阱**，而不是查手册式罗列接口。

核心目标：

> 当你在 `vector` / `deque` / `list` / `set` / `map` / `unordered_map` / `priority_queue` 之间做选择时，  
> 知道自己在拿什么换什么（时间 vs 空间 vs 局部性 vs 复杂度 vs 内存碎片）。

最后会简单讲一下 **自定义 allocator** 与 STL 的内存管理模型。

在正式讲容器前，先把一个核心对比说清楚：**C 风格数组 vs `std::vector`**。

---

### 0. 数组与 `std::vector` 的本质区别

#### 0.1 C 风格数组是什么

```cpp
int a[10];      // 栈上的固定大小数组
int* p = new int[10];  // 堆上动态分配，但大小一旦确定就不能变
```

特点：

- 大小在编译期（`int a[10]`）或分配时（`new int[n]`）就固定；  
- 不会自动扩容 / 缩容；  
- 不记录自身大小，只是“一块连续内存 + 类型”；  
- 需要手动管理生命周期（尤其是 `new[]` / `delete[]`）。  

#### 0.2 `std::vector` 在数组之上的增强

可以理解为：

> `vector` = “带有大小/容量信息 + 自动管理内存 + 自动调用构造/析构”的 **可扩展数组封装**。

主要增强点：

- 内部仍然是 **连续内存**，与数组一样对 cache 友好；  
- 持有 `size()` 与 `capacity()` 信息；  
- 支持自动扩容（`push_back` / `resize` / `reserve`）；  
- 析构时自动释放内存，并调用元素的析构函数；  
- 与 STL 其他算法配合良好（迭代器接口）。  

简单对比：

- 若你只需要一个固定大小、简单 POD 类型的局部数组：C 风格数组即可；  
- 只要大小可能变化，或者要与 STL 算法配合、存放非平凡类型，**优先考虑 `std::vector`**。  

---

## 一、`std::vector`：连续内存 + 动态数组

### 1.1 核心特性

- **连续内存**：元素存放在一块连续区域里（类似 `T*` 指向的数组）；  
- **随机访问 O(1)**：`v[i]`、迭代器加减都极快；  
- 尾部 `push_back` 摊还 O(1)，中间插入/删除 O(n)；  
- 扩容时搬家：容量不够会分配更大区域并移动所有元素。  

底层可以理解为：

```cpp
T* begin_;
T* end_;
T* capacity_end_;
```

### 1.2 适用场景

- 以 **顺序存储 + 随机访问** 为主，插入/删除主要发生在尾部；  
- 性能敏感代码里，尽量使用 `reserve()` 预分配以减少搬家次数。  

### 1.3 迭代器失效规则（高频考点）

- 增长容量导致重分配时：**所有** 迭代器、引用、指针全部失效；  
- 在中间插入/删除：从插入点开始之后的迭代器都会失效；  
- 仅在尾部 `push_back`，且未触发扩容：之前的迭代器依然有效。  

典型坑：

```cpp
std::vector<int> v = {1, 2, 3};
int* p = &v[0];
v.push_back(4);      // 可能触发扩容
// p 此时可能已悬空
```

---

## 二、`std::deque`：分段连续 + 两端高效

### 2.1 核心特性

- **分段连续内存**：内部是若干固定大小的块 + 一个块指针数组（map）；  
- 支持在头尾高效插入删除（`push_front` / `push_back`）——基本 O(1)；  
- 中间插入/删除仍然是 O(n)；  
- `operator[]` / 随机访问仍然是 O(1)，但比 `vector` 多一次 indirection。  

可以简单理解为：

> “头尾可以长出来/缩回去的分段数组”，而不是完整搬家。

### 2.2 适用场景

- 需要频繁在**两端**插入/删除，又希望保留 O(1) 随机访问；  
- 比如任务队列、滑动窗口、双向 BFS 等。  

### 2.3 与 `vector` 的取舍

- 若主要是尾部 `push_back` + 随机访问：优先 `vector`（更好 cache 局部性）；  
- 若需要头尾双向高效插入：考虑 `deque`。  

---

## 三、`std::list`：双向链表

### 3.1 核心特性

- **每个元素单独分配节点，节点通过指针串联**；  
- 任意位置插入/删除只需改指针，复杂度 O(1)（前提：已拿到那个位置的迭代器）；  
- 不支持随机访问（`operator[]` 不存在，迭代器只能 ++ / --）；  
- 内存不连续，cache 局部性较差。  

节点大致形态：

```cpp
struct Node {
    Node* prev;
    Node* next;
    T     value;
};
```

### 3.2 适用场景

- 你已经有迭代器，且需要在很多位置进行插入/删除；  
- 大规模 `splice` 操作（在 list 间移动整段元素，不拷贝、不移动元素）。  

### 3.3 不适合的场景

- 需要频繁随机访问（`i` → 元素）；  
- 在大多数现代 CPU 上，由于 cache 行为，常常 **`vector` + 移动元素** 仍比 `list` 快。  

---

## 四、`std::set` / `std::multiset`：有序集合（基于平衡树）

### 4.1 底层结构与复杂度

标准通常实现为 **红黑树**（自平衡二叉查找树）：

- 查找 / 插入 / 删除：`O(log n)`；  
- 元素按 key 有序存放；  
- `set` 不允许重复键，`multiset` 允许重复键。  

典型迭代有序：

```cpp
std::set<int> s = {3, 1, 4};
// 遍历时输出顺序：1, 3, 4
```

### 4.2 适用场景

- 需要自动去重 + 有序；  
- 频繁做“按 key 查找/插入/删除”，但不需要随机下标访问。  

### 4.3 和 `unordered_set` 对比

- `set`：有序，`log n`，底层树；  
- `unordered_set`：无序，期望 O(1)，底层哈希表；  
- 若需要有序遍历或范围查询（`lower_bound` / `upper_bound`），用 `set` / `multiset`。  

---

## 五、`std::map` / `std::unordered_map`：有序 vs 哈希

### 5.1 `std::map`：有序关联容器（通常红黑树）

特性：

- key 有序存储；  
- 查找 / 插入 / 删除：`O(log n)`；  
- 迭代器按 key 顺序遍历；  
- 支持范围查询（`lower_bound` / `upper_bound` / `equal_range`）。  

示例：

```cpp
std::map<std::string, int> freq;

freq["apple"]++;
freq["banana"]++;

for (auto& [k, v] : freq) {
    // 按 key 的字典序遍历
}
```

### 5.2 `std::unordered_map`：哈希表（无序）

特性：

- 底层通常是 **哈希桶数组 + 链表/开放寻址**；  
- 平均 `O(1)` 的查找/插入/删除，最坏 `O(n)`；  
- key 无序，迭代顺序不稳定；  
- 需要提供 `hash<Key>` 与 `==`。  

示例：

```cpp
std::unordered_map<std::string, int> freq;
freq["apple"]++;
// 迭代顺序与 key 的字典序无关
```

### 5.3 选择建议

- 需有序遍历 / 范围查询：`map`；  
- 以查找/插入为主，不关心顺序，需要最大化平均性能：`unordered_map`；  
- 对小规模数据，有时 `map` 甚至更快（树结构 + 较少内存碎片）。  

---

## 六、`std::priority_queue`：堆封装

### 6.1 底层结构

`std::priority_queue` 默认基于 `std::vector` 实现一个二叉堆：

- 最大堆（默认）：`top()` 返回最大元素；  
- 主要操作复杂度：  
  - `push`：`O(log n)`  
  - `pop`：`O(log n)`  
  - `top`：`O(1)`  

定义一个最小堆示例：

```cpp
#include <queue>
#include <vector>

std::priority_queue<
    int,
    std::vector<int>,
    std::greater<int>
> min_heap;
```

### 6.2 适用场景

- 调度系统（总是先处理“最高优先级”的任务）；  
- Dijkstra 最短路、A*、实时排行榜等。  

注意：

- `priority_queue` 不是通用“随时修改优先级”的容器，更新某元素优先级通常是**重新 push 一个新元素**，旧的延后被丢弃。  

---

## 七、STL 中的 Allocator 模型与自定义分配器

STL 容器的内存管理通过 **Allocator 模型** 抽象出来：

> 容器不直接调用 `new` / `delete`，而是通过 `Allocator` 接口向“内存资源”要空间、还空间。

### 7.1 `std::allocator`：默认分配器

所有标准容器都有一个模版参数 `Allocator`，默认使用 `std::allocator<T>`：

```cpp
template <
    class T,
    class Allocator = std::allocator<T>
> class vector;
```

它的职责包括：

- `allocate(n)`：分配一片可放 `n` 个 `T` 的原始内存（未构造）；  
- `deallocate(p, n)`：释放这块内存；  
- 由容器负责在这块内存上调用构造/析构函数。  

### 7.2 自定义 Allocator 的典型用途

- 统计内存使用（debug allocator）；  
- 使用自定义内存池 / arena；  
- 使用共享内存、HugePage、NUMA 绑定等系统级特性。  

一个极简“计数型” allocator 示例（只演示接口形态）：

```cpp
template <typename T>
class CountingAllocator {
public:
    using value_type = T;

    CountingAllocator() noexcept {}

    template <class U>
    CountingAllocator(const CountingAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        bytes_allocated += n * sizeof(T);
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept {
        ::operator delete(p);
    }

    static inline std::size_t bytes_allocated = 0;
};

// 比较相同模板的 allocator 是否“兼容”
template <class T, class U>
bool operator==(const CountingAllocator<T>&, const CountingAllocator<U>&) noexcept {
    return true;
}

template <class T, class U>
bool operator!=(const CountingAllocator<T>&, const CountingAllocator<U>&) noexcept {
    return false;
}
```

使用方式：

```cpp
using VecInt = std::vector<int, CountingAllocator<int>>;

void foo() {
    VecInt v;
    v.reserve(1000);
    v.push_back(1);
    // 之后可以查看 CountingAllocator<int>::bytes_allocated
}
```

这只是一个“教学级”例子，真实工程中，自定义 allocator 还会考虑：

- 拷贝 / 移动容器时 allocator 是否一起传播（`propagate_on_container_*` traits）；  
- 不同 allocator 实例间是否可以互相 deallocate；  
- 线程安全与内存池组织方式等。  

### 7.3 容器与 Allocator 的配合方式（底层视角）

以 `vector` 为例，简单流程是：

1. 当需要更多空间时，通过 allocator 的 `allocate(n)` 申请一块更大的内存；  
2. 在这块新内存上，用构造函数“搬运”旧元素（拷贝或移动）；  
3. 调用旧内存上元素的析构函数；  
4. 通过 allocator 的 `deallocate(old_ptr, old_n)` 释放旧内存。  

因此：

- **容器负责元素生命周期（构造/析构）与逻辑组织**；  
- **Allocator 负责底层字节的分配与回收**。  

理解这层分工，有助于你在需要极致性能或特殊内存布局时，自定义合适的 allocator（或者选择 `pmr` 相关设施）。  

---

## 八、STL 统一设计思想：容器 + 迭代器 + 算法 + allocator

理解 STL，绕不开一个整体视角：

> **容器只负责存数据；算法只通过迭代器看数据；内存则交给 allocator 管。**

可以概括成三个核心抽象：

- **容器（Container）**：管理元素的组织形式（数组 / 链表 / 树 / 堆 / 哈希表）；  
- **迭代器（Iterator）**：提供统一的“遍历接口”，把不同容器对算法“长得一样”这件事做出来；  
- **算法（Algorithm）**：`sort`、`find`、`accumulate`、`transform` 等，只依赖迭代器，不依赖具体容器；  
- **Allocator**：抽象内存管理策略，容器不直接 `new/delete`，而是“向 allocator 要/还内存”。  

这几点加在一起的效果是：

- 你可以给 `vector` / `deque` / `list` / `set` 等喂同一套算法；  
- 在**不改算法代码**的前提下，只换容器就能换掉底层数据结构；  
- 在**不改容器代码**的前提下，只换 allocator 就能换底层内存策略（普通堆 / 内存池 / 自定义区域）。  

一个典型组合：

```cpp
std::vector<int> v = {/*...*/};
std::sort(v.begin(), v.end());  // 算法只依赖 RandomAccessIterator
```

算法只要求“随机访问迭代器”，因此可以同样用于 `std::array` / `std::deque`，而不能用于 `std::list`（只提供双向迭代器）。

---

## 九、整体视角：如何“选容器”

可以用一句粗略的决策链来记：

1. **是否需要按 key 有序遍历？**  
   - 是 → `map` / `set` / `multiset`；  
   - 否 → 往 `unordered_*` / 序列容器方向看。  
2. **是否需要随机下标访问？**  
   - 是 → `vector` / `deque`；  
   - 否 → `list` / `forward_list` / `queue` / `priority_queue`。  
3. **插入/删除主要发生在哪里？**  
   - 尾部为主 → `vector`；  
   - 头尾两端 → `deque`；  
   - 已有迭代器、频繁在中间插入/删除 → `list`。  
4. **是否需要按优先级取出“最大/最小”？**  
   - 是 → `priority_queue`（堆）。  

最后，在对性能/内存布局有更高要求时，再考虑：

- 迭代器失效规则；  
- cache 局部性；  
- 是否需要自定义 allocator 或专门的内存池。  

这样，你对 STL 容器的理解就不止停留在“能查 API”，而是能从**整体系统设计**的角度去做权衡与选择。 

