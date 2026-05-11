#pragma once

namespace beamlab::core::units {

// Radius of a circle whose area equals exactly 1 cm² — i.e. r = √(1/π).
//
// Several legacy code paths assumed a "1 cm² nominal cross-section" when
// converting energy deposit to absorbed dose.  Expressing that assumption
// as an explicit radius lets BetheBlochEngine::dosePerStep_Gy take a single
// `beam_radius_cm` parameter that defaults to this value, preserving exact
// numerical compatibility while unifying the API with BioSim's
// EnergyScaleConverter (which already takes a radius).
//
// Numerical value: √(1/π) ≈ 0.5641895835477563
inline constexpr double kUnitAreaRadius_cm = 0.5641895835477563;

} // namespace beamlab::core::units
