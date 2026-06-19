#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include <unordered_map>

namespace beamlab::services::import {

/// Fuzzy header matching for Geant4 CSV columns.
///
/// Detects column positions by matching header names against known
/// canonical names (case-insensitive, underscore/hyphen tolerant).
class HeaderAnalyzer {
public:
    /// Canonical column name → internal enum.
    enum class Column : int {
        Unknown,
        X_cm, Y_cm, Z_cm,
        Edep_MeV, KinE_MeV,
        Momx_MeV, Momy_MeV, Momz_MeV,
        Time_ns,
        TrackID, ParentID, EventID,
        Pdg, ParticleName, SourceFile
    };

    /// Result of header analysis.
    struct Mapping {
        /// Column name → position in the row.
        std::unordered_map<Column, int> indices;
        /// Set of columns actually found in this file.
        std::vector<Column> foundColumns;
        /// Count of recognised columns.
        int recognisedCount{0};

        /// Whether this file has the minimum required columns (x,y,z + edep).
        bool isValid() const {
            return indices.count(Column::X_cm)
                && indices.count(Column::Y_cm)
                && indices.count(Column::Z_cm)
                && indices.count(Column::Edep_MeV);
        }

        /// Whether a specific column is present.
        bool has(Column c) const { return indices.count(c) > 0; }

        /// Get the value at a specific column from a row of tokens.
        const std::string& value(Column c, const std::vector<std::string>& tokens) const;

        /// Get the column index, or -1.
        int index(Column c) const {
            auto it = indices.find(c);
            return (it != indices.end()) ? it->second : -1;
        }
    };

    /// Analyse a comma-separated header line and build a Mapping.
    static Mapping analyse(const std::string& headerLine);

    /// Try to match a single column name to a canonical column.
    static Column matchColumnName(const std::string& rawName);

private:
    static std::string normalise(const std::string& s);
};

} // namespace beamlab::services::import
