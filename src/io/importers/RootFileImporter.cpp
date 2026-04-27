#include "io/importers/RootFileImporter.h"

#include <filesystem>

namespace beamlab::io {

std::string RootFileImporter::name() const
{
    return "RootFileImporter";
}

std::vector<std::string> RootFileImporter::supportedExtensions() const
{
    return {".root"};
}

ImporterCapabilityScore RootFileImporter::probe(const ProbeResult& probe_result) const
{
    ImporterCapabilityScore score{};
    score.importer_name = name();

    if (!probe_result.readable) {
        score.confidence = 0.0;
        return score;
    }

    if (probe_result.detected_format == FormatSignature::RootFile ||
        probe_result.extension == ".root") {
        score.confidence = 0.99;
    } else {
        score.confidence = 0.0;
    }

    return score;
}

InspectionReport RootFileImporter::inspect(const std::string& file_path) const
{
    InspectionReport report{};
    report.file_path = file_path;
    report.readable = std::filesystem::exists(file_path);

    report.warnings.push_back(beamlab::core::Warning{
        .severity = beamlab::core::Severity::Warning,
        .message = "Los archivos ROOT requieren conversión antes del análisis",
        .details = "Usa tools/root_to_beamlab_csv.py para convertir ROOT a CSV compatible con Geant4CsvImporter."
    });

    return report;
}

ImportSchema RootFileImporter::buildSchema(const std::string&,
                                           const InspectionReport&) const
{
    ImportSchema schema{};
    schema.schema_name = "root_binary_requires_conversion";
    return schema;
}

ImportResult RootFileImporter::import(const std::string& file_path,
                                      const ImportSchema&,
                                      const ImportContext&) const
{
    ImportResult result{};
    result.success = false;
    result.error = beamlab::core::Error{
        .code = beamlab::core::StatusCode::InvalidArgument,
        .severity = beamlab::core::Severity::Error,
        .message = "Importación ROOT nativa no habilitada en este build",
        .details = "Convierte primero con: python3 tools/root_to_beamlab_csv.py \"" +
                   file_path +
                   "\" --output examples/datasets/geant4_csv/from_root.csv"
    };

    return result;
}

} // namespace beamlab::io
