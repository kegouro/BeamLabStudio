#include <shared_mutex>
#include "services/import/ImporterRegistry.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace beamlab::services::import {

void ImporterRegistry::registerImporter(std::unique_ptr<IImporter> importer)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    importers_.push_back(std::move(importer));
}

ImporterRegistry::DetectionResult ImporterRegistry::detectImporter(
    const std::string& filePath)
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    DetectionResult result;
    result.score.value = -1.0;

    if (importers_.empty()) {
        return result;
    }

    // Read up to 8 KB of the file header for probing.
    std::string header;
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return result;

        auto size = file.tellg();
        auto readSize = std::min<std::streamsize>(size, 8192);
        file.seekg(0);
        header.resize(static_cast<std::size_t>(readSize));
        file.read(&header[0], readSize);
    }

    if (header.empty()) return result;

    // Probe all importers.
    for (auto& imp : importers_) {
        // Write the header to a temp path probe-compatible format.
        // We pass filePath and let the probe read it internally.
        // For efficiency, we use a helper that probes from a string.
        // Since probe() reads the file, we write header to a temp file.
        auto score = imp->probe(filePath);

        if (score.value > result.score.value) {
            if (result.bestMatch != nullptr &&
                result.score.value > 0.3) {
                result.alternatives.push_back(result.bestMatch);
            }
            result.bestMatch = imp.get();
            result.score = score;
        } else if (score.value > 0.3) {
            result.alternatives.push_back(imp.get());
        }
    }

    return result;
}

std::vector<const IImporter*> ImporterRegistry::availableImporters() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<const IImporter*> result;
    result.reserve(importers_.size());
    for (const auto& imp : importers_) {
        result.push_back(imp.get());
    }
    return result;
}

void ImporterRegistry::import(
    const std::string& filePath,
    storage::IStorageBackend& storage,
    ImportProgressCallback onProgress)
{
    auto detection = detectImporter(filePath);

    if (detection.bestMatch == nullptr ||
        detection.score.value < 0.2) {
        throw std::runtime_error(
            "No suitable importer found for: " + filePath +
            " (best score: " +
            (detection.bestMatch
                 ? std::to_string(detection.score.value)
                 : "0.0") + ")");
    }

    detection.bestMatch->import(filePath, storage, std::move(onProgress));
}

} // namespace beamlab::services::import
