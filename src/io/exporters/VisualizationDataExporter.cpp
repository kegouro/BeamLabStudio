#include "io/exporters/VisualizationDataExporter.h"

#include "core/foundation/Error.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <vector>

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

beamlab::core::Vec3 add(const beamlab::core::Vec3& a,
                        const beamlab::core::Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

beamlab::core::Vec3 scale(const beamlab::core::Vec3& v,
                          const double factor)
{
    return {v.x * factor, v.y * factor, v.z * factor};
}

beamlab::core::Vec3 envelopePointToWorld(const beamlab::core::Vec2& uv,
                                         const double axial_position_m,
                                         const beamlab::data::AxisFrame& axis_frame)
{
    auto position = axis_frame.origin;
    position = add(position, scale(axis_frame.longitudinal, axial_position_m));
    position = add(position, scale(axis_frame.transverse_u, uv.x));
    position = add(position, scale(axis_frame.transverse_v, uv.y));
    return position;
}

std::vector<std::size_t> evenlySpacedIndices(const std::size_t count,
                                             const std::size_t max_count)
{
    std::vector<std::size_t> indices{};

    if (count == 0 || max_count == 0) {
        return indices;
    }

    const std::size_t selected_count = std::min(count, max_count);
    indices.reserve(selected_count);

    if (selected_count == 1) {
        indices.push_back(0);
        return indices;
    }

    for (std::size_t i = 0; i < selected_count; ++i) {
        indices.push_back(i * (count - 1) / (selected_count - 1));
    }

    return indices;
}

} // namespace

std::string VisualizationDataExporter::name() const
{
    return "VisualizationDataExporter";
}

ExportResult VisualizationDataExporter::exportTrajectoryPreview(
    const beamlab::data::TrajectoryDataset& dataset,
    const std::string& output_path,
    const std::size_t max_trajectories,
    const std::size_t max_samples_per_trajectory) const
{
    ExportResult result{};

    if (dataset.trajectories.empty()) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::InvalidArgument,
            "No hay trayectorias para exportar preview",
            output_path
        );
        return result;
    }

    ensureParentDirectory(output_path);
    std::ofstream output(output_path);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear trajectories_preview.csv",
            output_path
        );
        return result;
    }

    output << "# BeamLabStudio visualization trajectory preview\n";
    output << "# This file is downsampled for UI use. It is not the full dataset.\n";
    output << "trajectory_index,sample_index,time_s,x_m,y_m,z_m\n";

    output << std::scientific << std::setprecision(12);

    const auto trajectory_indices = evenlySpacedIndices(
        dataset.trajectories.size(),
        max_trajectories
    );

    for (const auto trajectory_index : trajectory_indices) {
        const auto& trajectory = dataset.trajectories[trajectory_index];

        const auto sample_indices = evenlySpacedIndices(
            trajectory.samples.size(),
            max_samples_per_trajectory
        );

        for (const auto sample_index : sample_indices) {
            const auto& sample = trajectory.samples[sample_index];

            output << trajectory_index << ','
                   << sample_index << ','
                   << sample.time_s << ','
                   << sample.position_m.x << ','
                   << sample.position_m.y << ','
                   << sample.position_m.z << '\n';
        }
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

ExportResult VisualizationDataExporter::exportFocalSlicePoints(
    const beamlab::data::BeamSlice& slice,
    const std::string& output_path,
    const std::size_t max_points) const
{
    ExportResult result{};

    if (slice.points.empty()) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::InvalidArgument,
            "No hay puntos de slice focal para exportar",
            output_path
        );
        return result;
    }

    ensureParentDirectory(output_path);
    std::ofstream output(output_path);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear focal_slice_points.csv",
            output_path
        );
        return result;
    }

    output << "# BeamLabStudio focal slice point preview\n";
    output << "# This file may be downsampled for UI use.\n";
    output << "point_index,trajectory_index,sample_index,axial_position_m,u_m,v_m,x_m,y_m,z_m\n";

    output << std::scientific << std::setprecision(12);

    const auto point_indices = evenlySpacedIndices(slice.points.size(), max_points);

    for (const auto point_index : point_indices) {
        const auto& point = slice.points[point_index];

        output << point_index << ','
               << point.trajectory_index << ','
               << point.sample_index << ','
               << slice.axial_position_m << ','
               << point.projected_uv_m.x << ','
               << point.projected_uv_m.y << ','
               << point.position_m.x << ','
               << point.position_m.y << ','
               << point.position_m.z << '\n';
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

ExportResult VisualizationDataExporter::exportEnvelopeRings(
    const std::vector<beamlab::data::BeamEnvelope>& envelopes,
    const beamlab::data::AxisFrame& axis_frame,
    const std::string& output_path) const
{
    ExportResult result{};

    if (envelopes.empty()) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::InvalidArgument,
            "No hay contornos para exportar",
            output_path
        );
        return result;
    }

    ensureParentDirectory(output_path);
    std::ofstream output(output_path);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear envelope_rings.csv",
            output_path
        );
        return result;
    }

    output << "# BeamLabStudio envelope rings for UI visualization\n";
    output << "slice_index,ring_point_index,reference_axial_m,axial_position_m,u_m,v_m,x_m,y_m,z_m\n";

    output << std::scientific << std::setprecision(12);

    for (const auto& envelope : envelopes) {
        if (!envelope.valid) {
            continue;
        }

        for (std::size_t i = 0; i < envelope.boundary_points.size(); ++i) {
            const auto& uv = envelope.boundary_points[i];
            const auto world = envelopePointToWorld(
                uv,
                envelope.axial_position_m,
                axis_frame
            );

            output << envelope.slice_index << ','
                   << i << ','
                   << envelope.reference_time_s << ','
                   << envelope.axial_position_m << ','
                   << uv.x << ','
                   << uv.y << ','
                   << world.x << ','
                   << world.y << ','
                   << world.z << '\n';
        }
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

} // namespace beamlab::io
