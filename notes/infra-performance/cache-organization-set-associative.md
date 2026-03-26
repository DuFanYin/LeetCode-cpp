# CPU Cache 是怎么组织的：L1 / L2 / L3 与 Set-Associative（组相联）

这篇文档只回答一个问题：**L1/L2/L3 到底是怎样“分组+分路”组织起来的，CPU 如何把一个地址映射到某个 set/way，为什么会冲突（conflict miss）**。

（默认讨论现代 x86/ARM 上常见设计：cache line 64B、L1/L2 私有、L3/LLC 共享；但具体参数以你的 CPU 为准。）

## 1. 三个关键词：line / set / way

### 1.1 Cache line（缓存行）

cache 的搬运单位是 **cache line**，常见 64B。

- 你访问一个地址 `addr`，CPU 会把它所在的那条 64B line 当作一个整体缓存。
- 因此地址最低的 `log2(64)=6` bit 通常是 **offset（行内偏移）**。

### 1.2 Set（组）与 Way（路）

把 cache 想象成一个二维表：

- **一行**叫一个 set（组）
- **每行有 W 个槽位**，叫 W-way associative（W 路组相联）
- 每个槽位可以放 1 条 cache line（外加 tag/状态位）

所以 cache 的容量（只看数据部分）约为：

`Capacity = sets * ways * line_size`

例：32KiB 的 L1D，64B line，8-way，则：

- `sets = 32KiB / (64 * 8) = 64`

## 2. 地址如何映射到 set：offset / index / tag

对一个物理地址（或在某些层级上是虚拟地址，后面会说）：

- **offset**：行内偏移（例如 64B line → 6 bit）
- **index**：选择哪个 set
- **tag**：用来与 set 内各 way 的 tag 比较，判断是否命中

在“理想化”的组相联 cache 中，你可以把地址拆成：

`addr = [tag | index | offset]`

其中：

- `offset_bits = log2(line_size)`
- `index_bits = log2(sets)`
- tag 就是剩下的高位

### 2.1 一个完整例子（把参数带进去）

假设 L1D：32KiB，8-way，64B line。

- line size = 64B → offset bits = 6
- sets = `32KiB / (64 * 8) = 64` → index bits = `log2(64) = 6`

所以：
- offset：低 6 位
- index：再往上的 6 位（总共 12 位决定“哪条 line、哪个 set”）
- tag：更高位

这也解释了“为什么某些 stride 会冲突”：如果你访问的地址序列让 **index 恒定**（或落在很少的几个 index 上），你就会在同一组内把 way 用光，出现 **conflict miss**。

## 3. 直接映射 / 全相联 / 组相联：三种极端与折中

- **Direct-mapped（直接映射）**：1-way。每个 set 只有 1 个槽，硬件简单，但冲突最严重。
- **Fully associative（全相联）**：1 个 set，N-way。几乎不冲突，但 tag 比较与替换逻辑昂贵。
- **Set-associative（组相联）**：折中方案。既限制比较范围（只在一个 set 的 W 个 way 内比），又减少冲突。

现代 CPU 的 L1/L2/L3 基本都是不同参数的 set-associative。

## 4. 命中时发生什么：并行 tag compare + 数据阵列

一次访问在某层 cache 的典型流程（抽象）：

1) 用 index 选中一个 set  
2) 并行比较该 set 内所有 way 的 tag（看哪一个匹配且 valid）  
3) 命中：读取/写入该 way 的 data line，并更新替换元数据（LRU 近似信息等）  
4) 未命中：向更低层请求，把新 line 填进某个 way（涉及替换）  

## 5. 替换策略：LRU（近似）与为什么“抖动”会发生

当 set 满了，新 line 必须替换掉一个旧 way。理论 LRU 成本高，现实实现常见：

- pseudo-LRU（PLRU）
- SRRIP / BRRIP（偏向保留更可能被再次访问的 line）
- 其他近似/分层策略

对 HFT/低延迟代码的意义：

- 你看到的 “p99 抖动” 可能来自 **某个 set 的局部抖动**：访问模式在同一组内反复驱逐/回填（thrash）。

## 6. 为什么会有 conflict miss：从“地址映射”看本质

即使工作集小于 cache 容量，也可能 miss：因为容量够，但 **映射到某些 set 的密度太高**。

最经典的形态是固定 stride：

- 如果访问步长让 index 重复循环在少量 set 上，那些 set 的 way 会不够用 → conflict miss

工程策略（不展开实现细节，只给方向）：

- 改数据布局（让热数据更均匀落到 sets）
- 减少指针追逐、改成更连续的结构
- 对齐/填充避免多个热对象落在“同一组热点”

## 7. L1 / L2 / L3 的典型组织差异（直觉版）

### 7.1 L1（最快、最小、最敏感）

- 通常是 per-core 私有
- 组相联参数较小（例如 32KiB、8-way 这类常见组合）
- 对冲突/抖动非常敏感，因为容量小、延迟预算紧

### 7.2 L2（更大、仍然私有）

- 一般仍是 per-core 私有
- 容量更大、延迟更高
- 更像“过滤器”：把 L1 的 miss 吸收掉一部分，避免直接打到 LLC/DRAM

### 7.3 L3 / LLC（共享、与拓扑相关）

- 多核共享（同 socket 内）
- 仍是 set-associative，但实现更复杂：分 slice、跨核互联、以及一致性目录等
- 访问延迟与“在哪个 slice、从哪个 core 来”有关，尾延迟更容易被拓扑/竞争放大

## 8. 重要现实：并非所有层级都用“同一类地址”索引

一个常见但容易混淆的点：有些层级可能用虚拟地址的一部分做索引（为了更快），但 tag/一致性最终必须落到物理地址语义。

你只需要记住这句工程直觉：

- **“地址映射到 set 的那几位”如果受虚拟地址影响，就可能遇到别名（aliasing）/冲突更复杂；而一旦落到 LLC/DRAM，物理/NUMA 才是最终主宰。**

（这块如果你想更深入，可以再单开一节讲 VIPT/PIPT、page coloring 等。）

## 9. 小结：用 3 句话记住 set-associative

1) Cache 以 **cache line** 为单位缓存数据。  
2) Set-associative 是“**按 index 选 set、在 set 内并行比 tag、命中则取对应 way**”。  
3) **冲突 miss** 本质是“很多热地址被映射到同一组，way 不够用，导致相互驱逐”。  

