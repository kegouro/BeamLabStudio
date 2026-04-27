#include "io/exporters/VisualizationManifestExporter.h"

#include "core/foundation/Error.h"

#include <filesystem>
#include <fstream>

namespace beamlab::io {
namespace {

void ensureParentDirectory(const std::string& path)
{
    const std::filesystem::path p(path);
    const auto parent = p.parent_path();

    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

beamlab::core::Error makeError(beamlab::core::StatusCode code,
                               const std::string& message,
                               const std::string& details)
{
    return beamlab::core::Error{
        .code = code,
        .severity = beamlab::core::Severity::Error,
        .message = message,
        .details = details
    };
}

std::string escapeJson(const std::string& text)
{
    std::string out{};
    out.reserve(text.size());

    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(c); break;
        }
    }

    return out;
}

} // namespace

std::string VisualizationManifestExporter::name() const
{
    return "VisualizationManifestExporter";
}

ExportResult VisualizationManifestExporter::exportManifest(
    const AnalysisOutputManifest& manifest,
    const AnalysisRunConfiguration& configuration,
    const VisualizationManifestStats& stats,
    const std::string& output_path) const
{
    ExportResult result{};
    ensureParentDirectory(output_path);

    std::ofstream output(output_path);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear visualization_manifest.json",
            output_path
        );
        return result;
    }

    output << "{\n";
    output << "  \"schema_version\": \"0.1\",\n";
    output << "  \"purpose\": \"BeamLabStudio lightweight visualization inputs\",\n";
    output << "  \"downsampled\": true,\n";
    output << "  \"configuration\": {\n";
    output << "    \"axis_mode\": \"" << escapeJson(configuration.axis_mode) << "\",\n";
    output << "    \"reference_mode\": \"" << escapeJson(configuration.reference_mode) << "\",\n";
    output << "    \"binning_mode\": \"" << escapeJson(configuration.binning_mode) << "\",\n";
    output << "    \"axial_bin_count\": " << configuration.axial_bin_count << ",\n";
    output << "    \"half_window_frames\": " << configuration.half_window_frames << "\n";
    output << "  },\n";
    output << "  \"files\": {\n";
    output << "    \"trajectories_preview_csv\": \"" << escapeJson(manifest.trajectories_preview_csv) << "\",\n";
    output << "    \"focal_slice_points_csv\": \"" << escapeJson(manifest.focal_slice_points_csv) << "\",\n";
    output << "    \"envelope_rings_csv\": \"" << escapeJson(manifest.envelope_rings_csv) << "\",\n";
    output << "    \"beam_caustic_preview_obj\": \"" << escapeJson(manifest.beam_caustic_preview_obj) << "\",\n";
    output << "    \"effective_lens_disk_preview_obj\": \"" << escapeJson(manifest.effective_lens_disk_preview_obj) << "\"\n";
    output << "  },\n";
    output << "  \"row_counts\": {\n";
    output << "    \"trajectories_preview\": " << stats.trajectory_preview_rows << ",\n";
    output << "    \"focal_slice_points\": " << stats.focal_slice_point_rows << ",\n";
    output << "    \"envelope_rings\": " << stats.envelope_ring_rows << "\n";
    output << "  },\n";
    output << "  \"layer_semantics\": {\n";
    output << "    \"trajectories_preview_obj\": {\n";
    output << "      \"display_name\": \"Individual trajectories\",\n";
    output << "      \"status\": \"available\",\n";
    output << "      \"semantic_role\": \"sampled_particle_paths\"\n";
    output << "    },\n";
    output << "    \"beam_caustic_preview_obj\": {\n";
    output << "      \"display_name\": \"Focal envelope proxy\",\n";
    output << "      \"status\": \"available\",\n";
    output << "      \"scope\": \"focal_window\",\n";
    output << "      \"semantic_role\": \"boundary_surface_from_transverse_slices\",\n";
    output << "      \"description\": \"Boundary surface reconstructed from transverse beam slices. Not physical beamline geometry.\"\n";
    output << "    },\n";
    output << "    \"full_beam_envelope_obj\": {\n";
    output << "      \"display_name\": \"Full beam envelope\",\n";
    output << "      \"status\": \"pending\",\n";
    output << "      \"scope\": \"full_axial_range\"\n";
    output << "    },\n";
    output << "    \"effective_lens\": {\n";
    output << "      \"display_name\": \"Effective lens\",\n";
    output << "      \"status\": \"available\",\n";
    output << "      \"semantic_role\": \"focal_aperture_proxy\"\n";
    output << "    },\n";
    output << "    \"physical_beamline_geometry\": {\n";
    output << "      \"display_name\": \"Physical beamline geometry\",\n";
    output << "      \"status\": \"pending\"\n";
    output << "    }\n";
    output << "  }\n";
    output << "}\n";

    result.success = true;
    result.output_path = output_path;
    return result;
}

} // namespace beamlab::io
