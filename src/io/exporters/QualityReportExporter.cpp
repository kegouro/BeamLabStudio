#include "io/exporters/QualityReportExporter.h"

#include "core/foundation/Error.h"

#include <filesystem>
#include <fstream>

namespace beamlab::io {
namespace {

void ensureParentDirectory(const std::string& path)
{
    const std::filesystem::path p(path);
    const auto parent = p.parent_path();

    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

beamlab::core::Error makeError(beamlab::core::StatusCode code,
                               const std::string& message,
                               const std::string& details)
{
    return beamlab::core::Error{
        .code = code,
        .severity = beamlab::core::Severity::Error,
        .message = message,
        .details = details
    };
}

std::string severityName(const beamlab::analysis::QualitySeverity severity)
{
    switch (severity) {
    case beamlab::analysis::QualitySeverity::Info:
        return "INFO";
    case beamlab::analysis::QualitySeverity::Warning:
        return "WARNING";
    case beamlab::analysis::QualitySeverity::Error:
        return "ERROR";
    case beamlab::analysis::QualitySeverity::Critical:
        return "CRITICAL";
    }

    return "UNKNOWN";
}

std::string escapeCsv(std::string text)
{
    bool needs_quotes = false;

    for (const char c : text) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return text;
    }

    std::string escaped{"\""};

    for (const char c : text) {
        if (c == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(c);
        }
    }

    escaped += '"';
    return escaped;
}

} // namespace

std::string QualityReportExporter::name() const
{
    return "QualityReportExporter";
}

ExportResult QualityReportExporter::exportCsv(
    const beamlab::analysis::AnalysisQualityReport& report,
    const std::string& output_path) const
{
    ExportResult result{};
    ensureParentDirectory(output_path);

    std::ofstream output(output_path);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear quality_report.csv",
            output_path
        );
        return result;
    }

    output << "severity,code,title,detail,suggestion\n";

    for (const auto& issue : report.issues) {
        output << severityName(issue.severity) << ','
               << escapeCsv(issue.code) << ','
               << escapeCsv(issue.title) << ','
               << escapeCsv(issue.detail) << ','
               << escapeCsv(issue.suggestion) << '\n';
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

ExportResult QualityReportExporter::exportMarkdown(
    const beamlab::analysis::AnalysisQualityReport& report,
    const std::string& output_path) const
{
    ExportResult result{};
    ensureParentDirectory(output_path);

    std::ofstream output(output_path);

    if (!output) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudo crear quality_report.md",
            output_path
        );
        return result;
    }

    output << "# BeamLabStudio Quality Report\n\n";
    output << "## Verdict\n\n";
    output << "**" << report.verdict << "**\n\n";

    output << "| Severity | Count |\n";
    output << "|---|---:|\n";
    output << "| Info | " << report.info_count << " |\n";
    output << "| Warning | " << report.warning_count << " |\n";
    output << "| Error | " << report.error_count << " |\n";
    output << "| Critical | " << report.critical_count << " |\n\n";

    output << "## Issues\n\n";
    output << "| Severity | Code | Title | Detail | Suggestion |\n";
    output << "|---|---|---|---|---|\n";

    for (const auto& issue : report.issues) {
        output << "| " << severityName(issue.severity)
               << " | `" << issue.code << "`"
               << " | " << issue.title
               << " | " << issue.detail
               << " | " << issue.suggestion
               << " |\n";
    }

    result.success = true;
    result.output_path = output_path;
    return result;
}

} // namespace beamlab::io
