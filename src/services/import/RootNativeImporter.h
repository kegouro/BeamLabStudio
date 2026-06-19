#pragma once

#include "services/import/IImporter.h"

namespace beamlab::services::import {

/// Importer for CERN ROOT binary files.
///
/// Probe strategy:
///   Read the first 4 bytes of the file. ROOT format files start with
///   the bytes 0x72, 0x6F, 0x6F, 0x74 (ASCII "root").  If matched,
///   score is 1.0.  Any other binary header → score 0.0.
///
/// Note: This importer probes by extension AND magic bytes.  Even if
/// the extension is .root, the magic bytes are verified.
class RootNativeImporter final : public IImporter {
public:
    std::string name() const override { return "CERN ROOT"; }
    std::vector<std::string> supportedExtensions() const override { return {".root"}; }

    ImporterCapabilityScore probe(const std::string& filePath) override;

    void import(const std::string& filePath,
                storage::IStorageBackend& storage,
                ImportProgressCallback onProgress) override;
};

} // namespace beamlab::services::import
