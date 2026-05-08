//!@math-begin module="ScoringPlanes" title="Detección de Planos de Scoring — Análisis de Densidad"
//!@math Los planos de scoring (entrada, salida, contador) se detectan automáticamente
//!@math analizando la densidad de puntos a lo largo del eje longitudinal.
//!@section Histograma de densidad axial
//!@math Se divide el rango axial [s_min, s_max] en N_bins bins de ancho uniforme:
//!@formula Δs = (s_max − s_min) / N_bins
//!@math Cada bin acumula los puntos de todas las trayectorias cuya coordenada s cae
//!@math en su intervalo [s_min + k·Δs, s_min + (k+1)·Δs).
//!@section Detección de flancos (transiciones)
//!@math Sea P_peak el conteo máximo del histograma.  Se definen dos umbrales:
//!@formula umbral_alto = f_high · P_peak   (por defecto f_high = 0.30)
//!@formula umbral_bajo = f_low  · P_peak   (por defecto f_low  = 0.05)
//!@math Flanco ascendente (entrada): bin k con P[k] &lt; umbral_bajo  →  P[k+1] > umbral_alto
//!@math Flanco descendente (salida):  bin k con P[k] > umbral_alto  →  P[k+1] &lt; umbral_bajo
//!@note Sólo se marcan flancos con al menos min_crossings trayectorias (por defecto 10).
//!@section Plano contador genérico
//!@math Se añade automáticamente un plano contador en la posición media del haz:
//!@formula s_mid = (s_min + s_max) / 2
//!@math-end

#include "simulation/scoring/ScoringPlaneDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace beamlab::simulation {

namespace {

double axialCoord(const beamlab::core::Vec3& pos,
                  const beamlab::data::AxisFrame& frame)
{
    const double rx = pos.x - frame.origin.x;
    const double ry = pos.y - frame.origin.y;
    const double rz = pos.z - frame.origin.z;
    return rx * frame.longitudinal.x +
           ry * frame.longitudinal.y +
           rz * frame.longitudinal.z;
}

} // namespace

std::vector<ScoringPlane> ScoringPlaneDetector::detect(
    const beamlab::data::TrajectoryDataset& dataset,
    const ScoringDetectionParameters& params) const
{
    std::vector<ScoringPlane> planes{};

    if (dataset.trajectories.empty()) {
        return planes;
    }

    // Collect longitudinal coordinates of all samples
    double s_min = std::numeric_limits<double>::infinity();
    double s_max = -std::numeric_limits<double>::infinity();

    for (const auto& traj : dataset.trajectories) {
        for (const auto& sample : traj.samples) {
            const double s = axialCoord(sample.position_m, dataset.axis_frame);
            s_min = std::min(s_min, s);
            s_max = std::max(s_max, s);
        }
    }

    if (!(s_max > s_min)) {
        return planes;
    }

    const std::size_t bins = params.axial_bins;
    const double width = (s_max - s_min) / static_cast<double>(bins);

    // Histogram: count samples per bin
    std::vector<std::size_t> hist(bins, 0);

    for (const auto& traj : dataset.trajectories) {
        for (const auto& sample : traj.samples) {
            const double s = axialCoord(sample.position_m, dataset.axis_frame);
            auto bin = static_cast<std::size_t>(std::floor((s - s_min) / width));
            if (bin >= bins) {
                bin = bins - 1;
            }
            ++hist[bin];
        }
    }

    const std::size_t peak_count = *std::max_element(hist.begin(), hist.end());

    if (peak_count == 0) {
        return planes;
    }

    const std::size_t high_level = static_cast<std::size_t>(
        params.high_threshold * static_cast<double>(peak_count));
    const std::size_t low_level = static_cast<std::size_t>(
        params.low_threshold * static_cast<double>(peak_count));

    // Detect rising edges (entry) and falling edges (exit)
    // Transition: low→high = entry, high→low = exit

    // Additionally, the global first sample of the first trajectory is an Entry,
    // and the global last sample is an Exit, by convention.

    // Scan for edge transitions in the histogram
    std::size_t plane_id = 0;

    auto make_plane = [&](double axial_m, ScoringPlane::Role role) {
        ScoringPlane plane{};
        plane.id = (role == ScoringPlane::Role::Entry ? "entry_" : "exit_") +
                   std::to_string(plane_id++);
        plane.axial_position_m = axial_m;
        plane.role = role;
        plane.enabled = true;
        return plane;
    };

    bool was_high = false;

    for (std::size_t i = 0; i < bins; ++i) {
        const bool is_high = hist[i] > high_level;
        const bool is_low  = hist[i] < low_level;

        if (!was_high && is_high) {
            // Rising edge → entry plane
            const double axial_m = s_min + (static_cast<double>(i) + 0.5) * width;
            if (hist[i] >= params.min_crossings) {
                planes.push_back(make_plane(axial_m, ScoringPlane::Role::Entry));
            }
            was_high = true;
        } else if (was_high && is_low) {
            // Falling edge → exit plane
            const double axial_m = s_min + (static_cast<double>(i) - 0.5) * width;
            if (hist[i > 0 ? i - 1 : 0] >= params.min_crossings) {
                planes.push_back(make_plane(axial_m, ScoringPlane::Role::Exit));
            }
            was_high = false;
        }
    }

    // If no transitions found, add a global entry at s_min and exit at s_max
    if (planes.empty()) {
        ScoringPlane entry{};
        entry.id = "entry_0";
        entry.axial_position_m = s_min;
        entry.role = ScoringPlane::Role::Entry;
        entry.enabled = true;
        planes.push_back(entry);

        ScoringPlane exit{};
        exit.id = "exit_0";
        exit.axial_position_m = s_max;
        exit.role = ScoringPlane::Role::Exit;
        exit.enabled = true;
        planes.push_back(exit);
    }

    // Also add a generic counter at the midpoint (beam waist proxy)
    if (!planes.empty()) {
        ScoringPlane mid{};
        mid.id = "counter_mid";
        mid.axial_position_m = (s_min + s_max) * 0.5;
        mid.role = ScoringPlane::Role::Counter;
        mid.enabled = true;
        planes.push_back(mid);
    }

    return planes;
}

} // namespace beamlab::simulation
