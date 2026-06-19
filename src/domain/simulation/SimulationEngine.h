#pragma once

#include "domain/materials/Material.h"
#include "domain/materials/MaterialRegistry.h"
#include "domain/physics/Particle.h"
#include "domain/physics/ParticleRegistry.h"

#include <string>
#include <utility>
#include <vector>

namespace beamlab::domain::simulation {

/// Unified facade for medical-physics calculations.
///
/// Hides the complexity of StoppingPower, Straggling, Multiple Coulomb
/// Scattering, and Bragg peak computation behind a simple API that
/// operates on material and particle names (resolved from the
/// MaterialRegistry and ParticleRegistry).
class SimulationEngine {
public:
    SimulationEngine(const materials::MaterialRegistry& materials,
                     const physics::ParticleRegistry& particles);

    // ── Single step ────────────────────────────────────────────────

    struct StepResult {
        double dEdx_MeV_cm{0.0};
        double energyLoss_MeV{0.0};
        double mcsAngle_rad{0.0};
        double mcsDisplacement_cm{0.0};
        double stragglingSigma_MeV{0.0};
    };

    /// Compute all physics quantities for one step.
    ///
    /// \throws std::invalid_argument if materialName or particleName
    ///         is not found in the registries.
    StepResult computeStep(
        double kinE_MeV,
        double stepLength_cm,
        const std::string& materialName,
        const std::string& particleName) const;

    // ── Bragg curve ────────────────────────────────────────────────

    struct BraggCurve {
        std::vector<double> depth_cm;
        std::vector<double> dEdx_MeV_cm;
        double peakDepth_cm{0.0};
        double peakDEdx_MeV_cm{0.0};
    };

    /// Integrates computeStep() iteratively until kinetic energy
    /// drops below 0.01 MeV or the model leaves its validity range.
    /// nSteps is a safety cap: at 10 µm per step the default covers
    /// 1 m of material, beyond any realistic range at these energies.
    BraggCurve computeBraggCurve(
        double initialE_MeV,
        const std::string& materialName,
        const std::string& particleName,
        std::size_t nSteps = 100000) const;

    // ── Validation ─────────────────────────────────────────────────

    struct ValidationReport {
        bool passed{false};
        std::vector<std::pair<std::string, double>> deviations;
        std::string referenceSource;
    };

    /// Compare dE/dx at 150 MeV against NIST PSTAR reference value.
    /// Pass threshold: ±2%.
    ValidationReport validateAgainstNist(
        const std::string& materialName,
        const std::string& particleName) const;

private:
    const materials::MaterialRegistry& materials_;
    const physics::ParticleRegistry& particles_;

    // ── Physics helpers ────────────────────────────────────────────

    double betheBloch_dEdx_MeV_cm(
        double kinE_MeV,
        const physics::Particle& particle,
        const materials::Material& material) const;

    double sternheimerDelta(double log10_bg,
                            const materials::Material& mat) const;

    double mcsAngle_rad(double kinE_MeV,
                        double stepLength_cm,
                        const physics::Particle& particle,
                        const materials::Material& material) const;

    double stragglingSigma_MeV(double kinE_MeV,
                                double stepLength_cm,
                                const physics::Particle& particle,
                                const materials::Material& material) const;
};

} // namespace beamlab::domain::simulation
