#include "io/exporters/EnergyProfileExporter.h"

#include "core/foundation/Error.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>

namespace beamlab::io {

namespace {

void ensureParent(const std::string& path)
{
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

beamlab::core::Error ioError(const std::string& msg, const std::string& path)
{
    return beamlab::core::Error{
        .code = beamlab::core::StatusCode::IOError,
        .severity = beamlab::core::Severity::Error,
        .message = msg,
        .details = path
    };
}

double axialCoord(const beamlab::core::Vec3& pos,
                  const beamlab::data::AxisFrame& frame)
{
    const double rx = pos.x - frame.origin.x;
    const double ry = pos.y - frame.origin.y;
    const double rz = pos.z - frame.origin.z;
    return rx * frame.longitudinal.x +
           ry * frame.longitudinal.y +
           rz * frame.longitudinal.z;
}

} // namespace

ExportResult EnergyProfileExporter::exportStepProfile(
    const beamlab::data::TrajectoryDataset& dataset,
    const std::string& output_path) const
{
    ExportResult result{};
    ensureParent(output_path);

    std::ofstream out(output_path);
    if (!out) {
        result.success = false;
        result.error = ioError("No se pudo crear energy_step_profile.csv", output_path);
        return result;
    }

    out << "# BeamLabStudio energy step profile\n";
    out << "# straggling_model: gaussian\n";
    out << "event_id,track_id,step_index,axial_m,"
           "edep_MeV,edep_eV,kinE_MeV,dose_Gy,x_m,y_m,z_m\n";

    out << std::scientific << std::setprecision(9);

    for (const auto& traj : dataset.trajectories) {
        const int event_id = traj.particle.event_id;
        const int track_id = traj.particle.track_id;

        for (std::size_t i = 0; i < traj.samples.size(); ++i) {
            const auto& s = traj.samples[i];
            const double axial = axialCoord(s.position_m, dataset.axis_frame);

            out << event_id << ','
                << track_id << ','
                << i << ','
                << axial << ','
                << s.edep_MeV << ','
                << s.edep_eV << ','
                << s.kinE_MeV << ','
                << s.dose_Gy << ','
                << s.position_m.x << ','
                << s.position_m.y << ','
                << s.position_m.z << '\n';
        }
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

ExportResult EnergyProfileExporter::exportTrackSummary(
    const beamlab::data::TrajectoryDataset& dataset,
    const std::string& output_path) const
{
    ExportResult result{};
    ensureParent(output_path);

    std::ofstream out(output_path);
    if (!out) {
        result.success = false;
        result.error = ioError("No se pudo crear energy_track_summary.csv", output_path);
        return result;
    }

    out << "# BeamLabStudio per-track energy summary\n";
    out << "event_id,track_id,n_steps,"
           "total_edep_MeV,total_edep_eV,"
           "entry_kinE_MeV,exit_kinE_MeV,"
           "total_dose_Gy,range_m\n";

    out << std::scientific << std::setprecision(9);

    for (const auto& traj : dataset.trajectories) {
        const auto& p = traj.particle;
        const auto& sum = traj.summary;

        out << p.event_id << ','
            << p.track_id << ','
            << sum.sample_count << ','
            << sum.total_edep_MeV << ','
            << sum.total_edep_eV << ','
            << sum.entry_kinE_MeV << ','
            << sum.exit_kinE_MeV << ','
            << sum.total_dose_Gy << ','
            << sum.range_m << '\n';
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

ExportResult EnergyProfileExporter::exportScoringPlanes(
    const beamlab::data::TrajectoryDataset& dataset,
    const std::vector<beamlab::biosim::ScoringPlane>& planes,
    const std::string& output_path) const
{
    ExportResult result{};
    ensureParent(output_path);

    std::ofstream out(output_path);
    if (!out) {
        result.success = false;
        result.error = ioError("No se pudo crear scoring_planes.csv", output_path);
        return result;
    }

    // Score each plane against the dataset
    auto scored = planes; // copy to fill in statistics
    for (auto& plane : scored) {
        if (!plane.enabled) {
            continue;
        }

        std::size_t crossings = 0;
        double sum_kinE = 0.0;
        double sum_edep = 0.0;

        for (const auto& traj : dataset.trajectories) {
            for (std::size_t i = 0; i + 1 < traj.samples.size(); ++i) {
                const double s0 = axialCoord(traj.samples[i].position_m,
                                             dataset.axis_frame);
                const double s1 = axialCoord(traj.samples[i + 1].position_m,
                                             dataset.axis_frame);
                const double p  = plane.axial_position_m;

                if ((s0 <= p && p < s1) || (s1 <= p && p < s0)) {
                    ++crossings;
                    sum_kinE += traj.samples[i].kinE_MeV;
                    sum_edep += traj.samples[i].edep_MeV;
                    break; // count each trajectory once
                }
            }
        }

        plane.crossing_count     = crossings;
        plane.mean_kinE_MeV      = crossings > 0
            ? sum_kinE / static_cast<double>(crossings) : 0.0;
        plane.mean_edep_step_MeV = crossings > 0
            ? sum_edep / static_cast<double>(crossings) : 0.0;
    }

    out << "# BeamLabStudio scoring plane report\n";
    out << "id,role,axial_position_m,enabled,crossing_count,"
           "mean_kinE_MeV,mean_edep_step_MeV\n";

    for (const auto& plane : scored) {
        const char* role_str = "counter";
        if (plane.role == beamlab::biosim::ScoringPlane::Role::Entry) {
            role_str = "entry";
        } else if (plane.role == beamlab::biosim::ScoringPlane::Role::Exit) {
            role_str = "exit";
        }

        out << std::scientific << std::setprecision(9)
            << plane.id << ','
            << role_str << ','
            << plane.axial_position_m << ','
            << (plane.enabled ? "true" : "false") << ','
            << plane.crossing_count << ','
            << plane.mean_kinE_MeV << ','
            << plane.mean_edep_step_MeV << '\n';
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}} // namespace beamlab::io
