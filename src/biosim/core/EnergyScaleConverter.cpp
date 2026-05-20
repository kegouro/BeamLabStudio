#include "biosim/core/EnergyScaleConverter.h"

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

// ── Internal stopping-power helper ────────────────────────────────────────────
// Bethe-Bloch mean mass stopping power [MeV·cm²/g].
// Sternheimer density correction δ(βγ) is applied when mat.has_sternheimer.
//
// Reference: PDG 2022, eq. 34.5 + Sternheimer et al. 1984.
namespace {

double sternheimerDelta(double log10_bg, const BioMaterial& mat)
{
    if (!mat.has_sternheimer) return 0.0;

    const double x  = log10_bg;
    const double x0 = mat.sternheimer_x0;
    const double x1 = mat.sternheimer_x1;
    const double a  = mat.sternheimer_a;
    const double k  = mat.sternheimer_k;
    const double C  = mat.sternheimer_C_delta;
    const double d0 = mat.sternheimer_delta0;

    if (x < x0) {
        return d0 * std::pow(10.0, 2.0 * (x - x0));
    }
    if (x < x1) {
        return 4.6052 * x + C + a * std::pow(x1 - x, k);
    }
    return 4.6052 * x + C;
}

double betheBlochMassSP(double kinE_MeV,
                         double mass_MeV,
                         double z_charge,
                         const BioMaterial& mat)
{
    using namespace PhysConst;
    const double I_MeV = mat.I_eV * 1.0e-6;
    const double gamma  = 1.0 + kinE_MeV / mass_MeV;
    const double beta2  = 1.0 - 1.0 / (gamma * gamma);
    if (beta2 <= 1e-10 || beta2 >= 1.0) return 0.0;

    const double beta = std::sqrt(beta2);
    const double bg   = beta * gamma;   // β·γ

    // Maximum energy transfer to a free electron (PDG eq. 34.4)
    const double M        = mass_MeV;
    const double Wmax_num = 2.0 * m_e_MeV * bg * bg;
    const double Wmax_den = 1.0 + 2.0 * gamma * m_e_MeV / M + (m_e_MeV / M) * (m_e_MeV / M);
    const double Wmax     = Wmax_num / Wmax_den; // [MeV]

    const double log_arg = 2.0 * m_e_MeV * bg * bg * Wmax / (I_MeV * I_MeV);
    if (log_arg <= 0.0) return 0.0;

    const double delta = sternheimerDelta(std::log10(bg), mat);
    const double ZA    = mat.Z_eff / mat.A_eff;
    const double z2    = z_charge * z_charge;

    // PDG eq. 34.5 (mass stopping power)
    const double sp = k_BB * z2 * ZA / beta2 *
                      (0.5 * std::log(log_arg) - beta2 - delta / 2.0);

    return std::max(0.0, sp); // [MeV·cm²/g]
}

} // namespace

// ── Public methods ────────────────────────────────────────────────────────────

double EnergyScaleConverter::computeLET_MeV_cm(
    const double kinE_MeV,
    const BioMaterial& mat,
    const ParticleSpecies& particle) const
{
    if (kinE_MeV <= 0.0) return 0.0;
    const double sp = betheBlochMassSP(
        kinE_MeV, particle.mass_MeV, std::abs(particle.charge), mat);
    return sp * mat.density_g_cm3; // [MeV/cm]
}

double EnergyScaleConverter::estimateCsdaRange_cm(
    const double kinE_MeV,
    const BioMaterial& mat,
    const ParticleSpecies& particle) const
{
    if (kinE_MeV <= 0.0) return 0.0;
    const double LET = computeLET_MeV_cm(kinE_MeV, mat, particle);
    if (LET <= 0.0) return 0.0;
    // Single-point approximation: R ≈ T / (dE/dx)
    // BraggPeakCalculator gives the proper integrated range.
    return kinE_MeV / LET; // [cm]
}

double EnergyScaleConverter::estimateRBE(
    const double LET_keV_um,
    const BioMaterial& mat,
    const ParticleSpecies& particle) const
{
    // Proton RBE parameterization (Paganetti 2014, PMB 59 R419):
    //   RBE ≈ 1 + (α_x/β_x)⁻¹ · (0.434 · LET [keV/μm])
    // For other particles use WR directly (ICRP-103).
    if (particle.id == "proton") {
        const double ab = (mat.alpha_beta_ratio > 0.0) ? mat.alpha_beta_ratio : 10.0;
        return 1.0 + (1.0 / ab) * 0.434 * LET_keV_um;
    }
    // For ions (WR=20) the LET-based model is complex; use ICRP WR as proxy.
    return particle.WR;
}

EnergyScaleSet EnergyScaleConverter::compute(
    const double edep_MeV_orig,
    const double edep_MeV_sim,
    const double kinE_MeV_orig,
    const double kinE_MeV_sim,
    const double step_length_cm,
    const double depth_in_slab_cm,
    const BioMaterial* mat,
    const ParticleSpecies& particle,
    const int slab_index,
    const double beam_radius_cm) const
{
    using namespace PhysConst;
    EnergyScaleSet s;

    // ── Source ────────────────────────────────────────────────────────────────
    s.edep_MeV_original  = edep_MeV_orig;
    s.kinE_MeV_original  = kinE_MeV_orig;
    s.edep_MeV_simulated = edep_MeV_sim;
    s.kinE_MeV_simulated = kinE_MeV_sim;
    s.delta_edep_MeV     = edep_MeV_sim - edep_MeV_orig;
    s.slab_index         = slab_index;
    s.step_length_cm     = step_length_cm;
    s.depth_in_slab_cm   = depth_in_slab_cm;

    // ── Physical scales — energy deposit ──────────────────────────────────────
    // Always from edep_MeV_simulated, never from a derived field.
    const double E = edep_MeV_sim;
    s.edep_keV = E * 1.0e3;
    s.edep_eV  = E * 1.0e6;
    s.edep_J   = E * MeV_to_J;
    s.edep_erg = s.edep_J * J_to_erg;

    // ── Physical scales — kinetic energy ──────────────────────────────────────
    const double T = kinE_MeV_sim;
    s.kinE_keV = T * 1.0e3;
    s.kinE_eV  = T * 1.0e6;
    s.kinE_GeV = T * 1.0e-3;
    s.kinE_TeV = T * 1.0e-6;

    // ── LET ───────────────────────────────────────────────────────────────────
    if (mat != nullptr && T > 0.0) {
        s.LET_MeV_cm = computeLET_MeV_cm(T, *mat, particle);
        s.LET_keV_um = s.LET_MeV_cm * 0.1;   // 1 MeV/cm = 10³ keV / 10⁴ μm = 0.1 keV/μm
        s.LET_eV_nm  = s.LET_MeV_cm * 0.1;  // 1 MeV/cm = 10⁶ eV / 10⁷ nm  = 0.1 eV/nm
        s.CSDA_range_cm = estimateCsdaRange_cm(T, *mat, particle);
    }

    // ── Dosimetric scales ─────────────────────────────────────────────────────
    // Dose D = E_dep [J] / m [kg]
    // Mass: cylindrical voxel of radius beam_radius_cm, length step_length_cm
    // m = ρ [g/cm³] × π·r² [cm²] × dx [cm] × 1e-3 [kg/g]
    if (mat != nullptr && step_length_cm > 0.0 && E > 0.0) {
        const double r_cm = (beam_radius_cm > 0.0) ? beam_radius_cm : 0.5642;
        const double vol_cm3    = M_PI * r_cm * r_cm * step_length_cm;
        const double mass_kg    = mat->density_g_cm3 * vol_cm3 * 1.0e-3;
        if (mass_kg > 0.0) {
            s.dose_Gy   = (E * MeV_to_J) / mass_kg;
            s.dose_mGy  = s.dose_Gy * 1.0e3;
            s.dose_uGy  = s.dose_Gy * 1.0e6;
            s.dose_rad  = s.dose_Gy * Gy_to_rad;
            s.dose_mrad = s.dose_rad * 1.0e3;
        }
    }

    // ── Equivalent dose H = D × WR ────────────────────────────────────────────
    const double WR = (mat != nullptr) ? mat->WR : particle.WR;
    s.WR_applied = WR;
    s.H_Sv   = s.dose_Gy * WR;
    s.H_mSv  = s.H_Sv * 1.0e3;
    s.H_uSv  = s.H_Sv * 1.0e6;
    s.H_rem  = s.H_Sv * Sv_to_rem;
    s.H_mrem = s.H_rem * 1.0e3;

    // ── Effective dose E = H × WT ─────────────────────────────────────────────
    const double WT = (mat != nullptr) ? mat->WT : 0.0;
    s.WT_applied = WT;
    s.E_Sv  = s.H_Sv * WT;
    s.E_mSv = s.E_Sv * 1.0e3;

    // ── Radiobiological ───────────────────────────────────────────────────────
    if (mat != nullptr) {
        s.RBE              = estimateRBE(s.LET_keV_um, *mat, particle);
        s.alpha_beta_applied = mat->alpha_beta_ratio;
        const double ab    = (mat->alpha_beta_ratio > 0.0) ? mat->alpha_beta_ratio : 10.0;
        // BED = D × RBE × (1 + D·RBE / (α/β))
        const double D_rbe = s.dose_Gy * s.RBE;
        s.BED_Gy = D_rbe * (1.0 + D_rbe / ab);
    } else {
        s.RBE    = particle.WR;
        s.BED_Gy = s.dose_Gy * s.RBE;
    }

    return s;
}

} // namespace beamlab::biosim
