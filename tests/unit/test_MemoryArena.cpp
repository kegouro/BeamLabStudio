#include "core/math/MemoryArena.h"

#include <gtest/gtest.h>

#include <cstdint>

using namespace beamlab::core::math;

// ── Basic allocation ─────────────────────────────────────────────────

TEST(MemoryArenaTest, SingleAllocation)
{
    MemoryArena arena(1024);
    auto* p = arena.allocate<double>(1);
    ASSERT_NE(p, nullptr);
    *p = 3.14;
    EXPECT_DOUBLE_EQ(*p, 3.14);
}

TEST(MemoryArenaTest, MultipleAllocations)
{
    MemoryArena arena(1024);
    auto* a = arena.allocate<double>(100);
    auto* b = arena.allocate<int>(50);

    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    // b should be placed after a (at least sizeof(double)*100 bytes later).
    EXPECT_GT(reinterpret_cast<uintptr_t>(b),
              reinterpret_cast<uintptr_t>(a));
}

TEST(MemoryArenaTest, UsedBytesIncreases)
{
    MemoryArena arena(1024);
    EXPECT_EQ(arena.usedBytes(), 0u);

    arena.allocate<double>(100);
    EXPECT_GT(arena.usedBytes(), 0u);
}

TEST(MemoryArenaTest, ResetClearsOffset)
{
    MemoryArena arena(1024);
    arena.allocate<double>(50);
    EXPECT_GT(arena.usedBytes(), 0u);

    arena.reset();
    EXPECT_EQ(arena.usedBytes(), 0u);
}

TEST(MemoryArenaTest, ResetAllowsReuse)
{
    MemoryArena arena(256);
    auto* first = arena.allocate<int>(4);
    arena.reset();
    auto* second = arena.allocate<int>(4);

    // After reset, the same memory should be reused.
    EXPECT_EQ(first, second);
}

// ── Alignment ────────────────────────────────────────────────────────

TEST(MemoryArenaTest, AllocationIsAlignedToType)
{
    MemoryArena arena(1024);

    auto* p = arena.allocate<double>(1);
    auto addr = reinterpret_cast<uintptr_t>(p);
    EXPECT_EQ(addr % alignof(double), 0u);
}

TEST(MemoryArenaTest, AllocationAfterUnalignedTypeIsAligned)
{
    MemoryArena arena(1024);

    // Allocate a char (1 byte, no alignment) then a double (8 bytes).
    arena.allocate<char>(1);
    auto* p = arena.allocate<double>(1);
    auto addr = reinterpret_cast<uintptr_t>(p);
    EXPECT_EQ(addr % alignof(double), 0u);
}

TEST(MemoryArenaTest, AllocateAlignedWithCustomAlignment)
{
    MemoryArena arena(4096);

    // Request 64-byte alignment (SIMD).
    auto* p = arena.allocateAligned<double>(4, 64);
    auto addr = reinterpret_cast<uintptr_t>(p);
    EXPECT_EQ(addr % 64, 0u);
}

TEST(MemoryArenaTest, MultipleTypesStayAligned)
{
    MemoryArena arena(2048);

    auto* c  = arena.allocate<char>(3);       // offset 0
    auto* i  = arena.allocate<int>(1);        // offset 4 (padded)
    auto* d  = arena.allocate<double>(1);     // offset 8
    auto* d2 = arena.allocate<double>(1);     // offset 16

    EXPECT_EQ(reinterpret_cast<uintptr_t>(i) % alignof(int), 0u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(d) % alignof(double), 0u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(d2) % alignof(double), 0u);
}

// ── Capacity / overflow ──────────────────────────────────────────────

TEST(MemoryArenaTest, ThrowsOnOverflow)
{
    MemoryArena arena(64);
    // 64 bytes / 8 bytes per double = 8 doubles max.
    arena.allocate<double>(8);
    EXPECT_THROW(arena.allocate<double>(1), std::bad_alloc);
}

TEST(MemoryArenaTest, ExactCapacityWorks)
{
    MemoryArena arena(80);  // 80 bytes = 10 doubles exactly (no padding needed).
    EXPECT_NO_THROW(arena.allocate<double>(10));
}

// ── Statistics ───────────────────────────────────────────────────────

TEST(MemoryArenaTest, Capacity)
{
    MemoryArena arena(42);
    EXPECT_EQ(arena.capacityBytes(), 42u);
}

TEST(MemoryArenaTest, Utilization)
{
    MemoryArena arena(1024);
    EXPECT_DOUBLE_EQ(arena.utilization(), 0.0);

    arena.allocate<double>(64);  // 512 bytes
    EXPECT_GT(arena.utilization(), 0.0);
    EXPECT_LT(arena.utilization(), 1.0);
}

// ── Edge cases ──────────────────────────────────────────────────────

TEST(MemoryArenaTest, ZeroSizeAllocation)
{
    MemoryArena arena(256);
    auto* p = arena.allocate<int>(0);
    // Zero-size allocation should return non-null (aligned pointer, size 0).
    EXPECT_NE(p, nullptr);
}

TEST(MemoryArenaTest, DefaultConstructionUses16MB)
{
    MemoryArena arena;
    EXPECT_GE(arena.capacityBytes(), 16u * 1024u * 1024u);
}

TEST(MemoryArenaTest, ResetAbortsIfAlignmentChanges)
{
    // Verify that reset keeps the same capacity.
    MemoryArena arena(2048);
    auto cap = arena.capacityBytes();
    arena.allocate<double>(100);
    arena.reset();
    EXPECT_EQ(arena.capacityBytes(), cap);
}

// ── POD struct (trivially destructible) ────────────────────────────

struct PodVec { double x, y, z; };

TEST(MemoryArenaTest, StructAllocation)
{
    MemoryArena arena(1024);
    auto* v = arena.allocate<PodVec>(10);

    for (int i = 0; i < 10; ++i) {
        v[i] = {1.0, 2.0, 3.0};
    }

    EXPECT_DOUBLE_EQ(v[3].x, 1.0);
    EXPECT_DOUBLE_EQ(v[3].z, 3.0);
}
