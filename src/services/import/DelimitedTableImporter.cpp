#include "services/import/DelimitedTableImporter.h"

#include "data/model/TrajectorySample.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace beamlab::services::import {

// Check if a string can be parsed as a double.
static bool isNumeric(const std::string& s)
{
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

ImporterCapabilityScore DelimitedTableImporter::probe(
    const std::string& filePath)
{
    ImporterCapabilityScore result;

    std::ifstream file(filePath);
    if (!file.is_open()) return result;

    std::string line;

    // Read first line (potential header).
    if (!std::getline(file, line) || line.empty()) return result;

    auto firstCols = [&](const std::string& l) {
        std::stringstream ss(l);
        std::vector<std::string> tokens;
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            if (!tok.empty()) tokens.push_back(tok);
            if (tokens.size() >= 5) break;
        }
        return tokens;
    };

    auto cols = firstCols(line);

    // Need at least 3 columns to be interesting.
    if (cols.size() < 3) return result;

    // If the first line is a header (mostly non-numeric), check the
    // second line for numeric data.
    bool firstLineNumeric =
        std::all_of(cols.begin(), cols.end(),
                    [](const std::string& c) { return isNumeric(c); });

    if (!firstLineNumeric) {
        // First line is a header. Check second line for data.
        if (!std::getline(file, line) || line.empty()) return result;
        cols = firstCols(line);
    }

    // Check if we have at least 3 numeric columns.
    int numericCount = 0;
    for (const auto& c : cols) {
        if (isNumeric(c)) ++numericCount;
    }

    if (numericCount >= 3) {
        result.value = 0.30;
        result.matchedHeader = "Delimited table with " +
                               std::to_string(cols.size()) + " columns";
    }

    return result;
}

void DelimitedTableImporter::import(
    const std::string& filePath,
    storage::IStorageBackend& storage,
    ImportProgressCallback onProgress)
{
    auto fileSize = fs::file_size(filePath);

    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }

    std::string line;
    std::getline(file, line);  // Skip header.

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

        if (values.size() < 3) continue;

        beamlab::data::TrajectorySample sample;
        sample.position_m.x = values[0] / 100.0;
        sample.position_m.y = values.size() > 1 ? values[1] / 100.0 : 0.0;
        sample.position_m.z = values.size() > 2 ? values[2] / 100.0 : 0.0;
        sample.edep_MeV = values.size() > 3 ? values[3] : 0.0;
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
