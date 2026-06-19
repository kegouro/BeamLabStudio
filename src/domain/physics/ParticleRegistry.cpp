#include "domain/physics/ParticleRegistry.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace beamlab::domain::physics {

// ── Registration ─────────────────────────────────────────────────────

void ParticleRegistry::registerParticle(std::string name, Particle particle)
{
    particle.id = name;
    auto pdg = particle.pdgCode;

    byName_[name] = std::move(particle);
    byPdgCode_[pdg] = std::move(name);
}

void ParticleRegistry::registerParticlesFromJson(const std::string& jsonPath)
{
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "[ParticleRegistry] Cannot open " << jsonPath << std::endl;
        return;
    }

    try {
        auto json = nlohmann::json::parse(file);
        if (!json.is_array()) {
            std::cerr << "[ParticleRegistry] Expected JSON array in "
                      << jsonPath << std::endl;
            return;
        }
        for (const auto& item : json) {
            Particle p = item.get<Particle>();
            registerParticle(p.id, std::move(p));
        }
    } catch (const std::exception& e) {
        std::cerr << "[ParticleRegistry] Error parsing " << jsonPath
                  << ": " << e.what() << std::endl;
    }
}

// ── Query by name ────────────────────────────────────────────────────

const Particle& ParticleRegistry::get(const std::string& name) const
{
    auto it = byName_.find(name);
    if (it == byName_.end()) {
        throw std::out_of_range("Particle not found: " + name);
    }
    return it->second;
}

std::optional<std::reference_wrapper<const Particle>>
ParticleRegistry::find(const std::string& name) const
{
    auto it = byName_.find(name);
    if (it == byName_.end()) return std::nullopt;
    return std::cref(it->second);
}

// ── Query by PDG code ────────────────────────────────────────────────

const Particle& ParticleRegistry::getByPdgCode(int pdgCode) const
{
    auto it = byPdgCode_.find(pdgCode);
    if (it == byPdgCode_.end()) {
        throw std::out_of_range("Particle not found for PDG code: " +
                                std::to_string(pdgCode));
    }
    return get(it->second);
}

std::optional<std::reference_wrapper<const Particle>>
ParticleRegistry::findByPdgCode(int pdgCode) const
{
    auto it = byPdgCode_.find(pdgCode);
    if (it == byPdgCode_.end()) return std::nullopt;
    return find(it->second);
}

// ── Filter ───────────────────────────────────────────────────────────

std::vector<const Particle*> ParticleRegistry::findByCategory(
    ParticleCategory category) const
{
    std::vector<const Particle*> result;
    for (const auto& [_, p] : byName_) {
        if (p.category == category) result.push_back(&p);
    }
    return result;
}

std::vector<const Particle*> ParticleRegistry::findByProperty(
    std::function<bool(const Particle&)> predicate) const
{
    std::vector<const Particle*> result;
    for (const auto& [_, p] : byName_) {
        if (predicate(p)) result.push_back(&p);
    }
    return result;
}

// ── Enumeration ──────────────────────────────────────────────────────

std::vector<std::string> ParticleRegistry::names() const
{
    std::vector<std::string> result;
    result.reserve(byName_.size());
    for (const auto& [name, _] : byName_) {
        result.push_back(name);
    }
    return result;
}

// ── Built-in particles ───────────────────────────────────────────────

void ParticleRegistry::loadBuiltinParticles()
{
    auto insert = [&](Particle p) {
        auto id = p.id;
        if (byName_.count(id)) return;  // Idempotent
        byName_[id] = std::move(p);
        byPdgCode_[byName_.at(id).pdgCode] = id;
    };

    insert(makeElectron());
    insert(makePositron());
    insert(makeMuonMinus());
    insert(makeMuonPlus());
    insert(makeTauMinus());
    insert(makeTauPlus());
    insert(makeProton());
    insert(makeAntiproton());
    insert(makeNeutron());
    insert(makeDeuteron());
    insert(makeTriton());
    insert(makeHelium3());
    insert(makeAlpha());
    insert(makeCarbon12());
    insert(makeNitrogen14());
    insert(makeOxygen16());
    insert(makePionPlus());
    insert(makePionMinus());
    insert(makePionZero());
    insert(makeKaonPlus());
    insert(makeKaonMinus());
    insert(makeKaonZero());
    insert(makeGamma());
}

// ── Particle factories (PDG 2022 values) ─────────────────────────────

Particle ParticleRegistry::makeElectron()
{
    return {
        "electron", "Electron", "e⁻", "e-",
        ParticleCategory::Lepton,
        11,                         // PDG code
        0.51099895,                 // mass MeV/c²
        -1.0, 0.5,                  // charge, spin
        0, 1,                       // baryon, lepton number
        true, 0.0,                  // stable
        1.0                         // WR
    };
}

Particle ParticleRegistry::makePositron()
{
    return {
        "positron", "Positron", "e⁺", "e+",
        ParticleCategory::Lepton,
        -11,
        0.51099895, +1.0, 0.5, 0, -1,
        true, 0.0,
        1.0
    };
}

Particle ParticleRegistry::makeMuonMinus()
{
    return {
        "muon_minus", "Muon⁻", "μ⁻", "mu-",
        ParticleCategory::Lepton,
        13,
        105.6583755, -1.0, 0.5,
        0, 1,
        false, 2.1969811e-6,
        1.0
    };
}

Particle ParticleRegistry::makeMuonPlus()
{
    return {
        "muon_plus", "Muon⁺", "μ⁺", "mu+",
        ParticleCategory::Lepton,
        -13,
        105.6583755, +1.0, 0.5,
        0, -1,
        false, 2.1969811e-6,
        1.0
    };
}

Particle ParticleRegistry::makeTauMinus()
{
    return {
        "tau_minus", "Tau⁻", "τ⁻", "tau-",
        ParticleCategory::Lepton,
        15,
        1776.86, -1.0, 0.5,
        0, 1,
        false, 2.903e-13,
        1.0
    };
}

Particle ParticleRegistry::makeTauPlus()
{
    return {
        "tau_plus", "Tau⁺", "τ⁺", "tau+",
        ParticleCategory::Lepton,
        -15,
        1776.86, +1.0, 0.5,
        0, -1,
        false, 2.903e-13,
        1.0
    };
}

Particle ParticleRegistry::makeProton()
{
    return {
        "proton", "Proton", "p", "proton",
        ParticleCategory::Hadron,
        2212,
        938.27208816, +1.0, 0.5,
        1, 0,
        true, 0.0,
        2.0
    };
}

Particle ParticleRegistry::makeAntiproton()
{
    return {
        "antiproton", "Antiproton", "p̅", "anti_proton",
        ParticleCategory::Hadron,
        -2212,
        938.27208816, -1.0, 0.5,
        -1, 0,
        false, 0.0,
        2.0
    };
}

Particle ParticleRegistry::makeNeutron()
{
    return {
        "neutron", "Neutron", "n", "neutron",
        ParticleCategory::Hadron,
        2112,
        939.565420, 0.0, 0.5,
        1, 0,
        false, 878.4,          // τ = 14.64 min ≈ 879 s
        2.0
    };
}

Particle ParticleRegistry::makeDeuteron()
{
    return {
        "deuteron", "Deuteron", "d", "deuteron",
        ParticleCategory::Ion,
        1000010020,
        1875.612928, +1.0, 1.0,   // spin = 1 ℏ
        2, 0,
        true, 0.0,
        2.0
    };
}

Particle ParticleRegistry::makeTriton()
{
    return {
        "triton", "Triton", "t", "triton",
        ParticleCategory::Ion,
        1000010030,
        2808.921, +1.0, 0.5,
        3, 0,
        false, 3.888e8,          // 12.32 years
        2.0
    };
}

Particle ParticleRegistry::makeHelium3()
{
    return {
        "helium3", "Helium-3", "³He²⁺", "He3",
        ParticleCategory::Ion,
        1000020030,
        2808.391, +2.0, 0.5,
        3, 0,
        true, 0.0,
        2.0
    };
}

Particle ParticleRegistry::makeAlpha()
{
    return {
        "alpha", "Alpha Particle", "α", "alpha",
        ParticleCategory::Ion,
        1000020040,
        3727.3794, +2.0, 0.0,
        4, 0,
        true, 0.0,
        20.0
    };
}

Particle ParticleRegistry::makeCarbon12()
{
    return {
        "carbon12", "Carbon-12", "¹²C", "C12",
        ParticleCategory::Ion,
        1000060120,
        11174.86, +6.0, 0.0,
        12, 0,
        true, 0.0,
        20.0
    };
}

Particle ParticleRegistry::makeNitrogen14()
{
    return {
        "nitrogen14", "Nitrogen-14", "¹⁴N", "N14",
        ParticleCategory::Ion,
        1000070140,
        13040.20, +7.0, 1.0,
        14, 0,
        true, 0.0,
        20.0
    };
}

Particle ParticleRegistry::makeOxygen16()
{
    return {
        "oxygen16", "Oxygen-16", "¹⁶O", "O16",
        ParticleCategory::Ion,
        1000080160,
        14895.08, +8.0, 0.0,
        16, 0,
        true, 0.0,
        20.0
    };
}

Particle ParticleRegistry::makePionPlus()
{
    return {
        "pion_plus", "Pion⁺", "π⁺", "pi+",
        ParticleCategory::Hadron,
        211,
        139.57039, +1.0, 0.0,
        0, 0,
        false, 2.6033e-8,       // 26 ns
        1.0
    };
}

Particle ParticleRegistry::makePionMinus()
{
    return {
        "pion_minus", "Pion⁻", "π⁻", "pi-",
        ParticleCategory::Hadron,
        -211,
        139.57039, -1.0, 0.0,
        0, 0,
        false, 2.6033e-8,
        1.0
    };
}

Particle ParticleRegistry::makePionZero()
{
    return {
        "pion_zero", "Pion⁰", "π⁰", "pi0",
        ParticleCategory::Hadron,
        111,
        134.9768, 0.0, 0.0,
        0, 0,
        false, 8.43e-17,       // 84.3 attoseconds
        1.0
    };
}

Particle ParticleRegistry::makeKaonPlus()
{
    return {
        "kaon_plus", "Kaon⁺", "K⁺", "kaon+",
        ParticleCategory::Hadron,
        321,
        493.677, +1.0, 0.0,
        0, 0,
        false, 1.2380e-8,
        1.0
    };
}

Particle ParticleRegistry::makeKaonMinus()
{
    return {
        "kaon_minus", "Kaon⁻", "K⁻", "kaon-",
        ParticleCategory::Hadron,
        -321,
        493.677, -1.0, 0.0,
        0, 0,
        false, 1.2380e-8,
        1.0
    };
}

Particle ParticleRegistry::makeKaonZero()
{
    return {
        "kaon_zero", "Kaon⁰", "K⁰", "kaon0",
        ParticleCategory::Hadron,
        311,
        497.614, 0.0, 0.0,
        0, 0,
        false, 8.954e-11,      // K₀_S mean lifetime
        1.0
    };
}

Particle ParticleRegistry::makeGamma()
{
    return {
        "gamma", "Photon", "γ", "gamma",
        ParticleCategory::Boson,
        22,
        0.0, 0.0, 1.0,          // spin = 1 ℏ
        0, 0,
        true, 0.0,
        1.0
    };
}

} // namespace beamlab::domain::physics
