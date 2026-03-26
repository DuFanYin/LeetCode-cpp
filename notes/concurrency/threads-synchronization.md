## Concurrency & Multithreading（高级实战视角）

这一份从“写正确且可维护的并发代码”出发，系统讲解 C++ 中的并发与多线程技术栈，重点放在**线程生命周期管理、同步原语、任务模型和典型陷阱**。

核心问题：

> 多个线程如何安全地协同工作？  
> 任务如何分发与回收？  
> 出错 / 阻塞 / 高负载时如何不“拖垮”整个系统？

---

## 一、线程创建与生命周期管理（`std::thread`）

### 1.1 创建、`join`、`detach`

```cpp
#include <thread>

void worker(int id);

int main() {
    std::thread t(worker, 1);  // 启动线程
    t.join();                  // 等待结束
}
```

- **`join`**：阻塞当前线程，直到子线程结束；  
- **`detach`**：把线程交给运行时后台管理，线程结束时自动回收资源；  
- 若 `std::thread` 对象在析构时既没 `join()` 也没 `detach()` → 程序 `std::terminate()`。

最佳实践：

- 绝大多数业务线程应在某处被 `join`，避免“野线程”；  
- `detach` 仅用于确实需要“后台守护”且有清晰生命周期管理的线程。

### 1.2 线程对象的 RAII 包装

典型模式：用类封装线程行为，在构造启动，析构时确保 `join`/`detach`。

```cpp
class JoiningThread {
public:
    template <typename F, typename... Args>
    explicit JoiningThread(F&& f, Args&&... args)
        : t_(std::forward<F>(f), std::forward<Args>(args)...) {}

    ~JoiningThread() {
        if (t_.joinable()) {
            t_.join();
        }
    }

    std::thread& get() { return t_; }

private:
    std::thread t_;
};
```

这是一种**RAII 线程所有权模式**：对象销毁时自动把线程收尾。

---

## 二、线程参数传递与所有权（Passing Arguments Safely）

### 2.1 值传递与引用传递

默认按值复制参数到新线程：

```cpp
void worker(int x);
std::thread t(worker, 42);   // 传值
```

若要传引用，需用 `std::ref`：

```cpp
void worker(int& x);
int value = 0;
std::thread t(worker, std::ref(value));
```

注意生命周期：

- 被引用的对象（这里是 `value`）必须活得比线程久；  
- 避免把局部变量引用传入线程后，主线程先结束作用域。

### 2.2 传递 `this` 指针与成员函数

```cpp
class Task {
public:
    void run(int n);
};

Task task;
std::thread t(&Task::run, &task, 10);
```

保证：

- `task` 在整个线程执行期间保持存活（例如放在更外层作用域，或使用智能指针管理）。

---

## 三、数据竞争与未定义行为（Data Races & UB）

**数据竞争（data race）** 定义（简化版）：

> 至少两个线程在不使用同步原语保护的前提下，同时访问同一内存位置，其中至少一个是写操作。

一旦发生 data race，行为就是 **未定义（UB）**：

- 不是“偶尔出错”，而是“任何事情都可能发生”（包括看似“正常工作”）。  

典型错误示例：

```cpp
int x = 0;

void writer() { x = 42; }
void reader() { if (x == 42) { /* ... */ } }
```

如果没有任何同步（锁、原子 + 正确内存序），`reader` 可能永远看不到 42，也可能看到“撕裂”的值（对非原子类型）。

防御思路：

- 共享可写数据：使用 `mutex` / 原子；  
- 共享只读数据：在所有线程开始前完成构建；  
- 避免“同时读写 + 无同步”。  

---

## 四、互斥量与锁类型（Mutex Types）

常见互斥相关类型：

- `std::mutex`：最基本的互斥量；  
- `std::timed_mutex`：支持带超时的 `try_lock_for/try_lock_until`；  
- `std::recursive_mutex`：允许同一线程多次 lock（一般应避免，意味着设计有问题）；  
- `std::shared_mutex`（C++17）：读写锁，实现“多读单写”。  

多数情况下：

- 首选 `std::mutex`；  
- 需要读多写少的场景再考虑 `std::shared_mutex`。  

---

## 五、`lock_guard` 与 `unique_lock`（RAII 锁管理）

### 5.1 `std::lock_guard`

简单 RAII 封装：

```cpp
std::mutex m;

void f() {
    std::lock_guard<std::mutex> lk(m);  // 构造时加锁，析构时解锁
    // 受保护区域
}
```

特性：

- 不可解锁 / 重新加锁，仅用于“整个作用域全程持锁”的简单场景。

### 5.2 `std::unique_lock`

提供更灵活的锁管理：

```cpp
std::mutex m;

void f() {
    std::unique_lock<std::mutex> lk(m);   // 默认立即加锁
    // 可以中途 unlock / lock
    lk.unlock();
    // ...
    lk.lock();
}
```

优势：

- 可以延迟加锁（`std::defer_lock`）；  
- 可以与 `std::condition_variable` 一起使用（需要可解锁的锁对象）；  
- 支持移动，不支持拷贝。

---

## 六、读写锁：`std::shared_mutex` 与 `shared_lock`

在“读远多于写”的场景，可以使用读写锁（C++17）：

```cpp
#include <shared_mutex>

std::shared_mutex m;
int data = 0;

void reader() {
    std::shared_lock lk(m);   // 多个读者可以同时持有
    // 读取 data
}

void writer() {
    std::unique_lock lk(m);   // 写者独占
    ++data;
}
```

注意：

- 读写锁在写少读多时更有价值；  
- 若写入频繁，可能引入读/写饥饿问题，需要具体实现策略（某些实现偏向 writer）。  

---

## 七、条件变量与 `wait/notify`（Condition Variables）

适用于“一个线程等待某个条件变为真，由另一个线程唤醒”的场景：

```cpp
#include <condition_variable>

std::mutex              m;
std::condition_variable cv;
bool ready = false;

void producer() {
    {
        std::lock_guard lk(m);
        ready = true;
    }
    cv.notify_one();  // 或 notify_all()
}

void consumer() {
    std::unique_lock lk(m);
    cv.wait(lk, [] { return ready; });  // 使用谓词，防止虚假唤醒
    // ready == true，可以继续
}
```

关键点：

- `wait` 必须持有与 `condition_variable` 关联的互斥量；  
- `wait` 内部会：先解锁互斥量 → 挂起线程 → 被唤醒后重新加锁；
- 使用 **带谓词版本**：`wait(lock, pred)`，以正确应对**虚假唤醒**。

---

## 八、虚假唤醒（Spurious Wakeups）与正确写法

虚假唤醒指的是：

> 即使没有线程调用 `notify_one/notify_all`，`wait` 也有可能返回。

因此正确模式永远是：

```cpp
cv.wait(lk, [] { return condition; });
// 或者手动写循环：
while (!condition) {
    cv.wait(lk);
}
```

**不要写成**：

```cpp
cv.wait(lk);  // 醒了就当条件满足，这是错误的假设
```

---

## 九、`future` / `promise` / `async` 与异步任务

### 9.1 `std::async`

简化版异步执行接口：

```cpp
#include <future>

int compute();

std::future<int> fut = std::async(std::launch::async, compute);
int result = fut.get();   // 阻塞直到 compute 完成
```

`std::launch` 策略：

- `std::launch::async`：一定在新线程异步执行；  
- `std::launch::deferred`：在 `get()` 或 `wait()` 时 lazy 执行；  
- 可以组合（实现可以选择策略）。  

### 9.2 `promise` + `future`

手工在线程之间传递结果：

```cpp
void worker(std::promise<int> p) {
    try {
        int r = /* ... */;
        p.set_value(r);
    } catch (...) {
        p.set_exception(std::current_exception());
    }
}

std::promise<int> p;
std::future<int>  f = p.get_future();
std::thread t(worker, std::move(p));

int result = f.get();   // 若 worker 抛异常，这里会重新抛出
t.join();
```

---

## 十、跨线程传递异常（Exception Propagation）

如上所示，通过 `promise.set_exception(std::current_exception())` 可以把子线程中的异常传回主线程，在 `future.get()` 时重新抛出。

`std::async` 内部也会自动捕获函数中的异常，并在 `future.get()` 时重新抛出。

---

## 十一、工作队列与任务调度（Work Queues & Task Scheduling）

一个常见并发模式：**生产者–消费者 + 工作队列**。

简化示例（单队列、多工作线程）：

```cpp
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

std::queue<int>         q;
std::mutex              m;
std::condition_variable cv;
bool                    done = false;

void producer() {
    for (int i = 0; i < 100; ++i) {
        {
            std::lock_guard lk(m);
            q.push(i);
        }
        cv.notify_one();
    }
    {
        std::lock_guard lk(m);
        done = true;
    }
    cv.notify_all();
}

void consumer() {
    while (true) {
        std::unique_lock lk(m);
        cv.wait(lk, [] { return !q.empty() || done; });
        if (!q.empty()) {
            int x = q.front();
            q.pop();
            lk.unlock();
            // 处理 x
        } else if (done) {
            break;
        }
    }
}
```

这就是线程池 / 任务队列实现的基础形态。

---

## 十二、死锁 / 活锁 / 饥饿（Deadlock, Livelock, Starvation）

### 12.1 死锁（Deadlock）

**典型四条件**（简化）：

1. 互斥：资源一次只能被一个线程持有；  
2. 请求并保持：线程持有旧资源同时请求新资源；  
3. 不可抢占；  
4. 循环等待。  

常见场景：两个线程以不同顺序锁同一组互斥量：

```cpp
// T1: lock(m1) -> lock(m2)
// T2: lock(m2) -> lock(m1)
```

避免策略：

- 遵守**全局锁顺序**（所有线程按同一顺序获取多把锁）；  
- 使用 `std::lock` / `std::scoped_lock` 一次性锁多把互斥量。  

### 12.2 活锁（Livelock）

线程没有阻塞，但因为彼此不断“让步”或重试，系统整体没有前进（例如多个线程不断重试 CAS，但一直失败）。

### 12.3 饥饿（Starvation）

某些线程长期得不到资源（比如总被高优先级任务抢占）。

缓解手段：

- 公平锁策略；  
- 任务调度中考虑优先级与 aging。  

---

## 十三、降低竞争与提升伸缩性（Contention Reduction）

为了在多核上扩展性能，需要减少“所有线程抢同一把锁”的情况。

常见技巧：

- **分片锁 / 分段锁（sharding, striping）**：对哈希表等使用多把锁，每一部分独立；  
- **局部缓冲 / 批量提交**：线程本地累积结果，偶尔合并到共享结构；  
- **读–写分离**：用读写锁或版本化结构，让读取不阻塞写入；  
- 使用无锁结构时，减小共享热点（减少在同一个原子变量上的高频争用）。  

---

## 十四、伪共享与缓存对齐（False Sharing & Cache Alignment）

**False sharing（伪共享）**：

> 不同线程操作不同变量，但这些变量落在同一个 cache line 里，导致频繁 cache line 失效和总线同步。

简例：

```cpp
struct Counters {
    std::atomic<int> a;
    std::atomic<int> b;
};

Counters c;

// T1 频繁写 c.a，T2 频繁写 c.b
```

如果 `a` 和 `b` 在同一 cache line 中，两个线程会互相干扰，性能严重下降。

缓解方法：

- 使用 `alignas` 把热点字段分开放在不同 cache line：

```cpp
struct PaddedCounter {
    alignas(64) std::atomic<int> a;
    alignas(64) std::atomic<int> b;
};
```

- 或在布局上人为留出 padding，让热点变量不共享 cache line。  

---

## 十五、小结：并发编程的实践原则

1. **优先用高层抽象**：能用任务模型（线程池、`async`、工作队列）就不要到处 `std::thread`。  
2. **RAII 管理一切资源**：线程、锁、条件变量等待等，都用 RAII 封装，避免“忘记收尾”。  
3. **显式设计共享数据结构**：写代码前先画出“哪些变量被哪些线程共享，如何保护”的图。  
4. **从简到繁**：先用最简单正确的同步（粗粒度锁），再根据性能瓶颈细化；不要一开始就上 lock-free。  
5. **尊重内存模型**：原子 + 正确的 memory ordering 是高阶武器，确保理解 `acquire/release` 语义后再使用。  

把这些实践和你在 `concurrency_and_systems.md` 里的基础知识结合起来，可以形成一套从“API 级并发”到“系统级并发设计”的完整体系。 

