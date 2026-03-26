# Week 1：Virtual Memory / Page Table（高密度版）

## 你要建立的模型

VM 不是“把内存变大”的抽象，而是三件事的统一接口：

- **隔离**：地址空间隔离 + 权限位（U/S, R/W/X）。
- **稀疏映射**：虚拟地址连续，物理地址可离散。
- **按需承诺**：lazy allocation / COW / swap，把峰值需求转成时间换空间。

性能问题里，VM 的核心是：**地址翻译路径本身就是一条多级缓存链**。

## 地址翻译快照（x86_64 / RISC-V 思维都适用）

访存时的关键路径可抽象成：

1. `VA -> VPN + offset`
2. TLB lookup（并行于 cache tag lookup，具体取决于微架构）
3. TLB miss -> page walk（多级页表访问）
4. 拿到 PTE 后验证权限/状态位
5. `PA = PPN + offset`，进入 data cache hierarchy

重要点：

- TLB miss 不等于 page fault。  
  前者是“翻译缓存没命中”，后者是“PTE 不可用或权限不满足，需要内核介入”。
- page walk 本身会吃 cache；页表不在 cache 时，miss 代价级联放大。

## 代价层级（数量级直觉）

- L1 hit：~几 cycles
- L2/L3 hit：~十到几十 cycles
- DRAM：~百 cycles
- **TLB miss + page walk**：常见几十到上百 cycles（取决于层级命中）
- **minor page fault**（无需磁盘）：通常微秒级
- **major page fault**（涉及磁盘）：毫秒级起步

结论：**把 TLB miss 当成“小 cache miss”会低估问题；把 page fault 当成“慢一点的 miss”是灾难级误判。**

## 页表结构与工程含义

多级页表不是“教科书复杂化”，是对稀疏地址空间的空间优化：

- 大地址空间下，单级页表不可接受（常驻开销过大）。
- 多级 + 按需分配，把页表空间开销和实际映射规模绑定。

对性能敏感系统的含义：

- 热 working set 如果跨太多 page，会推高 TLB pressure。
- 大页（huge page）可降低 TLB miss，但会影响内存碎片和分配灵活性。
- NUMA 下 page placement 失误会把局部 miss 变成远端访问惩罚。

## 常见机制（你要会“推演代价”）

### Copy-on-Write（COW）

- fork 后父子共享只读页，写时 fault 再复制。
- 好处：快 fork、低初始内存。
- 代价：写热点阶段 fault 风暴，且 TLB shootdown / cache 扰动显著。

### Lazy allocation

- 只承诺虚拟范围，不立即分配物理页。
- 好处：启动快、峰值内存可控。
- 风险：首次触达延迟抖动，RT/低延迟路径要显式预热。

### mmap vs read

- `mmap` 省拷贝与 syscall 次数，但 fault 行为与回收策略更复杂。
- 对随机访问大文件通常更友好；对严格尾延迟场景需评估 fault 抖动。

## 观测与定位（不靠猜）

你至少应能回答三类问题：

- **是不是 TLB 压力**：dTLB/iTLB miss 是否与吞吐下降相关？
- **是不是 fault 抖动**：minor/major fault 峰值是否和 p99 对齐？
- **是不是布局问题**：对象跨页、cache line 对齐、访问步长是否恶化局部性？

工具侧（Linux 语义）常见入口：`perf stat`, `perf record`, `/proc/<pid>/smaps`, page-fault counters。

## 设计准则（面向低延迟）

- 把热路径工作集压到更少页内，减少 TLB footprint。
- 大页只给稳定大块热数据，不要全局盲开。
- 启动阶段做显式 pre-touch，避免交易时段首次 fault。
- 数据结构优化优先考虑访问模式，不只是“省内存字节数”。

## 高发误区

- “CPU 利用率不高，所以不是内存问题。”  
  -> 错。内存/翻译停顿会让流水线空转。
- “没有 major fault，就不可能有延迟尖刺。”  
  -> 错。minor fault + shootdown 一样能打爆 p99。
- “TLB miss 一定比 cache miss小很多。”  
  -> 不稳定，取决于 page walk 命中链路。
