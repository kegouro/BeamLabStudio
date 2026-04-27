#pragma once

#include "data/model/VariableDescriptor.h"

#include <string>
#include <unordered_map>

namespace beamlab::data {

class VariableRegistry {
public:
    void registerVariable(const VariableDescriptor& descriptor)
    {
        m_variables[descriptor.internal_name] = descriptor;
    }

    [[nodiscard]] bool hasVariable(const std::string& name) const
    {
        return m_variables.contains(name);
    }

    [[nodiscard]] const std::unordered_map<std::string, VariableDescriptor>& variables() const
    {
        return m_variables;
    }

private:
    std::unordered_map<std::string, VariableDescriptor> m_variables{};
};

} // namespace beamlab::data
