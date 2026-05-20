#include "biosim/ui/qt/EnergyColorMapper.h"

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

EnergyColorMapper::RGB EnergyColorMapper::lerp(RGB a, RGB b, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return {
        static_cast<uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<uint8_t>(a.b + (b.b - a.b) * t),
    };
}

EnergyColorMapper::Lut EnergyColorMapper::fromControlPoints(
    const std::vector<std::pair<double, RGB>>& cps)
{
    Lut lut{};
    for (int i = 0; i < kLutSize; ++i) {
        const double t = i / double(kLutSize - 1);
        // Find the segment containing t.
        std::size_t seg = 0;
        for (std::size_t j = 1; j < cps.size(); ++j) {
            if (t <= cps[j].first) { seg = j - 1; break; }
            seg = j - 1;
        }
        const auto& [ta, ca] = cps[seg];
        const auto& [tb, cb] = cps[std::min(seg + 1, cps.size() - 1)];
        const double span = tb - ta;
        const double u = (span > 0.0) ? (t - ta) / span : 0.0;
        lut[static_cast<std::size_t>(i)] = lerp(ca, cb, u);
    }
    return lut;
}

// ---------------------------------------------------------------------------
// Palette builders — control-point data from matplotlib / Google colormaps.
// Values are sampled at representative positions for accuracy.
// ---------------------------------------------------------------------------

EnergyColorMapper::Lut EnergyColorMapper::buildViridis()
{
    // matplotlib viridis — 17 anchor samples
    return fromControlPoints({
        {0.000, {68,   1, 84}},
        {0.063, {71,  19, 101}},
        {0.125, {72,  36, 117}},
        {0.188, {69,  53, 129}},
        {0.250, {63,  70, 136}},
        {0.313, {56,  87, 140}},
        {0.375, {49, 103, 142}},
        {0.438, {43, 120, 142}},
        {0.500, {38, 136, 141}},
        {0.563, {33, 152, 138}},
        {0.625, {37, 168, 131}},
        {0.688, {60, 183, 118}},
        {0.750, {96, 197,  99}},
        {0.813, {136, 211, 75}},
        {0.875, {178, 222, 44}},
        {0.938, {215, 231, 16}},
        {1.000, {253, 231, 37}},
    });
}

EnergyColorMapper::Lut EnergyColorMapper::buildPlasma()
{
    return fromControlPoints({
        {0.000, {13,   8, 135}},
        {0.125, {82,   1, 163}},
        {0.250, {139,  10, 165}},
        {0.375, {185,  50, 137}},
        {0.500, {219,  92, 104}},
        {0.625, {244, 136,  73}},
        {0.750, {254, 182,  44}},
        {0.875, {252, 228,  31}},
        {1.000, {240, 249,  33}},
    });
}

EnergyColorMapper::Lut EnergyColorMapper::buildInferno()
{
    return fromControlPoints({
        {0.000, {0,    0,   4}},
        {0.125, {25,   8,  50}},
        {0.250, {72,   4,  87}},
        {0.375, {120,  28, 109}},
        {0.500, {165,  66,  82}},
        {0.625, {207, 111,  57}},
        {0.750, {240, 163,  33}},
        {0.875, {253, 216,  45}},
        {1.000, {252, 255, 164}},
    });
}

EnergyColorMapper::Lut EnergyColorMapper::buildMagma()
{
    return fromControlPoints({
        {0.000, {0,   0,   4}},
        {0.125, {21,  11,  53}},
        {0.250, {63,  11,  87}},
        {0.375, {106,  26, 95}},
        {0.500, {152,  51,  86}},
        {0.625, {197,  87,  69}},
        {0.750, {233, 133,  52}},
        {0.875, {249, 188, 105}},
        {1.000, {252, 253, 191}},
    });
}

EnergyColorMapper::Lut EnergyColorMapper::buildTurbo()
{
    // Google Turbo — perceptually improved rainbow.
    return fromControlPoints({
        {0.000, {48,  18,  59}},
        {0.063, {64,  65, 133}},
        {0.125, {61, 115, 193}},
        {0.188, {36, 166, 214}},
        {0.250, {27, 208, 195}},
        {0.313, {54, 230, 148}},
        {0.375, {104, 240,  95}},
        {0.438, {162, 240,  45}},
        {0.500, {210, 226,  27}},
        {0.563, {241, 197,  18}},
        {0.625, {253, 158,  19}},
        {0.688, {246, 112,  23}},
        {0.750, {226,  65,  28}},
        {0.813, {195,  24,  24}},
        {0.875, {154,   4,  15}},
        {0.938, {108,   2,   4}},
        {1.000, {122,   4,   3}},
    });
}

EnergyColorMapper::Lut EnergyColorMapper::buildCoolWarm()
{
    // Blue–white–red diverging (Moreland 2009).
    return fromControlPoints({
        {0.000, {59,  76, 192}},
        {0.125, {98, 130, 234}},
        {0.250, {141, 176, 254}},
        {0.375, {184, 208, 249}},
        {0.500, {221, 221, 221}},
        {0.625, {243, 187, 168}},
        {0.750, {230, 132, 115}},
        {0.875, {196,  73,  67}},
        {1.000, {180,   4,  38}},
    });
}

EnergyColorMapper::Lut EnergyColorMapper::buildBraggPeak()
{
    // Custom: designed to highlight the Bragg peak (red/orange spike at high t)
    // while keeping the plateau region visually cool.
    return fromControlPoints({
        {0.000, {0,   0,   0}},
        {0.100, {0,   0,  80}},
        {0.250, {0,  80, 180}},
        {0.400, {0, 180, 200}},
        {0.550, {0, 220, 220}},
        {0.650, {255, 255, 255}},
        {0.720, {255, 220,  80}},
        {0.820, {255, 130,   0}},
        {0.920, {220,  20,   0}},
        {1.000, {255,   0,   0}},
    });
}

EnergyColorMapper::Lut EnergyColorMapper::buildMedicalDose()
{
    // AAPM-style dose map: cold blue (low) → green → yellow → red (high dose).
    return fromControlPoints({
        {0.000, {0,   0, 128}},
        {0.125, {0,  60, 200}},
        {0.250, {0, 160, 255}},
        {0.375, {0, 230, 200}},
        {0.500, {0, 255,   0}},
        {0.625, {200, 255,   0}},
        {0.750, {255, 255,   0}},
        {0.875, {255, 128,   0}},
        {1.000, {255,   0,   0}},
    });
}

// ---------------------------------------------------------------------------
// EnergyColorMapper
// ---------------------------------------------------------------------------

EnergyColorMapper::EnergyColorMapper()
{
    luts_[static_cast<int>(Palette::Viridis)]     = buildViridis();
    luts_[static_cast<int>(Palette::Plasma)]      = buildPlasma();
    luts_[static_cast<int>(Palette::Inferno)]     = buildInferno();
    luts_[static_cast<int>(Palette::Magma)]       = buildMagma();
    luts_[static_cast<int>(Palette::Turbo)]       = buildTurbo();
    luts_[static_cast<int>(Palette::CoolWarm)]    = buildCoolWarm();
    luts_[static_cast<int>(Palette::BraggPeak)]   = buildBraggPeak();
    luts_[static_cast<int>(Palette::MedicalDose)] = buildMedicalDose();
}

QColor EnergyColorMapper::map(double t, Palette palette) const
{
    t = std::clamp(t, 0.0, 1.0);
    const int idx = static_cast<int>(std::round(t * (kLutSize - 1)));
    const auto& rgb = luts_[static_cast<std::size_t>(static_cast<int>(palette))][static_cast<std::size_t>(idx)];
    return QColor(rgb.r, rgb.g, rgb.b);
}

QColor EnergyColorMapper::mapLinear(double value, double min_val,
                                     double max_val, Palette palette) const
{
    const double span = max_val - min_val;
    if (span == 0.0) return map(0.5, palette);
    return map((value - min_val) / span, palette);
}

std::vector<double> EnergyColorMapper::niceTicks(double min_val,
                                                   double max_val,
                                                   int target_ticks)
{
    if (min_val >= max_val) return {min_val};

    const double range = max_val - min_val;
    const double rough_step = range / (target_ticks - 1);
    const double mag = std::pow(10.0, std::floor(std::log10(rough_step)));
    const double nice_steps[] = {1.0, 2.0, 2.5, 5.0, 10.0};

    double step = mag;
    for (double ns : nice_steps) {
        const double candidate = mag * ns;
        if (candidate >= rough_step) { step = candidate; break; }
    }

    const double first = std::ceil(min_val / step) * step;
    std::vector<double> ticks;
    ticks.reserve(static_cast<std::size_t>(target_ticks + 2));
    for (double v = first; v <= max_val + step * 1e-9; v += step) {
        if (v >= min_val - step * 1e-9) ticks.push_back(v);
        if (static_cast<int>(ticks.size()) > target_ticks + 2) break;
    }
    return ticks;
}

QString EnergyColorMapper::paletteName(Palette palette)
{
    switch (palette) {
    case Palette::Viridis:     return QStringLiteral("Viridis");
    case Palette::Plasma:      return QStringLiteral("Plasma");
    case Palette::Inferno:     return QStringLiteral("Inferno");
    case Palette::Magma:       return QStringLiteral("Magma");
    case Palette::Turbo:       return QStringLiteral("Turbo");
    case Palette::CoolWarm:    return QStringLiteral("Cool/Warm");
    case Palette::BraggPeak:   return QStringLiteral("Bragg Peak");
    case Palette::MedicalDose: return QStringLiteral("Medical Dose");
    }
    return QStringLiteral("Unknown");
}

} // namespace beamlab::biosim
