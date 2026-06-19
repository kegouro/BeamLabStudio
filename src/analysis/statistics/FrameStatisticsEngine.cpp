//!@math-begin module="FrameStatistics" title="Estadísticas por Frame — Radio RMS Transversal"
//!@math Para cada bin axial se acumulan los puntos de todas las trayectorias
//!@math y se calculan las siguientes estadísticas transversales:
//!@section Proyección al marco de referencia
//!@math Sea p⃗ la posición de un muestra y ê_s, ê_u, ê_v los versores del marco:
//!@formula s = (p⃗ − origen) · ê_s   [coordenada longitudinal]
//!@formula u = (p⃗ − origen) · ê_u   [transversal 1]
//!@formula v = (p⃗ − origen) · ê_v   [transversal 2]
//!@section Momentos estadísticos (acumulador de un solo paso)
//!@math Con N puntos en el bin:
//!@formula ū = (1/N) Σᵢ uᵢ        [centroide u]
//!@formula v̄ = (1/N) Σᵢ vᵢ        [centroide v]
//!@formula σ_u² = (1/N) Σᵢ uᵢ² − ū²   [varianza u]
//!@formula σ_v² = (1/N) Σᵢ vᵢ² − v̄²   [varianza v]
//!@section Radio RMS transversal (proxy de foco)
//!@formula r_rms = √(σ_u² + σ_v²)
//!@note El foco geométrico se define como el bin con r_rms mínimo.
//!@note Se usa el estimador sesgado (1/N) por consistencia con el proxy de foco;
//!@note para N >> 1 la diferencia con el estimador insesgado 1/(N-1) es despreciable.
//!@section Modos de binning
//!@math Uniforme: bins de igual anchura Δs = (s_max − s_min) / N_bins
//!@math Equal-count: bins con igual número de puntos (percentiles).
//!@math Sincronizado: usa el índice de muestra como "tiempo" (requiere ≥80 % de
//!@math trayectorias con el mismo número de pasos).
//!@math-end

#include "analysis/statistics/FrameStatisticsEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace beamlab::analysis {
namespace {

struct ProjectedSample {
    double s{0.0};
    double u{0.0};
    double v{0.0};
};

struct FrameAccumulator {
    std::size_t count{0};

    double sum_s{0.0};
    double sum_u{0.0};
    double sum_v{0.0};

    double sum_u2{0.0};
    double sum_v2{0.0};

    double min_s{std::numeric_limits<double>::infinity()};
    double max_s{-std::numeric_limits<double>::infinity()};
};

std::size_t chooseBinCount(const std::size_t sample_count)
{
    if (sample_count < 1000) {
        return 101;
    }

    if (sample_count < 100000) {
        return 201;
    }

    return 501;
}

void addSample(FrameAccumulator& acc,
               const double s,
               const double u,
               const double v)
{
    ++acc.count;
    acc.sum_s += s;
    acc.sum_u += u;
    acc.sum_v += v;
    acc.sum_u2 += u * u;
    acc.sum_v2 += v * v;
    acc.min_s = std::min(acc.min_s, s);
    acc.max_s = std::max(acc.max_s, s);
}

FrameStatistics finalizeAccumulator(const FrameAccumulator& acc)
{
    FrameStatistics stats{};
    if (acc.count == 0) {
        stats.valid = false;
        return stats;
    }

    const double inv_n = 1.0 / static_cast<double>(acc.count);
    const double mean_s = acc.sum_s * inv_n;
    const double mean_u = acc.sum_u * inv_n;
    const double mean_v = acc.sum_v * inv_n;

    const double var_u = std::max(0.0, acc.sum_u2 * inv_n - mean_u * mean_u);
    const double var_v = std::max(0.0, acc.sum_v2 * inv_n - mean_v * mean_v);

    stats.reference_value = mean_s;
    stats.reference_min_value = acc.min_s;
    stats.reference_max_value = acc.max_s;
    stats.point_count = acc.count;
    stats.centroid_u = mean_u;
    stats.centroid_v = mean_v;
    stats.sigma_u = std::sqrt(var_u);
    stats.sigma_v = std::sqrt(var_v);
    stats.r_rms = std::sqrt(var_u + var_v);

    if (!(stats.reference_max_value > stats.reference_min_value)) {
        const double eps = std::max(1.0e-12, std::abs(stats.reference_value) * 1.0e-9);
        stats.reference_min_value = stats.reference_value - eps;
        stats.reference_max_value = stats.reference_value + eps;
    } else {
        const double width = stats.reference_max_value - stats.reference_min_value;
        const double margin = std::max(1.0e-12, width * 1.0e-6);
        stats.reference_min_value -= margin;
        stats.reference_max_value += margin;
    }

    return stats;
}

std::vector<ProjectedSample> projectSamples(const beamlab::data::TrajectoryDataset& dataset,
                                            double& s_min,
                                            double& s_max)
{
    std::vector<ProjectedSample> projected{};

    std::size_t total_samples = 0;
    for (const auto& trajectory : dataset.trajectories) {
        total_samples += trajectory.samples.size();
    }

    projected.reserve(total_samples);

    s_min = std::numeric_limits<double>::infinity();
    s_max = -std::numeric_limits<double>::infinity();

    for (const auto& trajectory : dataset.trajectories) {
        for (const auto& sample : trajectory.samples) {
            const auto relative = subtract(sample.position_m, dataset.axis_frame.origin);

            ProjectedSample p{};
            p.s = dot(relative, dataset.axis_frame.longitudinal);
            p.u = dot(relative, dataset.axis_frame.transverse_u);
            p.v = dot(relative, dataset.axis_frame.transverse_v);

            // Drop non-finite projections.  A NaN here would land in a
            // negative bin (floor((NaN - s_min) / width) is implementation
            // defined and typically produces a huge unsigned value) and
            // poison every downstream statistic.  Filter at the source.
            if (!std::isfinite(p.s) || !std::isfinite(p.u) || !std::isfinite(p.v)) {
                continue;
            }

            s_min = std::min(s_min, p.s);
            s_max = std::max(s_max, p.s);

            projected.push_back(p);
        }
    }

    return projected;
}

bool looksSynchronized(const beamlab::data::TrajectoryDataset& dataset,
                       std::size_t& reference_sample_count)
{
    std::size_t nonempty = 0;
    std::size_t same_count = 0;
    reference_sample_count = 0;

    for (const auto& trajectory : dataset.trajectories) {
        if (trajectory.samples.empty()) {
            continue;
        }

        if (reference_sample_count == 0) {
            reference_sample_count = trajectory.samples.size();
        }

        ++nonempty;

        if (trajectory.samples.size() == reference_sample_count) {
            ++same_count;
        }
    }

    if (nonempty < 3 || reference_sample_count < 2) {
        return false;
    }

    const double fraction =
        static_cast<double>(same_count) / static_cast<double>(nonempty);

    return fraction >= 0.80;
}

std::vector<FrameStatistics> computeSynchronizedFrames(
    const beamlab::data::TrajectoryDataset& dataset,
    const std::size_t reference_sample_count)
{
    std::vector<FrameAccumulator> accumulators(reference_sample_count);

    for (const auto& trajectory : dataset.trajectories) {
        if (trajectory.samples.size() != reference_sample_count) {
            continue;
        }

        for (std::size_t i = 0; i < trajectory.samples.size(); ++i) {
            const auto& sample = trajectory.samples[i];
            const auto relative = subtract(sample.position_m, dataset.axis_frame.origin);

            const double s = dot(relative, dataset.axis_frame.longitudinal);
            const double u = dot(relative, dataset.axis_frame.transverse_u);
            const double v = dot(relative, dataset.axis_frame.transverse_v);

            addSample(accumulators[i], s, u, v);
        }
    }

    std::vector<FrameStatistics> results{};
    results.reserve(accumulators.size());

    for (const auto& acc : accumulators) {
        if (acc.count < 3) {
            continue;
        }

        results.push_back(finalizeAccumulator(acc));
    }

    return results;
}

std::vector<FrameStatistics> computeUniformAxialBins(
    const std::vector<ProjectedSample>& projected,
    const double s_min,
    const double s_max,
    const std::size_t requested_bin_count)
{

    // The (s_max > s_min) test below is also false when either bound is NaN,
    // but the explicit isfinite guard documents the intent and protects
    // against future refactors that loosen the comparison.
    if (projected.empty() ||
        !std::isfinite(s_min) || !std::isfinite(s_max) ||
        !(s_max > s_min)) {
        return {};
    }

    const std::size_t bin_count =
        requested_bin_count > 1 ? requested_bin_count : chooseBinCount(projected.size());

    const double width = (s_max - s_min) / static_cast<double>(bin_count);

    std::vector<FrameAccumulator> accumulators(bin_count);

    for (const auto& p : projected) {
        auto bin = static_cast<std::size_t>(std::floor((p.s - s_min) / width));

        if (bin >= bin_count) {
            bin = bin_count - 1;
        }

        auto& acc = accumulators[bin];

        addSample(acc, p.s, p.u, p.v);

        acc.min_s = s_min + static_cast<double>(bin) * width;
        acc.max_s = acc.min_s + width;
    }

    std::vector<FrameStatistics> results{};
    results.reserve(bin_count);

    for (std::size_t bin = 0; bin < accumulators.size(); ++bin) {
        const auto& acc = accumulators[bin];

        if (acc.count < 3) {
            continue;
        }

        auto stats = finalizeAccumulator(acc);
        stats.reference_min_value = s_min + static_cast<double>(bin) * width;
        stats.reference_max_value = stats.reference_min_value + width;
        stats.reference_value = 0.5 * (stats.reference_min_value + stats.reference_max_value);
        results.push_back(stats);
    }

    return results;
}

std::vector<FrameStatistics> computeEqualCountAxialBins(
    std::vector<ProjectedSample>& projected,
    const double s_min,
    const double s_max,
    const std::size_t requested_bin_count)
{

    if (projected.empty() ||
        !std::isfinite(s_min) || !std::isfinite(s_max) ||
        !(s_max > s_min)) {
        return {};
    }

    const std::size_t bin_count =
        requested_bin_count > 1 ? requested_bin_count : chooseBinCount(projected.size());

    std::sort(projected.begin(), projected.end(),
              [](const auto& a, const auto& b) {
                  return a.s < b.s;
              });

    std::vector<FrameStatistics> results{};
    results.reserve(bin_count);

    const std::size_t n = projected.size();

    for (std::size_t bin = 0; bin < bin_count; ++bin) {
        const std::size_t begin = bin * n / bin_count;
        const std::size_t end = (bin + 1) * n / bin_count;

        if (end <= begin || end - begin < 3) {
            continue;
        }

        FrameAccumulator acc{};

        for (std::size_t i = begin; i < end; ++i) {
            const auto& p = projected[i];
            addSample(acc, p.s, p.u, p.v);
        }

        results.push_back(finalizeAccumulator(acc));
    }

    return results;
}

} // namespace

std::vector<FrameStatistics> FrameStatisticsEngine::compute(
    const beamlab::data::TrajectoryDataset& dataset) const
{
    FrameStatisticsParameters parameters{};
    return compute(dataset, parameters);
}

std::vector<FrameStatistics> FrameStatisticsEngine::compute(
    const beamlab::data::TrajectoryDataset& dataset,
    const FrameStatisticsParameters& parameters) const
{
    std::size_t reference_sample_count = 0;
    const bool synchronized = looksSynchronized(dataset, reference_sample_count);

    if (parameters.reference_mode == ReferenceMode::Synchronized) {
        return computeSynchronizedFrames(dataset, reference_sample_count);
    }

    if (parameters.reference_mode == ReferenceMode::Auto && synchronized) {
        return computeSynchronizedFrames(dataset, reference_sample_count);
    }

    double s_min = 0.0;
    double s_max = 0.0;
    auto projected = projectSamples(dataset, s_min, s_max);

    if (parameters.axial_binning_mode == AxialBinningMode::EqualCount) {
        return computeEqualCountAxialBins(projected, s_min, s_max, parameters.axial_bin_count);
    }

    return computeUniformAxialBins(projected, s_min, s_max, parameters.axial_bin_count);
}

} // namespace beamlab::analysis
