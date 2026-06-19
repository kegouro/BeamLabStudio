#pragma once

#include "domain/materials/Material.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace beamlab::domain::materials {

/// O(1) registry of named Materials.
///
/// Two tiers:
///   - **Built-in** materials (55+ ICRU/NIST presets) loaded via
///     loadBuiltinMaterials() or registerMaterialsFromJson().
///     Immutable at runtime — never modified after registration.
///   - **Custom** materials added/removed by the user via addCustom()
///     /removeCustom().  Mutually visible — a custom with the same
///     name overloads a built-in (marked isOverride=true).
///
/// Thread-safety: not built-in.  Register during startup, then
/// query from any thread during analysis.
class MaterialRegistry {
public:
    MaterialRegistry() = default;

    // ── Registration ───────────────────────────────────────────────

    /// Register a single material.  Overwrites any existing entry
    /// with the same id.  If the previous entry was built-in,
    /// the new one gets isOverride = true.
    void registerMaterial(std::string id, Material material);

    /// Load materials from a JSON file.
    /// Expected format: array of Material objects.
    void registerMaterialsFromJson(const std::string& jsonPath);

    /// Populate the registry with all 55+ built-in ICRU/NIST
    /// materials.  Safe to call multiple times (idempotent —
    /// skips already-registered built-in ids).
    void loadBuiltinMaterials();

    // ── Query (O(1)) ───────────────────────────────────────────────

    /// Get material by id.  Throws std::out_of_range if not found.
    [[nodiscard]] const Material& get(const std::string& id) const;

    /// Find material by id.  Returns nullopt if not found.
    [[nodiscard]] std::optional<
        std::reference_wrapper<const Material>> find(const std::string& id) const;

    // ── Filter ─────────────────────────────────────────────────────

    [[nodiscard]] std::vector<const Material*> findByCategory(
        MaterialCategory category) const;

    [[nodiscard]] std::vector<const Material*> findByProperty(
        std::function<bool(const Material&)> predicate) const;

    // ── Enumeration ────────────────────────────────────────────────

    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] std::size_t count() const { return map_.size(); }

    // ── Custom materials ───────────────────────────────────────────

    void addCustom(const Material& mat);
    bool removeCustom(const std::string& id);
    [[nodiscard]] std::vector<const Material*> customMaterials() const;

private:
    struct InternalMaterial {
        Material material;
        bool isBuiltin{false};
    };

    std::unordered_map<std::string, InternalMaterial> map_;

    // Built-in material factories (modelled after BioMaterialLibrary).
    static Material makeWater();
    static Material makeMuscleSkeletal();
    static Material makeBoneCortical();
    static Material makeBoneSpongy();
    static Material makeAdiposeTissue();
    static Material makeBrainGrey();
    static Material makeBrainWhite();
    static Material makeLungInflated();
    static Material makeLungDeflated();
    static Material makeBlood();
    static Material makeSkin();
    static Material makeLiver();
    static Material makeEyeLens();
    static Material makePMMA();
    static Material makePolyethylene();
    static Material makePolystyrene();
    static Material makeGraphite();
    static Material makeSilicon();
    static Material makeGermanium();
    static Material makeAirSTP();
    static Material makeAirAtm20C();
    static Material makeArgonGas();
    static Material makeHeliumGas();
    static Material makeCO2Gas();
    static Material makeAluminum();
    static Material makeIron();
    static Material makeCopper();
    static Material makeLead();
    static Material makeTungsten();
    static Material makeGold();
    static Material makeTitanium();
    static Material makeStainlessSteel();
    static Material makeBeryllium();
    static Material makeTantalum();
    static Material makeKapton();
    static Material makeMylar();
    static Material makeCarbonFiber();
    static Material makeConcrete();
    static Material makeHydroxyapatite();
    static Material makePTFE();
    static Material makeA150TissueEquiv();
};

} // namespace beamlab::domain::materials
