#pragma once

#include <string>

namespace beamlab::core {

struct Version {
    int major{0};
    int minor{1};
    int patch{0};

    [[nodiscard]] std::string toString() const
    {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch);
    }
};

} // namespace beamlab::core
