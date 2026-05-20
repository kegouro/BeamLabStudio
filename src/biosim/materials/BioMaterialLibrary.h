#pragma once

#include "biosim/materials/BioMaterial.h"

#include <optional>
#include <string>
#include <vector>

namespace beamlab::biosim {

// Catalogue of 55+ biological and detector materials.
//
// Sources:
//   ICRU Report 44  — tissue compositions
//   ICRU Report 49  — stopping powers, I values
//   NIST PSTAR      — proton stopping powers
//   Sternheimer et al. At.Data Nucl.Data Tables 30 (1984) — density correction
//   ICRP 103 (2007) — WR and WT
class BioMaterialLibrary {
public:
    BioMaterialLibrary();

    [[nodiscard]] const std::vector<BioMaterial>& all() const;

    // Find by exact id (case-insensitive). Returns nullopt if not found.
    [[nodiscard]] std::optional<BioMaterial> findById(const std::string& id) const;

    // Find by name or symbol substring (case-insensitive). Returns first match.
    [[nodiscard]] std::optional<BioMaterial> findByName(const std::string& name) const;

    // All materials in a given category.
    [[nodiscard]] std::vector<BioMaterial> byCategory(const std::string& category) const;

    // Add or replace a custom material. Persisted separately by the UI layer.
    void addCustom(const BioMaterial& mat);
    void removeCustom(const std::string& id);

    // Convenience static factories (used internally and by tests)
    static BioMaterial water();
    static BioMaterial muscleSkeletalICRU();
    static BioMaterial boneCorticalICRU();
    static BioMaterial boneSpongy();
    static BioMaterial adiposeTissue();
    static BioMaterial brainGrey();
    static BioMaterial brainWhite();
    static BioMaterial lungInflated();
    static BioMaterial lungDeflated();
    static BioMaterial blood();
    static BioMaterial skin();
    static BioMaterial liver();
    static BioMaterial eyeLens();
    static BioMaterial pmma();
    static BioMaterial polyethylene();
    static BioMaterial polystyrene();
    static BioMaterial graphite();
    static BioMaterial silicon();
    static BioMaterial germanium();
    static BioMaterial naiCrystal();
    static BioMaterial bgoCrystal();
    static BioMaterial lysoCrystal();
    static BioMaterial csiCrystal();
    static BioMaterial airSTP();
    static BioMaterial airAtm20C();
    static BioMaterial argonGas();
    static BioMaterial heliumGas();
    static BioMaterial co2Gas();
    static BioMaterial aluminum();
    static BioMaterial iron();
    static BioMaterial copper();
    static BioMaterial lead();
    static BioMaterial tungsten();
    static BioMaterial gold();
    static BioMaterial titanium();
    static BioMaterial stainlessSteel304();
    static BioMaterial beryllium();
    static BioMaterial tantalum();
    static BioMaterial uranium();
    static BioMaterial bismuth();
    static BioMaterial kapton();
    static BioMaterial mylar();
    static BioMaterial carbonFiber();
    static BioMaterial boratedPolyethylene();
    static BioMaterial paraffin();
    static BioMaterial concrete();
    static BioMaterial waterEquivCement();
    static BioMaterial hydroxyapatite();
    static BioMaterial ptfe();
    static BioMaterial solidWaterRW3();
    static BioMaterial virtualWater();
    static BioMaterial A150TissueEquiv();
    static BioMaterial muscleEquivPhantom();
    static BioMaterial boneEquivPhantom();

private:
    std::vector<BioMaterial> materials_{};
};

} // namespace beamlab::biosim
