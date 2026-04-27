#pragma once

#include <string>

namespace beamlab::io {

enum class ColumnType {
    Unknown,
    Integer,
    Floating,
    Text
};

class ColumnTypeInferer {
public:
    [[nodiscard]] static ColumnType infer(const std::string& sample);
};

} // namespace beamlab::io
