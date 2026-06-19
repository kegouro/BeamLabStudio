#pragma once

#include "services/import/IImporter.h"

#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace beamlab::services::import {

/// Registry of IImporter plugins with auto-detection.
///
/// ## Detection algorithm
///
/// 1. If the file has a recognised extension, importers supporting that
///    extension are tried first via probe().
/// 2. All registered importers are probed; the one with the highest
///    score wins.
/// 3. If the best score is < 0.2, import() throws.
///
/// Thread-safety: registerImporter() (write) and detectImporter() (read)
/// use std::shared_mutex — safe for concurrent calls.
class ImporterRegistry {
public:
    /// Result of format auto-detection.
    struct DetectionResult {
        IImporter* bestMatch{nullptr};
        ImporterCapabilityScore score;
        std::vector<IImporter*> alternatives;
    };

    /// Register an importer plugin.  Takes ownership.
    void registerImporter(std::unique_ptr<IImporter> importer);

    /// Auto-detect the best importer for the given file.
    /// Reads at most 8 KB from the file.
    DetectionResult detectImporter(const std::string& filePath);

    /// List all registered importers.
    [[nodiscard]] std::vector<const IImporter*> availableImporters() const;

    /// Detect the best importer, then run import().
    /// Throws std::runtime_error if no suitable importer is found (score < 0.2).
    void import(const std::string& filePath,
                storage::IStorageBackend& storage,
                ImportProgressCallback onProgress = nullptr);

private:
    std::vector<std::unique_ptr<IImporter>> importers_;
    mutable std::shared_mutex mutex_;
};

} // namespace beamlab::services::import
