//!@math-begin module="AxisFrame" title="Resolución del Marco de Referencia del Haz"
//!@math El marco de referencia (axis frame) transforma coordenadas de laboratorio
//!@math (x, y, z) a coordenadas del haz (s, u, v), donde s es el eje longitudinal.
//!@section Detección automática del eje longitudinal
//!@math Se calcula el rango de cada coordenada sobre todos los puntos del dataset:
//!@formula Rₓ = max(x) − min(x),   R_y = max(y) − min(y),   R_z = max(z) − min(z)
//!@math El eje con mayor rango es el eje longitudinal del haz:
//!@formula ê_s = ê_z  si  R_z ≥ R_x  y  R_z ≥ R_y   (caso más común en Geant4)
//!@section Signo del eje longitudinal
//!@math Se suma el vector (último_punto − primer_punto) de cada trayectoria:
//!@formula Σ_z = Σᵢ ( z_final_i − z_inicial_i )
//!@formula signo(ê_s) = +1  si  Σ_z ≥ 0,  −1  si  Σ_z &lt; 0
//!@note Esto garantiza que ê_s apunta en la dirección de propagación del haz.
//!@section Marco resultante
//!@math Para eje Z positivo (caso del CSV de muones):
//!@formula ê_s = (0, 0, +1),   ê_u = (1, 0, 0),   ê_v = (0, 1, 0)
//!@section Confianza del marco
//!@formula confidence = R_max / (Rₓ + R_y + R_z)   ∈ [1/3, 1]
//!@note confidence → 1 cuando el haz es puramente axial; → 1/3 cuando las tres
//!@note extensiones son iguales (ambigüedad).
//!@math-end

#include "io/normalization/AxisFrameResolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace beamlab::io {
namespace {

struct AxisStats {
    double min_x{std::numeric_limits<double>::infinity()};
    double min_y{std::numeric_limits<double>::infinity()};
    double min_z{std::numeric_limits<double>::infinity()};

    double max_x{-std::numeric_limits<double>::infinity()};
    double max_y{-std::numeric_limits<double>::infinity()};
    double max_z{-std::numeric_limits<double>::infinity()};

    double signed_dx_sum{0.0};
    double signed_dy_sum{0.0};
    double signed_dz_sum{0.0};

    std::size_t segment_count{0};
    bool valid{false};
};

AxisStats computeStats(const beamlab::data::TrajectoryDataset& dataset)
{
    AxisStats stats{};

    for (const auto& trajectory : dataset.trajectories) {
        for (const auto& sample : trajectory.samples) {
            const auto& p = sample.position_m;

            stats.min_x = std::min(stats.min_x, p.x);
            stats.min_y = std::min(stats.min_y, p.y);
            stats.min_z = std::min(stats.min_z, p.z);

            stats.max_x = std::max(stats.max_x, p.x);
            stats.max_y = std::max(stats.max_y, p.y);
            stats.max_z = std::max(stats.max_z, p.z);

            stats.valid = true;
        }

        if (trajectory.samples.size() >= 2) {
            const auto& first = trajectory.samples.front().position_m;
            const auto& last = trajectory.samples.back().position_m;

            stats.signed_dx_sum += last.x - first.x;
            stats.signed_dy_sum += last.y - first.y;
            stats.signed_dz_sum += last.z - first.z;
            ++stats.segment_count;
        }
    }

    return stats;
}

double signFromSum(const double value)
{
    return value < 0.0 ? -1.0 : 1.0;
}

} // namespace

beamlab::data::AxisFrame AxisFrameResolver::resolve(const beamlab::data::TrajectoryDataset& dataset) const
{
    const auto stats = computeStats(dataset);

    beamlab::data::AxisFrame frame{};
    frame.origin = {0.0, 0.0, 0.0};

    if (!stats.valid) {
        frame.longitudinal = {0.0, 1.0, 0.0};
        frame.transverse_u = {1.0, 0.0, 0.0};
        frame.transverse_v = {0.0, 0.0, 1.0};
        frame.derivation_method = "fallback_default_y_axis";
        frame.confidence = 0.05;
        return frame;
    }

    const double range_x = stats.max_x - stats.min_x;
    const double range_y = stats.max_y - stats.min_y;
    const double range_z = stats.max_z - stats.min_z;

    if (range_z >= range_x && range_z >= range_y) {
        const double s = signFromSum(stats.signed_dz_sum);
        frame.longitudinal = {0.0, 0.0, s};
        frame.transverse_u = {1.0, 0.0, 0.0};
        frame.transverse_v = {0.0, 1.0, 0.0};
        frame.derivation_method = "auto_largest_extent_z_axis";
    } else if (range_y >= range_x && range_y >= range_z) {
        const double s = signFromSum(stats.signed_dy_sum);
        frame.longitudinal = {0.0, s, 0.0};
        frame.transverse_u = {1.0, 0.0, 0.0};
        frame.transverse_v = {0.0, 0.0, 1.0};
        frame.derivation_method = "auto_largest_extent_y_axis";
    } else {
        const double s = signFromSum(stats.signed_dx_sum);
        frame.longitudinal = {s, 0.0, 0.0};
        frame.transverse_u = {0.0, 1.0, 0.0};
        frame.transverse_v = {0.0, 0.0, 1.0};
        frame.derivation_method = "auto_largest_extent_x_axis";
    }

    const double largest = std::max({range_x, range_y, range_z});
    const double total = range_x + range_y + range_z;
    frame.confidence = total > 0.0 ? largest / total : 0.05;

    return frame;
}

} // namespace beamlab::io
