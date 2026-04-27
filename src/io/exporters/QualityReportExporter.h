#pragma once

#include "analysis/quality/AnalysisQualityReport.h"
#include "io/common/ExportResult.h"
#include "io/exporters/IExporter.h"

#include <string>

namespace beamlab::io {

class QualityReportExporter final : public IExporter {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] ExportResult exportCsv(
        const beamlab::analysis::AnalysisQualityReport& report,
        const std::string& output_path) const;

    [[nodiscard]] ExportResult exportMarkdown(
        const beamlab::analysis::AnalysisQualityReport& report,
        const std::string& output_path) const;
};

} // namespace beamlab::io
