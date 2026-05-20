#pragma once

#include "data/model/TrajectoryDataset.h"
#include "io/common/ExportResult.h"
#include "biosim/core/ScoringPlane.h"
#include "simulation/tissue/TissueSlab.h"

#include <string>
#include <vector>

namespace beamlab::io {

// Exports per-step and per-track energy deposition profiles to CSV.
class EnergyProfileExporter {
public:
    // Full step-by-step energy deposition for all trajectories.
    // Columns: event_id, track_id, step_index, axial_m, edep_MeV, edep_eV,
    //          kinE_MeV, dose_Gy, x_m, y_m, z_m
    [[nodiscard]] ExportResult exportStepProfile(
        const beamlab::data::TrajectoryDataset& dataset,
        const std::string& output_path) const;

    // Per-trajectory summary (total edep, entry/exit kinE, range).
    // Columns: event_id, track_id, n_steps, total_edep_MeV, total_edep_eV,
    //          entry_kinE_MeV, exit_kinE_MeV, total_dose_Gy, range_m
    [[nodiscard]] ExportResult exportTrackSummary(
        const beamlab::data::TrajectoryDataset& dataset,
        const std::string& output_path) const;

    // Scoring plane report: crossing count and mean energy at each plane.
    [[nodiscard]] ExportResult exportScoringPlanes(
        const beamlab::data::TrajectoryDataset& dataset,
        const std::vector<beamlab::biosim::ScoringPlane>& planes,
        const std::string& output_path) const;

    // Tissue slab dose summary.
    [[nodiscard]] ExportResult exportSlabDose(
        const std::vector<beamlab::simulation::TissueSlab>& slabs,
        const beamlab::data::TrajectoryDataset& dataset,
        const std::string& output_path) const;
};

} // namespace beamlab::io
