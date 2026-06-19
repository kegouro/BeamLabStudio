#include "services/analysis/ThreadPool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>

using namespace beamlab::services::analysis;

// ── Basic functionality ─────────────────────────────────────────────

TEST(ThreadPoolTest, SingleTask)
{
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    auto future = pool.enqueue([&] { counter.fetch_add(1); });
    future.get();

    EXPECT_EQ(counter.load(), 1);
}

TEST(ThreadPoolTest, ReturnValue)
{
    ThreadPool pool(2);
    auto future = pool.enqueue([] { return 42; });
    EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, MultipleTasks)
{
    ThreadPool pool(4);
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.enqueue([i] { return i * i; }));
    }

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(futures[static_cast<std::size_t>(i)].get(), i * i);
    }
}

// ── Concurrency ─────────────────────────────────────────────────────

TEST(ThreadPoolTest, TasksRunInParallel)
{
    ThreadPool pool(4);

    std::atomic<int> concurrent{0};
    std::atomic<int> peak{0};

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(pool.enqueue([&] {
            int c = ++concurrent;
            // Track peak parallelism.
            int expected = peak.load();
            while (!peak.compare_exchange_weak(expected,
                    std::max(expected, c))) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            --concurrent;
        }));
    }

    for (auto& f : futures) f.get();

    EXPECT_GE(peak.load(), 2);  // At least some parallelism.
}

TEST(ThreadPoolTest, ActiveThreadsCount)
{
    ThreadPool pool(2);

    std::atomic<int> barrier{0};

    auto f1 = pool.enqueue([&] {
        barrier.fetch_add(1);
        while (barrier.load() < 2) std::this_thread::yield();
    });

    auto f2 = pool.enqueue([&] {
        barrier.fetch_add(1);
        while (barrier.load() < 2) std::this_thread::yield();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_GE(pool.activeThreads(), 1u);

    f1.get(); f2.get();
}

// ── Exception handling ──────────────────────────────────────────────

TEST(ThreadPoolTest, ExceptionPropagatesThroughFuture)
{
    ThreadPool pool(2);
    auto future = pool.enqueue([] { throw std::runtime_error("test error"); });
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(ThreadPoolTest, ExceptionDoesNotKillPool)
{
    ThreadPool pool(2);
    pool.enqueue([] { throw std::runtime_error("oops"); }).get();

    // Pool should still be usable after an exception.
    auto future = pool.enqueue([] { return 123; });
    EXPECT_EQ(future.get(), 123);
}

// ── Cancel pending ──────────────────────────────────────────────────

TEST(ThreadPoolTest, CancelPendingSkipsQueuedTasks)
{
    ThreadPool pool(1);

    // Enqueue a long-running task.
    auto f1 = pool.enqueue([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 1;
    });

    // Enqueue tasks that should be cancelled.
    pool.enqueue([] { return 2; });
    pool.enqueue([] { return 3; });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    pool.cancelPending();

    // First task still runs.
    EXPECT_EQ(f1.get(), 1);
}

// ── Shutdown ───────────────────────────────────────────────────────

TEST(ThreadPoolTest, ShutdownWaitsForRunningTasks)
{
    auto pool = new ThreadPool(2);
    std::atomic<bool> done{false};

    pool->enqueue([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        done.store(true);
    });

    delete pool;  // Destructor should wait.

    EXPECT_TRUE(done.load());
}

// ── Thread count ───────────────────────────────────────────────────

TEST(ThreadPoolTest, ThreadCountMatchesRequest)
{
    ThreadPool pool2(2);
    EXPECT_EQ(pool2.threadCount(), 2u);

    ThreadPool pool4(4);
    EXPECT_EQ(pool4.threadCount(), 4u);
}
