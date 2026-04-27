#pragma once

#include "io/importers/IImporter.h"

namespace beamlab::io {

class ComsolCsvImporter final : public IImporter {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::vector<std::string> supportedExtensions() const override;
    [[nodiscard]] ImporterCapabilityScore probe(const ProbeResult& probe_result) const override;
    [[nodiscard]] InspectionReport inspect(const std::string& file_path) const override;
    [[nodiscard]] ImportSchema buildSchema(const std::string& file_path,
                                           const InspectionReport& inspection) const override;
    [[nodiscard]] ImportResult import(const std::string& file_path,
                                      const ImportSchema& schema,
                                      const ImportContext& context) const override;
};

} // namespace beamlab::io
