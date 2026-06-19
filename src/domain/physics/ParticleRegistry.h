#pragma once

#include "domain/physics/Particle.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace beamlab::domain::physics {

/// O(1) registry of particle species indexed by name and PDG code.
///
/// Dual index:
///   - byName_:   unordered_map<string, Particle>
///   - byPdgCode_: unordered_map<int, string>  (PDG code → name)
///
/// Built-in particles (18+ PDG 2022 species) are loaded via
/// loadBuiltinParticles().  Custom particles can be added at runtime.
class ParticleRegistry {
public:
    ParticleRegistry() = default;

    // ── Registration ───────────────────────────────────────────────

    void registerParticle(std::string name, Particle particle);

    void registerParticlesFromJson(const std::string& jsonPath);

    /// Populate with all 18+ built-in PDG 2022 particle species.
    /// Idempotent — skips already-registered names.
    void loadBuiltinParticles();

    // ── Query by name (O(1)) ──────────────────────────────────────

    [[nodiscard]] const Particle& get(const std::string& name) const;

    [[nodiscard]] std::optional<
        std::reference_wrapper<const Particle>> find(const std::string& name) const;

    // ── Query by PDG code (O(1)) ──────────────────────────────────

    [[nodiscard]] const Particle& getByPdgCode(int pdgCode) const;

    [[nodiscard]] std::optional<
        std::reference_wrapper<const Particle>> findByPdgCode(int pdgCode) const;

    // ── Filter ─────────────────────────────────────────────────────

    [[nodiscard]] std::vector<const Particle*> findByCategory(
        ParticleCategory category) const;

    [[nodiscard]] std::vector<const Particle*> findByProperty(
        std::function<bool(const Particle&)> predicate) const;

    // ── Enumeration ────────────────────────────────────────────────

    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] std::size_t count() const { return byName_.size(); }

private:
    std::unordered_map<std::string, Particle> byName_;
    std::unordered_map<int, std::string> byPdgCode_;  // PDG code → name

    static Particle makeElectron();
    static Particle makePositron();
    static Particle makeMuonMinus();
    static Particle makeMuonPlus();
    static Particle makeTauMinus();
    static Particle makeTauPlus();
    static Particle makeProton();
    static Particle makeAntiproton();
    static Particle makeNeutron();
    static Particle makeDeuteron();
    static Particle makeTriton();
    static Particle makeHelium3();
    static Particle makeAlpha();
    static Particle makeCarbon12();
    static Particle makeNitrogen14();
    static Particle makeOxygen16();
    static Particle makePionPlus();
    static Particle makePionMinus();
    static Particle makePionZero();
    static Particle makeKaonPlus();
    static Particle makeKaonMinus();
    static Particle makeKaonZero();
    static Particle makeGamma();
};

} // namespace beamlab::domain::physics
