#pragma once

#include "data/model/TrajectorySample.h"
#include "data/ids/TrajectoryId.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace beamlab::test {

/// Generate a Geant4-format CSV with `nTrajectories` × `nSamplesPerTrajectory`.
inline std::string createSyntheticCSV(int nTrajectories,
                                       int nSamplesPerTrajectory,
                                       const std::string& path = "")
{
    auto csvPath = path;
    if (csvPath.empty()) {
        csvPath = (std::filesystem::temp_directory_path() /
                   "beamlab_test_XXXXXX.csv").string();
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto pos = csvPath.find("XXXXXX");
        if (pos != std::string::npos)
            csvPath.replace(pos, 6, std::to_string(now));
    }

    std::ofstream out(csvPath);
    // Geant4 15-column header.
    out << "x_cm,y_cm,z_cm,edep_MeV,kinE_MeV,"
           "momx_MeV,momy_MeV,momz_MeV,time_ns,"
           "trackID,parentID,eventID,pdg,particleName,source_file\n";

    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> pos(-5.0, 5.0);

    for (int t = 0; t < nTrajectories; ++t) {
        for (int s = 0; s < nSamplesPerTrajectory; ++s) {
            double z = static_cast<double>(s) /
                       static_cast<double>(nSamplesPerTrajectory - 1) * 30.0;
            out << pos(rng) << ","                      // x_cm
                << pos(rng) << ","                      // y_cm
                << (z * 100.0) << ","                  // z_cm → cm
                << "0.010,"                            // edep_MeV
                << "100.0,"                            // kinE_MeV
                << "10.0,0.0,0.0,"                    // momentum
                << (s * 0.1) << ","                   // time_ns
                << (t + 1) << ",0,1,13,mu-,simulated\n";  // trackID, ...
        }
    }
    out.close();
    return csvPath;
}

/// Generate samples with a known gaussian focus at z=z0.
inline std::string createFocusingCSV(int nTrajectories,
                                      int nSamplesPerTrajectory,
                                      double waist_mm = 0.5,
                                      double focusZ_m = 0.15)
{
    auto csvPath = createSyntheticCSV(nTrajectories, nSamplesPerTrajectory);
    // TODO: overwrite with gaussian beam distribution.
    return csvPath;
}

/// RAII temp directory cleanup.
struct TempDir {
    std::filesystem::path path;

    TempDir() {
        auto base = std::filesystem::temp_directory_path();
        auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
        path = base / ("beamlab_int_" + std::to_string(ts));
        std::filesystem::create_directories(path);
    }

    ~TempDir() {
        if (!path.empty())
            std::filesystem::remove_all(path);
    }

    std::string string() const { return path.string(); }
};

/// Equality comparison for TrajectorySample (for tests).
inline bool operator==(const beamlab::data::TrajectorySample& a,
                       const beamlab::data::TrajectorySample& b)
{
    return a.trajectory_id == b.trajectory_id
        && a.position_m.x == b.position_m.x
        && a.position_m.y == b.position_m.y
        && a.position_m.z == b.position_m.z
        && a.edep_MeV == b.edep_MeV;
}

} // namespace beamlab::test
