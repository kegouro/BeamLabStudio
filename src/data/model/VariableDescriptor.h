#pragma once

#include <string>

namespace beamlab::data {

struct VariableDescriptor {
    std::string internal_name{};
    std::string display_name{};
    std::string unit_symbol{};
    bool is_vector{false};
};

} // namespace beamlab::data
