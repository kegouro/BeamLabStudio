#pragma once

#include "core/math/AABB.h"

namespace beamlab::data {

struct TrajectorySummary {
    double time_min_s{0.0};
    double time_max_s{0.0};
    beamlab::core::AABB bounds{};
    std::size_t sample_count{0};

    // Physics summary (populated during normalization for Geant4 data)
    double total_edep_MeV{0.0};
    double total_edep_eV{0.0};
    double total_dose_Gy{0.0};   // summed over all steps; requires tissue context
    double entry_kinE_MeV{0.0};  // kinE at first sample
    double exit_kinE_MeV{0.0};   // kinE at last sample
    double range_m{0.0};         // path length computed from consecutive positions
};

} // namespace beamlab::data
