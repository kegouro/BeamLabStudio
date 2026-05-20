#include "biosim/physics/ParticleLibrary.h"

#include <cctype>

namespace beamlab::biosim {

namespace {
std::string toLower(std::string s)
{
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
} // namespace

// ── Static factories ──────────────────────────────────────────────────────────
// All masses from PDG 2022 summary tables.

ParticleSpecies ParticleLibrary::electronMinus()
{
    return {"electron_minus", "Electron", "e⁻", "e-",
            "lepton",
            0.51099895,  // mass MeV/c²
            -1.0,        // charge
            0.5,         // spin ℏ/2
            0, 1, true, 0.0,
            1.0};        // WR (ICRP-103: electrons = 1)
}

ParticleSpecies ParticleLibrary::electronPlus()
{
    return {"electron_plus", "Positron", "e⁺", "e+",
            "lepton", 0.51099895, +1.0, 0.5, 0, -1, true, 0.0, 1.0};
}

ParticleSpecies ParticleLibrary::muonMinus()
{
    return {"muon_minus", "Muon⁻", "μ⁻", "mu-",
            "lepton",
            105.6583755, // PDG 2022
            -1.0, 0.5,
            0, 1,
            false, 2.1969811e-6,  // τ = 2.197 μs
            1.0};
}

ParticleSpecies ParticleLibrary::muonPlus()
{
    return {"muon_plus", "Muon⁺", "μ⁺", "mu+",
            "lepton", 105.6583755, +1.0, 0.5, 0, -1, false, 2.1969811e-6, 1.0};
}

ParticleSpecies ParticleLibrary::tauMinus()
{
    return {"tau_minus", "Tau⁻", "τ⁻", "tau-",
            "lepton",
            1776.86,     // PDG 2022
            -1.0, 0.5,
            0, 1,
            false, 2.903e-13,  // τ = 290.3 fs
            1.0};
}

ParticleSpecies ParticleLibrary::proton()
{
    return {"proton", "Proton", "p", "proton",
            "baryon",
            938.27208816, // PDG 2022
            +1.0, 0.5,
            1, 0, true, 0.0,
            2.0};  // WR = 2 (ICRP-103)
}

ParticleSpecies ParticleLibrary::antiproton()
{
    return {"antiproton", "Antiproton", "p̅", "anti_proton",
            "baryon", 938.27208816, -1.0, 0.5, -1, 0, false, 0.0, 2.0};
}

ParticleSpecies ParticleLibrary::deuteron()
{
    return {"deuteron", "Deuteron", "d", "deuteron",
            "ion",
            1875.612928, // 2·mp − binding energy
            +1.0, 0.5,
            2, 0, true, 0.0,
            2.0};
}

ParticleSpecies ParticleLibrary::triton()
{
    return {"triton", "Triton", "t", "triton",
            "ion",
            2808.921,
            +1.0, 0.5,
            3, 0,
            false, 3.888e8,  // τ = 12.32 years
            2.0};
}

ParticleSpecies ParticleLibrary::helium3()
{
    return {"helium3", "Helium-3", "³He²⁺", "He3",
            "ion",
            2808.391,
            +2.0, 0.5,
            3, 0, true, 0.0,
            2.0};
}

ParticleSpecies ParticleLibrary::alpha()
{
    return {"alpha", "Alpha Particle", "α", "alpha",
            "ion",
            3727.3794,  // ⁴He nucleus: PDG 2022
            +2.0, 0.0,  // spin = 0
            4, 0, true, 0.0,
            20.0};  // WR = 20 (ICRP-103)
}

ParticleSpecies ParticleLibrary::carbon12()
{
    return {"carbon12", "Carbon-12 Ion", "¹²C⁶⁺", "C12",
            "ion",
            11177.929,  // 12 × u − 6·me (approx nuclear mass)
            +6.0, 0.0,
            12, 0, true, 0.0,
            20.0};  // WR = 20 for heavy ions
}

ParticleSpecies ParticleLibrary::nitrogen14()
{
    return {"nitrogen14", "Nitrogen-14 Ion", "¹⁴N", "N14",
            "ion",
            13040.863,
            +7.0, 0.5,
            14, 0, true, 0.0,
            20.0};
}

ParticleSpecies ParticleLibrary::oxygen16()
{
    return {"oxygen16", "Oxygen-16 Ion", "¹⁶O", "O16",
            "ion",
            14899.168,
            +8.0, 0.0,
            16, 0, true, 0.0,
            20.0};
}

ParticleSpecies ParticleLibrary::pionMinus()
{
    return {"pion_minus", "Pion⁻", "π⁻", "pi-",
            "meson",
            139.57039,  // PDG 2022
            -1.0, 0.0,
            0, 0,
            false, 2.6033e-8,  // τ = 26.03 ns
            1.0};
}

ParticleSpecies ParticleLibrary::pionPlus()
{
    return {"pion_plus", "Pion⁺", "π⁺", "pi+",
            "meson", 139.57039, +1.0, 0.0, 0, 0, false, 2.6033e-8, 1.0};
}

ParticleSpecies ParticleLibrary::kaonMinus()
{
    return {"kaon_minus", "Kaon⁻", "K⁻", "kaon-",
            "meson",
            493.677,    // PDG 2022
            -1.0, 0.0,
            0, 0,
            false, 1.238e-8,  // τ = 12.38 ns
            1.0};
}

ParticleSpecies ParticleLibrary::customParticle(
    const double mass_MeV,
    const double charge,
    const std::string& label)
{
    ParticleSpecies p;
    p.id = "custom";
    p.name = label.empty() ? "Custom Particle" : label;
    p.symbol = "?";
    p.geant4_name = "";
    p.category = "custom";
    p.mass_MeV = mass_MeV;
    p.charge = charge;
    p.spin = 0.5;
    p.is_stable = true;
    p.WR = 1.0;
    return p;
}

// ── Library constructor ───────────────────────────────────────────────────────

ParticleLibrary::ParticleLibrary()
{
    species_ = {
        electronMinus(), electronPlus(),
        muonMinus(), muonPlus(), tauMinus(),
        proton(), antiproton(),
        deuteron(), triton(), helium3(), alpha(),
        carbon12(), nitrogen14(), oxygen16(),
        pionMinus(), pionPlus(), kaonMinus()
    };
}

const std::vector<ParticleSpecies>& ParticleLibrary::all() const
{
    return species_;
}

std::optional<ParticleSpecies> ParticleLibrary::findById(const std::string& id) const
{
    const auto target = toLower(id);
    for (const auto& p : species_) {
        if (toLower(p.id) == target) return p;
    }
    return std::nullopt;
}

std::optional<ParticleSpecies> ParticleLibrary::findByGeant4Name(const std::string& g4name) const
{
    const auto target = toLower(g4name);
    for (const auto& p : species_) {
        if (toLower(p.geant4_name) == target) return p;
    }
    return std::nullopt;
}

} // namespace beamlab::biosim
