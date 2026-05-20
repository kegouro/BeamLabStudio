#pragma once

#include "io/importers/IImporter.h"

namespace beamlab::core {
class ISampleStorage;
class ProgressTracker;
} // namespace beamlab::core

namespace beamlab::io {

class Geant4CsvImporter final : public IImporter {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::vector<std::string> supportedExtensions() const override;
    [[nodiscard]] ImporterCapabilityScore probe(const ProbeResult& probe_result) const override;
    [[nodiscard]] InspectionReport inspect(const std::string& file_path) const override;
    [[nodiscard]] ImportSchema buildSchema(const std::string& file_path,
                                            const InspectionReport& inspection) const override;
    [[nodiscard]] ImportResult import(const std::string& file_path,
                                       const ImportSchema& schema,
                                       const ImportContext& context) const override;

    // Streaming import: writes directly to ISampleStorage, O(1) RAM.
    // Returns number of samples imported.
    [[nodiscard]] uint64_t importStreaming(
        const std::string& file_path,
        beamlab::core::ISampleStorage& storage,
        beamlab::core::ProgressTracker* progress = nullptr) const;
};

} // namespace beamlab::io
