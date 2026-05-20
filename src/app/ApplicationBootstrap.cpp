#include "app/ApplicationBootstrap.h"

#include "app/CommandLineOptions.h"
#include "analysis/envelope/ConvexHullEnvelopeExtractor.h"
#include "analysis/envelope/EnvelopeParameters.h"
#include "analysis/focus/FocusCurveBuilder.h"
#include "analysis/focus/FocusDetector.h"
#include "analysis/focus/FocusParameters.h"
#include "analysis/quality/AnalysisQualityChecker.h"
#include "analysis/slice/SliceExtractor.h"
#include "analysis/slice/SliceParameters.h"
#include "analysis/slice/SlicePlanner.h"
#include "analysis/statistics/FrameStatisticsEngine.h"
#include "analysis/statistics/FrameStatisticsParameters.h"
#include "analysis/surfaces/LensDiskBuildParameters.h"
#include "analysis/surfaces/LensDiskBuilder.h"
#include "analysis/surfaces/SurfaceBuildParameters.h"
#include "analysis/surfaces/SurfaceBuilder.h"
#include "data/ids/SampleId.h"
#include "data/ids/TrajectoryId.h"
#include "data/model/BeamEnvelope.h"
#include "data/model/Trajectory.h"
#include "data/model/TrajectoryDataset.h"
#include "data/model/TrajectorySample.h"
#include "io/common/ImportContext.h"
#include "io/detect/FileProbe.h"
#include "io/detect/ImportRouter.h"
#include "io/exporters/AnalysisReportExporter.h"
#include "io/exporters/MeshExporter.h"
#include "io/exporters/ParametricSurfaceExporter.h"
#include "io/exporters/QualityReportExporter.h"
#include "io/exporters/EnergyProfileExporter.h"
#include "io/exporters/VisualizationDataExporter.h"
#include "io/exporters/VisualizationManifestExporter.h"
#include "io/importers/ComsolCsvImporter.h"
#include "io/importers/DelimitedTableImporter.h"
#include "io/importers/Geant4CsvImporter.h"
#ifdef BEAMLAB_ENABLE_ROOT
#include "io/importers/RootNativeImporter.h"
#endif
#include "io/normalization/DatasetNormalizer.h"
#include "biosim/core/ScoringPlane.h"
#include "biosim/core/ScoringPlaneDetector.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace beamlab::app {
namespace {

std::string pathString(const std::filesystem::path& path)
{
    return path.lexically_normal().string();
}

double equivalentEnvelopeRadius(const beamlab::data::BeamEnvelope& envelope)
{
    if (!envelope.valid || envelope.area_m2 <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }

    return std::sqrt(envelope.area_m2 / std::numbers::pi);
}


beamlab::io::AnalysisRunConfiguration buildAnalysisRunConfiguration(
    const CommandLineOptions& options)
{
    beamlab::io::AnalysisRunConfiguration configuration{};

    configuration.axis_mode = options.axis_mode;
    configuration.reference_mode = options.reference_mode;
    configuration.binning_mode = options.binning_mode;

    configuration.axial_bin_count = options.axial_bin_count;
    configuration.half_window_frames = options.half_window_frames;

    configuration.caustic_resample_points = options.caustic_resample_points;
    configuration.caustic_preview_scale = options.caustic_preview_scale;

    configuration.lens_boundary_points = options.lens_boundary_points;
    configuration.lens_radial_layers = options.lens_radial_layers;
    configuration.lens_center_thickness_m = options.lens_center_thickness_m;
    configuration.lens_edge_thickness_m = options.lens_edge_thickness_m;
    configuration.lens_preview_scale = options.lens_preview_scale;

#ifdef BEAMLAB_ENABLE_ROOT
    configuration.root_native_enabled = true;
#else
    configuration.root_native_enabled = false;
#endif

    return configuration;
}


beamlab::io::AnalysisOutputManifest buildManifest(const CommandLineOptions& options)
{
    const std::filesystem::path root(options.output_directory);

    beamlab::io::AnalysisOutputManifest manifest{};
    manifest.output_root = pathString(root);

    manifest.beam_caustic_obj = pathString(root / "geometry" / "beam_caustic_surface.obj");
    manifest.beam_caustic_preview_obj = pathString(root / "geometry" / "beam_caustic_surface_preview.obj");
    manifest.beam_caustic_parametric_txt = pathString(root / "equations" / "beam_caustic_parametric_equation.txt");
    manifest.beam_caustic_parametric_csv = pathString(root / "equations" / "beam_caustic_parametric_samples.csv");

    manifest.effective_lens_disk_obj = pathString(root / "geometry" / "effective_lens_disk.obj");
    manifest.effective_lens_disk_preview_obj = pathString(root / "geometry" / "effective_lens_disk_preview.obj");
    manifest.effective_lens_disk_parametric_txt = pathString(root / "equations" / "effective_lens_disk_parametric_equation.txt");
    manifest.effective_lens_disk_parametric_csv = pathString(root / "equations" / "effective_lens_disk_parametric_samples.csv");

    manifest.focus_curve_csv = pathString(root / "tables" / "focus_curve.csv");
    manifest.envelope_summary_csv = pathString(root / "tables" / "envelope_summary.csv");

    manifest.run_metadata_json = pathString(root / "reports" / "run_metadata.json");
    manifest.analysis_summary_md = pathString(root / "reports" / "analysis_summary.md");
    manifest.quality_report_csv = pathString(root / "reports" / "quality_report.csv");
    manifest.quality_report_md = pathString(root / "reports" / "quality_report.md");
    manifest.trajectories_preview_csv = pathString(root / "visualization" / "trajectories_preview.csv");
    manifest.trajectories_preview_obj = pathString(root / "visualization" / "trajectories_preview.obj");
    manifest.focal_slice_points_csv = pathString(root / "visualization" / "focal_slice_points.csv");
    manifest.envelope_rings_csv = pathString(root / "visualization" / "envelope_rings.csv");
    manifest.visualization_manifest_json = pathString(root / "visualization" / "visualization_manifest.json");

    return manifest;
}


void applyManualAxisMode(beamlab::data::TrajectoryDataset& dataset,
                         const std::string& axis_mode)
{
    if (axis_mode == "auto") {
        return;
    }

    if (axis_mode == "x") {
        dataset.axis_frame.longitudinal = {1.0, 0.0, 0.0};
        dataset.axis_frame.transverse_u = {0.0, 1.0, 0.0};
        dataset.axis_frame.transverse_v = {0.0, 0.0, 1.0};
    } else if (axis_mode == "-x") {
        dataset.axis_frame.longitudinal = {-1.0, 0.0, 0.0};
        dataset.axis_frame.transverse_u = {0.0, 1.0, 0.0};
        dataset.axis_frame.transverse_v = {0.0, 0.0, 1.0};
    } else if (axis_mode == "y") {
        dataset.axis_frame.longitudinal = {0.0, 1.0, 0.0};
        dataset.axis_frame.transverse_u = {1.0, 0.0, 0.0};
        dataset.axis_frame.transverse_v = {0.0, 0.0, 1.0};
    } else if (axis_mode == "-y") {
        dataset.axis_frame.longitudinal = {0.0, -1.0, 0.0};
        dataset.axis_frame.transverse_u = {1.0, 0.0, 0.0};
        dataset.axis_frame.transverse_v = {0.0, 0.0, 1.0};
    } else if (axis_mode == "z") {
        dataset.axis_frame.longitudinal = {0.0, 0.0, 1.0};
        dataset.axis_frame.transverse_u = {1.0, 0.0, 0.0};
        dataset.axis_frame.transverse_v = {0.0, 1.0, 0.0};
    } else if (axis_mode == "-z") {
        dataset.axis_frame.longitudinal = {0.0, 0.0, -1.0};
        dataset.axis_frame.transverse_u = {1.0, 0.0, 0.0};
        dataset.axis_frame.transverse_v = {0.0, 1.0, 0.0};
    }

    dataset.axis_frame.derivation_method = "manual_axis_" + axis_mode;
    dataset.axis_frame.confidence = 1.0;
}

beamlab::analysis::FrameStatisticsParameters makeFrameStatisticsParameters(
    const CommandLineOptions& options)
{
    beamlab::analysis::FrameStatisticsParameters parameters{};
    parameters.axial_bin_count = options.axial_bin_count;

    if (options.reference_mode == "synchronized") {
        parameters.reference_mode = beamlab::analysis::ReferenceMode::Synchronized;
    } else if (options.reference_mode == "axial-bins") {
        parameters.reference_mode = beamlab::analysis::ReferenceMode::AxialBins;
    } else {
        parameters.reference_mode = beamlab::analysis::ReferenceMode::Auto;
    }

    if (options.binning_mode == "equal-count") {
        parameters.axial_binning_mode = beamlab::analysis::AxialBinningMode::EqualCount;
    } else {
        parameters.axial_binning_mode = beamlab::analysis::AxialBinningMode::Uniform;
    }

    return parameters;
}


void createOutputDirectories(const beamlab::io::AnalysisOutputManifest& manifest)
{
    const std::filesystem::path root(manifest.output_root);

    std::filesystem::create_directories(root / "geometry");
    std::filesystem::create_directories(root / "equations");
    std::filesystem::create_directories(root / "tables");
    std::filesystem::create_directories(root / "reports");
}

beamlab::data::TrajectoryDataset makeSyntheticFocusingDataset()
{
    beamlab::data::TrajectoryDataset dataset{};
    dataset.metadata.display_name = "synthetic_focusing_beam";
    dataset.metadata.source.source_type = "internal_demo";
    dataset.metadata.import.importer_name = "SyntheticGenerator";
    dataset.metadata.import.importer_version = "0.1";
    dataset.metadata.import.schema_name = "synthetic";

    constexpr std::size_t particle_count = 48;
    constexpr std::size_t frame_count = 15;

    std::uint64_t sample_counter = 1;
    dataset.trajectories.reserve(particle_count);

    for (std::size_t p = 0; p < particle_count; ++p) {
        beamlab::data::Trajectory trajectory{};
        trajectory.id = beamlab::data::TrajectoryId{p + 1};

        const double angle =
            2.0 * std::numbers::pi * static_cast<double>(p) /
            static_cast<double>(particle_count);

        trajectory.samples.reserve(frame_count);

        for (std::size_t f = 0; f < frame_count; ++f) {
            const double frame = static_cast<double>(f);
            const double focus_frame = 7.0;
            const double radius = 0.0015 + 0.0025 * std::abs(frame - focus_frame);

            beamlab::data::TrajectorySample sample{};
            sample.id = beamlab::data::SampleId{sample_counter++};
            sample.trajectory_id = trajectory.id;
            sample.time_s = frame * 1.0e-9;
            sample.position_m = {
                radius * std::cos(angle),
                -0.02 * frame,
                radius * std::sin(angle)
            };

            trajectory.samples.push_back(sample);
        }

        dataset.trajectories.push_back(std::move(trajectory));
    }

    return dataset;
}

beamlab::data::TrajectoryDataset loadDatasetFromFile(const std::string& file_path)
{
    const beamlab::io::FileProbe probe{};
    const auto probe_result = probe.probe(file_path);

    beamlab::io::ImportRouter router{};
    router.registerImporter(std::make_shared<beamlab::io::ComsolCsvImporter>());
    router.registerImporter(std::make_shared<beamlab::io::Geant4CsvImporter>());
#ifdef BEAMLAB_ENABLE_ROOT
    router.registerImporter(std::make_shared<beamlab::io::RootNativeImporter>());
#endif
    router.registerImporter(std::make_shared<beamlab::io::DelimitedTableImporter>());

    const auto importer = router.chooseBestImporter(probe_result);

    if (!importer) {
        throw std::runtime_error("No se encontró importador para el archivo: " + file_path);
    }

    std::cout << "Selected importer: " << importer->name() << '\n';

    const auto inspection = importer->inspect(file_path);
    const auto schema = importer->buildSchema(file_path, inspection);

    beamlab::io::ImportContext context{};
    const auto import_result = importer->import(file_path, schema, context);

    if (!import_result.success || !import_result.dataset.has_value()) {
        if (import_result.error.has_value()) {
            throw std::runtime_error(import_result.error->message + ": " +
                                     import_result.error->details);
        }

        throw std::runtime_error("Importación fallida sin error detallado");
    }

    for (const auto& warning : import_result.warnings) {
        std::cout << "Warning: " << warning.message << " | "
                  << warning.details << '\n';
    }

    return import_result.dataset.value();
}

void printFocusSummary(const beamlab::data::TrajectoryDataset& dataset,
                       const beamlab::data::FocusResult& focus)
{
    std::cout << "\n=== Focus analysis ===\n";
    std::cout << "Dataset: " << dataset.metadata.display_name << '\n';
    std::cout << "Trajectories: " << dataset.trajectories.size() << '\n';

    std::size_t total_samples = 0;
    for (const auto& trajectory : dataset.trajectories) {
        total_samples += trajectory.samples.size();
    }

    std::cout << "Samples: " << total_samples << '\n';
    std::cout << "Axis frame: " << dataset.axis_frame.derivation_method
              << " | confidence=" << dataset.axis_frame.confidence << '\n';

    if (!focus.valid) {
        std::cout << "Focus detection failed.\n";
        return;
    }

    std::cout << "Metric: " << focus.curve.metric_name << '\n';
    std::cout << "Focus index: " << focus.focus_index << '\n';
    std::cout << "Focus axial position: "
              << std::scientific << std::setprecision(6)
              << focus.focus_reference_value << " m\n";
    std::cout << "Minimum metric value: "
              << std::scientific << std::setprecision(6)
              << focus.focus_metric_value << " m\n";
    std::cout << "Confidence proxy: "
              << std::scientific << std::setprecision(6)
              << focus.confidence << '\n';
}

} // namespace

int ApplicationBootstrap::run(int argc, char** argv)
{
    try {
        const auto options = CommandLineParser::parse(argc, argv);

        if (options.show_help) {
            const std::string executable_name = argc > 0 ? argv[0] : "beamlab";
            CommandLineParser::printHelp(executable_name);
            return 0;
        }

        const auto manifest = buildManifest(options);
        const auto analysis_configuration = buildAnalysisRunConfiguration(options);
        createOutputDirectories(manifest);

        std::cout << "BeamLabStudio bootstrap OK\n";
        std::cout << "Output directory: " << manifest.output_root << '\n';

        beamlab::data::TrajectoryDataset dataset{};

        if (!options.input_file.empty()) {
            dataset = loadDatasetFromFile(options.input_file);
        } else {
            std::cout << "No input file provided. Using synthetic focusing dataset.\n";
            dataset = makeSyntheticFocusingDataset();
        }

        const beamlab::io::DatasetNormalizer normalizer{};
        dataset = normalizer.normalize(std::move(dataset));

        applyManualAxisMode(dataset, options.axis_mode);

        const auto frame_statistics_parameters = makeFrameStatisticsParameters(options);
        auto geometry_statistics_parameters = frame_statistics_parameters;
        geometry_statistics_parameters.reference_mode = beamlab::analysis::ReferenceMode::AxialBins;

        std::cout << "Axis mode: " << options.axis_mode << '\n';
        std::cout << "Reference mode: " << options.reference_mode << '\n';
        std::cout << "Binning mode: " << options.binning_mode << '\n';
        std::cout << "Axial bins: " << options.axial_bin_count << '\n';
        std::cout << "Geometry focus mode: axial-bins minimum transverse radius\n";

        const beamlab::analysis::FrameStatisticsEngine statistics_engine{};
        const auto frame_statistics =
            statistics_engine.compute(dataset, geometry_statistics_parameters);

        const beamlab::analysis::FocusParameters focus_parameters{};
        const beamlab::analysis::FocusCurveBuilder curve_builder{};
        const auto curve = curve_builder.build(frame_statistics, focus_parameters);

        const beamlab::analysis::FocusDetector focus_detector{};
        auto focus = focus_detector.detect(curve, focus_parameters);

        beamlab::analysis::SliceParameters slice_parameters{};
        slice_parameters.half_window_frames = options.half_window_frames;
        slice_parameters.max_slices = 2 * options.half_window_frames + 1;

        const beamlab::analysis::SlicePlanner slice_planner{};

        const beamlab::analysis::SliceExtractor slice_extractor{};
        const beamlab::analysis::ConvexHullEnvelopeExtractor envelope_extractor{};
        const beamlab::analysis::EnvelopeParameters envelope_parameters{};

        // Extract envelopes for all axial bins so the proxy surface, focal
        // slice and effective lens all use the same spatial reference.
        const std::size_t total_frames =
            focus.valid && !focus.curve.points.empty() ? focus.curve.points.size() : 0;

        std::vector<beamlab::data::BeamEnvelope> envelopes{};
        envelopes.reserve(total_frames);

        for (std::size_t frame_index = 0; frame_index < total_frames; ++frame_index) {
            const auto slice = slice_extractor.extractFrameSlice(dataset, focus.curve, frame_index);
            const auto envelope = envelope_extractor.extract(slice, envelope_parameters);

            envelopes.push_back(envelope);
        }

        if (!envelopes.empty() && !focus.curve.points.empty()) {
            auto envelope_curve = focus.curve;
            envelope_curve.metric_name = "equivalent_envelope_radius";

            for (auto& point : envelope_curve.points) {
                point.metric_value = std::numeric_limits<double>::infinity();
                point.point_count = 0;
            }

            for (const auto& envelope : envelopes) {
                if (envelope.slice_index >= envelope_curve.points.size()) {
                    continue;
                }

                auto& point = envelope_curve.points[envelope.slice_index];
                point.reference_value = envelope.axial_position_m;
                point.metric_value = equivalentEnvelopeRadius(envelope);
                point.point_count = envelope.input_point_count;
            }

            const auto envelope_focus = focus_detector.detect(envelope_curve, focus_parameters);

            if (envelope_focus.valid && std::isfinite(envelope_focus.focus_metric_value)) {
                focus = envelope_focus;
            }
        }

        printFocusSummary(dataset, focus);

        const auto focal_window_indices = slice_planner.planFrameSlices(focus, slice_parameters);

        std::cout << "\n=== Focal slice window + envelopes ===\n";
        std::cout << "slice_index,reference_axial_m,axial_position_m,points,boundary_points,area_m2,perimeter_m\n";

        for (const auto frame_index : focal_window_indices) {
            if (frame_index >= envelopes.size()) {
                continue;
            }

            const auto& envelope = envelopes[frame_index];

            std::cout << std::scientific << std::setprecision(6)
                      << envelope.slice_index << ','
                      << envelope.reference_time_s << ','
                      << envelope.axial_position_m << ','
                      << envelope.input_point_count << ','
                      << envelope.boundary_points.size() << ','
                      << envelope.area_m2 << ','
                      << envelope.perimeter_m << '\n';
        }

        beamlab::analysis::SurfaceBuildParameters caustic_parameters{};
        caustic_parameters.resample_point_count = options.caustic_resample_points;

        const beamlab::analysis::SurfaceBuilder caustic_builder{};
        const auto caustic_surface =
            caustic_builder.build(envelopes, dataset.axis_frame, caustic_parameters);

        auto focal_envelope_it = std::find_if(
            envelopes.begin(),
            envelopes.end(),
            [&focus](const auto& envelope) {
                return envelope.slice_index == focus.focus_index;
            }
        );

        if (focal_envelope_it == envelopes.end() && !envelopes.empty()) {
            focal_envelope_it = std::min_element(
                envelopes.begin(),
                envelopes.end(),
                [&focus](const auto& a, const auto& b) {
                    const auto da = a.slice_index > focus.focus_index
                        ? a.slice_index - focus.focus_index
                        : focus.focus_index - a.slice_index;

                    const auto db = b.slice_index > focus.focus_index
                        ? b.slice_index - focus.focus_index
                        : focus.focus_index - b.slice_index;

                    return da < db;
                }
            );
        }

        beamlab::data::LensSurfaceModel lens_disk{};

        beamlab::analysis::LensDiskBuildParameters lens_parameters{};
        lens_parameters.boundary_point_count = options.lens_boundary_points;
        lens_parameters.radial_layers = options.lens_radial_layers;
        lens_parameters.center_thickness_m = options.lens_center_thickness_m;
        lens_parameters.edge_thickness_m = options.lens_edge_thickness_m;

        if (focal_envelope_it != envelopes.end()) {
            const beamlab::analysis::LensDiskBuilder lens_builder{};
            lens_disk = lens_builder.build(*focal_envelope_it, dataset.axis_frame, lens_parameters);
        }

        std::cout << "\n=== Generated surfaces ===\n";
        std::cout << "beam_caustic_surface: valid=" << (caustic_surface.valid ? "true" : "false")
                  << ", vertices=" << caustic_surface.mesh.vertices.size()
                  << ", faces=" << caustic_surface.mesh.faces.size() << '\n';
        std::cout << "effective_lens_disk: valid=" << (lens_disk.valid ? "true" : "false")
                  << ", vertices=" << lens_disk.mesh.vertices.size()
                  << ", faces=" << lens_disk.mesh.faces.size() << '\n';

        const auto printExportStatus = [](const std::string& label,
                                          const beamlab::io::ExportResult& result) {
            if (result.success) {
                std::cout << "exported_" << label << ": "
                          << result.output_path << '\n';
                return;
            }

            std::cout << "export_failed_" << label << ": ";

            if (result.error.has_value()) {
                std::cout << result.error->message
                          << " | " << result.error->details << '\n';
            } else {
                std::cout << "unknown export error\n";
            }
        };

        const beamlab::analysis::AnalysisQualityChecker quality_checker{};
        const auto quality_report = quality_checker.check(
            dataset,
            focus,
            envelopes,
            caustic_surface,
            lens_disk
        );

        std::cout << "\n=== Quality checks ===\n";
        std::cout << "verdict: " << quality_report.verdict << '\n';
        std::cout << "info: " << quality_report.info_count
                  << ", warnings: " << quality_report.warning_count
                  << ", errors: " << quality_report.error_count
                  << ", critical: " << quality_report.critical_count << '\n';

        const beamlab::io::VisualizationDataExporter visualization_exporter{};

        const auto trajectories_preview_result =
            visualization_exporter.exportTrajectoryPreview(
                dataset,
                manifest.trajectories_preview_csv,
                options.preview_trajectory_count,
                options.preview_samples_per_trajectory
            );

        printExportStatus("trajectories_preview_csv", trajectories_preview_result);

        const auto trajectories_obj_result =
            visualization_exporter.exportTrajectoryPolylines(
                dataset,
                manifest.trajectories_preview_obj,
                options.preview_trajectory_count,
                options.preview_samples_per_trajectory
            );

        printExportStatus("trajectories_preview_obj", trajectories_obj_result);

        if (focal_envelope_it != envelopes.end()) {
            const auto focal_slice_for_visualization =
                slice_extractor.extractFrameSlice(
                    dataset,
                    focus.curve,
                    focal_envelope_it->slice_index
                );

            const auto focal_slice_preview_result =
                visualization_exporter.exportFocalSlicePoints(
                    focal_slice_for_visualization,
                    manifest.focal_slice_points_csv,
                    options.preview_slice_point_count
                );

            printExportStatus("focal_slice_points_csv", focal_slice_preview_result);
        }

        const auto envelope_rings_result =
            visualization_exporter.exportEnvelopeRings(
                envelopes,
                dataset.axis_frame,
                manifest.envelope_rings_csv
            );

        printExportStatus("envelope_rings_csv", envelope_rings_result);

        beamlab::io::VisualizationManifestStats visualization_stats{};
        visualization_stats.trajectory_preview_rows =
            options.preview_trajectory_count * options.preview_samples_per_trajectory;

        if (focal_envelope_it != envelopes.end()) {
            visualization_stats.focal_slice_point_rows =
                std::min(
                    focal_envelope_it->input_point_count,
                    options.preview_slice_point_count
                );
        }

        for (const auto& envelope : envelopes) {
            if (envelope.valid) {
                visualization_stats.envelope_ring_rows += envelope.boundary_points.size();
            }
        }

        const beamlab::io::VisualizationManifestExporter visualization_manifest_exporter{};
        const auto visualization_manifest_result =
            visualization_manifest_exporter.exportManifest(
                manifest,
                analysis_configuration,
                visualization_stats,
                manifest.visualization_manifest_json
            );

        printExportStatus("visualization_manifest_json", visualization_manifest_result);

        const beamlab::io::MeshExporter mesh_exporter{};

        if (caustic_surface.valid) {
            const auto caustic_obj_result = mesh_exporter.exportObj(
                caustic_surface.mesh,
                manifest.beam_caustic_obj
            );

            const auto caustic_preview_result = mesh_exporter.exportObjPreview(
                caustic_surface.mesh,
                manifest.beam_caustic_preview_obj,
                options.caustic_preview_scale
            );

            printExportStatus("beam_caustic_obj", caustic_obj_result);
            printExportStatus("beam_caustic_preview_obj", caustic_preview_result);
        }

        if (lens_disk.valid) {
            const auto lens_obj_result = mesh_exporter.exportObj(
                lens_disk.mesh,
                manifest.effective_lens_disk_obj
            );

            const auto lens_preview_result = mesh_exporter.exportObjPreview(
                lens_disk.mesh,
                manifest.effective_lens_disk_preview_obj,
                options.lens_preview_scale
            );

            printExportStatus("effective_lens_disk_obj", lens_obj_result);
            printExportStatus("effective_lens_disk_preview_obj", lens_preview_result);
        }

        const beamlab::io::ParametricSurfaceExporter parametric_exporter{};

        if (caustic_surface.valid) {
            const auto caustic_param_result =
                parametric_exporter.exportCausticParametricDescription(
                    envelopes,
                    dataset.axis_frame,
                    caustic_parameters,
                    manifest.beam_caustic_parametric_txt,
                    manifest.beam_caustic_parametric_csv
                );

            printExportStatus("beam_caustic_parametric", caustic_param_result);
        }

        if (focal_envelope_it != envelopes.end()) {
            const auto lens_param_result =
                parametric_exporter.exportLensDiskParametricDescription(
                    *focal_envelope_it,
                    dataset.axis_frame,
                    lens_parameters,
                    manifest.effective_lens_disk_parametric_txt,
                    manifest.effective_lens_disk_parametric_csv
                );

            printExportStatus("effective_lens_disk_parametric", lens_param_result);
        }

        const beamlab::io::AnalysisReportExporter report_exporter{};

        const auto focus_curve_result = report_exporter.exportFocusCurve(
            focus,
            manifest.focus_curve_csv
        );

        const auto envelope_summary_result = report_exporter.exportEnvelopeSummary(
            envelopes,
            manifest.envelope_summary_csv
        );

        const auto metadata_result = report_exporter.exportRunMetadata(
            dataset,
            focus,
            envelopes,
            caustic_surface,
            lens_disk,
            analysis_configuration,
            manifest
        );

        const auto markdown_summary_result = report_exporter.exportMarkdownSummary(
            dataset,
            focus,
            envelopes,
            caustic_surface,
            lens_disk,
            analysis_configuration,
            manifest
        );

        printExportStatus("focus_curve_csv", focus_curve_result);
        printExportStatus("envelope_summary_csv", envelope_summary_result);
        printExportStatus("run_metadata_json", metadata_result);
        printExportStatus("analysis_summary_md", markdown_summary_result);

        const beamlab::io::QualityReportExporter quality_exporter{};

        const auto quality_csv_result = quality_exporter.exportCsv(
            quality_report,
            manifest.quality_report_csv
        );

        const auto quality_md_result = quality_exporter.exportMarkdown(
            quality_report,
            manifest.quality_report_md
        );

        printExportStatus("quality_report_csv", quality_csv_result);
        printExportStatus("quality_report_md", quality_md_result);

        // ── Energy profile & scoring planes (Geant4 data) ──────────────────
        const std::filesystem::path energy_dir(manifest.output_root);
        const beamlab::io::EnergyProfileExporter energy_exporter{};

        if (options.export_energy_profile) {
            const auto step_profile_path =
                pathString(energy_dir / "tables" / "energy_step_profile.csv");
            const auto step_result =
                energy_exporter.exportStepProfile(dataset, step_profile_path);
            printExportStatus("energy_step_profile_csv", step_result);
        }

        if (options.export_track_summary) {
            const auto track_summary_path =
                pathString(energy_dir / "tables" / "energy_track_summary.csv");
            const auto track_result =
                energy_exporter.exportTrackSummary(dataset, track_summary_path);
            printExportStatus("energy_track_summary_csv", track_result);
        }

        if (options.detect_scoring_planes) {
            beamlab::biosim::ScoringPlaneDetector sp_detector{};
            const auto scoring_planes = sp_detector.detect(dataset);

            const auto sp_path =
                pathString(energy_dir / "tables" / "scoring_planes.csv");
            const auto sp_result =
                energy_exporter.exportScoringPlanes(dataset, scoring_planes, sp_path);
            printExportStatus("scoring_planes_csv", sp_result);

            std::cout << "\n=== Detected scoring planes ===\n";
            for (const auto& plane : scoring_planes) {
                const char* role = "counter";
                if (plane.role == beamlab::biosim::ScoringPlane::Role::Entry) role = "entry";
                if (plane.role == beamlab::biosim::ScoringPlane::Role::Exit)  role = "exit";
                std::cout << plane.id << "  role=" << role
                          << "  axial=" << std::scientific << std::setprecision(4)
                          << plane.axial_position_m << " m\n";
            }
        }

        std::cout << "\n=== Output files ===\n";
        std::cout << "summary: " << manifest.analysis_summary_md << '\n';
        std::cout << "metadata: " << manifest.run_metadata_json << '\n';
        std::cout << "quality report: " << manifest.quality_report_md << '\n';
        std::cout << "visualization: " << manifest.trajectories_preview_csv << '\n';
        std::cout << "visualization manifest: " << manifest.visualization_manifest_json << '\n';
        std::cout << "focus curve: " << manifest.focus_curve_csv << '\n';
        std::cout << "envelope summary: " << manifest.envelope_summary_csv << '\n';
        std::cout << "caustic OBJ: " << manifest.beam_caustic_obj << '\n';
        std::cout << "lens disk OBJ: " << manifest.effective_lens_disk_obj << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }
}

} // namespace beamlab::app
