#pragma once

#include "io/common/ExportPlan.h"
#include "io/common/ExportResult.h"

#include <string>

namespace beamlab::io {

class IExporter {
public:
    virtual ~IExporter() = default;

    [[nodiscard]] virtual std::string name() const = 0;
};

} // namespace beamlab::io
