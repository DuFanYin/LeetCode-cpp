# 操作系统学习精简路径（面向低延迟系统）

只保留最有价值部分，专门针对 low-latency / 系统方向。目标不是“学 OS”，而是搞清楚哪些东西会直接影响性能。

## 一、只学 5 个核心模块（其他全部跳过）

按这个顺序学习：

### 1. Virtual Memory（最重要）

你要搞清楚：

- page table 是怎么查的
- TLB 是什么（命中/未命中代价）
- 为什么会 page fault
- 用户态 -> 内核态地址隔离

你会直接理解：

**cache miss vs TLB miss vs page fault 的层级差距**

### 2. System Call & Trap

重点：

- syscall 是怎么触发的（`int` / `syscall` 指令）
- user -> kernel 切换发生什么
- 为什么 syscall 很慢（几百 cycles）

你会得到：

**为什么高频系统要避免 syscall**

### 3. Process / Context Switch

重点：

- context switch 保存/恢复什么（寄存器、TLB 等）
- scheduler 怎么决定切换
- 多进程 vs 多线程本质区别

核心结论：

**context switch = cache + TLB 全部打乱**

### 4. Lock / Concurrency

重点：

- spinlock vs sleep lock
- lock contention 会发生什么
- memory ordering（简单理解即可）

你会理解：

**lock != 慢，竞争才慢**

### 5. File System（只看一部分）

只需要懂：

- read/write 路径
- buffer cache
- 为什么 IO 慢

你会知道：

**IO = syscall + disk + cache，多重瓶颈叠加**

## 二、具体怎么学（最省时间）

不要全看，按这个顺序：

### Step 1：只看这几节 lecture

跳过 intro / history，直接看：

- Virtual memory
- Page table
- Traps / syscalls
- Scheduling
- Locking

> 每节控制在理解核心，不要纠结细节。

### Step 2：只看 xv6 book 的关键章节

只看：

- page table
- trap
- proc
- lock

跳过：

- file system 细节
- device driver

### Step 3：只做 2-3 个 lab（关键）

只做：

1. page table（必做）
2. syscall / trap（必做）
3. scheduler 或 lock（二选一）

> 不要全做，收益递减很快。

## 三、你真正要带走的东西（重点）

你学这门课不是为了 OS，而是为了这几个认知：

### 1. latency 层级（最重要）

从快到慢：

- register
- L1 / L2 / L3
- TLB
- memory
- syscall
- context switch
- disk

> 你现在做优化，本质就是在这些层级之间跳。

### 2. “为什么慢”

你以后看到慢代码，要本能想到：

- cache miss？
- TLB miss？
- syscall？
- lock contention？
- context switch？

### 3. user vs kernel boundary

核心理解：

**一旦进 kernel，你就付了很大固定成本。**

所以：

- mmap > read
- batch > 单次调用

### 4. 内存布局影响性能

page、cache line、alignment：

> 这和你写 orderbook / HFT 代码是同一层。

## 四、你可以完全跳过的

不要浪费时间在：

- 文件系统实现细节
- driver
- 早期 bootloader
- 历史内容

## 五、一句话总结

这个精简路径的目标是：

**让你写代码时，脑子里始终有“CPU + OS 在背后做什么”。**

## 六、4 周可打卡执行版（每周 6 天）

建议节奏：每天 60-90 分钟，第 7 天复盘或休息。

### Week 1：Virtual Memory + Page Table

本周目标：建立地址转换和延迟层级的“脑内模型”。

每日最小任务：

- Day 1：看 Virtual Memory 相关 lecture；写 10 行笔记（只记结论）。
- Day 2：看 Page Table 机制；画一张“VA -> PA”流程图。
- Day 3：整理 TLB 命中/未命中代价；写出三层差距：cache miss、TLB miss、page fault。
- Day 4：读 xv6 `page table` 章节；只回答“为什么需要多级页表”。
- Day 5：做 page table lab（第 1 次跑通）。
- Day 6：重做 page table lab（不看答案，自己复现）。

本周验收（必须满足）：

- 能口述一次完整的地址翻译路径（30-60 秒）。
- 能解释 page fault 为什么是“数量级更慢”。
- page table lab 至少独立完成 1 次。

### Week 2：System Call + Trap

本周目标：搞清 user/kernel 边界和固定成本来源。

每日最小任务：

- Day 1：看 Traps / Syscalls lecture，记录 3 个关键词：trap、mode switch、return path。
- Day 2：读 xv6 `trap` 章节；写“syscall 发生了哪些步骤”（5-8 条）。
- Day 3：总结为什么 syscall 慢（权限切换、流水线影响、缓存/TLB 扰动）。
- Day 4：做 syscall / trap lab（先跑通）。
- Day 5：同一个 lab 再做一遍，尽量减少查资料次数。
- Day 6：写一个 1 页小结：什么时候该减少 syscall 次数。

本周验收（必须满足）：

- 能解释 `mmap > read` 在哪些场景成立。
- 能给出 2 个“减少 syscall 频率”的实战策略（如 batch、缓存）。
- syscall / trap lab 至少独立完成 1 次。

### Week 3：Process / Context Switch + Scheduling

本周目标：理解切换成本，避免“线程越多越快”的误区。

每日最小任务：

- Day 1：看 Scheduling lecture；记下 scheduler 的目标（公平性、吞吐、延迟）。
- Day 2：读 xv6 `proc` 章节；整理 context switch 保存/恢复项。
- Day 3：写出“多进程 vs 多线程”在 cache/TLB 层面的差异。
- Day 4：在 scheduler 与 lock lab 中选一个，先跑通。
- Day 5：复盘 lab，定位你最容易卡住的 2 个点。
- Day 6：写一段 200 字总结：为什么 context switch 会导致性能抖动。

本周验收（必须满足）：

- 能解释“context switch = cache + TLB 扰动”的含义。
- 能说清高并发场景下何时多线程反而更慢。
- scheduler 或 lock lab 至少独立完成 1 次。

### Week 4：Lock / Concurrency + 最小 File System 认知

本周目标：形成并发与 IO 的性能直觉。

每日最小任务：

- Day 1：看 Locking lecture；区分 spinlock 与 sleep lock 使用场景。
- Day 2：读 xv6 `lock` 章节；写 lock contention 的 3 个典型症状。
- Day 3：补最小 file system 路径：read/write + buffer cache（只看主路径）。
- Day 4：写“IO 为什么慢”的分层拆解：syscall、cache、device。
- Day 5：做一次综合复盘：把前 3 周笔记压缩成 1 页。
- Day 6：做最终输出：写“低延迟系统性能排查清单（v1）”。

本周验收（必须满足）：

- 能快速判断慢点更可能在 CPU、内存、syscall 还是 IO。
- 能说出“lock != 慢，竞争才慢”的判断依据。
- 产出 1 份自己的性能排查清单（后续可迭代）。

### 每天打卡模板（可直接复制）

```markdown
- 日期：
- 今日模块：
- 今日输入（看了什么）：
- 今日输出（写了什么 / 做了什么 lab）：
- 卡点：
- 明天最小任务（<= 90 分钟）：
```

### 最终目标（4 周后）

你不需要“精通操作系统”，但要做到：

- 看到慢代码时，能先按层级定位（cache/TLB/syscall/context switch/IO）。
- 写性能敏感代码时，主动减少跨 user/kernel 边界次数。
- 对 page、cache line、alignment 有工程化直觉，而不只是概念记忆。
