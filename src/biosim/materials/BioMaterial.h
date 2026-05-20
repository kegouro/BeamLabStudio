#pragma once

#include <string>
#include <vector>

namespace beamlab::biosim {

// Physical properties of a biological or detector material for stopping-power
// and dosimetry calculations.
//
// Primary references:
//   ICRU Report 44 (1989)  — tissue compositions and I values
//   ICRU Report 49 (1993)  — stopping powers for protons/alphas
//   NIST PSTAR/ESTAR/ASTAR — digital stopping-power tables
//   ICRU Report 37 (1984)  — electron stopping powers
//   ICRP Publication 103 (2007) — WR and WT factors
//   Sternheimer et al., At.Data Nucl.Data Tables 30 (1984) — density correction
struct BioMaterial {

    // ── Identification ────────────────────────────────────────────────────────
    std::string id{};           // unique key, e.g. "water_icru", "muscle_skeletal"
    std::string name{};         // display name, e.g. "Water (ICRU-44)"
    std::string symbol{};       // chemical symbol or short label, e.g. "H₂O"
    std::string category{};     // "biological" | "phantom" | "gas" | "metal"
                                // "plastic" | "detector" | "shielding" | "custom"
    std::string reference{};    // bibliographic reference for the parameter set
    bool is_custom{false};      // true if user-defined (serialized to JSON)

    // ── Stopping power parameters (Bethe-Bloch) ───────────────────────────────
    double density_g_cm3{1.0};       // mass density ρ [g/cm³]
    double Z_eff{7.22};              // effective atomic number
    double A_eff{12.01};             // effective atomic mass [g/mol]
    double I_eV{75.0};               // mean excitation energy I [eV] (ICRU-49)

    // ── Sternheimer density-effect correction parameters ──────────────────────
    // Reference: Sternheimer, Berger & Seltzer, At.Data Nucl.Data Tables 30 (1984)
    // δ(βγ) corrects the Bethe-Bloch formula for the relativistic density effect.
    //
    // For x = log10(βγ):
    //   if x < x0:          δ = δ₀ · (10^(2(x-x0)))
    //   if x0 ≤ x < x1:    δ = 4.6052·x + C_delta + a·(x1-x)^k
    //   if x ≥ x1:          δ = 4.6052·x + C_delta
    //
    // where C_delta = -2·ln(I/ħω_p) - 1  and ħω_p = 28.816·√(ρ·Z/A) [eV]
    double sternheimer_a{0.0};       // parameter a (conductor adjustment)
    double sternheimer_k{3.0};       // exponent k (typically 3)
    double sternheimer_x0{0.0};      // lower threshold log10(βγ)
    double sternheimer_x1{2.0};      // upper threshold log10(βγ)
    double sternheimer_C_delta{0.0}; // Cδ = -2·ln(I/ħω_p) - 1
    double sternheimer_delta0{0.0};  // δ₀ for the low-βγ region (usually 0 for non-conductor)
    bool has_sternheimer{false};     // false → use δ = 0 (conservative, overestimates at high E)

    // ── Characteristic lengths ────────────────────────────────────────────────
    double radiation_length_cm{36.08};    // X₀ [cm] — for MCS and bremsstrahlung
    double nuclear_int_length_cm{83.30};  // λ_I [cm] — hadronic interaction length
    double moliere_radius_cm{9.00};       // Molière radius for lateral shower spread

    // ── Biological dosimetry (ICRP 103) ──────────────────────────────────────
    double WR{1.0};                  // radiation weighting factor (dimensionless)
                                     // μ/e: 1, p: 2, α: 20, heavy ions: 20
    double WT{0.0};                  // tissue weighting factor (0 if not an organ)
                                     // lung/stomach/colon/breast/marrow: 0.12, etc.
    double alpha_beta_ratio{10.0};   // α/β [Gy] for linear-quadratic (LQ) model
                                     // tumor: typically 10 Gy; late-responding: 3 Gy
    std::string organ_name{};        // ICRP organ name if WT is defined, else ""

    // ── Elemental composition (mass fractions) ────────────────────────────────
    // Σ fractions should equal 1.0 ± 0.001
    struct Element {
        std::string symbol{};   // chemical symbol, e.g. "H", "C", "N", "O"
        int Z{1};               // atomic number
        double fraction{0.0};   // mass fraction [0..1]
    };
    std::vector<Element> composition{};

    // ── Additional physical properties ────────────────────────────────────────
    double melting_point_K{273.15};  // melting point [K] (informational)
    double boiling_point_K{373.15};  // boiling point [K] (informational)
    int state{1};                    // 0 = solid, 1 = liquid, 2 = gas
    double bulk_modulus_GPa{0.0};    // bulk modulus [GPa] (informational)

    // ── Optical / scintillation properties ───────────────────────────────────
    double refraction_index{1.0};
    double scintillation_yield_photons_MeV{0.0}; // 0 = non-scintillator

    // ── Validation metadata ───────────────────────────────────────────────────
    double I_eV_uncertainty{5.0};        // ±[eV] from ICRU tables
    double density_uncertainty_rel{0.02}; // relative uncertainty on ρ

    // ── Convenience ───────────────────────────────────────────────────────────
    [[nodiscard]] bool isValid() const
    {
        return density_g_cm3 > 0.0 && Z_eff >= 1.0 && A_eff >= Z_eff && I_eV >= 10.0;
    }

    [[nodiscard]] bool isGas() const { return state == 2; }
    [[nodiscard]] bool isBiological() const { return category == "biological"; }
    [[nodiscard]] bool hasOrganData() const { return WT > 0.0 && !organ_name.empty(); }
};

} // namespace beamlab::biosim
