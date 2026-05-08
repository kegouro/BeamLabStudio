#include "io/exporters/MeshExporter.h"

#include "core/foundation/Error.h"

#include <algorithm>
#include <fstream>
#include <limits>

namespace beamlab::io {
namespace {

beamlab::core::Error makeError(beamlab::core::StatusCode code,
                               std::string message,
                               std::string details)
{
    return beamlab::core::Error{
        .code = code,
        .severity = beamlab::core::Severity::Error,
        .message = std::move(message),
        .details = std::move(details)
    };
}

} // namespace

std::string MeshExporter::name() const
{
    return "MeshExporter";
}

ExportResult MeshExporter::exportObj(const beamlab::geometry::Mesh& mesh,
                                     const std::string& output_path) const
{
    ExportResult result{};

    if (mesh.empty()) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::InvalidArgument,
            "No se puede exportar una malla vacía",
            output_path
        );
        return result;
    }

    std::ofstream output(output_path);
    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear el archivo OBJ",
            output_path
        );
        return result;
    }

    output << "# BeamLabStudio OBJ export\n";
    output << "o " << mesh.name << "\n";

    for (const auto& vertex : mesh.vertices) {
        output << "v "
               << vertex.position.x << ' '
               << vertex.position.y << ' '
               << vertex.position.z << "\n";
    }

    for (const auto& face : mesh.faces) {
        output << "f "
               << (face.a + 1) << ' '
               << (face.b + 1) << ' '
               << (face.c + 1) << "\n";
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

ExportResult MeshExporter::exportObjPreview(const beamlab::geometry::Mesh& mesh,
                                            const std::string& output_path,
                                            const double scale) const
{
    ExportResult result{};

    if (mesh.empty()) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::InvalidArgument,
            "No se puede exportar una malla vacía",
            output_path
        );
        return result;
    }

    std::ofstream output(output_path);
    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear el archivo OBJ preview",
            output_path
        );
        return result;
    }

    // The preview preserves world coordinates so it aligns correctly with the
    // combined 3D scene.  The `scale` parameter is kept for API compatibility
    // but defaults to 1.0; values other than 1.0 are only meaningful when the
    // caller explicitly wants unit conversion (e.g. m → cm).
    output << "# BeamLabStudio preview OBJ export (world coordinates)\n";
    output << "o " << mesh.name << "_preview\n";

    for (const auto& vertex : mesh.vertices) {
        output << "v "
               << vertex.position.x * scale << ' '
               << vertex.position.y * scale << ' '
               << vertex.position.z * scale << "\n";
    }

    for (const auto& face : mesh.faces) {
        output << "f "
               << (face.a + 1) << ' '
               << (face.b + 1) << ' '
               << (face.c + 1) << "\n";
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

} // namespace beamlab::io
