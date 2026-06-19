#include "core/math/MemoryArena.h"

namespace beamlab::core::math {

MemoryArena::MemoryArena(std::size_t capacityBytes)
    : buffer_(capacityBytes > 0 ? capacityBytes : 16 * 1024 * 1024)
    , offset_(0)
{
}

} // namespace beamlab::core::math
