#pragma once

#include <string>
#include <vector>

namespace beamlab::core {

struct ProvenanceRecord {
    std::string producer{};
    std::string producer_version{};
    std::vector<std::string> input_ids{};
    std::string parameters_summary{};
};

} // namespace beamlab::core
