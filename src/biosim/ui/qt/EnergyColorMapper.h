#pragma once

#include <QColor>
#include <QString>
#include <array>
#include <cstdint>
#include <vector>

namespace beamlab::biosim {

// Eight scientific colour palettes, each stored as a 256-entry RGB LUT.
// All palette data is generated analytically at construction and is completely
// self-contained — no image or resource file required.
class EnergyColorMapper {
public:
    enum class Palette {
        Viridis,      // violet→blue→green→yellow (perceptually uniform)
        Plasma,       // violet→red→yellow
        Inferno,      // black→purple→red→yellow
        Magma,        // black→purple→pink→white
        Turbo,        // cool rainbow (Google), diverges at cyan/green
        CoolWarm,     // blue→white→red (diverging)
        BraggPeak,    // black→blue→cyan→white→orange→red (Bragg highlight)
        MedicalDose,  // cold-blue→green→yellow→red (dose maps, AAPM style)
    };

    static constexpr int kLutSize = 256;

    EnergyColorMapper();

    // Map a normalised value t ∈ [0,1] to a QColor using the given palette.
    // Values outside [0,1] are clamped.
    [[nodiscard]] QColor map(double t, Palette palette) const;

    // Convenience: map a raw value to [0,1] using given min/max, then colour.
    [[nodiscard]] QColor mapLinear(double value, double min_val, double max_val,
                                   Palette palette) const;

    // Nice-number tick positions in [min_val, max_val] for a scale bar.
    // Returns between 4 and 8 tick values.
    [[nodiscard]] static std::vector<double> niceTicks(double min_val,
                                                        double max_val,
                                                        int target_ticks = 6);

    // Human-readable name for a palette (for combo boxes, etc.).
    [[nodiscard]] static QString paletteName(Palette palette);

    // Number of available palettes.
    static constexpr int kPaletteCount = 8;

private:
    struct RGB { uint8_t r, g, b; };
    using Lut = std::array<RGB, kLutSize>;

    std::array<Lut, kPaletteCount> luts_{};

    static Lut buildViridis();
    static Lut buildPlasma();
    static Lut buildInferno();
    static Lut buildMagma();
    static Lut buildTurbo();
    static Lut buildCoolWarm();
    static Lut buildBraggPeak();
    static Lut buildMedicalDose();

    // Interpolate between two RGB anchors at a fractional position in [0,1].
    static RGB lerp(RGB a, RGB b, double t);

    // Build a LUT from a list of (position, RGB) control points.
    static Lut fromControlPoints(
        const std::vector<std::pair<double, RGB>>& cps);
};

} // namespace beamlab::biosim
