#pragma once

#include "services/import/IImporter.h"

namespace beamlab::services::import {

/// Importer for COMSOL Multiphysics CSV export files.
///
/// Probe strategy:
///   Read first 10 lines; detect lines starting with "%%" or
///   containing "Model:" or "% Model" (COMSOL header markers).
///
///   - "%%" with "Model:" in first 5 lines → score 0.90
///   - "%%" present but no "Model:" → score 0.60
///   - No match → score 0.0
class ComsolCsvImporter final : public IImporter {
public:
    std::string name() const override { return "COMSOL CSV"; }
    std::vector<std::string> supportedExtensions() const override { return {".csv", ".txt"}; }

    ImporterCapabilityScore probe(const std::string& filePath) override;

    void import(const std::string& filePath,
                storage::IStorageBackend& storage,
                ImportProgressCallback onProgress) override;
};

} // namespace beamlab::services::import
