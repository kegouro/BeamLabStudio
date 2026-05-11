#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace beamlab::core {

// Parses a textual token as a finite double.  Returns std::nullopt if the
// token is empty, fails std::stod, or evaluates to NaN/Inf.
//
// Use this at every CSV/text input boundary instead of bare std::stod — bare
// parsing silently lets "NaN", "Infinity", or partially-parsed strings through,
// and those propagate into physics (Bethe-Bloch, dose) without diagnostic.
[[nodiscard]] std::optional<double> tryParseFiniteDouble(std::string_view token);

// Same contract for integers.  Rejects overflow and non-numeric tails.
[[nodiscard]] std::optional<long long> tryParseInteger(std::string_view token);

} // namespace beamlab::core
