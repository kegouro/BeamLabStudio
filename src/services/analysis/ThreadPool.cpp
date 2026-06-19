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
    std::promise<void> barrier;
    auto future = barrier.get_future();
    enqueue([&barrier]() { barrier.set_value(); });
    future.wait();
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
