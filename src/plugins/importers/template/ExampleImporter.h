// ══════════════════════════════════════════════════════════════════════
//  ExampleImporter.h
//  Template for BeamLabStudio import plugins.
//
//  Every importer plugin must:
//    1. Inherit from beamlab::services::import::IImporter
//    2. Implement name(), supportedExtensions(), probe(), import()
//    3. Export a C-linkage factory function: create_importer()
//
//  See README.md for build instructions.
// ══════════════════════════════════════════════════════════════════════
#pragma once

// ── BeamLabStudio SDK headers ─────────────────────────────────────
// These headers are provided by a BeamLabStudio installation or by
// pointing CMAKE_PREFIX_PATH at your BeamLabStudio build directory.
#include <beamlab/services/import/IImporter.h>

#include <string>
#include <vector>

namespace beamlab::plugins {

// ── ExampleImporter ───────────────────────────────────────────────
//
// A minimal importer that recognises a 4-byte magic number "EX01"
// at the beginning of the file and imports a simple x,y,z,energy
// column layout.
//
// Adapt this file to your own format by changing:
//   - name()               → e.g. "MyExperiment Format"
//   - supportedExtensions() → e.g. {".mef", ".h5"}
//   - probe()               → read your magic bytes or header text
//   - import()              → parse rows, call storage.writeSample()
//
class ExampleImporter final : public services::import::IImporter {
public:
    // ── Identity ──────────────────────────────────────────────────
    // Return a human-readable name displayed in the UI.
    std::string name() const override
    {
        return "ExampleFormat";
    }

    // File extensions this importer can handle.  Used as a hint
    // during file-dialog filtering.
    std::vector<std::string> supportedExtensions() const override
    {
        return {".example", ".ex", ".ex01"};
    }

    // ── Detection ─────────────────────────────────────────────────
    // Read the beginning of the file and return a score 0.0–1.0
    // indicating how confident we are that this is our format.
    //
    // Scoring guidelines:
    //   1.0  — exact magic bytes match
    //   0.9  — exact header text match (e.g. "BEGIN_EXAMPLE")
    //   0.7  — column headers match expected names
    //   0.5  — structural clues (e.g. looks like CSV with 5 cols)
    //   0.3  — generic fallback (extension matches only)
    //   0.0  — definitely not our format
    services::import::ImporterCapabilityScore probe(
        const std::string& filePath) override;

    // ── Import ────────────────────────────────────────────────────
    // Read the file, parse samples, and write them into the storage
    // backend.  Called after probe() has confirmed the format.
    //
    // Requirements:
    //   1. Call storage.beginWriteBatch() before the first write.
    //   2. Call storage.endWriteBatch() after the last write.
    //   3. Use RAII (scope guard) to ensure endWriteBatch() is
    //      called even if an exception is thrown.
    //   4. Call onProgress() periodically so the UI stays responsive.
    void import(const std::string& filePath,
                services::storage::IStorageBackend& storage,
                services::import::ImportProgressCallback onProgress) override;
};

} // namespace beamlab::plugins
