#include "io/importers/ComsolCsvImporter.h"

#include "io/parsing/ComsolHeaderParser.h"
#include "io/parsing/DelimiterDetector.h"

#include "data/ids/SampleId.h"
#include "data/ids/TrajectoryId.h"
#include "data/model/Trajectory.h"
#include "data/model/TrajectorySample.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace beamlab::io {
namespace {

struct ComsolColumnGroup {
    std::size_t qx_col{0};
    std::size_t qy_col{0};
    std::size_t qz_col{0};
    std::size_t pidx_col{0};
    std::size_t t_col{0};
};

std::string trim(std::string text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }

    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::vector<std::string> splitCsvLine(const std::string& line, const char delimiter)
{
    std::vector<std::string> tokens{};
    std::string token{};
    bool inside_quotes = false;

    for (const char c : line) {
        if (c == '"') {
            inside_quotes = !inside_quotes;
            continue;
        }

        if (c == delimiter && !inside_quotes) {
            tokens.push_back(trim(token));
            token.clear();
            continue;
        }

        token.push_back(c);
    }

    tokens.push_back(trim(token));
    return tokens;
}

std::optional<double> parseDouble(const std::string& token)
{
    try {
        std::size_t processed = 0;
        const double value = std::stod(token, &processed);

        if (processed == 0) {
            return std::nullopt;
        }

        return value;
    } catch (...) {
        return std::nullopt;
    }
}

bool containsToken(const std::string& text, const std::string& token)
{
    return text.find(token) != std::string::npos;
}

std::string removeCommentPrefix(std::string line)
{
    line = trim(std::move(line));

    if (!line.empty() && line.front() == '%') {
        line.erase(line.begin());
    }

    return trim(line);
}

bool isHeaderLine(const std::string& line)
{
    const auto cleaned = removeCommentPrefix(line);
    return cleaned.rfind("Índice", 0) == 0 ||
           cleaned.rfind("Indice", 0) == 0 ||
           cleaned.rfind("Index", 0) == 0;
}

std::vector<ComsolColumnGroup> detectGroupsFromHeader(const std::vector<std::string>& columns)
{
    std::vector<ComsolColumnGroup> groups{};

    if (columns.size() < 6) {
        return groups;
    }

    // COMSOL típico:
    // Índice, qx @ t=..., qy @ t=..., qz @ t=..., cpt.pidx @ t=..., t @ t=..., ...
    for (std::size_t i = 1; i + 4 < columns.size(); ++i) {
        const auto& c0 = columns[i];
        const auto& c1 = columns[i + 1];
        const auto& c2 = columns[i + 2];
        const auto& c3 = columns[i + 3];
        const auto& c4 = columns[i + 4];

        const bool looks_like_group =
            containsToken(c0, "qx") &&
            containsToken(c1, "qy") &&
            containsToken(c2, "qz") &&
            (containsToken(c3, "pidx") || containsToken(c3, "idx")) &&
            (containsToken(c4, "t ") || containsToken(c4, "t(") || containsToken(c4, "t (") || c4.rfind("t", 0) == 0);

        if (looks_like_group) {
            groups.push_back({i, i + 1, i + 2, i + 3, i + 4});
            i += 4;
        }
    }

    return groups;
}

std::vector<ComsolColumnGroup> fallbackGroups(const std::size_t column_count)
{
    std::vector<ComsolColumnGroup> groups{};

    if (column_count < 6) {
        return groups;
    }

    for (std::size_t i = 1; i + 4 < column_count; i += 5) {
        groups.push_back({i, i + 1, i + 2, i + 3, i + 4});
    }

    return groups;
}

} // namespace

std::string ComsolCsvImporter::name() const
{
    return "ComsolCsvImporter";
}

std::vector<std::string> ComsolCsvImporter::supportedExtensions() const
{
    return {".csv", ".txt"};
}

ImporterCapabilityScore ComsolCsvImporter::probe(const ProbeResult& probe_result) const
{
    ImporterCapabilityScore score{};
    score.importer_name = name();

    if (!probe_result.readable) {
        score.confidence = 0.0;
        return score;
    }

    switch (probe_result.detected_format) {
    case FormatSignature::ComsolCsv:
        score.confidence = 0.95;
        break;
    case FormatSignature::TextTable: {
        bool looks_like_comsol = ComsolHeaderParser::looksLikeComsolHeader(probe_result.first_line_preview);

        for (const auto& line : probe_result.preview_lines) {
            if (ComsolHeaderParser::looksLikeComsolHeader(line)) {
                looks_like_comsol = true;
                break;
            }
        }

        score.confidence = looks_like_comsol ? 0.80 : 0.20;
        break;
    }
    default:
        score.confidence = 0.05;
        break;
    }

    return score;
}

InspectionReport ComsolCsvImporter::inspect(const std::string& file_path) const
{
    InspectionReport report{};
    report.file_path = file_path;

    std::ifstream input(file_path);
    report.readable = static_cast<bool>(input);
    if (!input) {
        return report;
    }

    std::string line{};
    for (int i = 0; i < 12 && std::getline(input, line); ++i) {
        report.preview_lines.push_back(line);
    }

    return report;
}

ImportSchema ComsolCsvImporter::buildSchema(const std::string&,
                                            const InspectionReport& inspection) const
{
    ImportSchema schema{};
    schema.schema_name = "comsol_csv";

    for (const auto& line : inspection.preview_lines) {
        if (isHeaderLine(line)) {
            schema.delimiter = DelimiterDetector::detect(removeCommentPrefix(line));
            return schema;
        }
    }

    if (!inspection.preview_lines.empty()) {
        schema.delimiter = DelimiterDetector::detect(removeCommentPrefix(inspection.preview_lines.front()));
    }

    return schema;
}

ImportResult ComsolCsvImporter::import(const std::string& file_path,
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
            .message = "No se pudo abrir el archivo COMSOL",
            .details = file_path
        };
        return result;
    }

    std::string line{};
    std::vector<std::string> header_columns{};
    std::vector<ComsolColumnGroup> groups{};

    while (std::getline(input, line)) {
        if (isHeaderLine(line)) {
            const auto cleaned = removeCommentPrefix(line);
            header_columns = splitCsvLine(cleaned, schema.delimiter);
            groups = detectGroupsFromHeader(header_columns);
            break;
        }
    }

    if (groups.empty() && !header_columns.empty()) {
        groups = fallbackGroups(header_columns.size());
    }

    if (groups.empty()) {
        result.success = false;
        result.error = beamlab::core::Error{
            .code = beamlab::core::StatusCode::ParseError,
            .severity = beamlab::core::Severity::Error,
            .message = "No se pudieron detectar columnas qx, qy, qz, pidx, t en el CSV de COMSOL",
            .details = file_path
        };
        return result;
    }

    beamlab::data::TrajectoryDataset dataset{};
    dataset.metadata.display_name = context.preferred_display_name.empty()
        ? std::filesystem::path(file_path).stem().string()
        : context.preferred_display_name;
    dataset.metadata.source.original_path = file_path;
    dataset.metadata.source.source_type = "comsol_csv";
    dataset.metadata.source.display_name = dataset.metadata.display_name;
    dataset.metadata.import.importer_name = name();
    dataset.metadata.import.importer_version = "0.2";
    dataset.metadata.import.schema_name = schema.schema_name;

    dataset.variables.registerVariable({"position.x", "qx", "m", false});
    dataset.variables.registerVariable({"position.y", "qy", "m", false});
    dataset.variables.registerVariable({"position.z", "qz", "m", false});
    dataset.variables.registerVariable({"time", "t", "s", false});
    dataset.variables.registerVariable({"particle.index", "cpt.pidx", "1", false});

    std::uint64_t sample_counter = 1;
    std::size_t parsed_rows = 0;
    std::size_t skipped_rows = 0;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        if (!line.empty() && line.front() == '%') {
            continue;
        }

        const auto tokens = splitCsvLine(line, schema.delimiter);

        if (tokens.size() < 6) {
            ++skipped_rows;
            continue;
        }

        const auto row_index = parseDouble(tokens.front());
        if (!row_index.has_value()) {
            ++skipped_rows;
            continue;
        }

        const auto trajectory_id_value = static_cast<std::uint64_t>(std::llround(row_index.value()));

        beamlab::data::Trajectory trajectory{};
        trajectory.id = beamlab::data::TrajectoryId{trajectory_id_value};
        trajectory.samples.reserve(groups.size());

        for (const auto& group : groups) {
            if (group.t_col >= tokens.size()) {
                continue;
            }

            const auto qx = parseDouble(tokens[group.qx_col]);
            const auto qy = parseDouble(tokens[group.qy_col]);
            const auto qz = parseDouble(tokens[group.qz_col]);
            const auto time_s = parseDouble(tokens[group.t_col]);

            if (!qx.has_value() || !qy.has_value() || !qz.has_value() || !time_s.has_value()) {
                continue;
            }

            beamlab::data::TrajectorySample sample{};
            sample.id = beamlab::data::SampleId{sample_counter++};
            sample.trajectory_id = trajectory.id;
            sample.time_s = time_s.value();
            sample.position_m = {qx.value(), qy.value(), qz.value()};

            trajectory.samples.push_back(sample);
        }

        if (!trajectory.samples.empty()) {
            dataset.trajectories.push_back(std::move(trajectory));
            ++parsed_rows;
        } else {
            ++skipped_rows;
        }
    }

    if (dataset.trajectories.empty()) {
        result.success = false;
        result.error = beamlab::core::Error{
            .code = beamlab::core::StatusCode::ParseError,
            .severity = beamlab::core::Severity::Error,
            .message = "El CSV fue leído, pero no se construyó ninguna trayectoria",
            .details = file_path
        };
        return result;
    }

    if (skipped_rows > 0) {
        result.warnings.push_back(beamlab::core::Warning{
            .severity = beamlab::core::Severity::Warning,
            .message = "Algunas filas fueron ignoradas durante la importación COMSOL",
            .details = "parsed_rows=" + std::to_string(parsed_rows) +
                       ", skipped_rows=" + std::to_string(skipped_rows)
        });
    }

    result.success = true;
    result.dataset = std::move(dataset);
    return result;
}

} // namespace beamlab::io
