## 并发与系统级编程（Concurrency & Systems）

这一块是 C++ 在系统编程中的重要能力，回答的问题是：

> **如何在多核环境下安全、高效地让多个执行流（线程）协同工作？**

可以从几层来理解：

1. 线程：如何启动/管理执行流（`std::thread`）  
2. 同步原语：如何保护共享数据（`mutex` / `lock_guard` / `scoped_lock`）  
3. 原子操作：无锁地修改简单共享状态（`std::atomic`）  
4. 内存模型：不同线程之间“看见”写入的规则（memory ordering / acquire / release）  
5. 无锁结构：在高并发场景下减少锁的使用（lock-free programming，入门概念）  

---

## 一、线程：启动与管理执行流（`std::thread`）

线程是“执行流”的基本单位。C++11 提供了标准的 `std::thread`：

```cpp
#include <thread>
#include <iostream>

void worker(int id) {
    std::cout << "Worker " << id << " running\n";
}

int main() {
    std::thread t1(worker, 1);  // 启动一个新线程
    std::thread t2(worker, 2);  // 再启动一个

    // 等待两个线程结束
    t1.join();
    t2.join();
}
```

要点：

- 构造 `std::thread` 时，就启动了一个新线程，执行给定的函数/可调用对象；  
- `join()`：等待线程结束（阻塞当前线程）；  
- `detach()`：让线程在后台运行，主线程不再等待它（小心资源与生命周期）。  

常见坑：

- 如果线程对象在析构前既没 `join()` 也没 `detach()`，程序会 `std::terminate()`；  
- 所以要么在作用域结束前 `join()`，要么明确 `detach()`，不要“忘记管它”。  

---

## 二、锁与互斥量：保护共享数据（`mutex` / `lock_guard` / `scoped_lock`）

当多个线程访问同一份可写数据时，需要保证：

> **在任意时刻，只有一个线程可以修改这块数据。**

这就用到互斥量（`std::mutex`）和各种锁封装。

### 2.1 `std::mutex` 与手动加锁/解锁

```cpp
#include <mutex>
#include <thread>
#include <vector>

int counter = 0;
std::mutex m;

void increment() {
    for (int i = 0; i < 100000; ++i) {
        m.lock();
        ++counter;
        m.unlock();
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    t1.join();
    t2.join();
}
```

**问题**：`lock()` / `unlock()` 手写非常容易忘记在异常路径或早返回路径解锁。

### 2.2 `std::lock_guard`：RAII 封装锁

`std::lock_guard` 利用 RAII，在构造时加锁，在析构时自动解锁：

```cpp
void increment_safe() {
    for (int i = 0; i < 100000; ++i) {
        std::lock_guard<std::mutex> lk(m);  // 构造时 lock
        ++counter;
    }                                       // 作用域结束自动 unlock
}
```

只要写成这种“局部对象 + 作用域”的形式，即使中途 `return` 或抛异常，锁也会自动释放。

### 2.3 `std::scoped_lock`：一次锁多个互斥量（避免死锁）

当需要同时锁多个 `mutex` 时，容易造成死锁。`std::scoped_lock` 可以一次性安全锁多个：

```cpp
std::mutex m1, m2;

void work() {
    std::scoped_lock lk(m1, m2);  // 一次性锁定 m1 和 m2，避免死锁
    // 在这里安全访问受 m1/m2 保护的资源
}
```

它内部使用无死锁的加锁策略（类似 `std::lock`）。

---

## 三、原子操作：简单共享状态的无锁修改（`std::atomic`）

对于一些简单类型（如整数、指针、布尔值），可以用 `std::atomic` 在 **不使用互斥量** 的情况下安全读写：

```cpp
#include <atomic>

std::atomic<int> acounter{0};

void increment_atomic() {
    for (int i = 0; i < 100000; ++i) {
        acounter.fetch_add(1, std::memory_order_relaxed);
        // 或者 acounter++;
    }
}
```

特点：

- 对单个 `std::atomic<T>` 的读写是原子的（不会被其他线程“撕裂”）；  
- 对简单计数器、标志位、引用计数等非常适合；  
- 但原子并不自动解决所有同步问题——需要结合内存模型来理解“其他线程什么时候能看到这个写入”。  

`std::atomic` 的常用成员：

- `load()` / `store()`  
- `exchange()`  
- `fetch_add()` / `fetch_sub()` / `fetch_or()` / ...  

---

## 四、内存模型与内存序（Memory Model & Memory Ordering）

在多线程环境下，光做到“单个变量的修改是原子的”还不够，还要回答：

> **一个线程的写入，另一个线程何时、以什么顺序可见？**

C++ 内存模型通过 **memory ordering** 来描述这点，常见枚举值有：

- `std::memory_order_relaxed`  
- `std::memory_order_acquire`  
- `std::memory_order_release`  
- `std::memory_order_acq_rel`  
- `std::memory_order_seq_cst`（默认，最强保证）  

### 4.1 acquire / release 模式的直观理解

最常用的模式是 **release + acquire**，用于构建“安全发布（publish）”：

```cpp
std::atomic<bool> ready{false};
int data = 0;

void producer() {
    data = 42;                            // 1. 写入共享数据
    ready.store(true, std::memory_order_release);  // 2. 发布信号
}

void consumer() {
    while (!ready.load(std::memory_order_acquire)) {
        // 自旋等待
    }
    // 一旦 load(acquire) 看到 ready 为 true，
    // 就保证能看到 1 中对 data 的写入
    int x = data;  // 安全读取
}
```

语义可以粗略理解为：

- `release`：之前对内存的写入，在这个 release 操作**前面**，对其他 acquire 线程可见；  
- `acquire`：之后对内存的读写，在这个 acquire 操作**后面**，不会“跑到前面去”。  

比喻：

- `ready.store(..., release)` 像是“门卫放行前，先把屋子整理好”；  
- `ready.load(..., acquire)` 像是“进入屋子时，保证看到的是整理好的状态”。  

### 4.2 `relaxed` 与 `seq_cst`

- `relaxed`：只保证原子性，不保证任何顺序与可见性关系；  
- `seq_cst`：最强保证，逻辑上像所有原子操作都按某个全局顺序执行（默认内存序）。  

实践中：

- 初学/一般业务：可以先用默认的 `seq_cst`；  
- 性能敏感 + 有足够经验时，再精细用 `acquire/release/relaxed`。  

---

## 五、无锁结构与 lock-free 编程（Lock-Free Programming）

在高并发、低延迟系统（如撮合引擎、网络服务）中，互斥锁有时会成为性能瓶颈：

- 锁竞争导致上下文切换；  
- 阻塞导致延迟抖动；  

**无锁结构（lock-free）** 试图通过原子操作 + 内存模型构建数据结构，不使用传统互斥锁：

- lock-free 队列（MPSC/SPMC/MPMC）  
- lock-free 栈  
- 原子引用计数（如 `shared_ptr` 的内部实现）  

一个非常常见的基础原语是 **CAS（Compare-And-Swap）**，在 C++ 中对应：

- `std::atomic::compare_exchange_strong`  
- `std::atomic::compare_exchange_weak`  

示意代码（不建议直接用于生产，仅展示模式）：

```cpp
std::atomic<int> value{0};

void try_update(int expected, int desired) {
    value.compare_exchange_strong(
        expected,           // 预期旧值
        desired,            // 想要写入的新值
        std::memory_order_acq_rel,
        std::memory_order_relaxed
    );
}
```

要点：

- 如果 `value` 当前等于 `expected`，就写入 `desired`，并返回 `true`；  
- 否则不修改 `value`，并把当前值写回 `expected`，返回 `false`；  
- 通过循环 + CAS，可以在不加锁的情况下实现“乐观并发修改”。  

真正的 lock-free 数据结构设计对：

- ABA 问题  
- 内存回收（hazard pointers / epoch-based reclamation）  
- 内存序的精确控制  

要求都很高，一般是库作者、基础设施团队才会深入实现。日常业务更多是**理解其存在和大致原理**，选用成熟库。

---

## 六、整体小结：从线程到无锁

可以把 C++ 并发与系统编程这一块理解为一条从“粗粒度到细粒度”的能力梯度：

1. **线程**：`std::thread` 启动/管理执行流  
2. **互斥量与锁封装**：`std::mutex` + `lock_guard` / `scoped_lock` 保护共享数据  
3. **原子操作**：`std::atomic` 安全修改简单共享状态  
4. **内存模型**：memory ordering（尤其是 acquire/release）定义跨线程可见性  
5. **无锁结构**：在理解原子与内存模型的基础上，构建高性能 lock-free 数据结构  

掌握前 3 层 + 对内存模型的直观理解，已经足以应对大部分 C++ 并发场景；  
真正深入无锁编程，则更多是系统编程和底层库开发的主题。 

