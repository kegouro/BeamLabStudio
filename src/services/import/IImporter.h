#pragma once

#include "services/storage/IStorageBackend.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace beamlab::services::import {

/// Progress callback for import operations.
/// \param bytesRead    Bytes processed so far.
/// \param totalBytes   Total file size (0 if unknown).
using ImportProgressCallback = std::function<void(uint64_t bytesRead, uint64_t totalBytes)>;

/// Result of probing a file for format compatibility.
struct ImporterCapabilityScore {
    double value{0.0};        ///< 0.0 = incompatible, 1.0 = exact header match.
    std::string matchedHeader; ///< First matching header line, empty if no match.
};

/// Abstract interface for a data-format plugin.
///
/// Each concrete importer:
///  1. Declares which file extensions it can handle (supportedExtensions).
///  2. Probes a file by reading its first 8 KB to compute a capability score.
///  3. Imports the data into an IStorageBackend.
class IImporter {
public:
    virtual ~IImporter() = default;

    /// Human-readable name, e.g. "Geant4 CSV".
    [[nodiscard]] virtual std::string name() const = 0;

    /// File extensions this importer handles (e.g. ".csv", ".root").
    [[nodiscard]] virtual std::vector<std::string> supportedExtensions() const = 0;

    /// Probe the file and return a capability score.
    /// Reads at most 8 KB from the beginning of the file.
    /// \param filePath  Absolute or relative path to the file.
    [[nodiscard]] virtual ImporterCapabilityScore probe(const std::string& filePath) = 0;

    /// Import all samples from the file into the storage backend.
    /// \param filePath    Path to the input file.
    /// \param storage     Backend that receives the samples.
    /// \param onProgress  Optional progress callback (bytesRead, totalBytes).
    virtual void import(const std::string& filePath,
                        storage::IStorageBackend& storage,
                        ImportProgressCallback onProgress = nullptr) = 0;
};

} // namespace beamlab::services::import
