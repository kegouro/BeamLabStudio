#include "simulation/physics/MuonTrackSimulator.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace beamlab::simulation {

namespace {

// Project a world position onto the longitudinal axis of the dataset.
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

// Find which slab (if any) contains axial position `s`.
const TissueSlab* findSlab(const std::vector<TissueSlab>& slabs, const double s)
{
    for (const auto& slab : slabs) {
        if (!slab.enabled) {
            continue;
        }
        if (s >= slab.axial_start_m && s < slab.axialEnd()) {
            return &slab;
        }
    }
    return nullptr;
}

} // namespace

SimulationResult MuonTrackSimulator::simulate(
    const beamlab::data::TrajectoryDataset& dataset,
    const std::vector<TissueSlab>& slabs) const
{
    SimulationResult result{};

    if (slabs.empty()) {
        result.dataset = dataset; // deep copy — no slabs, nothing changes
        result.valid = true;
        result.message = "No slabs configured; dataset passed through unchanged.";
        return result;
    }

    result.dataset = dataset; // working copy

    // Build per-slab statistics accumulators
    std::unordered_map<std::string, SimulationResult::SlabStats> stats_map;
    for (const auto& slab : slabs) {
        if (slab.enabled) {
            stats_map[slab.id].slab_id = slab.id;
        }
    }

    for (auto& trajectory : result.dataset.trajectories) {
        if (trajectory.samples.size() < 2) {
            continue;
        }

        const double mass_MeV   = trajectory.particle.mass_MeV;
        const double charge     = trajectory.particle.charge;

        // Propagate kinetic energy along the track
        double current_kinE = trajectory.samples.front().kinE_MeV;

        for (std::size_t i = 0; i < trajectory.samples.size(); ++i) {
            auto& sample = trajectory.samples[i];

            // Update running kinetic energy from the importer value when available
            // (non-zero kinE from Geant4 takes precedence until we override it)
            if (i == 0) {
                current_kinE = sample.kinE_MeV;
            }

            if (current_kinE <= 0.0) {
                // Particle stopped
                sample.edep_MeV = 0.0;
                sample.edep_eV  = 0.0;
                sample.dose_Gy  = 0.0;
                continue;
            }

            // Step length from this sample to the next (m), projected along track
            double step_m = 0.0;
            if (i + 1 < trajectory.samples.size()) {
                const auto& next = trajectory.samples[i + 1];
                const double dx = next.position_m.x - sample.position_m.x;
                const double dy = next.position_m.y - sample.position_m.y;
                const double dz = next.position_m.z - sample.position_m.z;
                step_m = std::sqrt(dx*dx + dy*dy + dz*dz);
            }

            const double axial_s = axialCoord(sample.position_m, dataset.axis_frame);
            const TissueSlab* slab = findSlab(slabs, axial_s);

            if (slab == nullptr) {
                // Vacuum / no material — keep original edep from Geant4 if non-zero,
                // otherwise zero it out (pure simulation mode)
                // In hybrid mode we keep Geant4 values outside slabs.
                continue;
            }

            // Inside a slab: recompute energy loss using Bethe-Bloch
            const double step_cm = step_m * 100.0;
            double eloss_MeV = physics_.energyLoss_MeV(
                current_kinE, mass_MeV, charge, slab->material, step_cm);

            // Energy-conservation guard: BetheBloch's std::min already caps
            // eloss at current_kinE, but we still defend against NaN/negative
            // values that could appear if the material has degenerate
            // parameters or if a future refactor changes the engine.
            if (!std::isfinite(eloss_MeV) || eloss_MeV < 0.0) {
                eloss_MeV = 0.0;
            }

            sample.edep_MeV = eloss_MeV;
            sample.edep_eV  = eloss_MeV * 1.0e6;

            // Dose for this step (1 cm² cross-section convention)
            sample.dose_Gy = physics_.dosePerStep_Gy(eloss_MeV, slab->material, step_cm);

            current_kinE = std::max(0.0, current_kinE - eloss_MeV);
            sample.kinE_MeV = current_kinE;

            // Accumulate slab stats
            auto& st = stats_map[slab->id];
            st.total_edep_MeV += eloss_MeV;
        }

        // Count crossings per slab (trajectory that has any sample in the slab)
        for (const auto& sample : trajectory.samples) {
            const double axial_s = axialCoord(sample.position_m, dataset.axis_frame);
            const TissueSlab* slab = findSlab(slabs, axial_s);
            if (slab != nullptr) {
                stats_map[slab->id].tracks_crossing++;
                break; // count each trajectory once per slab (approximate)
            }
        }

        // Update trajectory summary physics
        double total_edep = 0.0;
        for (const auto& s : trajectory.samples) {
            total_edep += s.edep_MeV;
        }
        trajectory.summary.total_edep_MeV = total_edep;
        trajectory.summary.total_edep_eV  = total_edep * 1.0e6;
    }

    // Finalize slab stats
    for (auto& [id, st] : stats_map) {
        if (st.tracks_crossing > 0) {
            st.mean_edep_per_track_MeV =
                st.total_edep_MeV / static_cast<double>(st.tracks_crossing);
        }
        result.slab_stats.push_back(st);
    }

    result.valid   = true;
    result.message = "Simulation complete with " +
                     std::to_string(slabs.size()) + " tissue slab(s).";

    return result;
}

} // namespace beamlab::simulation
