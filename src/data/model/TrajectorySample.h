#pragma once

#include "core/math/Vec3.h"
#include "data/ids/SampleId.h"
#include "data/ids/TrajectoryId.h"
#include "data/model/SampleFlags.h"

namespace beamlab::data {

struct TrajectorySample {
    SampleId id{};
    TrajectoryId trajectory_id{};
    double time_s{0.0};
    beamlab::core::Vec3 position_m{};
    SampleFlags flags{SampleFlags::None};

    // Physics fields (populated by physics-aware importers such as Geant4CsvImporter)
    double edep_MeV{0.0};
    double edep_eV{0.0};     // edep_MeV * 1e6
    double kinE_MeV{0.0};
    beamlab::core::Vec3 momentum_MeV{};  // (momx, momy, momz) in MeV/c

    // Dose per step in Gy; only meaningful when a material density is known.
    // Populated by the tissue simulation subsystem or set to 0 by default.
    double dose_Gy{0.0};
};

} // namespace beamlab::data
