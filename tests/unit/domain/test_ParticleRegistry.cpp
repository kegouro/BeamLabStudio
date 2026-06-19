#include "domain/physics/ParticleRegistry.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace beamlab::domain::physics;

class ParticleRegistryTest : public ::testing::Test {
protected:
    ParticleRegistry reg;

    void SetUp() override
    {
        reg.loadBuiltinParticles();
    }
};

TEST_F(ParticleRegistryTest, CountBuiltinParticles)
{
    EXPECT_GE(reg.count(), 18u);
}

TEST_F(ParticleRegistryTest, LookupByName)
{
    const auto& p = reg.get("proton");
    EXPECT_EQ(p.name, "Proton");
    EXPECT_NEAR(p.mass_MeV, 938.27208816, 1e-8);
    EXPECT_EQ(p.charge_e, 1.0);
    EXPECT_EQ(p.pdgCode, 2212);
}

TEST_F(ParticleRegistryTest, LookupByPdgCode)
{
    const auto& p = reg.getByPdgCode(11);
    EXPECT_EQ(p.name, "Electron");
    EXPECT_NEAR(p.mass_MeV, 0.51099895, 1e-8);
    EXPECT_EQ(p.charge_e, -1.0);
}

TEST_F(ParticleRegistryTest, GetThrowsForMissingName)
{
    EXPECT_THROW(reg.get("nonexistent"), std::out_of_range);
}

TEST_F(ParticleRegistryTest, GetThrowsForMissingPdgCode)
{
    EXPECT_THROW(reg.getByPdgCode(999999), std::out_of_range);
}

TEST_F(ParticleRegistryTest, FindReturnsNulloptForMissing)
{
    EXPECT_FALSE(reg.find("nonexistent").has_value());
    EXPECT_FALSE(reg.findByPdgCode(999999).has_value());
}

TEST_F(ParticleRegistryTest, FindReturnsParticleForValidName)
{
    auto result = reg.find("muon_minus");
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->get().mass_MeV, 105.6583755, 1e-8);
    EXPECT_EQ(result->get().charge_e, -1.0);
}

TEST_F(ParticleRegistryTest, FilterByCategory)
{
    auto leptons = reg.findByCategory(ParticleCategory::Lepton);
    EXPECT_GE(leptons.size(), 6u);  // e±, μ±, τ±
    for (const auto* l : leptons) {
        EXPECT_EQ(l->category, ParticleCategory::Lepton);
    }

    auto ions = reg.findByCategory(ParticleCategory::Ion);
    EXPECT_GE(ions.size(), 5u);  // d, t, He3, α, C, N, O
}

TEST_F(ParticleRegistryTest, FilterByProperty)
{
    // Particles with charge > 1.
    auto multicharge = reg.findByProperty(
        [](const Particle& p) { return std::abs(p.charge_e) > 1.0; });
    EXPECT_GE(multicharge.size(), 3u);

    // Particles with WR = 20 (alpha particles and heavy ions).
    auto highWr = reg.findByProperty(
        [](const Particle& p) { return p.WR > 10.0; });
    EXPECT_GE(highWr.size(), 1u);
}

TEST_F(ParticleRegistryTest, Names)
{
    auto names = reg.names();
    EXPECT_GE(names.size(), 18u);

    bool hasProton = false;
    for (const auto& n : names) {
        if (n == "proton") { hasProton = true; break; }
    }
    EXPECT_TRUE(hasProton);
}

TEST_F(ParticleRegistryTest, ElectronProperties)
{
    const auto& e = reg.get("electron");
    EXPECT_EQ(e.pdgCode, 11);
    EXPECT_NEAR(e.mass_MeV, 0.51099895, 1e-8);
    EXPECT_EQ(e.charge_e, -1.0);
    EXPECT_EQ(e.spin, 0.5);
    EXPECT_EQ(e.baryonNumber, 0);
    EXPECT_EQ(e.leptonNumber, 1);
    EXPECT_TRUE(e.isStable);
}

TEST_F(ParticleRegistryTest, AlphaProperties)
{
    const auto& a = reg.get("alpha");
    EXPECT_EQ(a.pdgCode, 1000020040);
    EXPECT_NEAR(a.mass_MeV, 3727.3794, 1e-4);
    EXPECT_EQ(a.charge_e, 2.0);
    EXPECT_EQ(a.spin, 0.0);
    EXPECT_EQ(a.baryonNumber, 4);
    EXPECT_EQ(a.WR, 20.0);
    EXPECT_EQ(a.category, ParticleCategory::Ion);
}

TEST_F(ParticleRegistryTest, GammaProperties)
{
    const auto& g = reg.get("gamma");
    EXPECT_EQ(g.pdgCode, 22);
    EXPECT_NEAR(g.mass_MeV, 0.0, 1e-9);
    EXPECT_EQ(g.charge_e, 0.0);
    EXPECT_EQ(g.spin, 1.0);
    EXPECT_EQ(g.category, ParticleCategory::Boson);
}

TEST_F(ParticleRegistryTest, DualIndexConsistency)
{
    // Verify that name → PDG → name round-trips correctly.
    const auto& proton = reg.get("proton");
    const auto& fromPdg = reg.getByPdgCode(proton.pdgCode);
    EXPECT_EQ(proton.name, fromPdg.name);
    EXPECT_EQ(proton.mass_MeV, fromPdg.mass_MeV);
}

TEST_F(ParticleRegistryTest, JSONSerializationRoundTrip)
{
    Particle p;
    p.id = "test_particle";
    p.name = "Test Particle";
    p.pdgCode = 999;
    p.mass_MeV = 1000.0;
    p.charge_e = 2.0;
    p.spin = 0.5;
    p.category = ParticleCategory::Ion;
    p.geant4Name = "test";
    p.WR = 20.0;

    nlohmann::json j = p;
    Particle restored = j.get<Particle>();

    EXPECT_EQ(restored.id, "test_particle");
    EXPECT_EQ(restored.pdgCode, 999);
    EXPECT_NEAR(restored.mass_MeV, 1000.0, 1e-9);
    EXPECT_EQ(restored.charge_e, 2.0);
    EXPECT_EQ(restored.category, ParticleCategory::Ion);
}

TEST_F(ParticleRegistryTest, LoadFromJson)
{
    auto tmpPath = std::string("/tmp/test_particles.json");
    {
        nlohmann::json arr = nlohmann::json::array();
        arr.push_back({
            {"id", "custom_particle"},
            {"name", "Custom Particle"},
            {"pdgCode", 99999},
            {"mass_MeV", 500.0},
            {"charge_e", 1.0},
            {"category", "custom"},
        });
        arr.push_back({
            {"id", "exotic"},
            {"name", "Exotic"},
            {"pdgCode", 88888},
            {"mass_MeV", 2000.0},
            {"charge_e", 0.0},
            {"category", "boson"},
        });

        std::ofstream out(tmpPath);
        out << arr.dump(2);
        out.close();
    }

    ParticleRegistry jsonReg;
    jsonReg.registerParticlesFromJson(tmpPath);

    EXPECT_EQ(jsonReg.count(), 2u);
    EXPECT_TRUE(jsonReg.find("custom_particle").has_value());
    EXPECT_TRUE(jsonReg.find("exotic").has_value());
    EXPECT_NEAR(jsonReg.get("custom_particle").mass_MeV, 500.0, 1e-9);

    std::remove(tmpPath.c_str());
}

TEST_F(ParticleRegistryTest, KaonProperties)
{
    const auto& k = reg.get("kaon_plus");
    EXPECT_EQ(k.pdgCode, 321);
    EXPECT_NEAR(k.mass_MeV, 493.677, 1e-3);
    EXPECT_EQ(k.charge_e, 1.0);
    EXPECT_EQ(k.spin, 0.0);
    EXPECT_FALSE(k.isStable);
    EXPECT_NEAR(k.lifetime_s, 1.2380e-8, 1e-12);
}

TEST_F(ParticleRegistryTest, Comparison)
{
    Particle a, b;
    a.pdgCode = 11;
    b.pdgCode = 13;
    EXPECT_NE(a, b);
    b.pdgCode = 11;
    EXPECT_EQ(a, b);
}
