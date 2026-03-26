## 并发编程问题类型（高密度清单 + 一句话解释）

---

## 一、正确性（Correctness）

- **Data race**：多个线程无同步地并发读/写同一内存位置（至少一个写），C++ 中直接 UB（推理全部失效）。
- **Atomicity violation**：本应“整体原子”的复合操作（读-改-写、检查-更新等）被插入打断，导致丢更新或中间态被观察。
- **Order violation / Reordering**：编译器/CPU/内存系统允许重排，导致其他线程观察到的顺序与协议预期不一致（例如看到 flag 但读到旧数据）。
- **Memory visibility**：缺少 happens-before，同一份写入未必在目标时刻对其他线程可见（传播延迟 + 缓存/重排）。
- **Torn read/write**：对非原子对象的读写被拆成多次，读到“半新半旧”的值（尤其是跨字宽或非对齐访问）。
- **TOCTOU（check-then-act）**：先检查条件再执行动作，两者间窗口被其他线程改变状态，导致检查结果失效。

---

## 二、同步与锁

- **Deadlock**：锁获取存在环路（循环等待），导致所有相关线程永久阻塞。
- **Livelock**：线程都在运行但不断让步/重试，没有实质进展（忙等但不前进）。
- **Starvation**：某线程长期得不到锁/资源/CPU 时间片，即使系统整体在推进，它一直“饿死”。
- **Priority inversion**：低优先级线程持锁阻塞高优先级线程，中优先级线程抢占 CPU 使高优先级更难推进。
- **Lock convoying**：锁竞争导致线程排队+频繁切换，形成“锁车队”，吞吐显著下降。
- **Over-locking**：锁粒度过大/持锁过久，串行化过多导致性能差。
- **Under-locking**：保护不足或锁覆盖范围不全，关键共享状态仍可能发生 race。

---

## 三、Lock-free / 原子类

- **ABA problem**：CAS 看见值从 A→B→A，以为“没变”，但对象/历史已变（典型在指针/栈/链表头更新中）。
- **CAS contention**：多个线程同时 CAS 同一热点位置，自旋失败频繁，性能随线程数恶化。
- **Spurious failure**：`compare_exchange_weak` 允许无理由失败，需要循环重试（语义规定，不是 bug）。
- **Memory reclamation**：节点可能仍被其他线程访问时就被回收/复用（需要 epoch/hazard pointers/引用计数等方案）。
- **UAF（use-after-free）**：对象释放后仍被访问（无锁结构里常见于回收策略错误）。
- **Dangling pointer**：指针指向的对象生命周期已结束但指针仍存活，任何解引用都不可靠。
- **False progress assumption（lock-free≠wait-free）**：lock-free 只保证“系统整体有人前进”，不保证每个线程都能在有限步内前进。

---

## 四、内存模型（C++/CPU）

- **Reordering（编译器/CPU）**：合法重排使“源码顺序”不等于“别人观察到的顺序”，破坏通信协议。
- **Missing fences（acquire/release 不当）**：同步边用错/缺失，导致“看到信号但看不到数据”或“写入未正确发布”。
- **Out-of-thin-air reads**：极端模型下出现看似“凭空”的读值；实践中用于提醒：别依赖未定义/未同步的顺序推理。
- **Cache coherence latency**：写入通过一致性协议传播到其他核心有延迟；“已写”不等于“他核立刻可见”。

---

## 五、性能与硬件层

- **False sharing**：不同线程更新不同变量，但共享同一 cache line，导致 cache line 在核间来回抖动，性能暴跌。
- **Cache thrashing**：工作集/访问模式导致 cache 频繁失效与替换（或 cache line 频繁迁移），命中率很差。
- **NUMA effects**：跨 NUMA 节点访问远端内存延迟更高/带宽更低，分配与绑核不当会显著变慢。
- **Contention hotspot**：热点锁/热点原子变量导致扩展性差，线程越多越慢。
- **Busy-wait / spin waste**：自旋等待占用 CPU，浪费算力并可能挤压真正干活的线程。

---

## 六、结构设计问题

- **Improper ownership**：所有权/生命周期不清晰（谁负责释放、何时释放），引发 UAF/泄漏/竞态。
- **Shared mutable state**：共享可变状态过多，锁复杂度飙升，难以推理且易出错。
- **Unbounded queue / backpressure 缺失**：队列无限增长，缺少背压，导致内存膨胀与延迟失控。
- **Priority queue starvation**：调度策略不公平，低优先级任务长期得不到执行。
- **Work imbalance**：任务切分/分发不均，部分线程过载、部分闲置，整体吞吐不佳。

---

## 七、并发 I/O / 系统层

- **Blocking in critical path（锁内阻塞）**：持锁期间做 I/O/等待/睡眠，导致锁被长期占用、系统吞吐坍塌。
- **Thundering herd**：大量线程/连接被同时唤醒争抢同一资源，造成上下文切换风暴与抖动。
- **Epoll / select misuse**：事件循环/非阻塞 I/O 使用不当（忙轮询、漏事件、重复注册等），导致高 CPU 或逻辑错误。
- **Thread explosion**：线程数失控（每任务/每连接一线程），调度与内存开销爆炸。

---

## 八、时间与事件

- **Race with timers**：定时器回调与状态变更并发发生，导致超时路径和正常完成路径冲突（重复回调/重复释放等）。
- **Clock skew / non-monotonic time**：系统时间会跳（NTP/手动调整），用它做超时/排序可能倒退；应优先用 monotonic clock。
- **Timeout misuse**：超时语义设计不清（取消/清理不完整、把超时当成功等），产生“假成功/假失败”和资源泄漏。

---

## 九、测试与调试

- **Heisenbug**：加日志/调试后 bug 消失，因为时序被改变，原本的交错不再出现。
- **Non-determinism**：线程调度与交错不稳定，导致执行路径随机、难复现。
- **Insufficient stress testing**：缺少高并发、长时间、随机化/故障注入的压测，隐藏 bug 上线才暴雷。
- **Missing happens-before reasoning**：只看源码顺序不画同步边，不检查可见性与顺序保证，写出“看起来合理但不成立”的并发代码。
