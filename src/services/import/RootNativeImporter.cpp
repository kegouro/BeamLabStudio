#include "services/import/RootNativeImporter.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace beamlab::services::import {

// ROOT magic bytes: "root" at offset 0.
static constexpr uint8_t kRootMagic[4] = {0x72, 0x6F, 0x6F, 0x74};

ImporterCapabilityScore RootNativeImporter::probe(const std::string& filePath)
{
    ImporterCapabilityScore result;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return result;

    uint8_t magic[4] = {0};
    file.read(reinterpret_cast<char*>(magic), 4);

    if (std::memcmp(magic, kRootMagic, 4) == 0) {
        result.value = 1.0;
        result.matchedHeader = "ROOT binary format";
    }

    return result;
}

void RootNativeImporter::import(
    const std::string& filePath,
    storage::IStorageBackend& storage,
    ImportProgressCallback onProgress)
{
    auto fileSize = fs::file_size(filePath);

    // ROOT import requires the ROOT library (libCore, libTree).
    // This implementation provides a stub that detects ROOT but
    // delegates to the existing RootNativeImporter in src/io/importers/
    // which requires BEAMLAB_ENABLE_ROOT=ON.
    //
    // For now, throw a clear error with instructions.
    if (onProgress) onProgress(0, fileSize);

    throw std::runtime_error(
        "ROOT import requires BEAMLAB_ENABLE_ROOT=ON at build time. "
        "Use the full RootNativeImporter in src/io/importers/.");

    if (onProgress) onProgress(fileSize, fileSize);
}

} // namespace beamlab::services::import
