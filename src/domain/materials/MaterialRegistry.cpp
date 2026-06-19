#include "domain/materials/MaterialRegistry.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace beamlab::domain::materials {

// ── Registration ─────────────────────────────────────────────────────

void MaterialRegistry::registerMaterial(std::string id, Material material)
{
    material.id = id;
    auto it = map_.find(id);
    if (it != map_.end() && it->second.isBuiltin) {
        material.isOverride = true;
    }
    map_[id] = {std::move(material), false};
}

void MaterialRegistry::registerMaterialsFromJson(const std::string& jsonPath)
{
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "[MaterialRegistry] Cannot open " << jsonPath << std::endl;
        return;
    }

    try {
        auto json = nlohmann::json::parse(file);
        if (!json.is_array()) {
            std::cerr << "[MaterialRegistry] Expected JSON array in "
                      << jsonPath << std::endl;
            return;
        }

        for (const auto& item : json) {
            Material m = item.get<Material>();
            registerMaterial(m.id, std::move(m));
        }
    } catch (const std::exception& e) {
        std::cerr << "[MaterialRegistry] Error parsing " << jsonPath
                  << ": " << e.what() << std::endl;
    }
}

// ── Query ────────────────────────────────────────────────────────────

const Material& MaterialRegistry::get(const std::string& id) const
{
    auto it = map_.find(id);
    if (it == map_.end()) {
        throw std::out_of_range("Material not found: " + id);
    }
    return it->second.material;
}

std::optional<std::reference_wrapper<const Material>>
MaterialRegistry::find(const std::string& id) const
{
    auto it = map_.find(id);
    if (it == map_.end()) return std::nullopt;
    return std::cref(it->second.material);
}

// ── Filter ───────────────────────────────────────────────────────────

std::vector<const Material*> MaterialRegistry::findByCategory(
    MaterialCategory category) const
{
    std::vector<const Material*> result;
    for (const auto& [_, entry] : map_) {
        if (entry.material.category == category) {
            result.push_back(&entry.material);
        }
    }
    return result;
}

std::vector<const Material*> MaterialRegistry::findByProperty(
    std::function<bool(const Material&)> predicate) const
{
    std::vector<const Material*> result;
    for (const auto& [_, entry] : map_) {
        if (predicate(entry.material)) {
            result.push_back(&entry.material);
        }
    }
    return result;
}

// ── Enumeration ──────────────────────────────────────────────────────

std::vector<std::string> MaterialRegistry::names() const
{
    std::vector<std::string> result;
    result.reserve(map_.size());
    for (const auto& [id, _] : map_) {
        result.push_back(id);
    }
    return result;
}

// ── Custom materials ─────────────────────────────────────────────────

void MaterialRegistry::addCustom(const Material& mat)
{
    registerMaterial(mat.id, mat);
}

bool MaterialRegistry::removeCustom(const std::string& id)
{
    auto it = map_.find(id);
    if (it == map_.end()) return false;
    if (it->second.isBuiltin) return false;       // Cannot remove built-in
    map_.erase(it);
    return true;
}

std::vector<const Material*> MaterialRegistry::customMaterials() const
{
    std::vector<const Material*> result;
    for (const auto& [_, entry] : map_) {
        if (!entry.isBuiltin) {
            result.push_back(&entry.material);
        }
    }
    return result;
}

// ── Built-in materials ───────────────────────────────────────────────

void MaterialRegistry::loadBuiltinMaterials()
{
    auto insert = [&](Material m) {
        auto id = m.id;
        if (map_.count(id)) return;  // Idempotent
        map_[id] = {std::move(m), true};
    };

    insert(makeWater());
    insert(makeMuscleSkeletal());
    insert(makeBoneCortical());
    insert(makeBoneSpongy());
    insert(makeAdiposeTissue());
    insert(makeBrainGrey());
    insert(makeBrainWhite());
    insert(makeLungInflated());
    insert(makeLungDeflated());
    insert(makeBlood());
    insert(makeSkin());
    insert(makeLiver());
    insert(makeEyeLens());
    insert(makePMMA());
    insert(makePolyethylene());
    insert(makePolystyrene());
    insert(makeGraphite());
    insert(makeSilicon());
    insert(makeGermanium());
    insert(makeAirSTP());
    insert(makeAirAtm20C());
    insert(makeArgonGas());
    insert(makeHeliumGas());
    insert(makeCO2Gas());
    insert(makeAluminum());
    insert(makeIron());
    insert(makeCopper());
    insert(makeLead());
    insert(makeTungsten());
    insert(makeGold());
    insert(makeTitanium());
    insert(makeStainlessSteel());
    insert(makeBeryllium());
    insert(makeTantalum());
    insert(makeKapton());
    insert(makeMylar());
    insert(makeCarbonFiber());
    insert(makeConcrete());
    insert(makeHydroxyapatite());
    insert(makePTFE());
    insert(makeA150TissueEquiv());
}

// ── Material factories ───────────────────────────────────────────────

Material MaterialRegistry::makeWater()
{
    Material m;
    m.id = "water_icru";
    m.name = "Water (ICRU-90)";
    m.symbol = "H\u2082O";
    m.category = MaterialCategory::Biological;
    m.reference = "ICRU Report 90 (2016): I=78 eV; composition ICRU-44 (1989)";
    m.density_g_cm3 = 1.000;            // [g/cm^3] liquid water, ICRU-90
    m.Z_eff = 7.22;                     // [dimensionless] effective atomic number
    m.A_eff = 13.00;                    // [g/mol] effective atomic mass
    // Mean excitation energy. ICRU Report 90 (2016) revised liquid water
    // from the older 75 eV (ICRU-37/49, used by NIST PSTAR) up to 78 eV.
    // This is the configurable "I-value knob" (S1/S8). A higher I lowers
    // dE/dx by ~0.8% in 10-250 MeV vs the PSTAR reference (Emfietzoglou &
    // Nikjoo 2009; ICRU-90), well inside the +-5% validation target.
    m.meanExcitationEnergy_eV = 78.0;   // [eV] ICRU-90 (2016)
    m.sternheimer = {0.09116, 3.4773, 0.2400, 2.8004, -3.5017, 0.0, true};
    m.radiationLength_cm = 36.08;
    m.nuclearIntLength_cm = 83.30;
    m.moliereRadius_cm = 9.00;
    m.state = 1;
    m.meltingPoint_K = 273.15;
    m.boilingPoint_K = 373.15;
    m.refractionIndex = 1.333;
    m.I_eV_uncertainty = 3.0;
    m.densityUncertaintyRel = 0.001;
    m.composition = {{"H", 1, 0.111898}, {"O", 8, 0.888102}};
    return m;
}

Material MaterialRegistry::makeMuscleSkeletal()
{
    Material m;
    m.id = "muscle_skeletal";
    m.name = "Muscle, skeletal (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.05;
    m.Z_eff = 7.64;
    m.A_eff = 13.46;
    m.meanExcitationEnergy_eV = 75.0;
    m.sternheimer = {0.08919, 3.4632, 0.2100, 2.7630, -3.5521, 0.0, true};
    m.radiationLength_cm = 36.08;
    m.I_eV_uncertainty = 3.0;
    m.composition = {{"H", 1, 0.101997}, {"C", 6, 0.123000}, {"N", 7, 0.035000},
                     {"O", 8, 0.730003}, {"Na", 11, 0.001000}, {"P", 15, 0.002000},
                     {"S", 16, 0.005000}, {"Cl", 17, 0.001000}, {"K", 19, 0.001000}};
    return m;
}

Material MaterialRegistry::makeBoneCortical()
{
    Material m;
    m.id = "bone_cortical";
    m.name = "Bone, cortical (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.85;
    m.Z_eff = 13.84;
    m.A_eff = 22.48;
    m.meanExcitationEnergy_eV = 106.4;
    m.sternheimer = {0.11189, 3.2794, 0.1500, 2.5230, -4.2435, 0.0, true};
    m.radiationLength_cm = 14.23;
    m.I_eV_uncertainty = 5.0;
    m.composition = {{"H", 1, 0.034000}, {"C", 6, 0.155000}, {"N", 7, 0.042000},
                     {"O", 8, 0.435000}, {"P", 15, 0.103000}, {"Ca", 20, 0.225000},
                     {"Mg", 12, 0.002000}, {"S", 16, 0.003000}, {"Na", 11, 0.001000}};
    return m;
}

Material MaterialRegistry::makeBoneSpongy()
{
    Material m;
    m.id = "bone_spongy";
    m.name = "Bone, spongy (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.18;
    m.Z_eff = 12.33;
    m.A_eff = 19.02;
    m.meanExcitationEnergy_eV = 91.9;
    m.sternheimer = {0.10697, 3.3883, 0.1500, 2.6540, -4.0117, 0.0, true};
    m.composition = {{"H", 1, 0.085000}, {"C", 6, 0.402000}, {"N", 7, 0.028000},
                     {"O", 8, 0.362000}, {"P", 15, 0.029000}, {"Ca", 20, 0.074000},
                     {"Mg", 12, 0.001000}, {"S", 16, 0.003000}, {"Na", 11, 0.001000}};
    return m;
}

Material MaterialRegistry::makeAdiposeTissue()
{
    Material m;
    m.id = "adipose_tissue";
    m.name = "Adipose tissue (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 0.95;
    m.Z_eff = 6.46;
    m.A_eff = 11.73;
    m.meanExcitationEnergy_eV = 63.2;
    m.sternheimer = {0.09838, 3.4820, 0.2100, 2.7840, -3.4140, 0.0, true};
    m.radiationLength_cm = 44.12;
    m.composition = {{"H", 1, 0.119000}, {"C", 6, 0.637000}, {"N", 7, 0.007000},
                     {"O", 8, 0.232000}, {"S", 16, 0.001000}, {"Cl", 17, 0.001000}};
    return m;
}

Material MaterialRegistry::makeBrainGrey()
{
    Material m;
    m.id = "brain_grey";
    m.name = "Brain, grey (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.04;
    m.Z_eff = 7.58;
    m.A_eff = 13.30;
    m.meanExcitationEnergy_eV = 73.5;
    m.sternheimer = {0.08594, 3.4470, 0.1900, 2.7500, -3.5470, 0.0, true};
    m.composition = {{"H", 1, 0.110000}, {"C", 6, 0.130000}, {"N", 7, 0.020000},
                     {"O", 8, 0.734000}, {"Na", 11, 0.002000}, {"Cl", 17, 0.003000},
                     {"K", 19, 0.001000}};
    return m;
}

Material MaterialRegistry::makeBrainWhite()
{
    Material m;
    m.id = "brain_white";
    m.name = "Brain, white (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.04;
    m.Z_eff = 7.48;
    m.A_eff = 13.08;
    m.meanExcitationEnergy_eV = 72.5;
    m.sternheimer = {0.09329, 3.4708, 0.2000, 2.7790, -3.5220, 0.0, true};
    m.composition = {{"H", 1, 0.112000}, {"C", 6, 0.155000}, {"N", 7, 0.016000},
                     {"O", 8, 0.712000}, {"Na", 11, 0.002000}, {"Cl", 17, 0.002000},
                     {"K", 19, 0.001000}};
    return m;
}

Material MaterialRegistry::makeLungInflated()
{
    Material m;
    m.id = "lung_inflated";
    m.name = "Lung, inflated (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 0.26;
    m.Z_eff = 7.66;
    m.A_eff = 13.48;
    m.meanExcitationEnergy_eV = 75.0;
    m.composition = {{"H", 1, 0.103000}, {"C", 6, 0.105000}, {"N", 7, 0.031000},
                     {"O", 8, 0.749000}, {"Na", 11, 0.002000}, {"P", 15, 0.001000},
                     {"S", 16, 0.003000}, {"Cl", 17, 0.003000}, {"K", 19, 0.002000}};
    return m;
}

Material MaterialRegistry::makeBlood()
{
    Material m;
    m.id = "blood";
    m.name = "Blood (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.06;
    m.Z_eff = 7.79;
    m.A_eff = 13.52;
    m.meanExcitationEnergy_eV = 75.0;
    m.composition = {{"H", 1, 0.102000}, {"C", 6, 0.110000}, {"N", 7, 0.033000},
                     {"O", 8, 0.745000}, {"Na", 11, 0.001000}, {"P", 15, 0.001000},
                     {"S", 16, 0.002000}, {"Cl", 17, 0.003000}, {"K", 19, 0.002000},
                     {"Fe", 26, 0.001000}};
    return m;
}

Material MaterialRegistry::makeSkin()
{
    Material m;
    m.id = "skin";
    m.name = "Skin (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.09;
    m.Z_eff = 7.72;
    m.A_eff = 13.38;
    m.meanExcitationEnergy_eV = 75.0;
    m.composition = {{"H", 1, 0.100000}, {"C", 6, 0.204000}, {"N", 7, 0.042000},
                     {"O", 8, 0.645000}, {"Na", 11, 0.002000}, {"P", 15, 0.001000},
                     {"S", 16, 0.002000}, {"Cl", 17, 0.003000}, {"K", 19, 0.001000}};
    return m;
}

Material MaterialRegistry::makeLiver()
{
    Material m;
    m.id = "liver";
    m.name = "Liver (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.05;
    m.Z_eff = 7.64;
    m.A_eff = 13.38;
    m.meanExcitationEnergy_eV = 74.2;
    m.composition = {{"H", 1, 0.102000}, {"C", 6, 0.139000}, {"N", 7, 0.030000},
                     {"O", 8, 0.716000}, {"Na", 11, 0.002000}, {"P", 15, 0.003000},
                     {"S", 16, 0.003000}, {"Cl", 17, 0.002000}, {"K", 19, 0.003000}};
    return m;
}

Material MaterialRegistry::makePMMA()
{
    Material m;
    m.id = "pmma";
    m.name = "PMMA (acrylic)";
    m.symbol = "C\u2085O\u2082H\u2088";
    m.category = MaterialCategory::Plastic;
    m.density_g_cm3 = 1.19;
    m.Z_eff = 6.56;
    m.A_eff = 12.01;
    m.meanExcitationEnergy_eV = 74.0;
    m.sternheimer = {0.08484, 3.4782, 0.1880, 2.8090, -3.4191, 0.0, true};
    m.radiationLength_cm = 35.34;
    m.composition = {{"H", 1, 0.080541}, {"C", 6, 0.599847}, {"O", 8, 0.319612}};
    return m;
}

Material MaterialRegistry::makeAirSTP()
{
    Material m;
    m.id = "air_stp";
    m.name = "Air (STP, dry)";
    m.category = MaterialCategory::Gas;
    m.density_g_cm3 = 0.001205;
    m.Z_eff = 7.64;
    m.A_eff = 14.37;
    m.meanExcitationEnergy_eV = 85.7;
    m.sternheimer = {0.10118, 3.5286, 1.3032, 3.2130, -4.0742, 0.0, true};
    m.radiationLength_cm = 30420.0;
    m.state = 2;
    m.boilingPoint_K = 79.0;
    m.composition = {{"C", 6, 0.000124}, {"N", 7, 0.755267},
                     {"O", 8, 0.231781}, {"Ar", 18, 0.012827}};
    return m;
}

Material MaterialRegistry::makeAluminum()
{
    Material m;
    m.id = "aluminum";
    m.name = "Aluminum";
    m.symbol = "Al";
    m.category = MaterialCategory::Metal;
    m.density_g_cm3 = 2.699;
    m.Z_eff = 13.0;
    m.A_eff = 26.98;
    m.meanExcitationEnergy_eV = 166.0;
    m.sternheimer = {0.17541, 2.9470, 0.2200, 2.4390, -4.2395, 0.0, true};
    m.radiationLength_cm = 8.897;
    m.state = 0;
    m.meltingPoint_K = 933.5;
    m.boilingPoint_K = 2792.0;
    m.composition = {{"Al", 13, 1.0}};
    return m;
}

Material MaterialRegistry::makeLead()
{
    Material m;
    m.id = "lead";
    m.name = "Lead";
    m.symbol = "Pb";
    m.category = MaterialCategory::Shielding;
    m.density_g_cm3 = 11.35;
    m.Z_eff = 82.0;
    m.A_eff = 207.2;
    m.meanExcitationEnergy_eV = 823.0;
    m.sternheimer = {0.15488, 2.9632, 0.4300, 2.7900, -5.3856, 0.0, true};
    m.radiationLength_cm = 0.561;
    m.state = 0;
    m.meltingPoint_K = 600.6;
    m.boilingPoint_K = 2022.0;
    m.composition = {{"Pb", 82, 1.0}};
    return m;
}

Material MaterialRegistry::makeCopper()
{
    Material m;
    m.id = "copper";
    m.name = "Copper";
    m.symbol = "Cu";
    m.category = MaterialCategory::Metal;
    m.density_g_cm3 = 8.96;
    m.Z_eff = 29.0;
    m.A_eff = 63.546;
    m.meanExcitationEnergy_eV = 322.0;
    m.sternheimer = {0.14129, 2.9048, 0.2600, 2.4560, -4.8490, 0.0, true};
    m.radiationLength_cm = 1.436;
    m.state = 0;
    m.meltingPoint_K = 1358.0;
    m.boilingPoint_K = 3200.0;
    m.composition = {{"Cu", 29, 1.0}};
    return m;
}

Material MaterialRegistry::makeTungsten()
{
    Material m;
    m.id = "tungsten";
    m.name = "Tungsten";
    m.symbol = "W";
    m.category = MaterialCategory::Metal;
    m.density_g_cm3 = 19.3;
    m.Z_eff = 74.0;
    m.A_eff = 183.84;
    m.meanExcitationEnergy_eV = 727.0;
    m.sternheimer = {0.15276, 2.9471, 0.3600, 2.5310, -5.1696, 0.0, true};
    m.radiationLength_cm = 0.350;
    m.state = 0;
    m.meltingPoint_K = 3695.0;
    m.boilingPoint_K = 5828.0;
    m.composition = {{"W", 74, 1.0}};
    return m;
}

Material MaterialRegistry::makeGold()
{
    Material m;
    m.id = "gold";
    m.name = "Gold";
    m.symbol = "Au";
    m.category = MaterialCategory::Metal;
    m.density_g_cm3 = 19.32;
    m.Z_eff = 79.0;
    m.A_eff = 196.967;
    m.meanExcitationEnergy_eV = 790.0;
    m.sternheimer = {0.15622, 2.9561, 0.3900, 2.5810, -5.2945, 0.0, true};
    m.radiationLength_cm = 0.338;
    m.state = 0;
    m.meltingPoint_K = 1337.0;
    m.boilingPoint_K = 3129.0;
    m.composition = {{"Au", 79, 1.0}};
    return m;
}

Material MaterialRegistry::makeIron()
{
    Material m;
    m.id = "iron";
    m.name = "Iron";
    m.symbol = "Fe";
    m.category = MaterialCategory::Metal;
    m.density_g_cm3 = 7.87;
    m.Z_eff = 26.0;
    m.A_eff = 55.845;
    m.meanExcitationEnergy_eV = 286.0;
    m.sternheimer = {0.13801, 2.9125, 0.2500, 2.4630, -4.7588, 0.0, true};
    m.radiationLength_cm = 1.757;
    m.state = 0;
    m.meltingPoint_K = 1811.0;
    m.boilingPoint_K = 3134.0;
    m.composition = {{"Fe", 26, 1.0}};
    return m;
}

Material MaterialRegistry::makePolyethylene()
{
    Material m;
    m.id = "polyethylene";
    m.name = "Polyethylene";
    m.symbol = "C\u2082H\u2084";
    m.category = MaterialCategory::Plastic;
    m.density_g_cm3 = 0.93;
    m.Z_eff = 5.53;
    m.A_eff = 10.08;
    m.meanExcitationEnergy_eV = 57.4;
    m.sternheimer = {0.09414, 3.4136, 0.1400, 2.7500, -3.2660, 0.0, true};
    m.radiationLength_cm = 44.12;
    m.state = 0;
    m.meltingPoint_K = 383.0;
    m.composition = {{"H", 1, 0.143716}, {"C", 6, 0.856284}};
    return m;
}

Material MaterialRegistry::makeGraphite()
{
    Material m;
    m.id = "graphite";
    m.name = "Graphite";
    m.symbol = "C";
    m.category = MaterialCategory::Detector;
    m.density_g_cm3 = 2.210;
    m.Z_eff = 6.0;
    m.A_eff = 12.01;
    m.meanExcitationEnergy_eV = 78.0;
    m.sternheimer = {0.14244, 3.1252, 0.1800, 2.6170, -3.5965, 0.0, true};
    m.radiationLength_cm = 18.79;
    m.state = 0;
    m.meltingPoint_K = 3823.0;
    m.boilingPoint_K = 4098.0;
    m.composition = {{"C", 6, 1.0}};
    return m;
}

Material MaterialRegistry::makeSilicon()
{
    Material m;
    m.id = "silicon";
    m.name = "Silicon";
    m.symbol = "Si";
    m.category = MaterialCategory::Detector;
    m.density_g_cm3 = 2.33;
    m.Z_eff = 14.0;
    m.A_eff = 28.086;
    m.meanExcitationEnergy_eV = 173.0;
    m.sternheimer = {0.14922, 2.9998, 0.2300, 2.5170, -4.1924, 0.0, true};
    m.radiationLength_cm = 9.36;
    m.state = 0;
    m.meltingPoint_K = 1687.0;
    m.boilingPoint_K = 3538.0;
    m.composition = {{"Si", 14, 1.0}};
    return m;
}

Material MaterialRegistry::makeGermanium()
{
    Material m;
    m.id = "germanium";
    m.name = "Germanium";
    m.symbol = "Ge";
    m.category = MaterialCategory::Detector;
    m.density_g_cm3 = 5.323;
    m.Z_eff = 32.0;
    m.A_eff = 72.630;
    m.meanExcitationEnergy_eV = 350.0;
    m.sternheimer = {0.13529, 3.0026, 0.3000, 2.5770, -5.0055, 0.0, true};
    m.radiationLength_cm = 2.30;
    m.state = 0;
    m.meltingPoint_K = 1211.0;
    m.boilingPoint_K = 3106.0;
    m.composition = {{"Ge", 32, 1.0}};
    return m;
}

Material MaterialRegistry::makeConcrete()
{
    Material m;
    m.id = "concrete";
    m.name = "Concrete (ordinary)";
    m.category = MaterialCategory::Phantom;
    m.density_g_cm3 = 2.30;
    m.Z_eff = 13.46;
    m.A_eff = 21.68;
    m.meanExcitationEnergy_eV = 108.0;
    m.sternheimer = {0.10893, 3.3132, 0.1600, 2.5670, -4.1417, 0.0, true};
    m.radiationLength_cm = 10.70;
    m.state = 0;
    m.composition = {{"H", 1, 0.010000}, {"C", 6, 0.001000}, {"O", 8, 0.529000},
                     {"Mg", 12, 0.002000}, {"Al", 13, 0.034000}, {"Si", 14, 0.337000},
                     {"Ca", 20, 0.044000}, {"Fe", 26, 0.014000}};
    return m;
}

// ── Stubs for remaining materials ─────────────────────────────────────
// Full implementation maps fields from the existing BioMaterialLibrary
// for all 55+ materials.  Stubs below provide the skeleton.

Material MaterialRegistry::makeStainlessSteel()
{
    Material m;
    m.id = "stainless_steel_304";
    m.name = "Stainless Steel 304";
    m.category = MaterialCategory::Metal;
    m.density_g_cm3 = 8.00;
    m.Z_eff = 25.86;
    m.A_eff = 55.84;
    m.meanExcitationEnergy_eV = 312.0;
    m.radiationLength_cm = 1.75;
    m.state = 0;
    m.meltingPoint_K = 1673.0;
    m.composition = {{"Fe", 26, 0.695}, {"Cr", 24, 0.190},
                     {"Ni", 28, 0.095}, {"Mn", 25, 0.020}};
    return m;
}

Material MaterialRegistry::makeBeryllium()
{
    Material m;
    m.id = "beryllium";
    m.name = "Beryllium";
    m.symbol = "Be";
    m.category = MaterialCategory::Metal;
    m.density_g_cm3 = 1.848;
    m.Z_eff = 4.0;
    m.A_eff = 9.012;
    m.meanExcitationEnergy_eV = 63.7;
    m.sternheimer = {0.27296, 2.9730, 0.1200, 2.0700, -2.4793, 0.0, true};
    m.radiationLength_cm = 35.28;
    m.state = 0;
    m.meltingPoint_K = 1560.0;
    m.boilingPoint_K = 2742.0;
    m.composition = {{"Be", 4, 1.0}};
    return m;
}

Material MaterialRegistry::makeTitanium()
{
    Material m;
    m.id = "titanium";
    m.name = "Titanium";
    m.symbol = "Ti";
    m.category = MaterialCategory::Metal;
    m.density_g_cm3 = 4.54;
    m.Z_eff = 22.0;
    m.A_eff = 47.867;
    m.meanExcitationEnergy_eV = 233.0;
    m.radiationLength_cm = 3.56;
    m.state = 0;
    m.meltingPoint_K = 1941.0;
    m.composition = {{"Ti", 22, 1.0}};
    return m;
}

Material MaterialRegistry::makePTFE()
{
    Material m;
    m.id = "ptfe";
    m.name = "PTFE (Teflon)";
    m.symbol = "C\u2082F\u2084";
    m.category = MaterialCategory::Plastic;
    m.density_g_cm3 = 2.20;
    m.Z_eff = 8.50;
    m.A_eff = 16.23;
    m.meanExcitationEnergy_eV = 99.1;
    m.sternheimer = {0.07568, 3.7007, 0.2840, 3.0600, -3.3957, 0.0, true};
    m.radiationLength_cm = 16.23;
    m.state = 0;
    m.meltingPoint_K = 600.0;
    m.composition = {{"C", 6, 0.240183}, {"F", 9, 0.759817}};
    return m;
}

Material MaterialRegistry::makeA150TissueEquiv()
{
    Material m;
    m.id = "a150_tissue_equiv";
    m.name = "A-150 Tissue-Equivalent Plastic";
    m.category = MaterialCategory::Phantom;
    m.density_g_cm3 = 1.127;
    m.Z_eff = 7.28;
    m.A_eff = 12.98;
    m.meanExcitationEnergy_eV = 68.6;
    m.sternheimer = {0.08930, 3.4991, 0.1900, 2.8130, -3.4543, 0.0, true};
    m.radiationLength_cm = 37.54;
    m.composition = {{"H", 1, 0.101327}, {"C", 6, 0.775501}, {"N", 7, 0.035057},
                     {"O", 8, 0.052316}, {"F", 9, 0.017422}, {"Ca", 20, 0.018378}};
    return m;
}

Material MaterialRegistry::makeEyeLens()
{
    Material m;
    m.id = "eye_lens";
    m.name = "Eye lens (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.07;
    m.Z_eff = 7.53;
    m.A_eff = 13.07;
    m.meanExcitationEnergy_eV = 73.0;
    m.composition = {{"H", 1, 0.096000}, {"C", 6, 0.239000}, {"N", 7, 0.060000},
                     {"O", 8, 0.597000}, {"Na", 11, 0.002000}, {"Cl", 17, 0.001000},
                     {"K", 19, 0.002000}};
    return m;
}

Material MaterialRegistry::makeKapton()
{
    Material m;
    m.id = "kapton";
    m.name = "Kapton (polyimide)";
    m.symbol = "C\u2082\u2082H\u2081\u2080N\u2082O\u2085";
    m.category = MaterialCategory::Plastic;
    m.density_g_cm3 = 1.42;
    m.Z_eff = 6.97;
    m.A_eff = 13.10;
    m.meanExcitationEnergy_eV = 78.6;
    m.radiationLength_cm = 28.57;
    m.state = 0;
    m.composition = {{"H", 1, 0.026362}, {"C", 6, 0.691133},
                     {"N", 7, 0.073270}, {"O", 8, 0.209235}};
    return m;
}

Material MaterialRegistry::makeMylar()
{
    Material m;
    m.id = "mylar";
    m.name = "Mylar (polyester)";
    m.symbol = "C\u2081\u2080H\u2088O\u2084";
    m.category = MaterialCategory::Plastic;
    m.density_g_cm3 = 1.40;
    m.Z_eff = 6.61;
    m.A_eff = 12.29;
    m.meanExcitationEnergy_eV = 78.7;
    m.radiationLength_cm = 28.68;
    m.state = 0;
    m.composition = {{"H", 1, 0.041959}, {"C", 6, 0.625016}, {"O", 8, 0.333025}};
    return m;
}

Material MaterialRegistry::makeHydroxyapatite()
{
    Material m;
    m.id = "hydroxyapatite";
    m.name = "Hydroxyapatite";
    m.symbol = "Ca\u2081\u2080(PO\u2084)\u2086(OH)\u2082";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 3.16;
    m.Z_eff = 16.12;
    m.A_eff = 27.76;
    m.meanExcitationEnergy_eV = 128.0;
    m.state = 0;
    m.composition = {{"H", 1, 0.002008}, {"O", 8, 0.414652},
                     {"P", 15, 0.185713}, {"Ca", 20, 0.397627}};
    return m;
}

Material MaterialRegistry::makeCarbonFiber()
{
    Material m;
    m.id = "carbon_fiber";
    m.name = "Carbon fiber";
    m.category = MaterialCategory::Plastic;
    m.density_g_cm3 = 1.75;
    m.Z_eff = 6.0;
    m.A_eff = 12.01;
    m.meanExcitationEnergy_eV = 78.0;
    m.radiationLength_cm = 23.74;
    m.state = 0;
    m.meltingPoint_K = 3650.0;
    m.composition = {{"C", 6, 1.0}};
    return m;
}

Material MaterialRegistry::makeTantalum()
{
    Material m;
    m.id = "tantalum";
    m.name = "Tantalum";
    m.symbol = "Ta";
    m.category = MaterialCategory::Metal;
    m.density_g_cm3 = 16.654;
    m.Z_eff = 73.0;
    m.A_eff = 180.948;
    m.meanExcitationEnergy_eV = 718.0;
    m.radiationLength_cm = 0.410;
    m.state = 0;
    m.meltingPoint_K = 3290.0;
    m.boilingPoint_K = 5731.0;
    m.composition = {{"Ta", 73, 1.0}};
    return m;
}

Material MaterialRegistry::makePolystyrene()
{
    Material m;
    m.id = "polystyrene";
    m.name = "Polystyrene";
    m.symbol = "C\u2088H\u2088";
    m.category = MaterialCategory::Plastic;
    m.density_g_cm3 = 1.06;
    m.Z_eff = 5.75;
    m.A_eff = 10.42;
    m.meanExcitationEnergy_eV = 66.4;
    m.sternheimer = {0.09780, 3.4621, 0.1800, 2.7440, -3.3130, 0.0, true};
    m.radiationLength_cm = 41.12;
    m.state = 0;
    m.meltingPoint_K = 513.0;
    m.composition = {{"H", 1, 0.077419}, {"C", 6, 0.922581}};
    return m;
}

Material MaterialRegistry::makeArgonGas()
{
    Material m;
    m.id = "argon_gas";
    m.name = "Argon gas";
    m.symbol = "Ar";
    m.category = MaterialCategory::Gas;
    m.density_g_cm3 = 0.001782;
    m.Z_eff = 18.0;
    m.A_eff = 39.948;
    m.meanExcitationEnergy_eV = 188.0;
    m.radiationLength_cm = 11700.0;
    m.state = 2;
    m.boilingPoint_K = 87.3;
    m.composition = {{"Ar", 18, 1.0}};
    return m;
}

Material MaterialRegistry::makeHeliumGas()
{
    Material m;
    m.id = "helium_gas";
    m.name = "Helium gas";
    m.symbol = "He";
    m.category = MaterialCategory::Gas;
    m.density_g_cm3 = 0.0001785;
    m.Z_eff = 2.0;
    m.A_eff = 4.003;
    m.meanExcitationEnergy_eV = 41.8;
    m.radiationLength_cm = 567000.0;
    m.state = 2;
    m.boilingPoint_K = 4.2;
    m.composition = {{"He", 2, 1.0}};
    return m;
}

Material MaterialRegistry::makeCO2Gas()
{
    Material m;
    m.id = "co2_gas";
    m.name = "Carbon dioxide gas";
    m.symbol = "CO\u2082";
    m.category = MaterialCategory::Gas;
    m.density_g_cm3 = 0.001842;
    m.Z_eff = 7.64;
    m.A_eff = 14.37;
    m.meanExcitationEnergy_eV = 85.0;
    m.state = 2;
    m.composition = {{"C", 6, 0.272916}, {"O", 8, 0.727084}};
    return m;
}

Material MaterialRegistry::makeAirAtm20C()
{
    Material m;
    m.id = "air_atm_20c";
    m.name = "Air (1 atm, 20\u00B0C)";
    m.category = MaterialCategory::Gas;
    m.density_g_cm3 = 0.001205;
    m.Z_eff = 7.64;
    m.A_eff = 14.37;
    m.meanExcitationEnergy_eV = 85.7;
    m.radiationLength_cm = 30420.0;
    m.state = 2;
    m.composition = {{"N", 7, 0.755267}, {"O", 8, 0.231781},
                     {"Ar", 18, 0.012827}, {"C", 6, 0.000124}};
    return m;
}

// ── LungDeflated ─────────────────────────────────────────────────────

Material MaterialRegistry::makeLungDeflated()
{
    Material m;
    m.id = "lung_deflated";
    m.name = "Lung, deflated (ICRU-44)";
    m.category = MaterialCategory::Biological;
    m.density_g_cm3 = 1.05;
    m.Z_eff = 7.66;
    m.A_eff = 13.48;
    m.meanExcitationEnergy_eV = 75.0;
    m.composition = makeLungInflated().composition;  // Same composition, different density
    return m;
}

} // namespace beamlab::domain::materials
