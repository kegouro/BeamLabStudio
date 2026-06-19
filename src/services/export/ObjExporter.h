#pragma once

#include "services/export/IExporter.h"

namespace beamlab::services::export_ {

/// Exports 3D envelope geometry to Wavefront OBJ format.
///
/// This exporter reads mesh data from AnalysisResult engine metrics
/// (extracted by the FullEnvelopePreviewBuilder or SurfaceBuilder)
/// and writes a standard .OBJ file with vertices and faces.
///
/// ## Stub status
///
/// This implementation provides a functional placeholder that writes
/// a minimal OBJ with a metadata comment.  Full mesh extraction from
/// IStorageBackend requires the EnvelopeExtractor engine to store
/// vertex/face data in its metrics JSON — integration pending.
class ObjExporter final : public IExporter {
public:
    std::string name() const override { return "OBJ Exporter"; }
    std::string format() const override { return "obj"; }
    std::vector<std::string> fileExtensions() const override { return {".obj"}; }

    ExportResult exportData(
        const storage::IStorageBackend& storage,
        const analysis::AnalysisResult& result,
        const std::filesystem::path& outputPath,
        ExportProgressCallback onProgress) override;
};

} // namespace beamlab::services::export_
