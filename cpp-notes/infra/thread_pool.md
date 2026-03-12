## ThreadPool：固定线程池与任务提交模型

这个 `ThreadPool` 是一个**固定大小的线程池**，负责：

> 提前创建一组工作线程，把后续的任务排队交给这些线程执行，  
> 避免“每个任务都创建 / 销毁一次线程”的高开销。

对应代码：`codes/includes/thread_pool.h`。

---

### 一、接口轮廓：构造、析构与 `submit`

核心接口非常接近常见线程池模型：

```cpp
class ThreadPool {
public:
    explicit ThreadPool(std::size_t threadCount =
                        std::thread::hardware_concurrency());
    ~ThreadPool();

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;
};
```

- 构造函数：  
  - 根据 `threadCount` 启动对应数量的 worker 线程。  
  - 如果传入 0，则至少保证有 1 个线程。
- 析构函数：  
  - 发出停止信号，唤醒所有 worker，等待它们 `join()`。  
  - 确保池子销毁时，不再有悬挂线程。
- `submit`：  
  - 接受任意可调用对象和参数；  
  - 返回一个 `std::future<R>`，用于拿到任务的返回值。

---

### 二、内部结构：任务队列 + 条件变量

几个核心成员：

```cpp
std::vector<std::thread> workers_;              // 工作线程
std::queue<std::function<void()>> tasks_;       // 任务队列
std::mutex mutex_;                              // 保护 tasks_
std::condition_variable cv_;                    // 任务到来/停止信号
bool stop_;                                     // 线程池是否停止
```

worker 循环的典型结构：

```cpp
for (;;) {
    std::function<void()> task;
    {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
        if (stop_ && tasks_.empty()) {
            return;  // 退出线程
        }
        task = std::move(tasks_.front());
        tasks_.pop();
    }
    task();  // 在锁外执行任务
}
```

要点：

- 使用条件变量避免 worker 空转；  
- `stop_` + 空队列 作为退出条件；  
- 任务出队和 `stop_` 检查都在同一个临界区内完成。

---

### 三、`submit`：打包任务并放入队列

`submit` 的关键逻辑是把任意可调用对象 `F` 和参数 `Args...`：

1. 打包成一个 `std::packaged_task<R()>`；  
2. 用一个 `std::function<void()>` 间接调用；  
3. 把这个 `std::function` 推入队列；
4. 返回对应的 `std::future<R>`。

简化版本：

```cpp
using R = std::invoke_result_t<F, Args...>;

auto task = std::make_shared<std::packaged_task<R()>>(
    std::bind(std::forward<F>(f), std::forward<Args>(args)...));

{
    std::lock_guard<std::mutex> lk(mutex_);
    if (stop_) throw std::runtime_error("ThreadPool stopped");
    tasks_.emplace([task] { (*task)(); });
}

cv_.notify_one();
return task->get_future();
```

特点：

- 统一存储类型：队列里永远是 `std::function<void()>`；  
- 返回 future：调用方可以按需 `get()` 结果，也可以忽略返回值；  
- `stop_` 保护：停止后再提交会抛异常，避免静默丢任务。

---

### 四、示例对比：每任务一个线程 vs 线程池

示例：`codes/src/thread_pool_example.cpp`

对比两种做法：

```cpp
// Baseline: spawn one std::thread per task.
std::vector<std::thread> threads;
for (int i = 0; i < TASKS; ++i) {
    threads.emplace_back(work, N);
}
for (auto& t : threads) {
    t.join();
}

// Compare: reuse a fixed ThreadPool for the same tasks.
ThreadPool pool;
std::vector<std::future<void>> futs;
for (int i = 0; i < TASKS; ++i) {
    futs.push_back(pool.submit(work, N));
}
for (auto& f : futs) {
    f.get();
}
```

观察点：

- 任务数量 `TASKS` 较大时，反复创建 / 销毁线程成本很高；  
- 线程池把线程生命周期和任务生命周期解耦，线程长期存在，任务只是排队执行。

---

### 五、适用场景与扩展方向

**适用场景：**

- CPU-bound 或混合型任务，需要并行执行；  
- 任务粒度中等，单个任务执行时间不是“极短”；  
- 不希望在每个任务上都付出 `std::thread` 创建/销毁的完整成本。

**可以扩展的方向：**

- 任务队列使用无锁结构（如 SPSC/MPSC 队列）替换 `std::queue + mutex`；  
- 支持任务优先级、多队列、工作窃取等；  
- 更细粒度的 `stop` 语义（例如：drain 模式、立即终止模式）。

当前实现刻意保持简单，更适合教学与 demo：  
**展示最小可用的“线程池 + future”模型，而不是一个完整工业级执行器。**

---

### 六、小结：原理 & 使用场景

- **原理一句话**：  
  - 预先启动一组 worker 线程，在内部维护一个受 mutex + 条件变量保护的任务队列，`submit` 把任务打包成 `std::function<void()>` 入队，由 worker 循环取出执行并通过 `future` 把结果传回调用方。  
- **典型使用场景**：  
  - 有成批中等粒度任务需要并行执行，又不想为每个任务单独 `std::thread` 的场合（CPU 密集计算、批量 IO 回调处理、简单后台任务调度等）。

