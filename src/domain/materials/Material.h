#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace beamlab::domain::materials {

enum class MaterialCategory {
    Biological,
    Phantom,
    Gas,
    Metal,
    Plastic,
    Detector,
    Shielding,
    Custom
};

inline std::string to_string(MaterialCategory cat)
{
    switch (cat) {
        case MaterialCategory::Biological: return "biological";
        case MaterialCategory::Phantom:    return "phantom";
        case MaterialCategory::Gas:        return "gas";
        case MaterialCategory::Metal:      return "metal";
        case MaterialCategory::Plastic:    return "plastic";
        case MaterialCategory::Detector:   return "detector";
        case MaterialCategory::Shielding:  return "shielding";
        case MaterialCategory::Custom:     return "custom";
    }
    return "unknown";
}

inline MaterialCategory category_from_string(const std::string& s)
{
    if (s == "biological") return MaterialCategory::Biological;
    if (s == "phantom")    return MaterialCategory::Phantom;
    if (s == "gas")        return MaterialCategory::Gas;
    if (s == "metal")      return MaterialCategory::Metal;
    if (s == "plastic")    return MaterialCategory::Plastic;
    if (s == "detector")   return MaterialCategory::Detector;
    if (s == "shielding")  return MaterialCategory::Shielding;
    return MaterialCategory::Custom;
}

// ── Element ──────────────────────────────────────────────────────────

struct Element {
    std::string symbol;
    int Z{1};
    double massFraction{0.0};
};

inline void to_json(nlohmann::json& j, const Element& e)
{
    j = {{"symbol", e.symbol}, {"Z", e.Z}, {"massFraction", e.massFraction}};
}

inline void from_json(const nlohmann::json& j, Element& e)
{
    j.at("symbol").get_to(e.symbol);
    j.at("Z").get_to(e.Z);
    j.at("massFraction").get_to(e.massFraction);
}

// ── Sternheimer parameters ───────────────────────────────────────────

struct SternheimerParams {
    double a{0.0};
    double k{3.0};
    double x0{0.0};
    double x1{2.0};
    double C_delta{0.0};
    double delta0{0.0};
    bool hasData{false};
};

inline void to_json(nlohmann::json& j, const SternheimerParams& s)
{
    j = {{"a", s.a}, {"k", s.k}, {"x0", s.x0}, {"x1", s.x1},
         {"C_delta", s.C_delta}, {"delta0", s.delta0}, {"hasData", s.hasData}};
}

inline void from_json(const nlohmann::json& j, SternheimerParams& s)
{
    j.at("a").get_to(s.a); j.at("k").get_to(s.k);
    j.at("x0").get_to(s.x0); j.at("x1").get_to(s.x1);
    j.at("C_delta").get_to(s.C_delta); j.at("delta0").get_to(s.delta0);
    s.hasData = j.value("hasData", false);
}

// ── Material ─────────────────────────────────────────────────────────

struct Material {
    // ── Identification ─────────────────────────────────────────────
    std::string id{};
    std::string name{};
    std::string symbol{};
    MaterialCategory category{MaterialCategory::Custom};
    std::string reference{};
    bool isOverride{false};

    // ── Stopping power (Bethe-Bloch) ───────────────────────────────
    double density_g_cm3{1.0};
    double Z_eff{7.22};
    double A_eff{12.01};
    double meanExcitationEnergy_eV{75.0};

    // ── Sternheimer density-effect correction ──────────────────────
    SternheimerParams sternheimer{};

    // ── Characteristic lengths ─────────────────────────────────────
    double radiationLength_cm{36.08};
    double nuclearIntLength_cm{83.30};
    double moliereRadius_cm{9.00};

    // ── Biological dosimetry (ICRP 103) ───────────────────────────
    double WR{1.0};
    double WT{0.0};
    double alphaBetaRatio_Gy{10.0};
    std::string organName{};

    // ── Elemental composition ──────────────────────────────────────
    std::vector<Element> composition;
    double compositionSum{0.0};

    // ── Physical state ─────────────────────────────────────────────
    int state{1};   // 0=solid, 1=liquid, 2=gas
    double meltingPoint_K{273.15};
    double boilingPoint_K{373.15};
    double refractionIndex{1.0};
    double scintillationYield_photons_MeV{0.0};

    // ── Validation metadata ────────────────────────────────────────
    double I_eV_uncertainty{5.0};
    double densityUncertaintyRel{0.02};

    // ── Validation ─────────────────────────────────────────────────
    [[nodiscard]] bool isValid() const
    {
        return density_g_cm3 > 0.0 && Z_eff >= 1.0
            && A_eff >= Z_eff && meanExcitationEnergy_eV >= 10.0;
    }

    [[nodiscard]] bool isGas() const { return state == 2; }
    [[nodiscard]] bool isBiological() const
    {
        return category == MaterialCategory::Biological;
    }
    [[nodiscard]] bool hasOrganData() const
    {
        return WT > 0.0 && !organName.empty();
    }

    bool operator==(const Material& o) const { return id == o.id; }
    bool operator!=(const Material& o) const { return id != o.id; }
};

// ── JSON serialization ──────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const Material& m)
{
    j = {
        {"id", m.id},
        {"name", m.name},
        {"symbol", m.symbol},
        {"category", to_string(m.category)},
        {"reference", m.reference},
        {"density_g_cm3", m.density_g_cm3},
        {"Z_eff", m.Z_eff},
        {"A_eff", m.A_eff},
        {"meanExcitationEnergy_eV", m.meanExcitationEnergy_eV},
        {"sternheimer", m.sternheimer},
        {"radiationLength_cm", m.radiationLength_cm},
        {"nuclearIntLength_cm", m.nuclearIntLength_cm},
        {"moliereRadius_cm", m.moliereRadius_cm},
        {"WR", m.WR},
        {"WT", m.WT},
        {"alphaBetaRatio_Gy", m.alphaBetaRatio_Gy},
        {"organName", m.organName},
        {"composition", m.composition},
        {"state", m.state},
        {"meltingPoint_K", m.meltingPoint_K},
        {"boilingPoint_K", m.boilingPoint_K},
        {"refractionIndex", m.refractionIndex},
        {"scintillationYield_photons_MeV", m.scintillationYield_photons_MeV},
        {"I_eV_uncertainty", m.I_eV_uncertainty},
        {"densityUncertaintyRel", m.densityUncertaintyRel},
    };
}

inline void from_json(const nlohmann::json& j, Material& m)
{
    j.at("id").get_to(m.id);
    j.at("name").get_to(m.name);
    m.symbol = j.value("symbol", std::string{});
    m.category = category_from_string(j.value("category", std::string{"custom"}));
    m.reference = j.value("reference", std::string{});
    j.at("density_g_cm3").get_to(m.density_g_cm3);
    j.at("Z_eff").get_to(m.Z_eff);
    j.at("A_eff").get_to(m.A_eff);
    j.at("meanExcitationEnergy_eV").get_to(m.meanExcitationEnergy_eV);

    if (j.contains("sternheimer"))
        j.at("sternheimer").get_to(m.sternheimer);

    m.radiationLength_cm = j.value("radiationLength_cm", 0.0);  // optional; 0 = unknown
    m.nuclearIntLength_cm = j.value("nuclearIntLength_cm", 83.30);
    m.moliereRadius_cm = j.value("moliereRadius_cm", 9.0);
    m.WR = j.value("WR", 1.0);
    m.WT = j.value("WT", 0.0);
    m.alphaBetaRatio_Gy = j.value("alphaBetaRatio_Gy", 10.0);
    m.organName = j.value("organName", std::string{});

    if (j.contains("composition"))
        j.at("composition").get_to(m.composition);

    m.state = j.value("state", 1);
    m.meltingPoint_K = j.value("meltingPoint_K", 273.15);
    m.boilingPoint_K = j.value("boilingPoint_K", 373.15);
    m.refractionIndex = j.value("refractionIndex", 1.0);
    m.scintillationYield_photons_MeV = j.value("scintillationYield_photons_MeV", 0.0);
    m.I_eV_uncertainty = j.value("I_eV_uncertainty", 5.0);
    m.densityUncertaintyRel = j.value("densityUncertaintyRel", 0.02);
}

} // namespace beamlab::domain::materials
