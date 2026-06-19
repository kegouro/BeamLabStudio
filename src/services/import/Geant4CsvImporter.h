#pragma once

#include "services/import/IImporter.h"
#include "services/import/HeaderAnalyzer.h"

namespace beamlab::services::import {

/// EXTREMELY permissive Geant4 CSV/TSV importer.
///
/// Self-detects delimiter, BOM, CR/LF, and header format.
/// Works with 11- and 15-column Geant4 variants, COMSOL-like CSVs,
/// and generic delimited tables.  If a column is missing, fills 0.
class Geant4CsvImporter final : public IImporter {
public:
    std::string name() const override { return "Geant4 CSV"; }
    std::vector<std::string> supportedExtensions() const override {
        return {".csv", ".tsv", ".txt", ".dat"};
    }

    ImporterCapabilityScore probe(const std::string& filePath) override;

    void import(const std::string& filePath,
                storage::IStorageBackend& storage,
                ImportProgressCallback onProgress) override;

private:
    /// Detect the delimiter from a header line.
    static char detectDelimiter(const std::string& line);

    /// Split a line by delimiter, respecting quoted fields and trimming.
    static std::vector<std::string> splitLine(const std::string& line,
                                              char delim);

    /// Strip BOM (0xEF 0xBB 0xBF) and trailing \r from a line.
    static void sanitiseLine(std::string& line);

    /// Extract a column value by index (with bounds check).
    /// Returns the token or "" if the index is out of range.
    static const std::string& tokenAt(const std::vector<std::string>& tokens,
                                       int col);
};

} // namespace beamlab::services::import
