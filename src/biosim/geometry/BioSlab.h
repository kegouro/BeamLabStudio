#pragma once

#include "biosim/materials/BioMaterial.h"

#include <cmath>
#include <string>

namespace beamlab::biosim {

// A finite slab of material placed along the beam axis.
// The slab can be spatially bounded in the transverse plane
// (rectangle, cylinder, or ellipse) or infinite (fills the full beam cross-section).
struct BioSlab {

    std::string id{};          // unique identifier (e.g. "slab_0", "lung")
    std::string label{};       // display label shown in UI and exports
    BioMaterial material{};

    // ── Axial position ────────────────────────────────────────────────────────
    double axial_start_m{0.0};   // start position along beam axis [m]
    double thickness_m{0.01};    // slab thickness [m]

    // ── Transverse shape ─────────────────────────────────────────────────────
    enum class Shape { Infinite, Rectangle, Cylinder, Ellipse };
    Shape shape{Shape::Infinite};

    double width_m{0.20};       // Rectangle: full width in X [m]
    double height_m{0.20};      // Rectangle: full height in Y [m]
    double radius_m{0.10};      // Cylinder: radius [m]
    double radius_x_m{0.10};    // Ellipse: semi-axis X [m]
    double radius_y_m{0.08};    // Ellipse: semi-axis Y [m]

    // Centre offset in the transverse plane relative to beam axis [m]
    double center_x_m{0.0};
    double center_y_m{0.0};

    // ── State ─────────────────────────────────────────────────────────────────
    bool enabled{true};

    // ── Visual (used by BioViewport3D) ────────────────────────────────────────
    // RGBA encoded as 0xRRGGBBAA
    unsigned int color_rgba{0x64B4FF78};  // light-blue, 47% alpha by default

    // ── Derived ───────────────────────────────────────────────────────────────
    [[nodiscard]] double axialEnd_m() const { return axial_start_m + thickness_m; }

    // Returns true if the point (axial_m, x_m, y_m) is inside this slab.
    // axial_m: position along the beam axis [m].
    // x_m, y_m: transverse coordinates [m].
    [[nodiscard]] bool containsPoint(double axial_m,
                                      double x_m,
                                      double y_m) const
    {
        if (!enabled) return false;
        if (axial_m < axial_start_m || axial_m >= axialEnd_m()) return false;

        const double dx = x_m - center_x_m;
        const double dy = y_m - center_y_m;

        switch (shape) {
        case Shape::Infinite:
            return true;

        case Shape::Rectangle:
            return std::abs(dx) <= width_m  / 2.0 &&
                   std::abs(dy) <= height_m / 2.0;

        case Shape::Cylinder:
            return (dx * dx + dy * dy) <= (radius_m * radius_m);

        case Shape::Ellipse: {
            const double rx = radius_x_m, ry = radius_y_m;
            if (rx <= 0.0 || ry <= 0.0) return false;
            return (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry) <= 1.0;
        }
        }
        return false;
    }

    // Returns the depth [cm] of axial_m inside the slab.
    // Returns 0 if the point is not inside the slab.
    [[nodiscard]] double depthInSlab_cm(double axial_m) const
    {
        if (axial_m < axial_start_m || axial_m >= axialEnd_m()) return 0.0;
        return (axial_m - axial_start_m) * 100.0; // [m] → [cm]
    }
};

} // namespace beamlab::biosim
