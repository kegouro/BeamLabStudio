#include "services/analysis/engines/FrameStatisticsPlugin.h"
#include "services/storage/IStorageBackend.h"

#include "core/math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace beamlab::services::analysis {

using beamlab::core::Vec3;
using beamlab::core::dot;
using beamlab::core::subtract;

// ── Internal helpers ──────────────────────────────────────────────────

namespace {

struct FrameAccumulator {
    std::uint64_t count{0};
    double sum_s{0.0};
    double sum_u{0.0};
    double sum_v{0.0};
    double sum_u2{0.0};
    double sum_v2{0.0};
    double min_s{std::numeric_limits<double>::infinity()};
    double max_s{-std::numeric_limits<double>::infinity()};
};

// Deserialize a Vec3 from JSON: {"x": N, "y": N, "z": N}
Vec3 vec3FromJson(const nlohmann::json& j)
{
    return {j.value("x", 0.0), j.value("y", 0.0), j.value("z", 0.0)};
}

double projectS(const Vec3& position, const Vec3& origin, const Vec3& longitudinal)
{
    return dot(subtract(position, origin), longitudinal);
}

double projectU(const Vec3& position, const Vec3& origin, const Vec3& transverseU)
{
    return dot(subtract(position, origin), transverseU);
}

double projectV(const Vec3& position, const Vec3& origin, const Vec3& transverseV)
{
    return dot(subtract(position, origin), transverseV);
}

void addSample(FrameAccumulator& acc, double s, double u, double v)
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

struct FrameStatistics {
    bool valid{true};
    double reference_value{0.0};
    double reference_min_value{0.0};
    double reference_max_value{0.0};
    std::size_t point_count{0};
    double centroid_u{0.0};
    double centroid_v{0.0};
    double sigma_u{0.0};
    double sigma_v{0.0};
    double r_rms{0.0};
};

FrameStatistics finalizeAccumulator(const FrameAccumulator& acc)
{
    FrameStatistics stats{};
    if (acc.count == 0) {
        stats.valid = false;
        return stats;
    }

    auto inv_n = 1.0 / static_cast<double>(acc.count);
    auto mean_s = acc.sum_s * inv_n;
    auto mean_u = acc.sum_u * inv_n;
    auto mean_v = acc.sum_v * inv_n;

    auto var_u = std::max(0.0, acc.sum_u2 * inv_n - mean_u * mean_u);
    auto var_v = std::max(0.0, acc.sum_v2 * inv_n - mean_v * mean_v);

    stats.reference_value = mean_s;
    stats.reference_min_value = acc.min_s;
    stats.reference_max_value = acc.max_s;
    stats.point_count = static_cast<std::size_t>(acc.count);
    stats.centroid_u = mean_u;
    stats.centroid_v = mean_v;
    stats.sigma_u = std::sqrt(var_u);
    stats.sigma_v = std::sqrt(var_v);
    stats.r_rms = std::sqrt(var_u + var_v);

    if (!(stats.reference_max_value > stats.reference_min_value)) {
        auto eps = std::max(1.0e-12, std::abs(stats.reference_value) * 1.0e-9);
        stats.reference_min_value = stats.reference_value - eps;
        stats.reference_max_value = stats.reference_value + eps;
    } else {
        auto width = stats.reference_max_value - stats.reference_min_value;
        auto margin = std::max(1.0e-12, width * 1.0e-6);
        stats.reference_min_value -= margin;
        stats.reference_max_value += margin;
    }

    return stats;
}

} // anonymous namespace

// ── Constructor ───────────────────────────────────────────────────────

FrameStatisticsPlugin::FrameStatisticsPlugin() = default;

// ── Validation ────────────────────────────────────────────────────────

std::vector<std::string> FrameStatisticsPlugin::validateConfig(
    const nlohmann::json& config) const
{
    std::vector<std::string> issues;

    if (!config.contains("axis_frame") || !config["axis_frame"].is_object()) {
        issues.push_back("Missing required key: 'axis_frame' (object)");
    } else {
        const auto& af = config["axis_frame"];
        for (const auto& k : {"origin", "longitudinal", "transverse_u", "transverse_v"}) {
            if (!af.contains(k) || !af[k].is_object()) {
                issues.push_back(std::string("axis_frame.") + k + " must be an object with x/y/z");
            }
        }
    }

    if (config.contains("bin_count")) {
        auto bc = config["bin_count"].get<int64_t>();
        if (bc < 2) {
            issues.push_back("bin_count must be >= 2");
        }
        if (static_cast<std::size_t>(bc) > maxBins_) {
            issues.push_back("bin_count exceeds maximum (" + std::to_string(maxBins_) + ")");
        }
    }

    return issues;
}

// ── Execute ───────────────────────────────────────────────────────────

EngineResult FrameStatisticsPlugin::execute(
    const IStorageBackend& storage,
    const nlohmann::json& config,
    ProgressCallback onProgress)
{
    cancelled_.store(false);

    // ── Parse config ────────────────────────────────────────────────

    auto origin      = vec3FromJson(config["axis_frame"]["origin"]);
    auto longitudinal = vec3FromJson(config["axis_frame"]["longitudinal"]);
    auto transU      = vec3FromJson(config["axis_frame"]["transverse_u"]);
    auto transV      = vec3FromJson(config["axis_frame"]["transverse_v"]);

    auto binCount = config.value("bin_count", int64_t{501});
    auto batchSize = config.value("batch_size", int64_t{100000});

    auto totalSamples = storage.totalSamples();
    if (totalSamples == 0) {
        return EngineResult::ok({{"frames", 0}, {"total_samples", 0}});
    }

    // ── Pass 1: scan to find global s_min/s_max ─────────────────────

    if (onProgress) onProgress(0.0f, "Scanning axial range");

    std::uint64_t validSamples = 0;
    double sMin = std::numeric_limits<double>::infinity();
    double sMax = -std::numeric_limits<double>::infinity();

    for (std::uint64_t offset = 0; offset < totalSamples; offset += batchSize) {
        if (cancelled_.load()) {
            return EngineResult::fail("Cancelled by user",
                {{"samples_scanned", offset}});
        }

        auto batch = storage.readBatch(offset, static_cast<std::uint64_t>(batchSize));
        if (batch.data == nullptr || batch.count == 0) break;

        for (std::uint64_t i = 0; i < batch.count; ++i) {
            const auto& s = batch.data[i];
            auto sp = projectS(s.position_m, origin, longitudinal);
            auto u  = projectU(s.position_m, origin, transU);
            auto v  = projectV(s.position_m, origin, transV);

            if (!std::isfinite(sp) || !std::isfinite(u) || !std::isfinite(v)) {
                continue;
            }

            sMin = std::min(sMin, sp);
            sMax = std::max(sMax, sp);
            ++validSamples;
        }

        if (onProgress) {
            auto pct = static_cast<float>(offset) / static_cast<float>(totalSamples) * 0.5f;
            onProgress(pct, "Scanning axial range");
        }
    }

    if (validSamples == 0 || !(sMax > sMin)) {
        return EngineResult::ok({{"frames", 0},
                                 {"total_samples", validSamples},
                                 {"reason", "no valid samples or zero axial range"}});
    }

    // ── Pass 2: bin into accumulators ──────────────────────────────

    if (onProgress) onProgress(0.5f, "Binning samples");

    auto nBins = static_cast<std::size_t>(binCount);
    std::vector<FrameAccumulator> accumulators(nBins);
    double binWidth = (sMax - sMin) / static_cast<double>(nBins);

    for (std::uint64_t offset = 0; offset < totalSamples; offset += batchSize) {
        if (cancelled_.load()) {
            return EngineResult::fail("Cancelled by user",
                {{"bins_finalized", 0}, {"samples_scanned", offset}});
        }

        auto batch = storage.readBatch(offset, static_cast<std::uint64_t>(batchSize));
        if (batch.data == nullptr || batch.count == 0) break;

        for (std::uint64_t i = 0; i < batch.count; ++i) {
            const auto& smp = batch.data[i];
            auto sp = projectS(smp.position_m, origin, longitudinal);
            auto u  = projectU(smp.position_m, origin, transU);
            auto v  = projectV(smp.position_m, origin, transV);

            if (!std::isfinite(sp) || !std::isfinite(u) || !std::isfinite(v)) {
                continue;
            }

            auto binIdx = static_cast<std::size_t>(
                std::floor((sp - sMin) / binWidth));
            if (binIdx >= nBins) binIdx = nBins - 1;

            auto& acc = accumulators[binIdx];
            addSample(acc, sp, u, v);
            acc.min_s = sMin + static_cast<double>(binIdx) * binWidth;
            acc.max_s = acc.min_s + binWidth;
        }

        if (onProgress) {
            auto pct = 0.5f + static_cast<float>(offset) /
                        static_cast<float>(totalSamples) * 0.5f;
            onProgress(pct, "Binning samples");
        }
    }

    // ── Finalize accumulators ──────────────────────────────────────

    if (onProgress) onProgress(1.0f, "Finalizing");

    std::vector<nlohmann::json> frameJsonList;
    frameJsonList.reserve(nBins);

    for (std::size_t bin = 0; bin < nBins; ++bin) {
        const auto& acc = accumulators[bin];
        if (acc.count < 3) continue;

        auto stats = finalizeAccumulator(acc);
        stats.reference_min_value = sMin + static_cast<double>(bin) * binWidth;
        stats.reference_max_value = stats.reference_min_value + binWidth;
        stats.reference_value =
            0.5 * (stats.reference_min_value + stats.reference_max_value);

        frameJsonList.push_back({
            {"reference_value",     stats.reference_value},
            {"reference_min_value",  stats.reference_min_value},
            {"reference_max_value",  stats.reference_max_value},
            {"point_count",          stats.point_count},
            {"centroid_u",           stats.centroid_u},
            {"centroid_v",           stats.centroid_v},
            {"sigma_u",              stats.sigma_u},
            {"sigma_v",              stats.sigma_v},
            {"r_rms",                stats.r_rms},
            {"valid",                stats.valid},
        });
    }

    return EngineResult::ok({
        {"engine",       name()},
        {"version",      version()},
        {"frames",       frameJsonList.size()},
        {"total_samples", validSamples},
        {"bin_count",    nBins},
        {"s_min",        sMin},
        {"s_max",        sMax},
        {"bin_width",    binWidth},
        {"frames_data",  frameJsonList},
    });
}

} // namespace beamlab::services::analysis
