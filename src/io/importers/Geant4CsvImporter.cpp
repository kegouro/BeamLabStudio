//!@math-begin module="Geant4Import" title="Importación de CSV Geant4 — Conversión de Unidades"
//!@math El CSV de Geant4 usa unidades del sistema de simulación.
//!@math BeamLabStudio convierte internamente al SI (metros, segundos):
//!@section Conversión de posiciones
//!@formula x [m] = x_cm · 0.01
//!@formula y [m] = y_cm · 0.01
//!@formula z [m] = z_cm · 0.01
//!@section Conversión de tiempo
//!@formula t [s] = t_ns · 10⁻⁹
//!@section Energía — se preserva en MeV (unidad nativa de física de partículas)
//!@formula edep [MeV]  → se almacena tal cual
//!@formula edep [eV]   = edep [MeV] · 10⁶
//!@formula kinE [MeV]  → se almacena tal cual
//!@section Identificación de trayectorias
//!@math Cada (eventID, trackID) genera un ID de trayectoria único:
//!@formula unique_id = eventID · 10 000 000 + trackID + 1
//!@note El primer paso de cada trayectoria (edep ≈ 0, kinE = E_gun) define
//!@note la información de partícula: energía inicial, tipo (mu⁻ por defecto), carga.
//!@section Información de partícula por defecto (muón)
//!@formula tipo = "mu-",   carga = −1,   masa = 105.6583755 MeV/c²
//!@math-end

#include "io/importers/Geant4CsvImporter.h"

#include "core/foundation/NumericGuards.h"
#include "core/storage/ISampleStorage.h"
#include "core/pipeline/ProgressTracker.h"
#include "data/ids/SampleId.h"
#include "data/ids/TrajectoryId.h"
#include "data/model/Trajectory.h"
#include "data/model/TrajectorySample.h"
#include "io/parsing/DelimiterDetector.h"
#include "io/parsing/Geant4HeaderRecognizer.h"

#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
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
    std::size_t parentID{10};
    std::size_t eventID{11};
    std::size_t pdg{12};
    std::size_t particleName{13};
    std::size_t source_file{14};
    bool valid{false};
};

constexpr std::array<const char*, 15> kExpectedHeader = {
    "x_cm", "y_cm", "z_cm", "edep_MeV", "kinE_MeV",
    "momx_MeV", "momy_MeV", "momz_MeV", "time_ns",
    "trackID", "parentID", "eventID", "pdg",
    "particleName", "source_file"
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
    return beamlab::core::tryParseFiniteDouble(token);
}

std::optional<std::int64_t> parseInteger(const std::string& token)
{
    const auto value = beamlab::core::tryParseInteger(token);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(*value);
}

bool looksLikeGeant4Header(const std::string& line)
{
    return Geant4HeaderRecognizer::looksLikeHeader(line);
}

Geant4Columns detectColumns(const std::vector<std::string>& header)
{
    Geant4Columns columns{};

    if (header.size() < 15) {
        return columns;
    }

    for (std::size_t i = 0; i < 15; ++i) {
        if (lower(trim(header[i])) != lower(kExpectedHeader[i])) {
            return columns;
        }
    }

    columns.valid = true;
    return columns;
}

// Invariant: at most kMaxTracksPerEvent tracks per event.  The unique
// trajectory id packs (event, track) into a single 64-bit slot:
//   unique_id = event * kMaxTracksPerEvent + track + 1
// If track >= kMaxTracksPerEvent this packing overflows into the next
// event's slot — silently colliding two trajectories.  We detect the
// violation and return std::nullopt; the caller logs a warning and
// skips the row instead of producing fraudulent data.
constexpr std::uint64_t kMaxTracksPerEvent = 10'000'000ULL;

std::optional<std::uint64_t> makeUniqueTrajectoryId(
    const std::int64_t event_id,
    const std::int64_t track_id)
{
    const auto safe_event = event_id < 0 ? 0 : static_cast<std::uint64_t>(event_id);
    const auto safe_track = track_id < 0 ? 0 : static_cast<std::uint64_t>(track_id);

    if (safe_track >= kMaxTracksPerEvent) {
        return std::nullopt;
    }

    constexpr std::uint64_t kMaxEvent =
        std::numeric_limits<std::uint64_t>::max() / kMaxTracksPerEvent;
    if (safe_event > kMaxEvent) {
        return std::nullopt;
    }

    return safe_event * kMaxTracksPerEvent + safe_track + 1ULL;
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
            .message = "Geant4 header does not match expected 15-column format: x_cm y_cm z_cm edep_MeV kinE_MeV momx_MeV momy_MeV momz_MeV time_ns trackID parentID eventID pdg particleName source_file",
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

    // Aggregate per-reason warning counts.  We preserve the line number of the
    // first occurrence of each kind so the user can navigate to it; further
    // occurrences are summarised in a single warning record at the end.
    struct DropReasonStats {
        std::size_t count{0};
        std::size_t first_line{0};
    };
    std::unordered_map<std::string, DropReasonStats> drop_stats;
    const auto recordDrop = [&](const std::string& reason, std::size_t line_no) {
        auto& s = drop_stats[reason];
        if (s.count == 0) s.first_line = line_no;
        ++s.count;
    };

    // Header was consumed via std::getline above; subsequent reads are the
    // body.  We seek the file line number relative to the start so users can
    // jump straight to the offending row in their editor.
    // Track the running line number; the header consumed before this loop
    // determines the offset.  After the header read, input.tellg() can fail on
    // some text-mode streams, so we count lines explicitly.
    std::size_t line_number = 1; // header line itself
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }

        const auto tokens = splitLine(line, schema.delimiter);

        constexpr std::size_t required_size = 15;

        if (tokens.size() < required_size) {
            recordDrop("too few columns", line_number);
            continue;
        }

        const auto x_cm = parseDouble(tokens[columns.x_cm]);
        const auto y_cm = parseDouble(tokens[columns.y_cm]);
        const auto z_cm = parseDouble(tokens[columns.z_cm]);
        const auto time_ns = parseDouble(tokens[columns.time_ns]);
        const auto track_id = parseInteger(tokens[columns.trackID]);
        const auto event_id = parseInteger(tokens[columns.eventID]);

        if (!x_cm || !y_cm || !z_cm || !time_ns || !track_id || !event_id) {
            recordDrop("non-finite or non-numeric mandatory field", line_number);
            continue;
        }

        if (*time_ns < 0.0) {
            recordDrop("time_ns < 0", line_number);
            continue;
        }

        const auto pdg_val = parseInteger(tokens[columns.pdg]);
        std::string particle_name = tokens[columns.particleName];

        if (!pdg_val) {
            recordDrop("pdg not parseable", line_number);
            continue;
        }

        const auto unique_id_opt = makeUniqueTrajectoryId(*event_id, *track_id);
        if (!unique_id_opt) {
            recordDrop("trackID exceeds kMaxTracksPerEvent or eventID overflow", line_number);
            continue;
        }
        const auto unique_id = *unique_id_opt;

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

        // Read optional physics columns.  Distinguish three states:
        //   1. column index out of range  → field truly absent (use 0)
        //   2. column present, token blank → field absent in this row (use 0)
        //   3. column present, token non-blank but unparseable → row corrupt
        //      (drop with diagnostic — never silently coerce to 0)
        const auto readOptionalNumeric = [&](std::size_t col_idx,
                                              const char* col_name,
                                              std::optional<double>& out) -> bool {
            if (col_idx >= tokens.size()) return true;          // absent column
            const auto& tok = tokens[col_idx];
            if (tok.empty()) return true;                        // blank cell
            auto v = beamlab::core::tryParseFiniteDouble(tok);
            if (!v) {
                recordDrop(std::string{col_name} + " not finite/parseable",
                           line_number);
                return false;
            }
            out = v;
            return true;
        };

        std::optional<double> edep_opt, kine_opt, momx_opt, momy_opt, momz_opt;
        if (!readOptionalNumeric(columns.edep_MeV, "edep_MeV", edep_opt)) continue;
        if (!readOptionalNumeric(columns.kinE_MeV, "kinE_MeV", kine_opt)) continue;
        if (!readOptionalNumeric(columns.momx_MeV, "momx_MeV", momx_opt)) continue;
        if (!readOptionalNumeric(columns.momy_MeV, "momy_MeV", momy_opt)) continue;
        if (!readOptionalNumeric(columns.momz_MeV, "momz_MeV", momz_opt)) continue;

        // Reject negative energies — these are not physically meaningful and
        // would silently bias dose/energy aggregates downstream.
        if (edep_opt && *edep_opt < 0.0) {
            recordDrop("edep_MeV < 0", line_number);
            continue;
        }
        if (kine_opt && *kine_opt < 0.0) {
            recordDrop("kinE_MeV < 0", line_number);
            continue;
        }

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

        // Physics fields — stored in native Geant4 units (MeV, MeV/c)
        sample.edep_MeV  = edep_opt.value_or(0.0);
        sample.edep_eV   = sample.edep_MeV * 1.0e6;
        sample.kinE_MeV  = kine_opt.value_or(0.0);
        sample.momentum_MeV = {
            momx_opt.value_or(0.0),
            momy_opt.value_or(0.0),
            momz_opt.value_or(0.0)
        };

        if (trajectory.samples.empty()) {
            trajectory.particle.event_id        = static_cast<int>(*event_id);
            trajectory.particle.track_id        = static_cast<int>(*track_id);
            trajectory.particle.initial_kinE_MeV = sample.kinE_MeV;
            trajectory.particle.particle_type   = particle_name;

            const int pdg_abs = *pdg_val < 0 ? -static_cast<int>(*pdg_val)
                                             : static_cast<int>(*pdg_val);
            trajectory.particle.charge = (*pdg_val < 0) ? 1.0 : -1.0;
            if (pdg_abs == 13) {
                trajectory.particle.mass_MeV = 105.6583755;
            } else if (pdg_abs == 11) {
                trajectory.particle.mass_MeV = 0.51099895;
            } else if (pdg_abs == 2212) {
                trajectory.particle.mass_MeV = 938.272089;
            } else {
                trajectory.particle.mass_MeV = 105.6583755;
            }
        }

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

    if (!drop_stats.empty()) {
        std::size_t total_skipped = 0;
        for (const auto& [reason, stats] : drop_stats) {
            total_skipped += stats.count;
            result.warnings.push_back(beamlab::core::Warning{
                .severity = beamlab::core::Severity::Warning,
                .message = "Filas Geant4 CSV ignoradas: " + reason,
                .details = "count=" + std::to_string(stats.count) +
                           ", first_line=" + std::to_string(stats.first_line)
            });
        }
        result.warnings.push_back(beamlab::core::Warning{
            .severity = beamlab::core::Severity::Info,
            .message = "Resumen de importación Geant4 CSV",
            .details = "parsed_rows=" + std::to_string(parsed_rows) +
                       ", skipped_rows=" + std::to_string(total_skipped)
        });
    }

    result.success = true;
    result.dataset = std::move(dataset);
    return result;
}

uint64_t Geant4CsvImporter::importStreaming(
    const std::string& file_path,
    beamlab::core::ISampleStorage& storage,
    beamlab::core::ProgressTracker* progress) const
{
    std::ifstream input(file_path);
    if (!input) return 0;

    // Get file size for progress
    input.seekg(0, std::ios::end);
    auto fileSize = static_cast<int64_t>(input.tellg());
    input.seekg(0, std::ios::beg);

    std::string line;
    std::vector<std::string> header;
    Geant4Columns columns{};

    // Find header
    while (std::getline(input, line)) {
        if (looksLikeGeant4Header(line)) {
            auto delim = DelimiterDetector::detect(line);
            header = splitLine(line, delim);
            columns = detectColumns(header);
            break;
        }
    }
    if (!columns.valid) return 0;

    std::size_t line_number = 1;
    uint64_t sampleCount = 0;
    std::string currentTrajId;

    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty() || line[0] == '#') continue;

        const auto delim = DelimiterDetector::detect(line);
        const auto tokens = splitLine(line, delim);
        if (tokens.size() < 15) continue;

        const auto x_cm = parseDouble(tokens[0]);
        const auto y_cm = parseDouble(tokens[1]);
        const auto z_cm = parseDouble(tokens[2]);
        const auto edep = parseDouble(tokens[3]);
        const auto kine = parseDouble(tokens[4]);
        const auto time_ns = parseDouble(tokens[8]);
        const auto track_id = parseInteger(tokens[9]);
        const auto event_id = parseInteger(tokens[11]);
        const auto pdg_val = parseInteger(tokens[12]);

        if (!x_cm || !y_cm || !z_cm || !time_ns || !track_id || !event_id || !pdg_val)
            continue;

        std::string trajId = std::to_string(*event_id) + "_" + std::to_string(*track_id);
        if (trajId != currentTrajId) {
            if (!currentTrajId.empty()) storage.endTrajectory();
            storage.beginTrajectory(trajId);
            currentTrajId = trajId;
        }

        beamlab::data::TrajectorySample s{};
        s.position_m = {*x_cm * 0.01, *y_cm * 0.01, *z_cm * 0.01};
        s.time_s = *time_ns * 1.0e-9;
        s.edep_MeV = edep.value_or(0.0);
        s.edep_eV = s.edep_MeV * 1.0e6;
        s.kinE_MeV = kine.value_or(0.0);
        s.momentum_MeV = {
            tokens.size() > 5 ? parseDouble(tokens[5]).value_or(0.0) : 0.0,
            tokens.size() > 6 ? parseDouble(tokens[6]).value_or(0.0) : 0.0,
            tokens.size() > 7 ? parseDouble(tokens[7]).value_or(0.0) : 0.0
        };
        storage.addSample(s);
        ++sampleCount;

        if (sampleCount % 10000 == 0) {
            storage.flush();
            if (progress) {
                auto pos = static_cast<int64_t>(input.tellg());
                if (pos < 0) pos = static_cast<int64_t>(sampleCount) * 200;
                beamlab::core::AnalysisProgress p;
                p.fraction = fileSize > 0 ? static_cast<double>(pos) / static_cast<double>(fileSize) : 0.0;
                p.stage = "importing";
                p.bytesProcessed = pos;
                p.totalBytes = fileSize;
                progress->onProgress(p);
                if (progress->cancelled()) break;
            }
        }
    }
    if (!currentTrajId.empty()) storage.endTrajectory();
    storage.flush();

    if (progress) {
        beamlab::core::AnalysisProgress p;
        p.fraction = 1.0;
        p.stage = "importing";
        p.bytesProcessed = fileSize;
        p.totalBytes = fileSize;
        progress->onProgress(p);
    }
    return sampleCount;
}

} // namespace beamlab::io
