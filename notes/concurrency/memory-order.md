## C++ 内存序与乱序执行（高密度实战版）

> 面向“已经在写 lock-free / 多线程，希望彻底搞懂内存序”的自己。  
> 目标：  
> - 看一段并发代码，30 秒内判断是否有 data race / 顺序问题；  
> - 知道该选哪种 `memory_order`，以及为什么。

---

## 一、主线：各种 `memory_order` 的关系

先把大局排清楚，再看细节。

- 从“约束强度”上，可以粗略排成一条链：

  \[
  \text{relaxed} \;<\; \text{acquire/release} \;<\; \text{acq\_rel} \;<\; \text{seq\_cst}
  \]

- 但要注意：
  - **`acquire` / `release` 不是谁比谁“更强”，而是方向不同**：
    - `release`：约束“**前面的别跑到后面**”；
    - `acquire`：约束“**后面的别跑到前面**”。
  - `acq_rel`：两边都管（典型用于 RMW 原子操作）。
  - `seq_cst`：在 acquire/release 之上，再加一个**全局统一顺序感**。

可以用一组“承诺”来记忆：

- **`relaxed`**：我只承诺这次访问本身是原子的，**别拿我做同步边界**。
- **`release`**：我承诺把我之前的状态按顺序**发布**出去，但不负责接收别人。
- **`acquire`**：我承诺一旦看到了别人发布的信号，我之后的读都要**跟上**，但我不负责往外发布。
- **`acq_rel`**：我两边都承诺（典型是“读旧状态 + 写新状态”的边界）。
- **`seq_cst`**：不仅两边承诺，还尽量让大家看到一个**单一的全局顺序**。

---

## 二、四层顺序模型：从源码到别人眼里

所有内存序问题，最后都要落到“别的线程看到的顺序”。先把四层分开：

1. **Program Order（源码顺序）**  
   - 你写的先后：`data = 1; flag = true;`
2. **Compiler Order（编译器重排）**  
   - 只要不破坏单线程可观察行为，编译器可以随意重排 load/store。
3. **CPU / Memory Order（CPU + 内存子系统）**  
   - out-of-order 执行、store buffer、cache coherence 延迟等。
4. **Observed Order（其他核心实际观察到的顺序）**  
   - 真正决定并发正确性的，是别的线程最终以什么顺序看到这些写。

**`memory_order` 的使命**：在第 2/3 层上画红线，让第 4 层**看起来像你期望的顺序**。

---

## 三、五个基础概念：race / atomicity / ordering / visibility / sync

### 3.1 Data Race（数据竞争）

- 条件（简化版）：
  - 至少两个线程；
  - 并发访问同一内存位置；
  - 至少一个是写；
  - 且**没有同步**。
- 结果：C++ 直接 **UB**，所有推理全部失效。

### 3.2 Atomicity（原子性）

- 单次访问不可撕裂，遵守 atomic 类型规则。
- 解决的是“这次读/写是否合法的并发访问”。
- **原子性 ≠ 正确同步**：
  - `counter.fetch_add(1, memory_order_relaxed)` 是合法并发；
  - 但它不提供任何“谁先谁后看到更新”的顺序保证。

### 3.3 Ordering（顺序约束）

- 约束当前线程内**普通读写相对于某次 atomic 的先后关系是否允许重排**：
  - 防止“payload 写到 flag 之后”、“先读 flag 再读到旧 payload”之类的问题。

### 3.4 Visibility（可见性）

- 让别的线程在命中某个同步条件时（例如读到某个值），**必然看到你之前的写**。

### 3.5 Synchronization（同步 / happens-before）

- 通过 `release`/`acquire`、锁、`join`、条件变量等建立 **happens-before**：
  - 若 A happens-before B，则 B 必须看到 A 的所有效果。
- 只要这条边存在，语言就保证顺序 + 可见性；没有，就别相信自己的“肉眼直觉”。

---

## 四、每种 `memory_order` 到底在限制什么？

核心思路：

> `memory_order` 限制的是“**这个 atomic 周围的访问允许怎样重排，以及跨线程如何形成同步**”，  
> 不是只管这个 atomic 自己，而是管它附近的普通读写能不能穿过去。

### 4.1 `memory_order_relaxed`

给你：

- ✅ 原子性；
- ✅ 避免 data race（前提：所有访问都通过 atomic）；
- ✅ 单独看这次访问是合法并发。

不给你：

- ❌ 跨线程同步；
- ❌ 发布其他数据；
- ❌ 建立 happens-before；
- ❌ 任意顺序保证。

所以 relaxed 的本质是：

> **“这个变量要原子访问，但我不拿它当同步点。”**

**典型适用场景**：

- 纯统计计数器：请求数 / 丢包数 / 命中次数 / debug 计数；
- 这些值只是“自己要准”，不是“看到它变化就说明别的东西也准备好了”。

### 4.2 `memory_order_release`

- 常用于 **store**。
- 约束：
  - 当前线程中，**release 之前的读写不能被重排到 release 之后**。
- 本质作用：
  - 发布之前准备好的状态。

可以读作：

> “**我在这个 release store 之前做的那些写，现在可以一起对外宣布了。**”

**典型场景**（发布 ready flag / pointer / 状态）：

```cpp
payload = compute();
metadata = ...;
flag.store(true, std::memory_order_release);
```

语义：任何通过相应 `acquire` 看到 `flag == true` 的线程，都必须也能看到这些 `payload` / `metadata` 的写。

### 4.3 `memory_order_acquire`

- 常用于 **load**。
- 约束：
  - 当前线程中，**acquire 之后的读写不能被重排到 acquire 之前**。
- 本质作用：
  - 接收别人发布的状态。

可以读作：

> “**如果我通过这个 acquire load 观察到了某个已发布信号，那后续访问必须能看到那批被发布的数据。**”

**典型场景**（等待 ready flag / 读取已发布指针等）：

```cpp
if (flag.load(std::memory_order_acquire)) {
    use(payload);   // payload 必须是发布方 release 前写好的版本
}
```

### 4.4 `release` + `acquire` 的配对关系

- `release` 负责“推出去”，`acquire` 负责“接进来”；
- 它们配合后形成的不是“感觉上同步”，而是**真正的 happens-before 边**。

理解方式：

- `release` = 发布完成；
- `acquire` = 确认接收完成。

没有这对边，绝大多数“flag 通知 + 读数据”的写法都不合法。

### 4.5 `memory_order_acq_rel`

- 用于既读又写的原子操作（RMW）：
  - `fetch_add` / `exchange` / `compare_exchange` 等。
- 同时具备：
  - `acquire`：接收之前别人的发布；
  - `release`：把自己之前的写继续发布出去。

**典型场景**：

- lock-free 队列 / freelist 更新头指针；
- 自旋锁状态切换；
- work stealing 队列的 ticket 递增。

当一个原子操作既是“观察旧状态”，又是“写入新状态”的边界时，`acq_rel` 通常是合理默认。

### 4.6 `memory_order_seq_cst`

- 最强，也最接近人脑直觉。
- 包含 acquire/release 语义，且**额外要求**：
  - 所有 `seq_cst` 原子操作在全局上形成一个**单一一致的顺序**。

可以这么理解：

> “**所有 `seq_cst` 原子访问排成了一条全局时间线，所有线程看到的顺序是一致的。**”

**适用场景**：

- 第一次实现 / 调试阶段：**先写对再说**；
- 全局标志 / 全局状态切换；
- 对可读性和保守正确性优先的代码。

`seq_cst` 减少推理负担，是最容易“想清楚”的内存序。

---

## 五、通信模式与推荐写法

### 5.1 单向“发布数据 + 就绪标志”（最常见）

```cpp
// 生产者
payload = compute();
ready.store(true, std::memory_order_release);

// 消费者
while (!ready.load(std::memory_order_acquire)) { /* spin */ }
use(payload);
```

保证：

1. 生产者：`payload` 写在 release 之前，不能被重排到 `store(release)` 之后；
2. 消费者：所有在 acquire 之后的读（包括 `payload`）不能被重排到 acquire 之前；
3. 若 `load(acquire)` 看到的是发布方 `store(release)` 写入的那个值，则消费者**必须**看到发布方 release 之前的所有写。

这是最常见、最重要的通信模式：**发布-订阅**。

### 5.2 只要计数，不做同步

```cpp
std::atomic<uint64_t> requests{0};

void on_request() {
    requests.fetch_add(1, std::memory_order_relaxed);
}
```

- 只关心“最终计数是否合理”，不拿这个计数做“是否可以开始处理”的判断；
- `relaxed` 即可：原子性 + 最小顺序约束。

### 5.3 双缓冲 / 指针切换读取配置

```cpp
struct Config { /* ... */ };
std::atomic<Config*> global_cfg{nullptr};

// 更新线程
void reload() {
    Config* cfg = new Config(load_from_file());
    global_cfg.store(cfg, std::memory_order_release);
}

// 工作线程
void worker() {
    Config* cfg = global_cfg.load(std::memory_order_acquire);
    if (cfg) use(*cfg);
}
```

- 指针是“信号 + 入口”，配置结构体是 payload；
- release/acquire 确保只要看到新指针，就能看到新配置的完整内容。

### 5.4 自旋锁（示意）

```cpp
std::atomic<bool> locked{false};

void lock() {
    bool expected = false;
    while (!locked.compare_exchange_weak(
        expected, true,
        std::memory_order_acquire,   // 成功获得锁：需要 acquire
        std::memory_order_relaxed    // 失败：只是重试
    )) {
        expected = false;
    }
}

void unlock() {
    locked.store(false, std::memory_order_release);
}
```

- 获得锁的一刻，需要 acquire，把临界区之前的写“接收”进来；
- 释放锁时需要 release，把临界区的写发布给后续持锁者；
- RMW 操作常用 acq_rel，这里用“成功 acquire / 失败 relaxed”更精确表达意图。

---

## 六：默认 `seq_cst` vs 显式 `memory_order`：到底在“优化”什么？

### 6.1 默认不写时是什么？

如果你写：

```cpp
x.store(v);
x.load();
x.fetch_add(1);
```

不写 `memory_order`，**默认就是 `seq_cst`**（最强）。

### 6.2 写上显式 `memory_order` 在干什么？

当你写：

```cpp
x.store(v, std::memory_order_release);
x.load(std::memory_order_acquire);
counter.fetch_add(1, std::memory_order_relaxed);
```

你在告诉编译器和 CPU：

> “**我不需要 `seq_cst` 那么强，只需要这些更窄的保证。**”

本质上就是：

- **减少约束** → 编译器可重排空间更大，必要的 fence / 屏障更少 → 潜在性能更好。

### 6.3 不是所有“explicit”都是在优化

区分三类：

- A. 默认 `seq_cst` → 显式 `seq_cst`
  - 只是写明白，并未放松约束，不是优化。
- B. 默认 `seq_cst` → `acquire/release/acq_rel`
  - 在放松约束，通常有性能收益，也更贴近真实语义。
- C. 默认 `seq_cst` → `relaxed`
  - 大幅度放松约束，常用在性能敏感的计数/指标，但也**最容易写错**。

所以“写 explicit”≠“一定更快”；  
**真正优化的是“把同步语义放弱到刚好够用”。**

### 6.4 为什么放弱能优化？

强顺序意味着：

- 编译器可做的重排更少；
- CPU 需要更强的排序约束；
- 某些平台上要插更重的 barrier；
- `seq_cst` 还要维护全局线性顺序。

在 ARM / Power 等弱内存模型平台，这些开销差异会很明显；  
在 x86 上有时差异没那么大，但语义差异依旧存在，不能因此忽略。

### 6.5 什么时候值得从 `seq_cst` 往下调？

最好同时满足：

1. **你已经明确知道同步协议是什么**：
   - 哪个变量在发布，哪个在接收。
2. **这是热点路径**：
   - 高频队列、撮合、计数核心循环等。
3. **你能证明更弱语义仍然正确**：
   - 不是“测起来没问题”，而是能画出 happens-before 图，自证正确。

否则，先用 `seq_cst` 非常合理。

---

## 七、架构差异与“在我机器上没问题”的陷阱

### 7.1 x86：偏强模型，容易误导

- x86 TSO：
  - 很多写-读重排在硬件层面本就禁止；
  - 不加 acquire/release 的代码，**经常“看起来”也能跑**。
- 导致典型误解：
  - “普通变量也差不多”；
  - “`relaxed` 其实也 OK”；
  - “barrier/fence 不重要”。

一旦：

- 换 ARM / Power / RISC-V；
- 或编译器优化更激进；

这些“在我机器上没问题”的代码会直接炸。

### 7.2 正确姿势：以“语言内存模型”为准

永远只问两件事：

1. C++ 标准是否保证这段代码**没有 data race**？
2. C++ 标准是否通过 happens-before，保证我期望的可见性和顺序？

能在这两点上自证正确，你的代码才算真正可移植。

---

## 八、常见错误模式速览

### 8.1 “只把 flag 改成 atomic 就完事了”

```cpp
std::atomic<bool> ready{false};
int data = 0;

void producer() {
    data = 42;                                  // 普通写
    ready.store(true, std::memory_order_relaxed);
}

void consumer() {
    while (!ready.load(std::memory_order_relaxed)) {}
    use(data);                                  // 希望是 42，但没有任何语言保证
}
```

问题：

- `ready` 虽然原子，但 `data` 完全没参与同步；
- 使用 `relaxed`，没有任何 happens-before 边。

正确方式：至少需要 `release` + `acquire` 把 `data` 的写绑定到 flag 上。

### 8.2 “读多几次，总会看到最新的”

在 data race / 缺少同步的场景下：

- “多读几次就 eventually 一致”**没有任何标准依据**；
- 只有在“无 data race + 存在 happens-before”前提下，才谈得上“最终看到更新”。

### 8.3 把 `relaxed` 用在协议变量上

任何充当：

- 条件变量；
- 状态机节点；
- 发布/完成标志；

的变量，**不能随便用 `relaxed`**，除非你能证明：

- 它只是一个“旁路信息”，真正的同步边在别的地方。

经验规则：

- “**看这个变量的值来决定是否可以访问某块数据**”的场景：
  - 默认假设要 `release` + `acquire`；
  - 再推理是否有其他同步工具已经提供了同样的 happens-before。

---

## 九、写并发代码时的一条“内存序决策链”

每当你拿起一个 `std::atomic`，可以按这个流程决策：

1. **这只是保护它自己，还是承担同步协议？**
   - 只保护自己（例如纯计数器）：可以考虑 `relaxed` 或直接 `seq_cst`；
   - 承担协议：必须考虑 acquire/release / acq_rel / seq_cst。
2. **是否需要“看到它变化后，也看到别的数据的更新”？**
   - 需要：至少要 `release` / `acquire` 配对；
   - 不需要：`relaxed` 有可能足够。
3. **这段代码是不是热点？是否懒得细抠？**
   - 不热 / 懒得细抠：用 `seq_cst`；
   - 热点 / 在乎性能：在证明正确的前提下，往 `acquire/release/acq_rel/relaxed` 调低。

结合上一节，再压缩成“三类场景记忆法”：

- **第一类：纯原子计数** → `relaxed` 候选。
- **第二类：发布-接收（flag / pointer / state）** → `release` / `acquire` 候选。
- **第三类：复杂 RMW / 锁实现 / 不想踩雷** → `acq_rel` 或 `seq_cst` 候选。

这三类基本覆盖了绝大多数实际代码。

---

## 十、一句话总结

现代编译器和 CPU 为了跑得更快，会在不破坏单线程语义的前提下任意重排；  
而多线程正确性要求：**某些顺序必须对其他线程肯定成立**。  
`memory_order` 用来在语言层面声明这些必须成立的顺序；  
锁、原子操作与 fence 则在实现层面，保证任何合法执行都遵守这份“顺序契约”。 