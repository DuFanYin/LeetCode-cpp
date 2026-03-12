#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// Basic fixed-size thread pool for fire-and-forget tasks with futures.
class ThreadPool {
public:
    // Spawn worker threads immediately; threadCount==0 falls back to 1.
    explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency())
        : stop_(false) {
        if (threadCount == 0) {
            threadCount = 1;
        }
        for (std::size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    // Join all workers and drain remaining tasks.
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    template <typename F, typename... Args>
    // Enqueue a callable and get a future for its result.
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool stopped");
            }
            tasks_.emplace([task] { (*task)(); });
        }

        cv_.notify_one();
        return task->get_future();
    }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(mutex_);
                cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_;
};

