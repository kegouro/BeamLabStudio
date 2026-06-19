#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace beamlab::services::analysis {

/// Fixed-size thread pool for parallel task execution.
///
/// Tasks are submitted via enqueue() and executed by N worker threads.
/// The destructor waits for all pending tasks to complete.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t threadCount =
                            std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// Enqueue a void-returning task.
    void enqueue(std::function<void()> task);

    /// Enqueue a task that returns a value via std::future.
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>
    {
        using ReturnType = typename std::invoke_result_t<F, Args...>;
        auto pkg = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto future = pkg->get_future();

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (stopped_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.emplace([pkg]() { (*pkg)(); });
        }
        condition_.notify_one();
        return future;
    }

    /// Block until all submitted tasks complete.
    void waitAll();

    /// Number of worker threads.
    std::size_t threadCount() const { return workers_.size(); }

    /// Number of threads currently executing a task.
    std::size_t activeThreads() const { return active_.load(); }

    /// Approximate number of tasks waiting in the queue.
    std::size_t pendingTasks() const
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return tasks_.size();
    }

    /// Gracefully wait for all pending + running tasks to finish.
    /// Workers stay alive; only the tasks are awaited.
    void shutdown();

    /// Cancel all pending tasks (running tasks are not interrupted).
    void cancelPending();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stopped_{false};
    std::atomic<std::size_t> active_{0};

    void workerLoop();
};

} // namespace beamlab::services::analysis
