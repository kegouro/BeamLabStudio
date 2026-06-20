#include "services/analysis/ThreadPool.h"

namespace beamlab::services::analysis {

ThreadPool::ThreadPool(std::size_t threadCount)
{
    workers_.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stopped_ = true;
    }
    condition_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (stopped_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks_.push(std::move(task));
    }
    condition_.notify_one();
}

void ThreadPool::waitAll()
{
    // Spin until both queue is empty and no thread is active.
    // ponytail: busy-yield polling for simplicity; upgrade: CV with active_==0 && queue.empty
    while (true) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (tasks_.empty() && active_.load() == 0) return;
        }
        std::this_thread::yield();
    }
}

void ThreadPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stopped_ = true;
    }
    condition_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

void ThreadPool::cancelPending()
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    std::queue<std::function<void()>> empty;
    std::swap(tasks_, empty);
}

void ThreadPool::workerLoop()
{
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            condition_.wait(lock, [this] {
                return stopped_ || !tasks_.empty();
            });

            if (stopped_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        ++active_;
        task();
        --active_;
    }
}

} // namespace beamlab::services::analysis
