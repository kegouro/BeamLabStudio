#pragma once

namespace beamlab::biosim {

// All energy representations for a single trajectory step.
//
// Convention:
//   - All _original fields are verbatim from Geant4 (never modified).
//   - All _simulated fields reflect Bethe-Bloch recalculation inside slabs;
//     outside slabs they are equal to the original values.
//   - Unit conversions are always derived from edep_MeV_simulated directly
//     (never chained: eV ≠ keV × 1000 from a rounded keV value).
struct EnergyScaleSet {

    // ── Source fields (Geant4 original) ──────────────────────────────────────
    double edep_MeV_original{0.0};   // energy deposit as recorded by Geant4 [MeV]
    double kinE_MeV_original{0.0};   // kinetic energy as recorded by Geant4 [MeV]

    // ── Simulated fields (after Bethe-Bloch inside active slabs) ─────────────
    double edep_MeV_simulated{0.0};  // energy deposit after biosim recalculation [MeV]
    double kinE_MeV_simulated{0.0};  // kinetic energy after biosim propagation [MeV]

    // ── Physical scales — energy deposit ─────────────────────────────────────
    // All derived from edep_MeV_simulated, exact arithmetic, no rounding cascade.
    double edep_keV{0.0};            // [keV]   = edep_MeV × 1e3
    double edep_eV{0.0};             // [eV]    = edep_MeV × 1e6
    double edep_J{0.0};              // [J]     = edep_MeV × 1.602176634e-13
    double edep_erg{0.0};            // [erg]   = edep_J × 1e7

    // ── Physical scales — kinetic energy ─────────────────────────────────────
    double kinE_keV{0.0};            // [keV]
    double kinE_eV{0.0};             // [eV]
    double kinE_GeV{0.0};            // [GeV]
    double kinE_TeV{0.0};            // [TeV]

    // ── Linear Energy Transfer (LET) ─────────────────────────────────────────
    // LET = -dE/dx evaluated at this step's kinetic energy and material.
    double LET_MeV_cm{0.0};         // [MeV/cm]   — linear stopping power
    double LET_keV_um{0.0};         // [keV/μm]   = LET_MeV_cm × 0.1
    double LET_eV_nm{0.0};          // [eV/nm]    = LET_MeV_cm × 0.1

    // ── Dosimetric scales ─────────────────────────────────────────────────────
    // Dose D = edep / mass.  Mass model: ρ × (A_ref × step_cm)
    // where A_ref = 1 cm² per track (unit dose); set by BioSimConfig.beam_radius_cm.
    double dose_Gy{0.0};            // [Gy]  = [J/kg]
    double dose_mGy{0.0};           // [mGy] = dose_Gy × 1e3
    double dose_uGy{0.0};           // [μGy] = dose_Gy × 1e6
    double dose_rad{0.0};           // [rad] = dose_Gy × 100  (CGS, historical)
    double dose_mrad{0.0};          // [mrad]= dose_rad × 1e3

    // ── Equivalent dose H = D × WR (ICRP-103) ────────────────────────────────
    double WR_applied{1.0};         // radiation weighting factor used
    double H_Sv{0.0};               // [Sv]  = dose_Gy × WR
    double H_mSv{0.0};              // [mSv] = H_Sv × 1e3
    double H_uSv{0.0};              // [μSv] = H_Sv × 1e6
    double H_rem{0.0};              // [rem] = H_Sv × 100  (historical)
    double H_mrem{0.0};             // [mrem]= H_rem × 1e3

    // ── Effective dose E = H × WT (ICRP-103) ─────────────────────────────────
    // Only meaningful when the slab is assigned to a specific organ.
    double WT_applied{0.0};         // tissue weighting factor used (0 if no organ)
    double E_Sv{0.0};               // [Sv]  = H_Sv × WT
    double E_mSv{0.0};              // [mSv] = E_Sv × 1e3

    // ── Radiobiological estimates (linear-quadratic model) ────────────────────
    double RBE{1.0};                // Relative Biological Effectiveness (LET-based estimate)
    double BED_Gy{0.0};            // Biological Effective Dose [Gy] = D × RBE × (1 + D/(α/β))
    double alpha_beta_applied{10.0};// α/β ratio [Gy] used for BED calculation

    // ── Step geometry ─────────────────────────────────────────────────────────
    double step_length_cm{0.0};     // Euclidean step length [cm]
    double depth_in_slab_cm{0.0};   // depth of this step inside its active slab [cm]
    double CSDA_range_cm{0.0};      // estimated CSDA range from this step's KinE [cm]

    // ── Differential quantities ───────────────────────────────────────────────
    double delta_edep_MeV{0.0};     // edep_MeV_simulated - edep_MeV_original

    // ── Slab context ─────────────────────────────────────────────────────────
    // Empty string if the step is outside all active slabs.
    // (Stored as index to avoid string overhead in tight loops — resolved to
    //  name by BioSimResult.)
    int slab_index{-1};            // -1 = no slab
};

} // namespace beamlab::biosim
