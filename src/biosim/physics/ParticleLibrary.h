#pragma once

#include <optional>
#include <string>
#include <vector>

namespace beamlab::biosim {

// Physical properties of a particle species for stopping-power calculations.
// Mass and charge values from PDG 2022.
struct ParticleSpecies {
    std::string id{};            // unique key, e.g. "muon_minus"
    std::string name{};          // display name, e.g. "Muon−"
    std::string symbol{};        // Unicode symbol, e.g. "μ−"
    std::string geant4_name{};   // Geant4 particle name, e.g. "mu-"
    std::string category{};      // "lepton" | "baryon" | "meson" | "ion" | "custom"
    double mass_MeV{0.0};        // rest mass [MeV/c²]
    double charge{0.0};          // charge in units of |e|
    double spin{0.0};            // spin in units of ℏ/2
    int baryon_number{0};
    int lepton_number{0};
    bool is_stable{true};
    double lifetime_s{0.0};      // mean lifetime [s]; 0 = stable or unknown
    double WR{1.0};              // radiation weighting factor (ICRP-103) — same as BioMaterial.WR
};

// Catalogue of 18 particle species used in beam physics and therapy.
// Reference: PDG 2022 (https://pdg.lbl.gov)
class ParticleLibrary {
public:
    ParticleLibrary();

    [[nodiscard]] const std::vector<ParticleSpecies>& all() const;

    // Find by id (case-insensitive exact match).
    [[nodiscard]] std::optional<ParticleSpecies> findById(const std::string& id) const;

    // Find by Geant4 particle name (e.g. "mu-").
    [[nodiscard]] std::optional<ParticleSpecies> findByGeant4Name(const std::string& g4name) const;

    // Convenience accessors
    static ParticleSpecies electronMinus();
    static ParticleSpecies electronPlus();
    static ParticleSpecies muonMinus();
    static ParticleSpecies muonPlus();
    static ParticleSpecies tauMinus();
    static ParticleSpecies proton();
    static ParticleSpecies antiproton();
    static ParticleSpecies deuteron();
    static ParticleSpecies triton();
    static ParticleSpecies helium3();
    static ParticleSpecies alpha();
    static ParticleSpecies carbon12();
    static ParticleSpecies nitrogen14();
    static ParticleSpecies oxygen16();
    static ParticleSpecies pionMinus();
    static ParticleSpecies pionPlus();
    static ParticleSpecies kaonMinus();
    static ParticleSpecies customParticle(double mass_MeV, double charge,
                                          const std::string& label);

private:
    std::vector<ParticleSpecies> species_{};
};

} // namespace beamlab::biosim
