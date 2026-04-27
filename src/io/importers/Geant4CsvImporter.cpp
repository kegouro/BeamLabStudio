#include "io/importers/Geant4CsvImporter.h"

#include "data/ids/SampleId.h"
#include "data/ids/TrajectoryId.h"
#include "data/model/Trajectory.h"
#include "data/model/TrajectorySample.h"
#include "io/parsing/DelimiterDetector.h"

#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace beamlab::io {
namespace {

struct Geant4Columns {
    std::size_t x_cm{0};
    std::size_t y_cm{1};
    std::size_t z_cm{2};
    std::size_t edep_MeV{3};
    std::size_t kinE_MeV{4};
    std::size_t momx_MeV{5};
    std::size_t momy_MeV{6};
    std::size_t momz_MeV{7};
    std::size_t time_ns{8};
    std::size_t trackID{9};
    std::size_t eventID{10};
    bool valid{false};
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

std::string lower(std::string text)
{
    for (auto& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return text;
}

std::vector<std::string> splitLine(const std::string& line, const char delimiter)
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

std::optional<std::int64_t> parseInteger(const std::string& token)
{
    const auto value = parseDouble(token);
    if (!value.has_value()) {
        return std::nullopt;
    }

    return static_cast<std::int64_t>(std::llround(value.value()));
}

bool looksLikeGeant4Header(const std::string& line)
{
    const auto text = lower(line);

    return text.find("x_cm") != std::string::npos &&
           text.find("y_cm") != std::string::npos &&
           text.find("z_cm") != std::string::npos &&
           text.find("time_ns") != std::string::npos &&
           text.find("trackid") != std::string::npos &&
           text.find("eventid") != std::string::npos;
}

Geant4Columns detectColumns(const std::vector<std::string>& header)
{
    Geant4Columns columns{};

    std::unordered_map<std::string, std::size_t> index_by_name{};
    for (std::size_t i = 0; i < header.size(); ++i) {
        index_by_name[lower(trim(header[i]))] = i;
    }

    const auto lookup = [&](const std::string& name) -> std::optional<std::size_t> {
        const auto it = index_by_name.find(name);
        if (it == index_by_name.end()) {
            return std::nullopt;
        }

        return it->second;
    };

    const auto x = lookup("x_cm");
    const auto y = lookup("y_cm");
    const auto z = lookup("z_cm");
    const auto edep = lookup("edep_mev");
    const auto kine = lookup("kine_mev");
    const auto momx = lookup("momx_mev");
    const auto momy = lookup("momy_mev");
    const auto momz = lookup("momz_mev");
    const auto time = lookup("time_ns");
    const auto track = lookup("trackid");
    const auto event = lookup("eventid");

    if (!x || !y || !z || !time || !track || !event) {
        columns.valid = false;
        return columns;
    }

    columns.x_cm = *x;
    columns.y_cm = *y;
    columns.z_cm = *z;
    columns.edep_MeV = edep.value_or(3);
    columns.kinE_MeV = kine.value_or(4);
    columns.momx_MeV = momx.value_or(5);
    columns.momy_MeV = momy.value_or(6);
    columns.momz_MeV = momz.value_or(7);
    columns.time_ns = *time;
    columns.trackID = *track;
    columns.eventID = *event;
    columns.valid = true;

    return columns;
}

std::uint64_t makeUniqueTrajectoryId(const std::int64_t event_id,
                                     const std::int64_t track_id)
{
    const auto safe_event = event_id < 0 ? 0 : static_cast<std::uint64_t>(event_id);
    const auto safe_track = track_id < 0 ? 0 : static_cast<std::uint64_t>(track_id);

    return safe_event * 10000000ULL + safe_track + 1ULL;
}

} // namespace

std::string Geant4CsvImporter::name() const
{
    return "Geant4CsvImporter";
}

std::vector<std::string> Geant4CsvImporter::supportedExtensions() const
{
    return {".csv", ".txt"};
}

ImporterCapabilityScore Geant4CsvImporter::probe(const ProbeResult& probe_result) const
{
    ImporterCapabilityScore score{};
    score.importer_name = name();

    if (!probe_result.readable) {
        score.confidence = 0.0;
        return score;
    }

    if (probe_result.detected_format == FormatSignature::Geant4Csv) {
        score.confidence = 0.98;
        return score;
    }

    bool looks_like_geant4 = looksLikeGeant4Header(probe_result.first_line_preview);

    for (const auto& line : probe_result.preview_lines) {
        if (looksLikeGeant4Header(line)) {
            looks_like_geant4 = true;
            break;
        }
    }

    score.confidence = looks_like_geant4 ? 0.85 : 0.05;
    return score;
}

InspectionReport Geant4CsvImporter::inspect(const std::string& file_path) const
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

ImportSchema Geant4CsvImporter::buildSchema(const std::string&,
                                            const InspectionReport& inspection) const
{
    ImportSchema schema{};
    schema.schema_name = "geant4_csv";

    for (const auto& line : inspection.preview_lines) {
        if (looksLikeGeant4Header(line)) {
            schema.delimiter = DelimiterDetector::detect(line);
            return schema;
        }
    }

    if (!inspection.preview_lines.empty()) {
        schema.delimiter = DelimiterDetector::detect(inspection.preview_lines.front());
    }

    return schema;
}

ImportResult Geant4CsvImporter::import(const std::string& file_path,
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
            .message = "No se pudo abrir el archivo Geant4 CSV",
            .details = file_path
        };
        return result;
    }

    std::string line{};
    std::vector<std::string> header{};
    Geant4Columns columns{};

    while (std::getline(input, line)) {
        if (looksLikeGeant4Header(line)) {
            header = splitLine(line, schema.delimiter);
            columns = detectColumns(header);
            break;
        }
    }

    if (!columns.valid) {
        result.success = false;
        result.error = beamlab::core::Error{
            .code = beamlab::core::StatusCode::ParseError,
            .severity = beamlab::core::Severity::Error,
            .message = "No se pudieron detectar columnas Geant4 x_cm,y_cm,z_cm,time_ns,trackID,eventID",
            .details = file_path
        };
        return result;
    }

    beamlab::data::TrajectoryDataset dataset{};
    dataset.metadata.display_name = context.preferred_display_name.empty()
        ? std::filesystem::path(file_path).stem().string()
        : context.preferred_display_name;
    dataset.metadata.source.original_path = file_path;
    dataset.metadata.source.source_type = "geant4_csv";
    dataset.metadata.source.display_name = dataset.metadata.display_name;
    dataset.metadata.import.importer_name = name();
    dataset.metadata.import.importer_version = "0.1";
    dataset.metadata.import.schema_name = schema.schema_name;

    dataset.variables.registerVariable({"position.x", "x", "m", false});
    dataset.variables.registerVariable({"position.y", "y", "m", false});
    dataset.variables.registerVariable({"position.z", "z", "m", false});
    dataset.variables.registerVariable({"time", "time", "s", false});
    dataset.variables.registerVariable({"energy_deposit", "edep", "MeV", true});
    dataset.variables.registerVariable({"kinetic_energy", "kinE", "MeV", true});
    dataset.variables.registerVariable({"momentum.x", "momx", "MeV", true});
    dataset.variables.registerVariable({"momentum.y", "momy", "MeV", true});
    dataset.variables.registerVariable({"momentum.z", "momz", "MeV", true});
    dataset.variables.registerVariable({"track_id", "trackID", "1", false});
    dataset.variables.registerVariable({"event_id", "eventID", "1", false});

    std::unordered_map<std::uint64_t, std::size_t> trajectory_index_by_id{};

    std::uint64_t sample_counter = 1;
    std::size_t parsed_rows = 0;
    std::size_t skipped_rows = 0;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        const auto tokens = splitLine(line, schema.delimiter);

        const std::size_t required_size =
            std::max({columns.x_cm, columns.y_cm, columns.z_cm,
                      columns.time_ns, columns.trackID, columns.eventID}) + 1;

        if (tokens.size() < required_size) {
            ++skipped_rows;
            continue;
        }

        const auto x_cm = parseDouble(tokens[columns.x_cm]);
        const auto y_cm = parseDouble(tokens[columns.y_cm]);
        const auto z_cm = parseDouble(tokens[columns.z_cm]);
        const auto time_ns = parseDouble(tokens[columns.time_ns]);
        const auto track_id = parseInteger(tokens[columns.trackID]);
        const auto event_id = parseInteger(tokens[columns.eventID]);

        if (!x_cm || !y_cm || !z_cm || !time_ns || !track_id || !event_id) {
            ++skipped_rows;
            continue;
        }

        const auto unique_id = makeUniqueTrajectoryId(*event_id, *track_id);

        auto index_it = trajectory_index_by_id.find(unique_id);
        if (index_it == trajectory_index_by_id.end()) {
            beamlab::data::Trajectory trajectory{};
            trajectory.id = beamlab::data::TrajectoryId{unique_id};

            dataset.trajectories.push_back(std::move(trajectory));
            const std::size_t new_index = dataset.trajectories.size() - 1;
            trajectory_index_by_id[unique_id] = new_index;
            index_it = trajectory_index_by_id.find(unique_id);
        }

        auto& trajectory = dataset.trajectories[index_it->second];

        beamlab::data::TrajectorySample sample{};
        sample.id = beamlab::data::SampleId{sample_counter++};
        sample.trajectory_id = trajectory.id;

        // Geant4 exporta cm y ns. El modelo interno usa m y s.
        sample.position_m = {
            x_cm.value() * 0.01,
            y_cm.value() * 0.01,
            z_cm.value() * 0.01
        };
        sample.time_s = time_ns.value() * 1.0e-9;

        trajectory.samples.push_back(sample);
        ++parsed_rows;
    }

    if (dataset.trajectories.empty()) {
        result.success = false;
        result.error = beamlab::core::Error{
            .code = beamlab::core::StatusCode::ParseError,
            .severity = beamlab::core::Severity::Error,
            .message = "El CSV Geant4 fue leído, pero no se construyó ninguna trayectoria",
            .details = file_path
        };
        return result;
    }

    if (skipped_rows > 0) {
        result.warnings.push_back(beamlab::core::Warning{
            .severity = beamlab::core::Severity::Warning,
            .message = "Algunas filas fueron ignoradas durante la importación Geant4 CSV",
            .details = "parsed_rows=" + std::to_string(parsed_rows) +
                       ", skipped_rows=" + std::to_string(skipped_rows)
        });
    }

    result.success = true;
    result.dataset = std::move(dataset);
    return result;
}

} // namespace beamlab::io
