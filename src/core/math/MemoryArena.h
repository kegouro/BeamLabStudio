#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace beamlab::core::math {

/// High-speed sequential allocator for trivially-destructible types.
///
/// Pre-reserves a large block of memory and doles it out via allocate()
/// with a monotonically-increasing offset.  reset() rewinds the offset
/// to zero in O(1) — no individual deallocations, no destructors called.
///
/// ## Safety
///
/// This arena NEVER calls destructors.  It is safe ONLY for types that
/// satisfy `std::is_trivially_destructible_v<T>`:
///   - OK:  double, int, Vec3, pod structs, arrays of POD
///   - NOT OK:  std::string, std::vector, unique_ptr, QObject
///
/// If you store a non-trivial type in the arena, its destructor will
/// silently never run.  Reset is NOT a substitute for placement-delete.
///
/// ## Usage pattern
///
/// ```cpp
/// MemoryArena arena(32 * 1024 * 1024);  // 32 MB
/// auto* buf = arena.allocate<double>(100000);
/// // ... process ...
/// arena.reset();  // O(1), ready for next batch
/// ```
class MemoryArena {
public:
    /// \param capacityBytes  Total size of the internal buffer.
    ///                       Must be > 0.  Default: 16 MiB.
    explicit MemoryArena(std::size_t capacityBytes = 16 * 1024 * 1024);

    MemoryArena(const MemoryArena&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;
    MemoryArena(MemoryArena&&) = delete;
    MemoryArena& operator=(MemoryArena&&) = delete;

    /// Allocate `count` objects of type T, aligned to alignof(T).
    /// \throws std::bad_alloc if the arena is full.
    template<typename T>
    [[nodiscard]] T* allocate(std::size_t count = 1)
    {
        return allocateAligned<T>(count, alignof(T));
    }

    /// Allocate `count` objects of type T with explicit alignment.
    /// Use for SIMD types that need 16-, 32-, or 64-byte alignment.
    /// \throws std::bad_alloc if the arena is full.
    template<typename T>
    [[nodiscard]] T* allocateAligned(std::size_t count,
                                      std::size_t alignment);

    /// Reset the arena for reuse.  O(1).
    /// Does NOT call destructors on any previously-allocated objects.
    void reset() noexcept { offset_ = 0; }

    /// Bytes currently consumed.
    [[nodiscard]] std::size_t usedBytes() const noexcept
    {
        return offset_;
    }

    /// Total arena capacity in bytes.
    [[nodiscard]] std::size_t capacityBytes() const noexcept
    {
        return buffer_.size();
    }

    /// Fraction of the arena that has been used (0.0–1.0).
    [[nodiscard]] double utilization() const noexcept
    {
        auto cap = capacityBytes();
        return (cap > 0) ? static_cast<double>(usedBytes()) /
                            static_cast<double>(cap) : 0.0;
    }

private:
    std::vector<std::uint8_t> buffer_;
    std::size_t offset_{0};

    static constexpr std::size_t kDefaultAlignment =
        alignof(std::max_align_t);
};

// ── Template implementation ──────────────────────────────────────────

template<typename T>
T* MemoryArena::allocateAligned(std::size_t count,
                                 [[maybe_unused]] std::size_t alignment)
{
    std::size_t size = count * sizeof(T);

    // Align the offset up to the required alignment.
    std::size_t alignedOffset = offset_;
    std::size_t mask = alignment - 1;
    if ((alignedOffset & mask) != 0) {
        alignedOffset = (alignedOffset + mask) & ~mask;
    }

    if (alignedOffset + size > buffer_.size()) {
        throw std::bad_alloc();
    }

    auto* ptr = reinterpret_cast<T*>(buffer_.data() + alignedOffset);
    offset_ = alignedOffset + size;
    return ptr;
}

} // namespace beamlab::core::math
