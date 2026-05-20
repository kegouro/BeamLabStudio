#pragma once

namespace beamlab::core {

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

[[nodiscard]] inline double dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] inline Vec3 subtract(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

} // namespace beamlab::core
