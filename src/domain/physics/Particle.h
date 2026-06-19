#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace beamlab::domain::physics {

enum class ParticleCategory {
    Lepton,
    Hadron,
    Boson,
    Ion,
    Custom
};

inline std::string to_string(ParticleCategory cat)
{
    switch (cat) {
        case ParticleCategory::Lepton: return "lepton";
        case ParticleCategory::Hadron: return "hadron";
        case ParticleCategory::Boson:  return "boson";
        case ParticleCategory::Ion:    return "ion";
        case ParticleCategory::Custom: return "custom";
    }
    return "unknown";
}

inline ParticleCategory particle_category_from_string(const std::string& s)
{
    if (s == "lepton") return ParticleCategory::Lepton;
    if (s == "hadron") return ParticleCategory::Hadron;
    if (s == "boson")  return ParticleCategory::Boson;
    if (s == "ion")    return ParticleCategory::Ion;
    return ParticleCategory::Custom;
}

struct Particle {
    // ── Identification ─────────────────────────────────────────────
    std::string id{};             // unique key, e.g. "proton"
    std::string name{};           // display name, e.g. "Proton"
    std::string symbol{};         // Unicode, e.g. "p"
    std::string geant4Name{};     // Geant4 name, e.g. "proton"
    ParticleCategory category{ParticleCategory::Custom};

    // ── PDG properties ─────────────────────────────────────────────
    int pdgCode{0};               // PDG Monte Carlo number
    double mass_MeV{0.0};         // rest mass [MeV/c²]
    double charge_e{0.0};         // charge in units of |e|
    double spin{0.0};             // spin (units of ℏ/2, so 0.5 = spin-½)
    int baryonNumber{0};
    int leptonNumber{0};

    // ── Lifetime ───────────────────────────────────────────────────
    bool isStable{true};
    double lifetime_s{0.0};       // mean lifetime [s]; 0 = stable/unknown

    // ── Dosimetry ──────────────────────────────────────────────────
    double WR{1.0};               // radiation weighting factor (ICRP-103)

    bool operator==(const Particle& o) const { return pdgCode == o.pdgCode; }
    bool operator!=(const Particle& o) const { return pdgCode != o.pdgCode; }
};

// ── JSON serialization ──────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const Particle& p)
{
    j = {
        {"id", p.id},
        {"name", p.name},
        {"symbol", p.symbol},
        {"geant4Name", p.geant4Name},
        {"category", to_string(p.category)},
        {"pdgCode", p.pdgCode},
        {"mass_MeV", p.mass_MeV},
        {"charge_e", p.charge_e},
        {"spin", p.spin},
        {"baryonNumber", p.baryonNumber},
        {"leptonNumber", p.leptonNumber},
        {"isStable", p.isStable},
        {"lifetime_s", p.lifetime_s},
        {"WR", p.WR},
    };
}

inline void from_json(const nlohmann::json& j, Particle& p)
{
    j.at("id").get_to(p.id);
    p.name = j.value("name", std::string{});
    p.symbol = j.value("symbol", std::string{});
    p.geant4Name = j.value("geant4Name", std::string{});
    p.category = particle_category_from_string(
        j.value("category", std::string{"custom"}));
    j.at("pdgCode").get_to(p.pdgCode);
    j.at("mass_MeV").get_to(p.mass_MeV);
    j.at("charge_e").get_to(p.charge_e);
    p.spin = j.value("spin", 0.0);
    p.baryonNumber = j.value("baryonNumber", 0);
    p.leptonNumber = j.value("leptonNumber", 0);
    p.isStable = j.value("isStable", true);
    p.lifetime_s = j.value("lifetime_s", 0.0);
    p.WR = j.value("WR", 1.0);
}

} // namespace beamlab::domain::physics
