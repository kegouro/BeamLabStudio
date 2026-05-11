#include "core/foundation/NumericGuards.h"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>

namespace beamlab::core {

namespace {

bool allWhitespace(std::string_view s)
{
    for (const char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

} // namespace

std::optional<double> tryParseFiniteDouble(std::string_view token)
{
    if (token.empty() || allWhitespace(token)) {
        return std::nullopt;
    }

    // std::stod requires a null-terminated string — the only safe bridge from
    // string_view in C++20 without copying is std::string.
    const std::string buffer(token);

    try {
        std::size_t consumed = 0;
        const double value = std::stod(buffer, &consumed);

        if (consumed == 0) {
            return std::nullopt;
        }

        if (!std::isfinite(value)) {
            return std::nullopt;
        }

        return value;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<long long> tryParseInteger(std::string_view token)
{
    if (token.empty() || allWhitespace(token)) {
        return std::nullopt;
    }

    const std::string buffer(token);

    try {
        std::size_t consumed = 0;
        const long long value = std::stoll(buffer, &consumed);

        if (consumed == 0) {
            return std::nullopt;
        }

        return value;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace beamlab::core
