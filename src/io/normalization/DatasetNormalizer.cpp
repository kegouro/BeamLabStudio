//!@math-begin module="Normalization" title="Normalización del Dataset — Resúmenes por Trayectoria"
//!@math La normalización ordena las muestras de cada trayectoria por tiempo y
//!@math calcula los resúmenes físicos.
//!@section Ordenación temporal
//!@math Las muestras de cada trayectoria se ordenan por t [s] ascendente.
//!@section Longitud de trayectoria (range)
//!@math Se calcula como la suma de las distancias euclídeas entre pasos consecutivos:
//!@formula L = Σᵢ |p⃗ᵢ − p⃗ᵢ₋₁|   con  |Δp⃗| = √(Δx² + Δy² + Δz²)
//!@section Energía total depositada
//!@formula E_dep_total [MeV] = Σᵢ edep_MeVᵢ
//!@formula E_dep_total [eV]  = E_dep_total [MeV] · 10⁶
//!@section Energías de entrada y salida
//!@formula E_entry = kinE_MeV del primer paso (después de ordenar por t)
//!@formula E_exit  = kinE_MeV del último paso
//!@note E_entry ≈ E_gun para el primer paso del primario en Geant4.
//!@math-end

#include "io/normalization/DatasetNormalizer.h"

#include "io/normalization/AxisFrameResolver.h"
#include "io/normalization/EvolutionAxisResolver.h"

#include <algorithm>
#include <cmath>

namespace beamlab::io {

namespace {

double vec3dist(const beamlab::core::Vec3& a, const beamlab::core::Vec3& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

} // namespace

beamlab::data::TrajectoryDataset DatasetNormalizer::normalize(beamlab::data::TrajectoryDataset dataset) const
{
    for (auto& trajectory : dataset.trajectories) {
        std::sort(trajectory.samples.begin(), trajectory.samples.end(),
                  [](const auto& a, const auto& b) {
                      return a.time_s < b.time_s;
                  });

        trajectory.summary.sample_count = trajectory.samples.size();

        if (!trajectory.samples.empty()) {
            trajectory.summary.time_min_s = trajectory.samples.front().time_s;
            trajectory.summary.time_max_s = trajectory.samples.back().time_s;
            trajectory.summary.entry_kinE_MeV = trajectory.samples.front().kinE_MeV;
            trajectory.summary.exit_kinE_MeV  = trajectory.samples.back().kinE_MeV;

            trajectory.summary.bounds.min = trajectory.samples.front().position_m;
            trajectory.summary.bounds.max = trajectory.samples.front().position_m;

            double total_edep = 0.0;
            double path_length = 0.0;

            for (std::size_t i = 0; i < trajectory.samples.size(); ++i) {
                const auto& sample = trajectory.samples[i];

                trajectory.summary.bounds.min.x = std::min(trajectory.summary.bounds.min.x, sample.position_m.x);
                trajectory.summary.bounds.min.y = std::min(trajectory.summary.bounds.min.y, sample.position_m.y);
                trajectory.summary.bounds.min.z = std::min(trajectory.summary.bounds.min.z, sample.position_m.z);

                trajectory.summary.bounds.max.x = std::max(trajectory.summary.bounds.max.x, sample.position_m.x);
                trajectory.summary.bounds.max.y = std::max(trajectory.summary.bounds.max.y, sample.position_m.y);
                trajectory.summary.bounds.max.z = std::max(trajectory.summary.bounds.max.z, sample.position_m.z);

                total_edep += sample.edep_MeV;

                if (i > 0) {
                    path_length += vec3dist(sample.position_m,
                                           trajectory.samples[i - 1].position_m);
                }
            }

            trajectory.summary.total_edep_MeV = total_edep;
            trajectory.summary.total_edep_eV  = total_edep * 1.0e6;
            trajectory.summary.range_m        = path_length;
        }
    }

    const EvolutionAxisResolver evolution_resolver{};
    const auto evolution_axis = evolution_resolver.resolve(dataset);
    (void)evolution_axis;

    const AxisFrameResolver axis_frame_resolver{};
    dataset.axis_frame = axis_frame_resolver.resolve(dataset);

    return dataset;
}

} // namespace beamlab::io
