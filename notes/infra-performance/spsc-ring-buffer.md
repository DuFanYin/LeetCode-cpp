## SPSC Ring Buffer：单生产者单消费者环形队列

`SpscRingBuffer<T, N>` 是一个**单生产者 / 单消费者**的环形缓冲区，用原子变量和掩码运算实现无锁入队 / 出队（在给定并发模型下）。  
它是一个经典的 **concurrency-friendly 数据结构**。

对应代码：`codes/includes/spsc_ring_buffer.h`。

---

### 一、接口与并发模型

接口非常紧凑：

```cpp
template <typename T, std::size_t N>
class SpscRingBuffer {
public:
    SpscRingBuffer();

    bool push(const T& value);   // 生产者入队
    bool push(T&& value);        // 移动入队

    std::optional<T> pop();      // 消费者出队

    bool empty() const noexcept;
    bool full() const noexcept;
    std::size_t size() const noexcept;
};
```

约束与假设：

- **SPSC**：  
  - 恰好 1 个生产者线程调用 `push`；  
  - 恰好 1 个消费者线程调用 `pop`；  
  - 其他模式（多生产者、多消费者）不在支持范围内。
- **容量为 N**，且要求：

  ```cpp
  static_assert((N & (N - 1)) == 0, "N must be power of two");
  ```

  即 N 必须是 2 的幂，方便用 `index & (N - 1)` 做取模。

---

### 二、内部表示：数组 + head/tail 原子指针

核心成员：

```cpp
T buffer_[N];                          // 环形缓冲区
std::atomic<std::size_t> head_;       // 消费位置
std::atomic<std::size_t> tail_;       // 生产位置
```

可以这样理解：

- `head_`：下一个将要被消费者读出的索引；  
- `tail_`：下一个将要被生产者写入的索引；  
- 有效元素区间是 \[head, tail)（模 N）。

数组访问使用掩码运算：

```cpp
constexpr std::size_t mask() const noexcept { return N - 1; }

buffer_[index & mask()]
```

这样在索引不断递增的同时，不需要显式取模运算（`%`），只要 `N` 是 2 的幂即可。

---

### 三、`push` 与 `pop` 的核心逻辑

入队（内部通过 `emplace` 实现）：

```cpp
std::size_t tail = tail_.load(std::memory_order_relaxed);
std::size_t nextTail = tail + 1;
if (nextTail - head_.load(std::memory_order_acquire) > N) {
    return false;  // 满
}
buffer_[tail & mask()] = std::forward<U>(value);
tail_.store(nextTail, std::memory_order_release);
return true;
```

关键点：

- 使用 `nextTail - head > N` 来检测“将满”；  
- 生产者只写 `tail_`，读 `head_`；  
- 写入数据后用 `memory_order_release` 更新 `tail_`，保证元素可见性。

出队：

```cpp
std::size_t head = head_.load(std::memory_order_relaxed);
if (head == tail_.load(std::memory_order_acquire)) {
    return std::nullopt;  // 空
}
T value = std::move(buffer_[head & mask()]);
head_.store(head + 1, std::memory_order_release);
return value;
```

关键点：

- 消费者只写 `head_`，读 `tail_`；  
- 读 `tail_` 使用 `memory_order_acquire`，配合生产者的 release；  
- 返回时把元素移动出去。

---

### 四、`tail` 追上 `head`（空）与“满”时的处理策略

这里有两种“边界状态”，对应两类策略选择：

#### 4.1 `tail == head`：队列为空（consumer 读不到）

在实现里，这意味着：

- `pop()` 返回 `std::nullopt`（空）。

调用方常见策略：

- **忙等（busy-wait / spin）**：不断尝试 `pop()` 直到有值
  - **适合**：极低延迟路径、producer 很快就会写入（空窗口很短）、线程专用核心（core-pinned）场景。
  - **代价**：空窗口变长时会浪费整核 CPU；在共享机器上会挤压其他线程，导致整体吞吐下降。
  - **常见变体**：只在短时间/少次数自旋，超过阈值就切换到 backoff/park。
- **退避（backoff）**：空时先自旋少量次数，然后逐步“让出 CPU”
  - **思路**：把“等数据”的代价从“持续占用 CPU”转为“以可控方式降低占用”。
  - **常用手段**：CPU pause（降低功耗与总线压力）→ `yield()`（让调度器切换）→ 短 sleep（微秒级）→ 更长 sleep。
  - **适合**：producer 可能间歇性停顿、或系统不允许持续占满 CPU 的服务端。
- **阻塞/唤醒（parking）**：当 ring 为空时主动阻塞，等待 producer 通知
  - **关键点**：SPSC ring 只是数据结构，通常不会内置 OS 同步；工程里会在外层配对一个“有数据”的通知机制（信号量/条件变量/事件）。
  - **适合**：producer 写入频率低、空闲时间长、希望节能或避免 busy loop 的场景。
  - **代价**：增加系统调用/唤醒延迟；如果消息很密集，频繁 park/unpark 反而更慢。
- **返回上层（polling 模型）**：把“空”当成正常结果，交由上层调度（事件循环/帧循环/批处理器）
  - **适合**：有天然 tick 的系统（游戏主循环、音频 callback、IO loop），每次 tick 顺手 poll 一次即可。
  - **好处**：避免把“等待”写死在 ring 层；可把节奏控制放在更了解业务的地方。

#### 4.2 “满”：producer 写不进去（`nextTail - head > N`）

在实现里，这意味着：

- `push(...)` 返回 `false`（满）。

调用方常见策略：

- **忙等直到有空位**：持续重试 `push()`
  - **适合**：短暂背压（consumer 很快会追上）、延迟优先的实时链路。
  - **风险**：如果 consumer 跟不上，producer 会长期自旋占核，导致“更慢更堵”的反效果（你在 benchmark 里看到的典型现象）。
- **退避重试（backpressure friendly）**：失败后 backoff（spin→yield→sleep）
  - **目标**：当系统进入持续背压时，让 producer 降速、降低 CPU 压力，给 consumer 和其他线程生存空间。
  - **常见策略**：指数退避 + 上限；或者按队列占用率分级退避（接近满时更激进地让出 CPU）。
- **丢数据（drop-new / drop-this）**：满了就放弃本次写入
  - **适合**：可丢场景（日志、metrics、采样数据、非关键心跳）。
  - **配套**：一般要有计数/告警（drop rate），否则“默默丢”会让系统变成黑盒。
- **覆盖旧数据（overwrite / drop-oldest）**：满了就丢最旧元素，保证“最新值优先”
  - **适合**：只关心最新状态的流（UI 状态、最新传感器读数、最新行情快照）。
  - **语义变化**：队列不再保证“每条消息都送达”，而是保证“消费者看到尽可能新的数据”。
- **扩容 / 多级队列**
  - **扩容**：当前实现 N 固定，工程里可用动态 ring（堆上分配）或分段 ring（chunk 链接）来扩容量。
  - **多级队列**：小 ring 做低延迟快路径，满了再 fallback 到更慢但更大的结构（例如 mutex 队列/磁盘缓冲），以降低极端情况下的丢包率。
- **改变生产模型（限速/合并/批处理）**
  - **限速**：producer 本身根据背压主动降频（token bucket 等）。
  - **合并/去重**：把多条更新合并成一条（例如只保留每个 key 的最新值）。
  - **批处理**：一次 push 多个元素（需要 ring API 支持批量），减少 per-message 原子开销。

> 小结：`tail==head`（空）和“满”本身不是 bug，它们是流控信号。  
> SPSC ring 的职责是**快速返回空/满**，策略（等、退避、丢、覆盖、阻塞）由上层按业务选择。

---

### 五、示例：和 mutex 队列对比

示例文件：`codes/src/spsc_example.cpp`

对比结构：

- **baseline**：一个 `std::queue<int>` + 单个 `std::mutex`；  
- **对照组**：`SpscRingBuffer<int, 1 << 16>`。

代码片段（对照部分）：

```cpp
// Baseline: std::queue guarded by a single mutex.
std::queue<int> q;
std::mutex m;
// producer: lock + push
// consumer: lock + while (!q.empty()) pop

// Compare: lock-free SPSC ring buffer with the same traffic.
SpscRingBuffer<int, 1 << 16> rb;
std::atomic<bool> done{false};

std::thread prod([&] {
    for (std::size_t i = 0; i < N; ++i) {
        while (!rb.push(int(i))) {
            // busy-wait; in real systems consider backoff/parking
        }
    }
    done.store(true, std::memory_order_release);
});

std::thread cons([&] {
    std::size_t count = 0;
    for (;;) {
        if (auto v = rb.pop()) {
            ++count;
        } else if (done.load(std::memory_order_acquire) && rb.empty()) {
            break;
        }
    }
});
```

观察点：

- mutex 队列在高并发 push/pop 下存在**锁竞争 + cache 抖动**；  
- SPSC ring buffer 在符合模型时，`push/pop` 都不需要锁，吞吐 / 延迟更可控。

---

### 六、使用与注意事项

适合场景：

- 生产者 / 消费者模型非常清晰：确实只有一个 producer 和一个 consumer；  
- 消息粒度较小、数量巨大；  
- 对延迟敏感，希望尽量避免锁和系统调用。

注意事项：

- **不能简单扩展到 MPSC / MPMC**：  
  - 多生产者或多消费者场景需要完全不同的设计。  
- **容量固定**：  
  - 满了之后 `push` 会返回 `false`，调用方需要决定是丢数据还是等待。  
- **busy-wait 策略**：  
  - 示例代码中使用了简单的 busy loop；在真实系统中，需要加 backoff / yield / park 等策略，避免浪费 CPU。

在整个 `codes` 的 demo 中，`SpscRingBuffer` 代表的是：

> 在明确的并发模型下，  
> 如何用“结构 + 内存序 + 原子操作”  
> 构建一个高效的 lock-free 数据结构。

---

### 六、小结：原理 & 使用场景

- **原理一句话**：  
  - 用一个定长数组 + 两个原子索引 `head_/tail_` 表示环形队列，要求单生产者 / 单消费者，通过 `index & (N - 1)` 做取模和 acquire/release 内存序，实现在给定并发模型下的无锁 push/pop。  
- **典型使用场景**：  
  - 明确的一对一生产者 / 消费者通道，消息很多但单条很小，对吞吐和延迟敏感（如音频处理、网络 IO 中的单向队列、实时系统中的事件通道等）。

