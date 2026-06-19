#include "services/import/ComsolCsvImporter.h"

#include "data/model/TrajectorySample.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace beamlab::services::import {

ImporterCapabilityScore ComsolCsvImporter::probe(const std::string& filePath)
{
    ImporterCapabilityScore result;

    std::ifstream file(filePath);
    if (!file.is_open()) return result;

    std::string line;
    bool hasPercentPercent = false;
    bool hasModel = false;

    for (int i = 0; i < 10 && std::getline(file, line); ++i) {
        // Trim leading whitespace.
        auto start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos) continue;
        if (line[start] == '%') {
            hasPercentPercent = true;
            if (line.find("Model:") != std::string::npos ||
                line.find("model:") != std::string::npos ||
                line.find("MODEL:") != std::string::npos) {
                hasModel = true;
            }
        }
    }

    if (hasPercentPercent) {
        if (hasModel) {
            result.value = 0.90;
            result.matchedHeader = "COMSOL Model with %% comments";
        } else {
            result.value = 0.60;
            result.matchedHeader = "COMSOL-style %% commentary";
        }
    }

    return result;
}

void ComsolCsvImporter::import(
    const std::string& filePath,
    storage::IStorageBackend& storage,
    ImportProgressCallback onProgress)
{
    auto fileSize = fs::file_size(filePath);

    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }

    // Skip COMSOL header (%% comment lines).
    std::string line;
    while (std::getline(file, line)) {
        auto start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos) continue;
        if (line[start] != '%') break;  // First non-comment line is the header.
    }

    // The line we broke on is actually the CSV header.
    // Now parse data rows.

    storage.beginWriteBatch();
    struct BatchGuard {
        storage::IStorageBackend& b;
        bool committed = false;
        ~BatchGuard() { if (!committed) { try { b.endWriteBatch(); } catch (...) {} } }
    };
    BatchGuard guard{storage};

    uint64_t sampleCount = 0;
    static constexpr uint64_t kNotifyInterval = 100000;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<double> values;

        while (std::getline(ss, token, ',')) {
            try { values.push_back(std::stod(token)); }
            catch (...) { break; }
        }

        if (values.size() < 4) continue;

        // COMSOL columns are typically: x, y, z, field_value, ...
        beamlab::data::TrajectorySample sample;
        sample.position_m.x = values[0] / 100.0;
        sample.position_m.y = values[1] / 100.0;
        sample.position_m.z = values[2] / 100.0;
        sample.edep_MeV = values[3];
        sample.kinE_MeV = values.size() > 4 ? values[4] : 0.0;
        sample.trajectory_id = beamlab::data::TrajectoryId(
            static_cast<uint64_t>(sampleCount));

        storage.writeSample(sample);
        ++sampleCount;

        if (sampleCount % kNotifyInterval == 0 && onProgress) {
            auto pos = file.tellg();
            onProgress(static_cast<uint64_t>(pos),
                       static_cast<uint64_t>(fileSize));
        }
    }

    guard.committed = true;
    storage.endWriteBatch();

    if (onProgress) onProgress(fileSize, fileSize);
}

} // namespace beamlab::services::import
