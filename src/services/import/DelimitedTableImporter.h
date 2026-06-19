#pragma once

#include "services/import/IImporter.h"

namespace beamlab::services::import {

/// Generic fallback importer for delimited text tables.
///
/// Probe strategy:
///   If the file is plain text with at least 3 numeric columns and
///   a header row (first line contains non-numeric tokens), score 0.30.
///   This is intentionally low so that specialised importers (Geant4,
///   COMSOL) take priority.
///
/// This importer is always registered last so it acts as a catch-all.
class DelimitedTableImporter final : public IImporter {
public:
    std::string name() const override { return "Delimited Table"; }
    std::vector<std::string> supportedExtensions() const override
    {
        return {".csv", ".tsv", ".txt", ".dat"};
    }

    ImporterCapabilityScore probe(const std::string& filePath) override;

    void import(const std::string& filePath,
                storage::IStorageBackend& storage,
                ImportProgressCallback onProgress) override;
};

} // namespace beamlab::services::import
