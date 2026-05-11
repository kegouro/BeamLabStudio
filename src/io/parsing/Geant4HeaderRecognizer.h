#pragma once

#include <string>
#include <string_view>

namespace beamlab::io {

// Strict, tokenization-based header recognition.  Used by both FileProbe
// (format detection) and Geant4CsvImporter (column lookup) so the two
// always agree on what counts as a valid Geant4 trajectory CSV.
//
// The legacy implementation used substring matching — `text.find("x_cm")`
// would match `x_cm_comment`, and `text.find("t")` (in the COMSOL probe)
// matched literally any text.  This module rejects those false positives
// by tokenizing the line and requiring exact (case-insensitive) column
// names after trimming.
class Geant4HeaderRecognizer {
public:
    // Returns true iff `line` is a header row that contains the mandatory
    // Geant4 trajectory columns (x_cm, y_cm, z_cm, time_ns, trackID,
    // eventID — case-insensitive, exact token match) plus at least one
    // physics payload column (edep_MeV, kinE_MeV, or momX_MeV).
    [[nodiscard]] static bool looksLikeHeader(std::string_view line);
};

// Mirrors the same idea for COMSOL trajectory exports.  The required
// columns are qx, qy, qz (or x/y/z aliases) plus pidx (or "particle")
// plus an explicit time column (`t` / `time` / `t_s` / `Time`).  The
// previous version used `text.find("t")` which matched the letter t in
// any header text.
class ComsolHeaderRecognizer {
public:
    [[nodiscard]] static bool looksLikeHeader(std::string_view line);
};

} // namespace beamlab::io
