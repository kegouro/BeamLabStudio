#include "platform/EventBus.h"

#include <gtest/gtest.h>

#include <string>

using namespace beamlab::platform;

// ── Publish / Subscribe ─────────────────────────────────────────────

TEST(EventBusTest, PublishSubscribe)
{
    EventBus bus;
    int count = 0;

    bus.subscribe<ImportProgress>([&](const ImportProgress&) {
        ++count;
    });
    bus.publish(ImportProgress{100, 1000, 5.0});

    EXPECT_EQ(count, 1);
}

TEST(EventBusTest, MultipleSubscribers)
{
    EventBus bus;
    int a = 0, b = 0;

    bus.subscribe<ImportCompleted>([&](const ImportCompleted&) { ++a; });
    bus.subscribe<ImportCompleted>([&](const ImportCompleted&) { ++b; });

    bus.publish(ImportCompleted{{1}, 500});

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
}

TEST(EventBusTest, SubscribeScopedAutoUnsubscribes)
{
    EventBus bus;
    int count = 0;

    {
        auto sub = bus.subscribeScoped<AnalysisProgress>(
            [&](const AnalysisProgress&) { ++count; });
        bus.publish(AnalysisProgress{50, "Computing"});
        EXPECT_EQ(count, 1);
    }

    // ScopedSubscription was destroyed → handler unsubscribed.
    bus.publish(AnalysisProgress{100, "Done"});
    EXPECT_EQ(count, 1);  // Still 1 — no new delivery.
}

// ── Unsubscribe ─────────────────────────────────────────────────────

TEST(EventBusTest, UnsubscribeStopsDelivery)
{
    EventBus bus;
    int count = 0;

    auto id = bus.subscribe<ExportCompleted>(
        [&](const ExportCompleted&) { ++count; });

    bus.publish(ExportCompleted{"/tmp/out"});
    EXPECT_EQ(count, 1);

    bus.unsubscribe(id);
    bus.publish(ExportCompleted{"/tmp/out2"});
    EXPECT_EQ(count, 1);  // Unchanged.
}

// ── Edge cases ──────────────────────────────────────────────────────

TEST(EventBusTest, NoSubscribersNoCrash)
{
    EventBus bus;
    // Publishing an event nobody subscribed to should not crash.
    EXPECT_NO_THROW(bus.publish(ImportStarted{"test.csv", 1024}));
}

TEST(EventBusTest, PublishWithNoMatchingSubscribersNoCrash)
{
    EventBus bus;
    bus.subscribe<ImportCompleted>([](const ImportCompleted&) {});
    // Publish a different event type — should be no-op.
    EXPECT_NO_THROW(bus.publish(AnalysisStarted{{2}}));
}

// ── Event data ──────────────────────────────────────────────────────

TEST(EventBusTest, EventDataIsDeliveredCorrectly)
{
    EventBus bus;
    std::string receivedPath;
    uint64_t receivedSize = 0;

    bus.subscribe<ImportStarted>(
        [&](const ImportStarted& e) {
            receivedPath = e.filePath;
            receivedSize = e.fileSize;
        });

    bus.publish(ImportStarted{"/data/run.csv", 42 * 1024});

    EXPECT_EQ(receivedPath, "/data/run.csv");
    EXPECT_EQ(receivedSize, 42 * 1024);
}

TEST(EventBusTest, SubscriberCount)
{
    EventBus bus;
    EXPECT_EQ(bus.subscriberCount(), 0u);

    bus.subscribe<ImportProgress>([](const ImportProgress&) {});
    EXPECT_EQ(bus.subscriberCount(), 1u);

    bus.subscribe<AnalysisCompleted>([](const AnalysisCompleted&) {});
    EXPECT_EQ(bus.subscriberCount(), 2u);
}

TEST(EventBusTest, ClearRemovesAllSubscribers)
{
    EventBus bus;
    bus.subscribe<ErrorOccurred>([](const ErrorOccurred&) {});
    bus.subscribe<ImportStarted>([](const ImportStarted&) {});
    EXPECT_GE(bus.subscriberCount(), 2u);

    bus.clear();
    EXPECT_EQ(bus.subscriberCount(), 0u);
}

// ── Multiple types, same bus ────────────────────────────────────────

TEST(EventBusTest, MultipleEventTypesIndependent)
{
    EventBus bus;
    int importCount = 0;
    int exportCount = 0;

    bus.subscribe<ImportCompleted>([&](const ImportCompleted&) { ++importCount; });
    bus.subscribe<ExportCompleted>([&](const ExportCompleted&) { ++exportCount; });

    bus.publish(ImportCompleted{{3}, 100});
    bus.publish(ExportCompleted{"/tmp"});

    EXPECT_EQ(importCount, 1);
    EXPECT_EQ(exportCount, 1);
}
