#pragma once

#include "data/model/BeamEnvelope.h"
#include "data/model/FocusResult.h"
#include "data/model/LensSurfaceModel.h"
#include "data/model/TrajectoryDataset.h"
#include "io/common/ExportResult.h"
#include "io/exporters/IExporter.h"

#include <string>
#include <vector>

namespace beamlab::io {

struct AnalysisRunConfiguration {
    std::string axis_mode{"auto"};
    std::string reference_mode{"auto"};
    std::string binning_mode{"uniform"};

    std::size_t axial_bin_count{501};
    std::size_t half_window_frames{5};

    std::size_t caustic_resample_points{96};
    double caustic_preview_scale{50.0};

    std::size_t lens_boundary_points{128};
    std::size_t lens_radial_layers{16};
    double lens_center_thickness_m{0.003};
    double lens_edge_thickness_m{0.0005};
    double lens_preview_scale{80.0};

    bool root_native_enabled{false};
};

struct AnalysisOutputManifest {
    std::string output_root{};

    std::string beam_caustic_obj{};
    std::string beam_caustic_preview_obj{};
    std::string beam_caustic_parametric_txt{};
    std::string beam_caustic_parametric_csv{};

    std::string effective_lens_disk_obj{};
    std::string effective_lens_disk_preview_obj{};
    std::string effective_lens_disk_parametric_txt{};
    std::string effective_lens_disk_parametric_csv{};

    std::string focus_curve_csv{};
    std::string envelope_summary_csv{};
    std::string run_metadata_json{};
    std::string analysis_summary_md{};
    std::string quality_report_csv{};
    std::string quality_report_md{};
    std::string trajectories_preview_csv{};
    std::string focal_slice_points_csv{};
    std::string envelope_rings_csv{};
    std::string visualization_manifest_json{};
};

class AnalysisReportExporter final : public IExporter {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] ExportResult exportFocusCurve(
        const beamlab::data::FocusResult& focus,
        const std::string& output_path) const;

    [[nodiscard]] ExportResult exportEnvelopeSummary(
        const std::vector<beamlab::data::BeamEnvelope>& envelopes,
        const std::string& output_path) const;

    [[nodiscard]] ExportResult exportRunMetadata(
        const beamlab::data::TrajectoryDataset& dataset,
        const beamlab::data::FocusResult& focus,
        const std::vector<beamlab::data::BeamEnvelope>& envelopes,
        const beamlab::data::LensSurfaceModel& caustic_surface,
        const beamlab::data::LensSurfaceModel& lens_disk,
        const AnalysisRunConfiguration& configuration,
        const AnalysisOutputManifest& manifest) const;

    [[nodiscard]] ExportResult exportMarkdownSummary(
        const beamlab::data::TrajectoryDataset& dataset,
        const beamlab::data::FocusResult& focus,
        const std::vector<beamlab::data::BeamEnvelope>& envelopes,
        const beamlab::data::LensSurfaceModel& caustic_surface,
        const beamlab::data::LensSurfaceModel& lens_disk,
        const AnalysisRunConfiguration& configuration,
        const AnalysisOutputManifest& manifest) const;
};

} // namespace beamlab::io
