#pragma once

#include "io/common/ImportContext.h"
#include "io/common/ImportResult.h"
#include "io/common/ImportSchema.h"
#include "io/common/InspectionReport.h"
#include "io/detect/ImporterCapabilityScore.h"
#include "io/detect/ProbeResult.h"

#include <string>
#include <vector>

namespace beamlab::io {

class IImporter {
public:
    virtual ~IImporter() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::vector<std::string> supportedExtensions() const = 0;
    [[nodiscard]] virtual ImporterCapabilityScore probe(const ProbeResult& probe_result) const = 0;
    [[nodiscard]] virtual InspectionReport inspect(const std::string& file_path) const = 0;
    [[nodiscard]] virtual ImportSchema buildSchema(const std::string& file_path,
                                                   const InspectionReport& inspection) const = 0;
    [[nodiscard]] virtual ImportResult import(const std::string& file_path,
                                              const ImportSchema& schema,
                                              const ImportContext& context) const = 0;
};

} // namespace beamlab::io
