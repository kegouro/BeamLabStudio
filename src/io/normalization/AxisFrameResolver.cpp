//!@math-begin module="AxisFrame" title="Resolución del Marco de Referencia del Haz"
//!@math El marco de referencia (axis frame) transforma coordenadas de laboratorio
//!@math (x, y, z) a coordenadas del haz (s, u, v), donde s es el eje longitudinal.
//!@section Detección automática del eje longitudinal (PCA sobre desplazamientos)
//!@math Para cada trayectoria con ≥2 muestras, se computan vectores de paso
//!@math Δ_i = pos_{i+1} − pos_i.
//!@math Sobre todos los Δ se construye la matriz de covarianza 3×3:
//!@formula Σ = (1/N) Σ_i (Δ_i − Δ̄)(Δ_i − Δ̄)ᵀ
//!@math PCA: se calcula el eigenvector dominante ê₁ (λ₁ ≥ λ₂ ≥ λ₃).
//!@math La confianza del eje es la fracción de varianza explicada:
//!@formula confidence = λ₁ / (λ₁ + λ₂ + λ₃) = λ₁ / tr(Σ)   ∈ [1/3, 1]
//!@math El signo de ê₁ se ajusta para que apunte en la dirección de propagación:
//!@formula signo = +1 si (Δ̄ · ê₁) ≥ 0, −1 en caso contrario.
//!@section Fallback por extensión espacial
//!@math Si hay menos de 3 vectores de desplazamiento total, se usa el algoritmo
//!@math de rango espacial: ê_s = eje de mayor rango (R_max), con confianza
//!@math confidence = R_max / (R_x + R_y + R_z).
//!@math-end

#include "io/normalization/AxisFrameResolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace beamlab::io {
namespace {

using beamlab::core::Vec3;

[[nodiscard]] Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

[[nodiscard]] double magnitude(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

[[nodiscard]] Vec3 normalize(const Vec3& v) {
    const double m = magnitude(v);
    if (m < 1e-30) return {0.0, 0.0, 0.0};
    return {v.x / m, v.y / m, v.z / m};
}

void powerIteration(const double cov[3][3], Vec3& eigenvector, double& eigenvalue, int maxIter = 20) {
    eigenvector = {1.0, 0.0, 0.0};
    for (int iter = 0; iter < maxIter; ++iter) {
        Vec3 y = {
            cov[0][0] * eigenvector.x + cov[0][1] * eigenvector.y + cov[0][2] * eigenvector.z,
            cov[1][0] * eigenvector.x + cov[1][1] * eigenvector.y + cov[1][2] * eigenvector.z,
            cov[2][0] * eigenvector.x + cov[2][1] * eigenvector.y + cov[2][2] * eigenvector.z};
        eigenvector = normalize(y);
    }
    Vec3 y = {
        cov[0][0] * eigenvector.x + cov[0][1] * eigenvector.y + cov[0][2] * eigenvector.z,
        cov[1][0] * eigenvector.x + cov[1][1] * eigenvector.y + cov[1][2] * eigenvector.z,
        cov[2][0] * eigenvector.x + cov[2][1] * eigenvector.y + cov[2][2] * eigenvector.z};
    eigenvalue = dot(eigenvector, y);
}

struct DisplacementStats {
    Vec3 sum{0.0, 0.0, 0.0};
    double cov_xx{0.0}, cov_yy{0.0}, cov_zz{0.0};
    double cov_xy{0.0}, cov_xz{0.0}, cov_yz{0.0};
    double range_x{0.0}, range_y{0.0}, range_z{0.0};
    double signed_dx_sum{0.0}, signed_dy_sum{0.0}, signed_dz_sum{0.0};
    double min_x{0.0}, max_x{0.0}, min_y{0.0}, max_y{0.0}, min_z{0.0}, max_z{0.0};
    std::size_t disp_count{0};
    bool valid{false};
};

DisplacementStats computeDisplacementStats(const beamlab::data::TrajectoryDataset& dataset) {
    DisplacementStats s{};
    s.min_x = std::numeric_limits<double>::infinity();
    s.min_y = std::numeric_limits<double>::infinity();
    s.min_z = std::numeric_limits<double>::infinity();
    s.max_x = -std::numeric_limits<double>::infinity();
    s.max_y = -std::numeric_limits<double>::infinity();
    s.max_z = -std::numeric_limits<double>::infinity();

    for (const auto& trajectory : dataset.trajectories) {
        for (const auto& sample : trajectory.samples) {
            const auto& p = sample.position_m;
            s.min_x = std::min(s.min_x, p.x);
            s.min_y = std::min(s.min_y, p.y);
            s.min_z = std::min(s.min_z, p.z);
            s.max_x = std::max(s.max_x, p.x);
            s.max_y = std::max(s.max_y, p.y);
            s.max_z = std::max(s.max_z, p.z);
            s.valid = true;
        }
        if (trajectory.samples.size() >= 2) {
            const auto& first = trajectory.samples.front().position_m;
            const auto& last = trajectory.samples.back().position_m;
            s.signed_dx_sum += last.x - first.x;
            s.signed_dy_sum += last.y - first.y;
            s.signed_dz_sum += last.z - first.z;
        }
    }

    s.range_x = s.max_x - s.min_x;
    s.range_y = s.max_y - s.min_y;
    s.range_z = s.max_z - s.min_z;

    // PCA on displacement vectors
    Vec3 disp_mean{0.0, 0.0, 0.0};
    for (const auto& trajectory : dataset.trajectories) {
        const auto& samples = trajectory.samples;
        const auto n = samples.size();
        for (std::size_t i = 0; i + 1 < n; ++i) {
            const auto& p0 = samples[i].position_m;
            const auto& p1 = samples[i + 1].position_m;
            const Vec3 d = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
            s.sum.x += d.x;
            s.sum.y += d.y;
            s.sum.z += d.z;
            ++s.disp_count;
        }
    }
    if (s.disp_count > 0) {
        disp_mean = {s.sum.x / static_cast<double>(s.disp_count),
                     s.sum.y / static_cast<double>(s.disp_count),
                     s.sum.z / static_cast<double>(s.disp_count)};
    }

    for (const auto& trajectory : dataset.trajectories) {
        const auto& samples = trajectory.samples;
        const auto n = samples.size();
        for (std::size_t i = 0; i + 1 < n; ++i) {
            const auto& p0 = samples[i].position_m;
            const auto& p1 = samples[i + 1].position_m;
            const double dx = (p1.x - p0.x) - disp_mean.x;
            const double dy = (p1.y - p0.y) - disp_mean.y;
            const double dz = (p1.z - p0.z) - disp_mean.z;
            s.cov_xx += dx * dx;
            s.cov_yy += dy * dy;
            s.cov_zz += dz * dz;
            s.cov_xy += dx * dy;
            s.cov_xz += dx * dz;
            s.cov_yz += dy * dz;
        }
    }

    if (s.disp_count > 0) {
        const double inv = 1.0 / static_cast<double>(s.disp_count);
        s.cov_xx *= inv;
        s.cov_yy *= inv;
        s.cov_zz *= inv;
        s.cov_xy *= inv;
        s.cov_xz *= inv;
        s.cov_yz *= inv;
    }

    return s;
}

double signFromSum(const double value) {
    return value < 0.0 ? -1.0 : 1.0;
}

[[nodiscard]] Vec3 orthogonal(const Vec3& v) {
    if (std::abs(v.x) < std::abs(v.y) && std::abs(v.x) < std::abs(v.z)) {
        return {0.0, -v.z, v.y};
    }
    if (std::abs(v.y) < std::abs(v.z)) {
        return {-v.z, 0.0, v.x};
    }
    return {-v.y, v.x, 0.0};
}

beamlab::data::AxisFrame resolveFallback(const DisplacementStats& s) {
    beamlab::data::AxisFrame frame{};
    frame.origin = {0.0, 0.0, 0.0};

    if (!s.valid) {
        frame.longitudinal = {0.0, 1.0, 0.0};
        frame.transverse_u = {1.0, 0.0, 0.0};
        frame.transverse_v = {0.0, 0.0, 1.0};
        frame.derivation_method = "fallback_default_y_axis";
        frame.confidence = 0.05;
        return frame;
    }

    if (s.range_z >= s.range_x && s.range_z >= s.range_y) {
        frame.longitudinal = {0.0, 0.0, signFromSum(s.signed_dz_sum)};
        frame.transverse_u = {1.0, 0.0, 0.0};
        frame.transverse_v = {0.0, 1.0, 0.0};
        frame.derivation_method = "auto_largest_extent_z_axis";
    } else if (s.range_y >= s.range_x && s.range_y >= s.range_z) {
        frame.longitudinal = {0.0, signFromSum(s.signed_dy_sum), 0.0};
        frame.transverse_u = {1.0, 0.0, 0.0};
        frame.transverse_v = {0.0, 0.0, 1.0};
        frame.derivation_method = "auto_largest_extent_y_axis";
    } else {
        frame.longitudinal = {signFromSum(s.signed_dx_sum), 0.0, 0.0};
        frame.transverse_u = {0.0, 1.0, 0.0};
        frame.transverse_v = {0.0, 0.0, 1.0};
        frame.derivation_method = "auto_largest_extent_x_axis";
    }

    const double largest = std::max({s.range_x, s.range_y, s.range_z});
    const double total = s.range_x + s.range_y + s.range_z;
    frame.confidence = total > 0.0 ? largest / total : 0.05;

    return frame;
}

} // namespace

beamlab::data::AxisFrame AxisFrameResolver::resolve(const beamlab::data::TrajectoryDataset& dataset) const
{
    const auto s = computeDisplacementStats(dataset);

    // Fallback if not enough displacement vectors for PCA
    if (s.disp_count < 3 || !s.valid) {
        return resolveFallback(s);
    }

    // Build 3x3 covariance matrix
    const double cov[3][3] = {
        {s.cov_xx, s.cov_xy, s.cov_xz},
        {s.cov_xy, s.cov_yy, s.cov_yz},
        {s.cov_xz, s.cov_yz, s.cov_zz}
    };

    const double trace = cov[0][0] + cov[1][1] + cov[2][2];
    if (trace < 1e-30) {
        return resolveFallback(s);
    }

    // PCA via power iteration
    Vec3 eigen{};
    double eigenvalue = 0.0;
    powerIteration(cov, eigen, eigenvalue);

    const double confidence = trace > 0.0 ? eigenvalue / trace : 0.05;

    // Determine sign: displacement mean must point in direction of propagation
    const Vec3 disp_mean = {s.sum.x / static_cast<double>(s.disp_count),
                            s.sum.y / static_cast<double>(s.disp_count),
                            s.sum.z / static_cast<double>(s.disp_count)};
    if (dot(disp_mean, eigen) < 0.0) {
        eigen.x = -eigen.x;
        eigen.y = -eigen.y;
        eigen.z = -eigen.z;
    }

    const Vec3 longitudinal = normalize(eigen);
    const Vec3 transverse_u = normalize(cross(longitudinal, orthogonal(longitudinal)));
    const Vec3 transverse_v = normalize(cross(longitudinal, transverse_u));

    beamlab::data::AxisFrame frame{};
    frame.origin = {0.0, 0.0, 0.0};
    frame.longitudinal = longitudinal;
    frame.transverse_u = transverse_u;
    frame.transverse_v = transverse_v;
    frame.derivation_method = "pca_on_displacement_vectors";
    frame.confidence = std::clamp(confidence, 0.0, 1.0);

    return frame;
}

} // namespace beamlab::io
