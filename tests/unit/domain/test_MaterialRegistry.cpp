#include "domain/materials/MaterialRegistry.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace beamlab::domain::materials;

// ── Fixture ─────────────────────────────────────────────────────────

class MaterialRegistryTest : public ::testing::Test {
protected:
    MaterialRegistry reg;

    void SetUp() override
    {
        reg.loadBuiltinMaterials();
    }
};

// ── Tests ────────────────────────────────────────────────────────────

TEST_F(MaterialRegistryTest, CountBuiltinMaterials)
{
    EXPECT_GE(reg.count(), 40u);
}

TEST_F(MaterialRegistryTest, O1LookupByName)
{
    const auto& water = reg.get("water_icru");
    EXPECT_EQ(water.name, "Water (ICRU-44)");
    EXPECT_NEAR(water.density_g_cm3, 1.000, 1e-6);
    EXPECT_NEAR(water.meanExcitationEnergy_eV, 75.0, 1e-6);
}

TEST_F(MaterialRegistryTest, GetThrowsForMissingMaterial)
{
    EXPECT_THROW(reg.get("nonexistent_material"), std::out_of_range);
}

TEST_F(MaterialRegistryTest, FindReturnsNulloptForMissing)
{
    auto result = reg.find("does_not_exist");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MaterialRegistryTest, FindReturnsMaterialForValidId)
{
    auto result = reg.find("water_icru");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get().symbol, "H\u2082O");
}

TEST_F(MaterialRegistryTest, FilterByCategory)
{
    auto gases = reg.findByCategory(MaterialCategory::Gas);
    EXPECT_GE(gases.size(), 3u);
    for (const auto* g : gases) {
        EXPECT_TRUE(g->isGas());
    }

    auto metals = reg.findByCategory(MaterialCategory::Metal);
    EXPECT_GE(metals.size(), 7u);
}

TEST_F(MaterialRegistryTest, FilterByProperty)
{
    // Find all materials with density > 10 g/cm³.
    auto dense = reg.findByProperty(
        [](const Material& m) { return m.density_g_cm3 > 10.0; });
    EXPECT_GE(dense.size(), 3u);

    // Find all biological materials with organ data.
    auto organs = reg.findByProperty(
        [](const Material& m) { return m.hasOrganData(); });
    // None of the built-in materials currently have organ data set.
    EXPECT_TRUE(organs.empty());
}

TEST_F(MaterialRegistryTest, Names)
{
    auto names = reg.names();
    EXPECT_GE(names.size(), 40u);

    // Names should include water.
    bool hasWater = false;
    for (const auto& n : names) {
        if (n == "water_icru") { hasWater = true; break; }
    }
    EXPECT_TRUE(hasWater);
}

TEST_F(MaterialRegistryTest, AddCustomMaterial)
{
    Material custom;
    custom.id = "my_tissue";
    custom.name = "My Custom Tissue";
    custom.density_g_cm3 = 1.05;
    custom.Z_eff = 7.5;
    custom.A_eff = 13.0;
    custom.meanExcitationEnergy_eV = 72.0;
    custom.category = MaterialCategory::Custom;

    reg.addCustom(custom);

    // Must be findable.
    auto found = reg.find("my_tissue");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->get().density_g_cm3, 1.05);
}

TEST_F(MaterialRegistryTest, RemoveCustomMaterial)
{
    Material custom;
    custom.id = "temp_material";
    custom.name = "Temporary";
    custom.density_g_cm3 = 1.0;
    custom.Z_eff = 7.0;
    custom.A_eff = 12.0;
    custom.meanExcitationEnergy_eV = 70.0;
    custom.category = MaterialCategory::Custom;

    reg.addCustom(custom);
    EXPECT_TRUE(reg.find("temp_material").has_value());

    bool removed = reg.removeCustom("temp_material");
    EXPECT_TRUE(removed);
    EXPECT_FALSE(reg.find("temp_material").has_value());
}

TEST_F(MaterialRegistryTest, CannotRemoveBuiltinMaterial)
{
    bool removed = reg.removeCustom("water_icru");
    EXPECT_FALSE(removed);  // Built-in materials cannot be removed

    // Water must still be accessible.
    EXPECT_TRUE(reg.find("water_icru").has_value());
}

TEST_F(MaterialRegistryTest, CustomOverridesBuiltin)
{
    // Override water with a custom version.
    Material customWater;
    customWater.id = "water_icru";
    customWater.name = "Custom Override Water";
    customWater.density_g_cm3 = 0.999;
    customWater.Z_eff = 7.22;
    customWater.A_eff = 13.00;
    customWater.meanExcitationEnergy_eV = 75.0;

    reg.addCustom(customWater);

    const auto& retrieved = reg.get("water_icru");
    EXPECT_TRUE(retrieved.isOverride);
    EXPECT_NEAR(retrieved.density_g_cm3, 0.999, 1e-6);
}

TEST_F(MaterialRegistryTest, ListCustomMaterials)
{
    auto initial = reg.customMaterials();
    std::size_t initialCount = initial.size();

    Material custom1;
    custom1.id = "custom_a";
    custom1.name = "Custom A";
    custom1.density_g_cm3 = 1.0;
    custom1.Z_eff = 7.0;
    custom1.A_eff = 12.0;
    custom1.meanExcitationEnergy_eV = 70.0;
    custom1.category = MaterialCategory::Custom;
    reg.addCustom(custom1);

    Material custom2;
    custom2.id = "custom_b";
    custom2.name = "Custom B";
    custom2.density_g_cm3 = 1.0;
    custom2.Z_eff = 7.0;
    custom2.A_eff = 12.0;
    custom2.meanExcitationEnergy_eV = 70.0;
    custom2.category = MaterialCategory::Custom;
    reg.addCustom(custom2);

    auto allCustom = reg.customMaterials();
    EXPECT_EQ(allCustom.size(), initialCount + 2);
}

TEST_F(MaterialRegistryTest, IdempotentLoadBuiltins)
{
    std::size_t before = reg.count();
    reg.loadBuiltinMaterials();  // Second call — should be a no-op.
    EXPECT_EQ(reg.count(), before);
}

TEST_F(MaterialRegistryTest, JSONSerializationRoundTrip)
{
    Material m;
    m.id = "json_test";
    m.name = "JSON Test Material";
    m.density_g_cm3 = 2.5;
    m.Z_eff = 10.0;
    m.A_eff = 20.0;
    m.meanExcitationEnergy_eV = 150.0;
    m.category = MaterialCategory::Phantom;
    m.composition = {{"C", 6, 0.5}, {"O", 8, 0.5}};

    // Serialize to JSON.
    nlohmann::json j = m;

    // Deserialize back.
    Material restored = j.get<Material>();

    EXPECT_EQ(restored.id, m.id);
    EXPECT_EQ(restored.name, m.name);
    EXPECT_NEAR(restored.density_g_cm3, m.density_g_cm3, 1e-9);
    EXPECT_EQ(restored.category, m.category);
    ASSERT_EQ(restored.composition.size(), m.composition.size());
    EXPECT_EQ(restored.composition[0].symbol, "C");
    EXPECT_NEAR(restored.composition[1].massFraction, 0.5, 1e-9);
}

TEST_F(MaterialRegistryTest, LoadMaterialsFromJson)
{
    // Create a temporary JSON file.
    auto tmpPath = std::string("/tmp/test_materials.json");
    {
        nlohmann::json arr = nlohmann::json::array();
        arr.push_back({
            {"id", "test_json_water"},
            {"name", "Test JSON Water"},
            {"density_g_cm3", 0.998},
            {"Z_eff", 7.22},
            {"A_eff", 13.00},
            {"meanExcitationEnergy_eV", 75.0},
            {"category", "biological"},
            {"composition", {
                {{"symbol", "H"}, {"Z", 1}, {"massFraction", 0.111898}},
                {{"symbol", "O"}, {"Z", 8}, {"massFraction", 0.888102}}
            }},
            {"state", 1},
        });
        arr.push_back({
            {"id", "test_json_air"},
            {"name", "Test JSON Air"},
            {"density_g_cm3", 0.001205},
            {"Z_eff", 7.64},
            {"A_eff", 14.37},
            {"meanExcitationEnergy_eV", 85.7},
            {"category", "gas"},
            {"state", 2},
        });

        std::ofstream out(tmpPath);
        out << arr.dump(2);
        out.close();
    }

    MaterialRegistry jsonReg;
    jsonReg.registerMaterialsFromJson(tmpPath);

    EXPECT_EQ(jsonReg.count(), 2u);
    EXPECT_TRUE(jsonReg.find("test_json_water").has_value());
    EXPECT_TRUE(jsonReg.find("test_json_air").has_value());

    const auto& water = jsonReg.get("test_json_water");
    EXPECT_NEAR(water.density_g_cm3, 0.998, 1e-9);
    EXPECT_EQ(water.category, MaterialCategory::Biological);

    std::remove(tmpPath.c_str());
}

TEST_F(MaterialRegistryTest, WaterPhysicsProperties)
{
    const auto& water = reg.get("water_icru");

    // Bethe-Bloch parameters.
    EXPECT_NEAR(water.Z_eff, 7.22, 1e-6);
    EXPECT_NEAR(water.A_eff, 13.00, 1e-6);
    EXPECT_NEAR(water.meanExcitationEnergy_eV, 75.0, 1e-6);
    EXPECT_NEAR(water.I_eV_uncertainty, 3.0, 1e-6);

    // Sternheimer correction.
    EXPECT_TRUE(water.sternheimer.hasData);
    EXPECT_NEAR(water.sternheimer.a, 0.09116, 1e-5);
    EXPECT_NEAR(water.sternheimer.x0, 0.2400, 1e-4);
    EXPECT_NEAR(water.sternheimer.x1, 2.8004, 1e-4);
    EXPECT_NEAR(water.sternheimer.C_delta, -3.5017, 1e-4);

    // Radiation length.
    EXPECT_NEAR(water.radiationLength_cm, 36.08, 1e-2);

    // Composition.
    ASSERT_EQ(water.composition.size(), 2u);
    EXPECT_EQ(water.composition[0].symbol, "H");
    EXPECT_NEAR(water.composition[0].massFraction, 0.111898, 1e-6);
    EXPECT_EQ(water.composition[1].symbol, "O");
    EXPECT_NEAR(water.composition[1].massFraction, 0.888102, 1e-6);

    // State.
    EXPECT_EQ(water.state, 1);  // liquid
    EXPECT_FALSE(water.isGas());

    // Validation.
    EXPECT_TRUE(water.isValid());
}

TEST_F(MaterialRegistryTest, Comparison)
{
    Material a, b;
    a.id = "test_a";
    b.id = "test_b";
    EXPECT_NE(a, b);
    b.id = "test_a";
    EXPECT_EQ(a, b);
}
