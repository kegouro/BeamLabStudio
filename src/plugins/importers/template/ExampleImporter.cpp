// ══════════════════════════════════════════════════════════════════════
//  ExampleImporter.cpp
//  Implementation of a minimal BeamLabStudio import plugin.
//
//  This file demonstrates the full lifecycle of importing data:
//    1. Probe the file (check magic bytes)
//    2. Open the file and parse rows
//    3. Write samples to the IStorageBackend
//    4. Report progress via callback
//    5. Export the factory function via extern "C"
// ══════════════════════════════════════════════════════════════════════

#include "ExampleImporter.h"

// Standard library headers available to any C++17 plugin.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace beamlab::plugins {

// ═════════════════════════════════════════════════════════════════════
//  Probe
// ═════════════════════════════════════════════════════════════════════

services::import::ImporterCapabilityScore ExampleImporter::probe(
    const std::string& filePath)
{
    services::import::ImporterCapabilityScore result;

    // ── Open the file in binary mode ──────────────────────────────
    // Read the first few bytes to check magic numbers or header text.
    // Reading more than 8 KB is discouraged — the registry reads at
    // most 8 KB from the beginning for all probes combined.
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return result;  // score remains 0.0
    }

    // ── Check magic bytes ─────────────────────────────────────────
    // Magic bytes are the most reliable detection mechanism.
    // Replace "EX01" with your format's signature.
    char magic[4] = {0};
    file.read(magic, 4);

    if (std::memcmp(magic, "EX01", 4) == 0) {
        result.value = 1.0;                                // ← exact match
        result.matchedHeader = "EX01 magic bytes";          // ← for debugging
        return result;
    }

    // ── Fallback: check extension ────────────────────────────────
    // If magic bytes don't match but the extension is right,
    // return a low score so other importers with stronger evidence
    // take priority.
    //
    // Extension-only matching:
    //   result.value = 0.3;   // weak evidence
    //   return result;

    return result;  // score remains 0.0 — not our format
}

// ═════════════════════════════════════════════════════════════════════
//  Import
// ═════════════════════════════════════════════════════════════════════

void ExampleImporter::import(
    const std::string& filePath,
    services::storage::IStorageBackend& storage,
    services::import::ImportProgressCallback onProgress)
{
    namespace fs = std::filesystem;

    // ── Get file size for progress reporting ──────────────────────
    auto fileSize = fs::file_size(filePath);

    // ── Open file ─────────────────────────────────────────────────
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }

    // ── Begin write batch ─────────────────────────────────────────
    // Groups multiple writeSample() calls into a single transaction
    // for performance (especially important for SQLite backends).
    storage.beginWriteBatch();

    // ── RAII guard ────────────────────────────────────────────────
    // Ensures endWriteBatch() is called even if an exception is
    // thrown during parsing.  Without this guard, the backend may
    // be left in an inconsistent state.
    struct BatchGuard {
        services::storage::IStorageBackend& b;
        bool committed = false;
        ~BatchGuard() {
            if (!committed) {
                try { b.endWriteBatch(); } catch (...) {}
            }
        }
    };
    BatchGuard guard{storage};

    // ── Parse rows ────────────────────────────────────────────────
    // This example parses a simple CSV-like format with 4 columns:
    //   x,y,z,energy
    //
    // Adapt this loop to your format.  Common patterns:
    //   - CSV/TSV:            std::getline + stringstream
    //   - Fixed-width binary: file.read(reinterpret_cast<char*>(&val), sizeof(val))
    //   - HDF5/NetCDF:        use the HDF5 C API
    //   - JSON Lines:         nlohmann::json::parse + getline loop

    std::string line;
    uint64_t sampleCount = 0;

    // Skip header line (if your format has one).
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // ── Parse comma-separated values ──────────────────────────
        std::vector<double> values;
        std::stringstream ss(line);
        std::string token;

        while (std::getline(ss, token, ',')) {
            try {
                values.push_back(std::stod(token));
            } catch (const std::exception&) {
                break;  // skip malformed row
            }
        }

        // Expect at least 3 columns (x, y, z).
        if (values.size() < 3) continue;

        // ── Build a TrajectorySample ──────────────────────────────
        beamlab::data::TrajectorySample sample;

        // Position in metres (convert from cm if needed).
        sample.position_m.x = values[0] / 100.0;
        sample.position_m.y = values[1] / 100.0;
        sample.position_m.z = values[2] / 100.0;

        // Energy deposition in MeV (0.0 if not available).
        sample.edep_MeV = values.size() > 3 ? values[3] : 0.0;

        // Kinetic energy in MeV (0.0 if not available).
        sample.kinE_MeV = values.size() > 4 ? values[4] : 0.0;

        // Trajectory ID (each unique ID starts a new trajectory).
        sample.trajectory_id = beamlab::data::TrajectoryId(
            static_cast<uint64_t>(sampleCount));

        // ── Write to backend ──────────────────────────────────────
        storage.writeSample(sample);
        ++sampleCount;

        // ── Report progress every 100 000 samples ─────────────────
        static constexpr uint64_t kNotifyInterval = 100000;
        if (sampleCount % kNotifyInterval == 0 && onProgress) {
            auto pos = static_cast<uint64_t>(file.tellg());
            onProgress(pos, static_cast<uint64_t>(fileSize));
        }
    }

    // ── Finalise ──────────────────────────────────────────────────
    guard.committed = true;
    storage.endWriteBatch();

    // Notify caller that we're done (100%).
    if (onProgress) onProgress(fileSize, fileSize);
}

} // namespace beamlab::plugins

// ═════════════════════════════════════════════════════════════════════
//  Factory function (C linkage)
// ═════════════════════════════════════════════════════════════════════
//
// This is the only symbol that BeamLabStudio's PluginHost looks for
// in a shared library.  It must be:
//   1. extern "C"                — prevent C++ name mangling
//   2. __attribute__((visibility("default")))  — export from .so
//   3. return a raw pointer       — PluginHost takes ownership
//
// The returned pointer is wrapped in unique_ptr<IPlugin> by the host.
// The host calls plugin->shutdown() and plugin->destroy() on unload.
//
// ── Naming convention ────────────────────────────────────────────────
// The function name "create_importer" is fixed.  BeamLabStudio looks
// for exactly this symbol with dlsym(RTLD_DEFAULT, "create_importer").

extern "C" __attribute__((visibility("default")))
beamlab::services::import::IImporter* create_importer()
{
    // Return a new instance.  The caller (PluginHost) takes ownership
    // via std::unique_ptr.  Never return nullptr — it is treated as
    // a load failure and the library is unloaded.
    return new beamlab::plugins::ExampleImporter();
}
