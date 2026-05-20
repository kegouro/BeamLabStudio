#include "core/storage/ISampleStorage.h"
#include "core/storage/InMemoryStorage.h"

#include "data/model/TrajectorySample.h"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

using namespace beamlab::core;

static beamlab::data::TrajectorySample makeSample(double x, double y, double z,
                                                   double edep, double kine)
{
    beamlab::data::TrajectorySample s{};
    s.position_m = {x, y, z};
    s.edep_MeV = edep;
    s.kinE_MeV = kine;
    s.edep_eV = edep * 1.0e6;
    return s;
}

class StorageTest : public ::testing::TestWithParam<std::function<std::unique_ptr<ISampleStorage>()>> {
protected:
    void SetUp() override { storage_ = GetParam()(); }
    std::unique_ptr<ISampleStorage> storage_;
};

static auto makeInMemory = []() { return std::make_unique<InMemoryStorage>(); };

INSTANTIATE_TEST_SUITE_P(InMemory, StorageTest,
    ::testing::Values(makeInMemory));

TEST_P(StorageTest, EmptyStorage)
{
    EXPECT_EQ(storage_->totalSampleCount(), 0u);
    EXPECT_EQ(storage_->trajectoryCount(), 0u);
    EXPECT_TRUE(storage_->trajectoryIds().empty());
}

TEST_P(StorageTest, InsertSingleTrajectory)
{
    storage_->beginTrajectory("1");
    storage_->addSample(makeSample(0.0, 0.0, 0.0, 0.01, 100.0));
    storage_->addSample(makeSample(0.0, 0.0, 1.0, 0.02, 99.0));
    storage_->addSample(makeSample(0.0, 0.0, 2.0, 0.03, 98.0));
    storage_->endTrajectory();

    EXPECT_EQ(storage_->totalSampleCount(), 3u);
    EXPECT_EQ(storage_->trajectoryCount(), 1u);

    auto ids = storage_->trajectoryIds();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], "1");

    auto samples = storage_->getSamplesByTrajectory("1");
    ASSERT_EQ(samples.size(), 3u);
    EXPECT_DOUBLE_EQ(samples[0].kinE_MeV, 100.0);
    EXPECT_DOUBLE_EQ(samples[1].kinE_MeV, 99.0);
    EXPECT_DOUBLE_EQ(samples[2].kinE_MeV, 98.0);
}

TEST_P(StorageTest, BatchQuery)
{
    for (int i = 0; i < 10; ++i) {
        storage_->beginTrajectory(std::to_string(i));
        storage_->addSample(makeSample(0.0, 0.0, static_cast<double>(i), 0.01, 100.0 - i));
        storage_->endTrajectory();
    }

    EXPECT_EQ(storage_->totalSampleCount(), 10u);

    auto batch = storage_->getBatch(3, 4);
    ASSERT_EQ(batch.size(), 4u);
    EXPECT_DOUBLE_EQ(batch[0].position_m.z, 3.0);
    EXPECT_DOUBLE_EQ(batch[3].position_m.z, 6.0);
}

TEST_P(StorageTest, AxialRangeQuery)
{
    for (int i = 0; i < 20; ++i) {
        storage_->beginTrajectory(std::to_string(i));
        storage_->addSample(makeSample(0.0, 0.0, static_cast<double>(i) * 0.5, 0.01, 100.0));
        storage_->endTrajectory();
    }

    auto range = storage_->getAxialRange(3.0, 6.0);
    // z = 0, 0.5, 1.0, ..., 9.5. Range [3.0, 6.0) captures z=3.0,3.5,4.0,4.5,5.0,5.5
    EXPECT_EQ(range.size(), 6u);
    EXPECT_GE(range[0].position_m.z, 3.0);
    EXPECT_LT(range.back().position_m.z, 6.0);
}

TEST_P(StorageTest, MultipleTrajectories)
{
    storage_->beginTrajectory("A");
    storage_->addSample(makeSample(1.0, 0.0, 0.0, 0.1, 50.0));
    storage_->endTrajectory();

    storage_->beginTrajectory("B");
    storage_->addSample(makeSample(2.0, 0.0, 0.0, 0.2, 60.0));
    storage_->addSample(makeSample(2.0, 0.0, 1.0, 0.3, 59.0));
    storage_->endTrajectory();

    EXPECT_EQ(storage_->totalSampleCount(), 3u);
    EXPECT_EQ(storage_->trajectoryCount(), 2u);

    auto a = storage_->getSamplesByTrajectory("A");
    auto b = storage_->getSamplesByTrajectory("B");
    ASSERT_EQ(a.size(), 1u);
    ASSERT_EQ(b.size(), 2u);
    EXPECT_DOUBLE_EQ(a[0].position_m.x, 1.0);
    EXPECT_DOUBLE_EQ(b[1].position_m.z, 1.0);
}

TEST_P(StorageTest, NonExistentTrajectory)
{
    auto samples = storage_->getSamplesByTrajectory("nonexistent");
    EXPECT_TRUE(samples.empty());
}

TEST_P(StorageTest, BatchOutOfRange)
{
    storage_->beginTrajectory("1");
    storage_->addSample(makeSample(0.0, 0.0, 0.0, 0.01, 100.0));
    storage_->endTrajectory();

    auto batch = storage_->getBatch(10, 5);
    EXPECT_TRUE(batch.empty());
}

TEST_P(StorageTest, FactoryCreatesInMemoryForSmallFiles)
{
    auto storage = ISampleStorage::create(50ULL * 1024 * 1024); // 50 MB
    EXPECT_NE(storage, nullptr);
    storage->beginTrajectory("test");
    storage->addSample(makeSample(0.0, 0.0, 0.0, 0.01, 100.0));
    storage->endTrajectory();
    EXPECT_EQ(storage->totalSampleCount(), 1u);
}

TEST(InMemoryStorageTest, FlushIsNoOp)
{
    InMemoryStorage storage;
    storage.beginTrajectory("1");
    storage.addSample(makeSample(0.0, 0.0, 0.0, 0.01, 100.0));
    storage.flush();  // should not throw or lose data
    EXPECT_EQ(storage.totalSampleCount(), 1u);
}
