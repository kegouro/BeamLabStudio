#include "io/exporters/AnalysisReportExporter.h"

#include "core/foundation/Error.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

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
    std::ostringstream escaped{};

    for (const char c : text) {
        switch (c) {
        case '"': escaped << "\\\""; break;
        case '\\': escaped << "\\\\"; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default: escaped << c; break;
        }
    }

    return escaped.str();
}

std::size_t totalSampleCount(const beamlab::data::TrajectoryDataset& dataset)
{
    std::size_t total = 0;

    for (const auto& trajectory : dataset.trajectories) {
        total += trajectory.samples.size();
    }

    return total;
}

std::size_t validEnvelopeCount(const std::vector<beamlab::data::BeamEnvelope>& envelopes)
{
    std::size_t count = 0;

    for (const auto& envelope : envelopes) {
        if (envelope.valid) {
            ++count;
        }
    }

    return count;
}

} // namespace

std::string AnalysisReportExporter::name() const
{
    return "AnalysisReportExporter";
}

ExportResult AnalysisReportExporter::exportFocusCurve(
    const beamlab::data::FocusResult& focus,
    const std::string& output_path) const
{
    ExportResult result{};
    ensureParentDirectory(output_path);

    std::ofstream output(output_path);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear focus_curve.csv",
            output_path
        );
        return result;
    }

    output << "# BeamLabStudio focus curve export\n";
    output << "# units: reference_value_m in seconds, metric_value_m in meters\n";
    output << "# metric: " << focus.curve.metric_name << "\n";
    output << "# focus_index: " << focus.focus_index << "\n";
    output << "# focus_reference_value_m: "
           << std::scientific << std::setprecision(12)
           << focus.focus_reference_value << "\n";
    output << "# focus_metric_value_m: "
           << std::scientific << std::setprecision(12)
           << focus.focus_metric_value << "\n";
    output << "index,reference_value_m,metric_name,metric_value_m,point_count,is_focus\n";

    for (std::size_t i = 0; i < focus.curve.points.size(); ++i) {
        const auto& point = focus.curve.points[i];

        output << i << ','
               << std::scientific << std::setprecision(12)
               << point.reference_value << ','
               << focus.curve.metric_name << ','
               << point.metric_value << ','
               << point.point_count << ','
               << (focus.valid && i == focus.focus_index ? "true" : "false")
               << '\n';
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

ExportResult AnalysisReportExporter::exportEnvelopeSummary(
    const std::vector<beamlab::data::BeamEnvelope>& envelopes,
    const std::string& output_path) const
{
    ExportResult result{};
    ensureParentDirectory(output_path);

    std::ofstream output(output_path);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear envelope_summary.csv",
            output_path
        );
        return result;
    }

    output << "# BeamLabStudio envelope summary export\n";
    output << "# units: reference and axial position in meters, area in square meters, perimeter in meters\n";
    output << "slice_index,reference_value_m,axial_position_m,method_name,input_points,boundary_points,area_m2,perimeter_m,valid\n";

    for (const auto& envelope : envelopes) {
        output << envelope.slice_index << ','
               << std::scientific << std::setprecision(12)
               << envelope.reference_time_s << ','
               << envelope.axial_position_m << ','
               << envelope.method_name << ','
               << envelope.input_point_count << ','
               << envelope.boundary_points.size() << ','
               << envelope.area_m2 << ','
               << envelope.perimeter_m << ','
               << (envelope.valid ? "true" : "false")
               << '\n';
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

ExportResult AnalysisReportExporter::exportRunMetadata(
    const beamlab::data::TrajectoryDataset& dataset,
    const beamlab::data::FocusResult& focus,
    const std::vector<beamlab::data::BeamEnvelope>& envelopes,
    const beamlab::data::LensSurfaceModel& caustic_surface,
    const beamlab::data::LensSurfaceModel& lens_disk,
    const AnalysisRunConfiguration& configuration,
    const AnalysisOutputManifest& manifest) const
{
    ExportResult result{};
    ensureParentDirectory(manifest.run_metadata_json);

    std::ofstream output(manifest.run_metadata_json);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear run_metadata.json",
            manifest.run_metadata_json
        );
        return result;
    }

    output << std::scientific << std::setprecision(12);

    output << "{\n";
    output << "  \"application\": \"BeamLabStudio\",\n";
    output << "  \"schema_version\": \"0.2\",\n";
    output << "  \"output_root\": \"" << escapeJson(manifest.output_root) << "\",\n";

    output << "  \"configuration\": {\n";
    output << "    \"axis_mode\": \"" << escapeJson(configuration.axis_mode) << "\",\n";
    output << "    \"reference_mode\": \"" << escapeJson(configuration.reference_mode) << "\",\n";
    output << "    \"binning_mode\": \"" << escapeJson(configuration.binning_mode) << "\",\n";
    output << "    \"axial_bin_count\": " << configuration.axial_bin_count << ",\n";
    output << "    \"half_window_frames\": " << configuration.half_window_frames << ",\n";
    output << "    \"caustic_resample_points\": " << configuration.caustic_resample_points << ",\n";
    output << "    \"caustic_preview_scale\": " << configuration.caustic_preview_scale << ",\n";
    output << "    \"lens_boundary_points\": " << configuration.lens_boundary_points << ",\n";
    output << "    \"lens_radial_layers\": " << configuration.lens_radial_layers << ",\n";
    output << "    \"lens_center_thickness_m\": " << configuration.lens_center_thickness_m << ",\n";
    output << "    \"lens_edge_thickness_m\": " << configuration.lens_edge_thickness_m << ",\n";
    output << "    \"lens_preview_scale\": " << configuration.lens_preview_scale << ",\n";
    output << "    \"root_native_enabled\": " << (configuration.root_native_enabled ? "true" : "false") << "\n";
    output << "  },\n";

    output << "  \"dataset\": {\n";
    output << "    \"display_name\": \"" << escapeJson(dataset.metadata.display_name) << "\",\n";
    output << "    \"source_path\": \"" << escapeJson(dataset.metadata.source.original_path) << "\",\n";
    output << "    \"source_type\": \"" << escapeJson(dataset.metadata.source.source_type) << "\",\n";
    output << "    \"importer_name\": \"" << escapeJson(dataset.metadata.import.importer_name) << "\",\n";
    output << "    \"importer_version\": \"" << escapeJson(dataset.metadata.import.importer_version) << "\",\n";
    output << "    \"trajectory_count\": " << dataset.trajectories.size() << ",\n";
    output << "    \"sample_count\": " << totalSampleCount(dataset) << "\n";
    output << "  },\n";

    output << "  \"focus\": {\n";
    output << "    \"valid\": " << (focus.valid ? "true" : "false") << ",\n";
    output << "    \"metric_name\": \"" << escapeJson(focus.curve.metric_name) << "\",\n";
    output << "    \"focus_index\": " << focus.focus_index << ",\n";
    output << "    \"focus_reference_value_m\": " << focus.focus_reference_value << ",\n";
    output << "    \"focus_metric_value_m\": " << focus.focus_metric_value << ",\n";
    output << "    \"confidence\": " << focus.confidence << ",\n";
    output << "    \"detection_method\": \"" << escapeJson(focus.detection_method) << "\"\n";
    output << "  },\n";

    output << "  \"envelopes\": {\n";
    output << "    \"count\": " << envelopes.size() << ",\n";
    output << "    \"valid_count\": " << validEnvelopeCount(envelopes) << "\n";
    output << "  },\n";

    output << "  \"surfaces\": {\n";
    output << "    \"beam_caustic_surface\": {\n";
    output << "      \"valid\": " << (caustic_surface.valid ? "true" : "false") << ",\n";
    output << "      \"display_name\": \"Focal envelope proxy\",\n";
    output << "      \"scope\": \"focal_window\",\n";
    output << "      \"description\": \"Boundary surface reconstructed from transverse beam slices. Not physical beamline geometry.\",\n";
    output << "      \"method_name\": \"" << escapeJson(caustic_surface.method_name) << "\",\n";
    output << "      \"slice_count\": " << caustic_surface.slice_count << ",\n";
    output << "      \"points_per_slice\": " << caustic_surface.points_per_slice << ",\n";
    output << "      \"vertex_count\": " << caustic_surface.mesh.vertices.size() << ",\n";
    output << "      \"face_count\": " << caustic_surface.mesh.faces.size() << ",\n";
    output << "      \"obj_path\": \"" << escapeJson(manifest.beam_caustic_obj) << "\",\n";
    output << "      \"preview_obj_path\": \"" << escapeJson(manifest.beam_caustic_preview_obj) << "\",\n";
    output << "      \"parametric_equation_path\": \"" << escapeJson(manifest.beam_caustic_parametric_txt) << "\",\n";
    output << "      \"parametric_samples_path\": \"" << escapeJson(manifest.beam_caustic_parametric_csv) << "\"\n";
    output << "    },\n";

    output << "    \"effective_lens_disk\": {\n";
    output << "      \"valid\": " << (lens_disk.valid ? "true" : "false") << ",\n";
    output << "      \"method_name\": \"" << escapeJson(lens_disk.method_name) << "\",\n";
    output << "      \"slice_count\": " << lens_disk.slice_count << ",\n";
    output << "      \"points_per_slice\": " << lens_disk.points_per_slice << ",\n";
    output << "      \"vertex_count\": " << lens_disk.mesh.vertices.size() << ",\n";
    output << "      \"face_count\": " << lens_disk.mesh.faces.size() << ",\n";
    output << "      \"obj_path\": \"" << escapeJson(manifest.effective_lens_disk_obj) << "\",\n";
    output << "      \"preview_obj_path\": \"" << escapeJson(manifest.effective_lens_disk_preview_obj) << "\",\n";
    output << "      \"parametric_equation_path\": \"" << escapeJson(manifest.effective_lens_disk_parametric_txt) << "\",\n";
    output << "      \"parametric_samples_path\": \"" << escapeJson(manifest.effective_lens_disk_parametric_csv) << "\"\n";
    output << "    },\n";
    output << "    \"full_beam_envelope\": {\n";
    output << "      \"display_name\": \"Full beam envelope\",\n";
    output << "      \"status\": \"pending\",\n";
    output << "      \"scope\": \"full_axial_range\"\n";
    output << "    },\n";
    output << "    \"physical_beamline_geometry\": {\n";
    output << "      \"display_name\": \"Physical beamline geometry\",\n";
    output << "      \"status\": \"pending\"\n";
    output << "    }\n";
    output << "  },\n";

    output << "  \"tables\": {\n";
    output << "    \"focus_curve_csv\": \"" << escapeJson(manifest.focus_curve_csv) << "\",\n";
    output << "    \"envelope_summary_csv\": \"" << escapeJson(manifest.envelope_summary_csv) << "\"\n";
    output << "  },\n";

    output << "  \"reports\": {\n";
    output << "    \"analysis_summary_md\": \"" << escapeJson(manifest.analysis_summary_md) << "\",\n";
    output << "    \"run_metadata_json\": \"" << escapeJson(manifest.run_metadata_json) << "\"\n";
    output << "  }\n";
    output << "}\n";

    result.success = true;
    result.output_path = manifest.run_metadata_json;
    return result;
}

ExportResult AnalysisReportExporter::exportMarkdownSummary(
    const beamlab::data::TrajectoryDataset& dataset,
    const beamlab::data::FocusResult& focus,
    const std::vector<beamlab::data::BeamEnvelope>& envelopes,
    const beamlab::data::LensSurfaceModel& caustic_surface,
    const beamlab::data::LensSurfaceModel& lens_disk,
    const AnalysisRunConfiguration& configuration,
    const AnalysisOutputManifest& manifest) const
{
    ExportResult result{};
    ensureParentDirectory(manifest.analysis_summary_md);

    std::ofstream output(manifest.analysis_summary_md);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear analysis_summary.md",
            manifest.analysis_summary_md
        );
        return result;
    }

    output << std::scientific << std::setprecision(6);

    output << "# BeamLabStudio Analysis Summary\n\n";

    output << "## Run configuration\n\n";
    output << "| Parameter | Value |\n";
    output << "|---|---:|\n";
    output << "| Axis mode | `" << configuration.axis_mode << "` |\n";
    output << "| Reference mode | `" << configuration.reference_mode << "` |\n";
    output << "| Binning mode | `" << configuration.binning_mode << "` |\n";
    output << "| Axial bins | " << configuration.axial_bin_count << " |\n";
    output << "| Half window frames/bins | " << configuration.half_window_frames << " |\n";
    output << "| Envelope proxy resample points | " << configuration.caustic_resample_points << " |\n";
    output << "| Envelope proxy preview scale | " << configuration.caustic_preview_scale << " |\n";
    output << "| Lens boundary points | " << configuration.lens_boundary_points << " |\n";
    output << "| Lens radial layers | " << configuration.lens_radial_layers << " |\n";
    output << "| Lens center thickness [m] | " << configuration.lens_center_thickness_m << " |\n";
    output << "| Lens edge thickness [m] | " << configuration.lens_edge_thickness_m << " |\n";
    output << "| Lens preview scale | " << configuration.lens_preview_scale << " |\n";
    output << "| ROOT native enabled | " << (configuration.root_native_enabled ? "true" : "false") << " |\n\n";

    output << "## Dataset\n\n";
    output << "| Field | Value |\n";
    output << "|---|---:|\n";
    output << "| Name | `" << dataset.metadata.display_name << "` |\n";
    output << "| Source type | `" << dataset.metadata.source.source_type << "` |\n";
    output << "| Importer | `" << dataset.metadata.import.importer_name << "` |\n";
    output << "| Trajectories | " << dataset.trajectories.size() << " |\n";
    output << "| Samples | " << totalSampleCount(dataset) << " |\n\n";

    output << "## Focus\n\n";
    output << "| Field | Value |\n";
    output << "|---|---:|\n";
    output << "| Valid | " << (focus.valid ? "true" : "false") << " |\n";
    output << "| Metric | `" << focus.curve.metric_name << "` |\n";
    output << "| Focus index | " << focus.focus_index << " |\n";
    output << "| Focus axial position [m] | " << focus.focus_reference_value << " |\n";
    output << "| Minimum transverse RMS radius [m] | " << focus.focus_metric_value << " |\n";
    output << "| Confidence proxy | " << focus.confidence << " |\n\n";

    output << "## Envelopes\n\n";
    output << "| Field | Value |\n";
    output << "|---|---:|\n";
    output << "| Envelope count | " << envelopes.size() << " |\n";
    output << "| Valid envelopes | " << validEnvelopeCount(envelopes) << " |\n\n";

    output << "## Surfaces\n\n";
    output << "| Surface | Valid | Vertices | Faces | Meaning |\n";
    output << "|---|---:|---:|---:|---|\n";
    output << "| Focal envelope proxy | "
           << (caustic_surface.valid ? "true" : "false") << " | "
           << caustic_surface.mesh.vertices.size() << " | "
           << caustic_surface.mesh.faces.size()
           << " | Boundary surface reconstructed from transverse slices near focus |\n";
    output << "| Effective lens disk | "
           << (lens_disk.valid ? "true" : "false") << " | "
           << lens_disk.mesh.vertices.size() << " | "
           << lens_disk.mesh.faces.size()
           << " | Closed disk-like effective lens at focal plane |\n\n";

    output << "Pending conceptual layers: Full beam envelope over the complete axial range; "
              "physical beamline geometry as a separate imported or reconstructed layer.\n\n";

    output << "## Output files\n\n";
    output << "### Geometry\n\n";
    output << "- `" << manifest.beam_caustic_obj << "`\n";
    output << "- `" << manifest.beam_caustic_preview_obj << "`\n";
    output << "- `" << manifest.effective_lens_disk_obj << "`\n";
    output << "- `" << manifest.effective_lens_disk_preview_obj << "`\n\n";

    output << "### Parametric equations\n\n";
    output << "- `" << manifest.beam_caustic_parametric_txt << "`\n";
    output << "- `" << manifest.beam_caustic_parametric_csv << "`\n";
    output << "- `" << manifest.effective_lens_disk_parametric_txt << "`\n";
    output << "- `" << manifest.effective_lens_disk_parametric_csv << "`\n\n";

    output << "### Tables and metadata\n\n";
    output << "- `" << manifest.focus_curve_csv << "`\n";
    output << "- `" << manifest.envelope_summary_csv << "`\n";
    output << "- `" << manifest.run_metadata_json << "`\n\n";

    output << "### Visualization previews\n\n";
    output << "- `" << manifest.trajectories_preview_csv << "`\n";
    output << "- `" << manifest.focal_slice_points_csv << "`\n";
    output << "- `" << manifest.envelope_rings_csv << "`\n";

    result.success = true;
    result.output_path = manifest.analysis_summary_md;
    return result;
}

} // namespace beamlab::io
