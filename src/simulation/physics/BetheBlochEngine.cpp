//!@math-begin module="BetheBloch" title="Fórmula de Bethe-Bloch — Pérdida de Energía"
//!@math Calcula el poder de frenado (stopping power) medio de una partícula cargada
//!@math pesada atravesando un material.  Referencia: PDG 2022, cap. 34, ec. 34.5.
//!@math Válido para 0.1 &lt; β·γ &lt; ~1000 (bien sobre el umbral de producción de rayos δ).
//!@formula -dE/dx = K · z² · (Z/A) / β² · [ ½ · ln( 2mₑc²β²γ²Wmax / I² ) − β² ]
//!@var K = 0.307075 MeV·cm²/g  ← constante de Bethe (K = 4πNₐrₑ²mₑc²)
//!@var z = carga de la partícula incidente en unidades de |e|  (muón → z = 1)
//!@var Z/A = número atómico efectivo / masa atómica efectiva del material
//!@var β = v/c,  γ = 1/√(1−β²),  donde  γ = 1 + T/Mc²  (T = energía cinética)
//!@var Wmax = 2mₑc²β²γ² / [1 + 2γmₑ/M + (mₑ/M)²]  ← transferencia máxima al electrón
//!@var I = energía media de excitación del material (tabla ICRU-49)
//!@note Se usa el poder de frenado másico S(β) = -dE/ρdx [MeV·cm²/g];
//!@note el lineal se obtiene multiplicando por la densidad ρ [g/cm³]:
//!@formula -dE/dx [MeV/cm] = S(β) · ρ
//!@note Para el muón: M = 105.6583755 MeV/c², z = ±1.
//!@section Dosis absorbida (Grays)
//!@math La dosis absorbida D en un material de masa m cuando se deposita una
//!@math energía ΔE es:
//!@formula D [Gy] = ΔE [J] / m [kg]   donde  1 MeV = 1.602176634 × 10⁻¹³ J
//!@note  1 Gy = 1 J/kg.  En radioterapia 1 Gy ≡ 1 Sv para radiación de bajo LET.
//!@note Para un paso de longitud Δx [cm] en un material de densidad ρ [g/cm³]
//!@note y sección transversal A = 1 cm²:
//!@formula m = ρ [g/cm³] · 10⁻³ [kg/g] · A [cm²] · 10⁻⁴ [m²/cm²] · Δx [cm] · 10⁻² [m/cm]
//!@math-end

#include "simulation/physics/BetheBlochEngine.h"

#include <algorithm>
#include <cmath>

namespace beamlab::simulation {

// Physical constants
namespace {

constexpr double k_MeV_cm2_g = 0.307075; // Bethe-Bloch K/A constant in MeV·cm²/g
                                            // K = 4πNₐrₑ²mₑc²  (PDG eq. 34.5)
constexpr double m_e_MeV = 0.51099895;     // electron rest mass in MeV
constexpr double MeV_to_J = 1.602176634e-13; // 1 MeV = 1.602e-13 J

// Bethe-Bloch mean stopping power -dE/dx in MeV/(g/cm²) (mass stopping power).
// PDG 2022 eq. 34.5, valid in the range 0.1 < β·γ < several hundred.
double betheBlochMass(const double kinE_MeV,
                      const double mass_MeV,
                      const double z,   // particle charge in |e|
                      const double Z_eff,
                      const double A_eff,
                      const double I_eV)
{
    const double I_MeV = I_eV * 1.0e-6;

    const double gamma = 1.0 + kinE_MeV / mass_MeV;
    const double beta2 = 1.0 - 1.0 / (gamma * gamma);

    if (beta2 <= 0.0 || beta2 >= 1.0) {
        return 0.0;
    }

    const double beta  = std::sqrt(beta2);
    const double bg    = beta * gamma;   // β·γ

    // Wmax: maximum energy transfer to a free electron in a single collision (PDG eq. 34.4)
    const double M = mass_MeV;
    const double num_Wmax = 2.0 * m_e_MeV * bg * bg;
    const double den_Wmax = 1.0 + 2.0 * gamma * m_e_MeV / M +
                             (m_e_MeV / M) * (m_e_MeV / M);
    const double Wmax_MeV = num_Wmax / den_Wmax;

    // Bethe-Bloch core (no density or shell corrections for simplicity)
    const double log_arg = 2.0 * m_e_MeV * bg * bg * Wmax_MeV / (I_MeV * I_MeV);

    if (log_arg <= 0.0) {
        return 0.0;
    }

    const double z2 = z * z;
    const double ZA = Z_eff / A_eff;

    const double stopping = k_MeV_cm2_g * z2 * ZA / beta2 *
                            (0.5 * std::log(log_arg) - beta2);

    return std::max(0.0, stopping); // MeV·cm²/g
}

} // namespace

double BetheBlochEngine::dEdx_MeV_cm(const double kinE_MeV,
                                      const double mass_MeV,
                                      const double charge,
                                      const TissueMaterial& material) const
{
    const double mass_sp = betheBlochMass(kinE_MeV,
                                          mass_MeV,
                                          std::abs(charge),
                                          material.Z_eff,
                                          material.A_eff,
                                          material.I_eV);
    // Convert mass stopping power (MeV·cm²/g) → linear (MeV/cm)
    return mass_sp * material.density_g_cm3;
}

double BetheBlochEngine::energyLoss_MeV(const double kinE_MeV,
                                         const double mass_MeV,
                                         const double charge,
                                         const TissueMaterial& material,
                                         const double dx_cm) const
{
    if (dx_cm <= 0.0 || kinE_MeV <= 0.0) {
        return 0.0;
    }

    const double dEdx = dEdx_MeV_cm(kinE_MeV, mass_MeV, charge, material);
    const double loss = dEdx * dx_cm;

    return std::min(loss, kinE_MeV); // cannot lose more than available kinetic energy
}

double BetheBlochEngine::dose_Gy(const double edep_MeV, const double mass_kg)
{
    if (mass_kg <= 0.0) {
        return 0.0;
    }

    return (edep_MeV * MeV_to_J) / mass_kg;
}

double BetheBlochEngine::dosePerStep_Gy(const double edep_MeV,
                                         const TissueMaterial& material,
                                         const double dx_cm) const
{
    if (dx_cm <= 0.0 || edep_MeV <= 0.0) {
        return 0.0;
    }

    // Treat the step as a thin slab: mass = density × volume = ρ × (A × dx)
    // We use A = 1 cm² as the nominal cross-section per track (unit dose).
    // This gives dose per unit area (Gy per cm² of beam).
    // For absolute dose, the user must supply the actual beam cross-section.
    // Here we return the dose for a 1 cm² column.
    const double dx_m = dx_cm * 0.01;
    const double volume_m3 = 1.0e-4 * dx_m; // 1 cm² × dx in m³
    const double density_kg_m3 = material.density_g_cm3 * 1000.0;
    const double mass_kg = density_kg_m3 * volume_m3;

    return dose_Gy(edep_MeV, mass_kg);
}

} // namespace beamlab::simulation
