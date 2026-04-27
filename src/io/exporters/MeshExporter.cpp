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

    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double min_z = std::numeric_limits<double>::infinity();

    double max_x = -std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    double max_z = -std::numeric_limits<double>::infinity();

    for (const auto& vertex : mesh.vertices) {
        min_x = std::min(min_x, vertex.position.x);
        min_y = std::min(min_y, vertex.position.y);
        min_z = std::min(min_z, vertex.position.z);

        max_x = std::max(max_x, vertex.position.x);
        max_y = std::max(max_y, vertex.position.y);
        max_z = std::max(max_z, vertex.position.z);
    }

    const double cx = 0.5 * (min_x + max_x);
    const double cy = 0.5 * (min_y + max_y);
    const double cz = 0.5 * (min_z + max_z);

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

    output << "# BeamLabStudio centered/scaled preview OBJ export\n";
    output << "o " << mesh.name << "_preview\n";

    for (const auto& vertex : mesh.vertices) {
        output << "v "
               << (vertex.position.x - cx) * scale << ' '
               << (vertex.position.y - cy) * scale << ' '
               << (vertex.position.z - cz) * scale << "\n";
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
