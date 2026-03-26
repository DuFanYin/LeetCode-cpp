# 面向高频交易（HFT）的 CPU Cache 效率（延迟视角）

本文讲的是：在**超低延迟**系统（行情接入、策略计算、下单链路）中，如何让 **CPU 的内存层级（L1/L2/L3/DRAM）**为你服务。重点放在 **cache 行为、一致性（coherence）以及访问模式**——当你已经去掉显而易见的开销后，真正主宰 **p99 / p99.9 延迟**的往往就是这些因素。

## 0. HFT 的语境：什么叫“高效”

- **吞吐 vs 延迟**：很多提高吞吐的手段（更大的批处理、更深的队列）会抬高**尾延迟**。在 HFT 中，你通常更在意**可预测、有上界的延迟**。
- **真正的敌人是方差**：cache miss 不只是“更慢”，它会制造**抖动**（依赖型 miss、coherence stall、TLB miss、page fault、跨 NUMA 跳转）。
- **工作集（working set）就是预算**：最快的代码往往是那种**热工作集能留在 L1/L2**（至少留在本地 LLC），并且不会在核间来回“弹跳”cache line 的代码。

## 1. 实用的 cache 心智模型（够用且能推理）

### 1.1 cache line 是搬运的基本单位

cache 以 **cache line** 为单位搬运数据（常见 64B）。你只访问 1 个字节，CPU 也通常会把整个 line 拉进来。

直接推论：
- **空间局部性（spatial locality）**很重要：连续数据更容易保持“热”。
- **伪共享（false sharing）**很重要：同一条 line 上的无关变量会互相牵制。
- **指针追逐（pointer chasing）**很致命：它破坏空间局部性，也让预取（prefetch）变得困难。

### 1.2 把三类成本分开看

- **命中延迟**（L1/L2/L3）
- **缺失延迟**（DRAM / 远端 NUMA / 存储介入导致的 major fault）
- **一致性/排序成本**（cache line 所有权变化、失效（invalidation）、fence）

很多“看起来很神秘”的卡顿并不是 DRAM miss，而是来自共享热点 line 的 **coherence stall**（原子操作、锁、共享计数器等）。

### 1.3 一致性：写需要独占所有权

一个 core 想写某条 line，通常需要它处于可写状态（exclusive/modified）。如果其他 core 也持有这条 line，CPU 就需要**让其他副本失效**或**转移所有权**。因此，“一个全局原子计数器”很容易变成延迟制造机。

## 2. HFT 常见的内存访问形态（以及 cache 的反应）

### 2.1 行情接入流水线

常见流程：
- 解析报文
- 规范化消息
- 更新盘口/特征状态
- 发布给策略线程

cache 角度的要点：
- 对热字段优先采用 **SoA（structure-of-arrays）**
- 每个标的（instrument）状态尽量**紧凑**且**对齐**
- 避免“全局共享一切”；更偏向 **按 core 分片（per-core shards）**

### 2.2 盘口（order book）更新

盘口更新常带来：
- 按标的随机-ish 访问
- 高频小更新
- 多组件读取

你的目标通常是：
- 让热路径只操作一个**很小的热集合**（例如 top-of-book / 最优档）
- 在关键路径避免冷指针与重量级容器
- 尽量减少跨核写（盘口更新本质是写）

## 3. 布局胜过小聪明：数据组织策略

### 3.1 热/冷拆分（hot/cold splitting，也叫“struct peeling”）

把频繁使用的字段放进紧凑的“热结构体”；把很少用的字段放进“冷结构体”，按需访问。

收益：
- 热工作集更小 → 更容易留在 L1/L2
- 每次操作触及的 cache line 更少

### 3.2 AoS vs SoA（以及各自何时更好）

- **AoS**（array of structs）：如果你每次都要用到一个元素的大多数字段，AoS 往往更合适。
- **SoA**（struct of arrays）：如果你每次只用少数字段，或你能做向量化，SoA 往往更合适。

HFT 中常见做法是：对热的数值字段（price、size、flags）偏向 **SoA**；当一组字段总是一起被用到且打包后更利于局部性时，再考虑 **AoS**。

### 3.3 避免指针追逐

链式结构（链表、堆上节点的树）通常会导致：
- 局部性差
- 分支不可预测
- 预取效果差

更常用的替代：
- 扁平数组 / `std::vector`（预先分配、复用）
- 开放寻址（open addressing）的哈希表
- ring buffer

如果不得不用指针，可以考虑：
- arena 分配（尽量让对象在内存中更“连续”）
- 基于 index 的引用（数组偏移而不是裸指针）

### 3.4 对齐与 padding（避免不小心共享 cache line）

两类经典问题：

- **伪共享（false sharing）**：两个线程写不同变量，但它们落在同一条 cache line 上。
- **真共享（true sharing）**：多个 core 争用同一条可写 line（原子变量/锁）。

常见修复：
- 把频繁写的 per-thread/per-core 状态按 cache line 大小对齐
- 用 padding 把不同写者隔离到不同 line

本仓库里已经有一个可运行示例：
- `codes/src/perf/cache_alignment_example.cpp`

## 4. 写比读更难：控制 coherence 成本

### 4.1 “单个共享计数器”陷阱

即使是 relaxed 原子，在多核同时自增同一个计数器时也可能很贵：每次自增都需要拿到写所有权，并触发失效传播。

更常见的替代：
- per-core 计数器 + 周期性聚合
- 允许的话用近似计数

### 4.2 避免热路径上的跨核交接（handoff）

典型队列交接模式：
- core A 的 producer 写 → core B 的 consumer 读
- 队列元数据所在的 cache line 在两个 core 之间来回“弹跳”

缓解方式：
- SPSC ring buffer（读/写索引分离，且仔细 padding）
- per-core mailbox（N 个 producer 推送到每个 consumer 的队列）
- 有上界的 batching（用更少 coherence 事件换取吞吐，但要小心尾延迟）

### 4.3 原子与内存序（不只是正确性，也影响 cache）

更强的内存序（acquire/release/seq_cst）可能带来：
- 额外的 fence
- 推测执行受阻
- 在竞争 cache line 时更高的额外延迟

经验原则：
- 在正确的前提下使用**尽可能弱**的内存序
- 把同步变量放到**独立的 cache line** 上

## 5. 预取（prefetch）：何时有用，何时有害

### 5.1 硬件预取器喜欢规律

它擅长：
- 线性扫描
- 可预测步长（stride）

它不擅长：
- 指针追逐
- 随机分支导致的不可预测访问

因此最好的“预取优化”常常不是加 `prefetch` 指令，而是**把数据布局改到可预取**。

### 5.2 软件预取（少用、谨慎用）

软件预取可能有效的前提：
- 你能较早预测下一次访问地址
- 预取到真正使用之间有足够计算，能隐藏延迟

风险（尤其在延迟敏感的 HFT）：
- 预取错行 → cache 污染
- 带宽/压力增加 → 尾延迟变差

## 6. TLB 与页：cache 效率的近亲

即使 cache 本身没问题，TLB miss 也可能成为主导因素。

建议：
- 热工作集要按**页数**来控制，不只按字节数
- 对大而热、连续的数据结构（盘口/特征数组）可考虑 **huge pages**，但要验证运维与碎片化等权衡
- 避免在热路径出现频繁 page fault 的映射/访问模式

## 7. NUMA 与拓扑：只有“本地”才够快

在多路（multi-socket）系统上：
- 本地 DRAM 通常比远端 DRAM 更快、更可预测
- LLC 的拓扑也很关键（共享/分段）

实用规则：
- 给关键线程绑核（affinity）
- 在同一 NUMA 节点上分配并使用内存（first-touch 或显式策略）
- 避免跨 socket 的共享可写数据

## 8. 延迟优先的检查清单（按顺序做）

### 8.1 让热数据变小

- 热/冷拆分
- 缩小结构体（字段重排、更小类型）
- 减少间接层（indirection）

### 8.2 让访问变可预测

- 连续数组
- 稳定的遍历顺序
- 避免“随机内存 + 不可预测分支”的组合

### 8.3 消除共享可写 cache line

- per-core 状态
- 对齐/隔离同步变量
- 移除竞争计数器/锁

### 8.4 控制分配与生命周期

- 热路径避免动态分配
- 复用 buffer/object（pool）
- arena 尽量做到 per-thread/per-core

### 8.5 用测量验证（不要靠猜）

- 用真实流量分布测 p50/p99/p99.9
- 追踪 cache miss、LLC miss、内存 stall 周期、以及 coherence 相关事件
- 一次只改一个变量

## 9. 如何测 cache 效率（更贴近 HFT）

### 9.1 基准测试：避免自欺

常见坑：
- microbenchmark 全部落在 cache 里（看起来很快但不代表真实场景）
- 数据分布不真实（标的太少 / 盘口太小）
- 计时噪声大或没有绑核
- 用“平均延迟”掩盖尾延迟回退

### 9.2 值得关注的信号

即使不指定具体工具，你一般也会想观察：
- L1/L2/LLC miss 率
- stalled cycles（卡在内存上的周期）
- coherence invalidation / line transfer
- 远端 NUMA 访问

关键是：把一次**延迟尖峰**关联回某种**内存行为**（miss、coherence stall、TLB miss），再通过重设布局/所有权去解决。

## 10. 在 HFT 中通常会赢的设计模式

- **每个数据分片单写者（single-writer）**（所有权是核心）
- **按 core 分区（per-core partitioning）**（状态 + 队列）
- **扁平、连续的数据结构**，并显式管理生命周期
- **有上界的 batching**（只在能减少 coherence 且不拉高尾延迟时使用）
- **热/冷拆分** + 热字段 SoA

## 11. 总结（一段话）

HFT 里的 CPU cache 效率，本质上是让**热工作集足够小且足够本地**，并且**消除共享可写 cache line**引发的 coherence 流量。数据布局（连续性、热/冷拆分）、所有权（single-writer、per-core 分片）与拓扑（NUMA 本地性、绑核）通常比“指令级小聪明”更决定成败。正确的工作流是：先按局部性/所有权重设设计，再用延迟分布和内存/coherence 指标确认效果。

