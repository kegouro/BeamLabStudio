#include "io/importers/DelimitedTableImporter.h"

#include "io/parsing/DelimiterDetector.h"

#include <filesystem>
#include <fstream>

namespace beamlab::io {

std::string DelimitedTableImporter::name() const
{
    return "DelimitedTableImporter";
}

std::vector<std::string> DelimitedTableImporter::supportedExtensions() const
{
    return {".csv", ".tsv", ".txt"};
}

ImporterCapabilityScore DelimitedTableImporter::probe(const ProbeResult& probe_result) const
{
    ImporterCapabilityScore score{};
    score.importer_name = name();

    if (!probe_result.readable) {
        score.confidence = 0.0;
        return score;
    }

    switch (probe_result.detected_format) {
    case FormatSignature::TextTable:
        score.confidence = 0.85;
        break;
    case FormatSignature::ComsolCsv:
        score.confidence = 0.40;
        break;
    default:
        score.confidence = 0.10;
        break;
    }

    return score;
}

InspectionReport DelimitedTableImporter::inspect(const std::string& file_path) const
{
    InspectionReport report{};
    report.file_path = file_path;

    std::ifstream input(file_path);
    report.readable = static_cast<bool>(input);
    if (!input) {
        return report;
    }

    std::string line{};
    for (int i = 0; i < 3 && std::getline(input, line); ++i) {
        report.preview_lines.push_back(line);
    }

    return report;
}

ImportSchema DelimitedTableImporter::buildSchema(const std::string&,
                                                 const InspectionReport& inspection) const
{
    ImportSchema schema{};
    schema.schema_name = "delimited_table";

    if (!inspection.preview_lines.empty()) {
        schema.delimiter = DelimiterDetector::detect(inspection.preview_lines.front());
    }

    return schema;
}

ImportResult DelimitedTableImporter::import(const std::string& file_path,
                                            const ImportSchema& schema,
                                            const ImportContext& context) const
{
    ImportResult result{};

    std::ifstream input(file_path);
    if (!input) {
        result.success = false;
        result.error = beamlab::core::Error{
            .code = beamlab::core::StatusCode::IOError,
            .severity = beamlab::core::Severity::Error,
            .message = "No se pudo abrir el archivo tabular",
            .details = file_path
        };
        return result;
    }

    beamlab::data::TrajectoryDataset dataset{};
    dataset.metadata.display_name = context.preferred_display_name.empty()
        ? std::filesystem::path(file_path).stem().string()
        : context.preferred_display_name;
    dataset.metadata.source.original_path = file_path;
    dataset.metadata.source.source_type = "delimited_table";
    dataset.metadata.source.display_name = dataset.metadata.display_name;
    dataset.metadata.import.importer_name = name();
    dataset.metadata.import.importer_version = "0.1";
    dataset.metadata.import.schema_name = schema.schema_name;

    result.success = true;
    result.dataset = std::move(dataset);
    return result;
}

} // namespace beamlab::io
