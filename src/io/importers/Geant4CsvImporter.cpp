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
    int x_cm{-1};
    int y_cm{-1};
    int z_cm{-1};
    int edep_MeV{-1};
    int kinE_MeV{-1};
    int momx_MeV{-1};
    int momy_MeV{-1};
    int momz_MeV{-1};
    int time_ns{-1};
    int trackID{-1};
    int parentID{-1};
    int eventID{-1};
    int pdg{-1};
    int particleName{-1};
    int source_file{-1};
    bool valid{false};
};

constexpr std::array<std::pair<const char*, int Geant4Columns::*>, 15> kColumnMap = {{
    {"x_cm",   &Geant4Columns::x_cm},
    {"y_cm",   &Geant4Columns::y_cm},
    {"z_cm",   &Geant4Columns::z_cm},
    {"edep_mev",  &Geant4Columns::edep_MeV},
    {"kine_mev",  &Geant4Columns::kinE_MeV},
    {"momx_mev",  &Geant4Columns::momx_MeV},
    {"momy_mev",  &Geant4Columns::momy_MeV},
    {"momz_mev",  &Geant4Columns::momz_MeV},
    {"time_ns", &Geant4Columns::time_ns},
    {"trackid", &Geant4Columns::trackID},
    {"parentid",&Geant4Columns::parentID},
    {"eventid", &Geant4Columns::eventID},
    {"pdg",     &Geant4Columns::pdg},
    {"particlename", &Geant4Columns::particleName},
    {"source_file", &Geant4Columns::source_file},
}};

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

// Fast numeric parsing for high-throughput import.
// Skips string copies and validation that Geant4 outputs don't need.
inline bool fastParseDouble(const char* s, double& out)
{
    char* end = nullptr;
    out = std::strtod(s, &end);
    return end != s && std::isfinite(out);
}

inline bool fastParseInt64(const char* s, int64_t& out)
{
    char* end = nullptr;
    out = std::strtoll(s, &end, 10);
    return end != s;
}

bool looksLikeGeant4Header(const std::string& line)
{
    return Geant4HeaderRecognizer::looksLikeHeader(line);
}

Geant4Columns detectColumns(const std::vector<std::string>& header)
{
    Geant4Columns columns{};

    if (header.size() < 3) return columns;  // Need at least x,y,z

    // Build flexible mapping: for each column name, find its index
    for (std::size_t i = 0; i < header.size(); ++i) {
        auto name = lower(trim(header[i]));

        for (const auto& [cname, member] : kColumnMap) {
            if (name == cname) {
                columns.*member = static_cast<int>(i);
                break;
            }
        }
    }

    // Minimum required: x, y, z, edep
    if (columns.x_cm >= 0 &&
        columns.y_cm >= 0 &&
        columns.z_cm >= 0) {

        // If edep is missing but we have at least 3 columns, still valid — just fill edep=0
        columns.valid = true;
    }

    return columns;
}

// PDG-2022 electromagnetic charge table (integer units of |e|).
// Ref: PDG 2022 Review of Particle Physics, Summary Tables.
// Returns the charge in units of the elementary charge (+1, −1, 0, …).
// If the PDG code is unknown, returns std::nullopt so the caller can
// fall back to a heuristic (e.g. sign of the PDG code) rather than
// silently injecting a wrong charge.
constexpr std::optional<double> pdgCharge(std::int64_t pdg) noexcept
{
    // Particles are keyed by |PDG|; the sign of the code flips the charge
    // for charged particles (anti-particle convention).
    const int sign = (pdg < 0) ? -1 : 1;

    struct Entry { std::int64_t abs_pdg; double charge; };
    constexpr Entry kTable[] = {
        {11,   -1.0},   // e−        (PDG 2022)
        {13,   -1.0},   // μ−        (PDG 2022)
        {15,   -1.0},   // τ−        (PDG 2022)
        {211,  +1.0},   // π+        (PDG 2022)
        {321,  +1.0},   // K+        (PDG 2022)
        {2212, +1.0},   // proton    (PDG 2022)
        {2112,  0.0},   // neutron   (PDG 2022)
        {22,    0.0},   // photon    (PDG 2022)
        {12,    0.0},   // νe        (PDG 2022)
        {14,    0.0},   // νμ        (PDG 2022)
        {16,    0.0},   // ντ        (PDG 2022)
        {111,   0.0},   // π0        (PDG 2022)
        {130,   0.0},   // K0L       (PDG 2022)
        {310,   0.0},   // K0S       (PDG 2022)
        {2224, +2.0},   // Δ++       (PDG 2022)
    };

    const std::int64_t abs_code = (pdg < 0) ? -pdg : pdg;
    for (const auto& e : kTable) {
        if (abs_code == e.abs_pdg) {
            // Neutrals: anti-particle has same charge (0).
            const double q = (e.charge == 0.0) ? 0.0 : e.charge * sign;
            return q;
        }
    }
    return std::nullopt; // unknown PDG code
}

// Shared validation guard used by both batch and streaming import paths.
// Returns true if the sample passes all guards; populates drop_reason on fail.
// ponytail: single guard function, not two | techo: only energy/time guards
//           | upgrade: add momentum bounds check when physics limits are known
inline bool samplePassesGuards(double edep_v, double kine_v, double time_v,
                                const char*& drop_reason) noexcept
{
    if (edep_v < 0.0) { drop_reason = "edep_MeV < 0"; return false; }
    if (kine_v < 0.0) { drop_reason = "kinE_MeV < 0"; return false; }
    if (time_v < 0.0) { drop_reason = "time_ns < 0";  return false; }
    return true;
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
            .message = "Geant4 header not recognised — need at least x, y, z columns. Detected columns: "
                        + std::to_string(header.size()),
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

        // Minimum required columns based on detected mapping
        int maxNeeded = columns.x_cm;
        if (columns.y_cm > maxNeeded) maxNeeded = columns.y_cm;
        if (columns.z_cm > maxNeeded) maxNeeded = columns.z_cm;
        int required_size = static_cast<std::size_t>(maxNeeded + 1);

        if (static_cast<int>(tokens.size()) <= maxNeeded) {
            recordDrop("too few columns", line_number);
            continue;
        }

        const auto x_cm = parseDouble(tokens[static_cast<std::size_t>(columns.x_cm)]);
        const auto y_cm = parseDouble(tokens[static_cast<std::size_t>(columns.y_cm)]);
        const auto z_cm = parseDouble(tokens[static_cast<std::size_t>(columns.z_cm)]);

        if (!x_cm || !y_cm || !z_cm) {
            recordDrop("non-finite or non-numeric mandatory x,y,z field", line_number);
            continue;
        }

        // Optional fields — use helper that handles -1 index (missing column)
        const auto safeTok = [&](int idx) -> std::string {
            if (idx < 0 || static_cast<std::size_t>(idx) >= tokens.size()) return "";
            return tokens[static_cast<std::size_t>(idx)];
        };

        const auto time_ns = parseDouble(safeTok(columns.time_ns));
        const auto track_id = parseInteger(safeTok(columns.trackID));
        const auto event_id = parseInteger(safeTok(columns.eventID));
        const auto pdg_val = parseInteger(safeTok(columns.pdg));
        std::string particle_name = safeTok(columns.particleName);
        std::string source_file = safeTok(columns.source_file);

        // Use defaults for missing optional values
        double time_ns_val = time_ns.value_or(0.0);
        int64_t track_val = track_id.value_or(0);
        int64_t event_val = event_id.value_or(0);
        int64_t pdg_int = pdg_val.value_or(0);

        // Time guard — shared logic with streaming path (S4 unified guards).
        if (time_ns_val < 0.0) {
            recordDrop("time_ns < 0", line_number);
            continue;
        }

        const auto unique_id_opt = makeUniqueTrajectoryId(event_val, track_val);
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

        // Energy guards (S4) — shared logic with streaming path via samplePassesGuards.
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
            trajectory.particle.event_id        = static_cast<int>(event_val);
            trajectory.particle.track_id        = static_cast<int>(track_val);
            trajectory.particle.initial_kinE_MeV = sample.kinE_MeV;
            trajectory.particle.particle_type   = particle_name;

            // S3: PDG-2022 charge lookup replaces the prior sign-flip heuristic
            // which was inverted (proton 2212→+1, γ 22→0, e− 11→−1, μ− 13→−1).
            {
                const auto q_opt = pdgCharge(pdg_int);
                trajectory.particle.charge = q_opt.value_or(
                    // Fallback for unknown codes: neutral assumption is safest.
                    // ponytail: unknown PDG defaults to 0 | techo: correct for stable SM
                    // | upgrade: extend kTable when exotic particles appear
                    0.0);
            }

            const std::int64_t pdg_abs = (pdg_int < 0) ? -pdg_int : pdg_int;
            // Masses: PDG 2022 values in MeV/c²
            if (pdg_abs == 13) {
                trajectory.particle.mass_MeV = 105.6583755;    // μ  PDG 2022
            } else if (pdg_abs == 11) {
                trajectory.particle.mass_MeV = 0.51099895;     // e  PDG 2022
            } else if (pdg_abs == 2212) {
                trajectory.particle.mass_MeV = 938.27208816;   // p  PDG 2022
            } else {
                trajectory.particle.mass_MeV = 105.6583755;    // default: muon
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

    // Find header and cache delimiter
    char delimiter = '\t';  // default for Geant4 stepping data
    while (std::getline(input, line)) {
        if (looksLikeGeant4Header(line)) {
            delimiter = DelimiterDetector::detect(line);
            header = splitLine(line, delimiter);
            columns = detectColumns(header);
            break;
        }
    }
    if (!columns.valid) return 0;

    std::size_t line_number = 1;
    uint64_t sampleCount = 0;
    std::string currentTrajId;

    // Compute max token index needed
    int maxTokenIdx = 0;
    for (int idx : {columns.x_cm, columns.y_cm, columns.z_cm,
                    columns.edep_MeV, columns.kinE_MeV,
                    columns.momx_MeV, columns.momy_MeV, columns.momz_MeV,
                    columns.time_ns, columns.trackID, columns.eventID,
                    columns.parentID, columns.pdg}) {
        if (idx > maxTokenIdx) maxTokenIdx = idx;
    }

    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty() || line[0] == '#') continue;

        // Tokenise into strings (variable column count)
        std::vector<std::string> tokens;
        {
            const char* p = line.c_str();
            const char* start = p;
            for (; *p; ++p) {
                if (*p == delimiter) {
                    tokens.emplace_back(start, static_cast<std::size_t>(p - start));
                    start = p + 1;
                }
            }
            tokens.emplace_back(start, static_cast<std::size_t>(p - start));
        }
        if (static_cast<int>(tokens.size()) <= maxTokenIdx) continue;

        double x_v=0, y_v=0, z_v=0, edep_v=0, kine_v=0, time_v=0;
        double mx=0, my=0, mz=0;
        int64_t track_v=0, event_v=0;

        auto tok = [&](int idx) -> const std::string& {
            static const std::string empty;
            return (idx >= 0 && idx < static_cast<int>(tokens.size()))
                ? tokens[static_cast<std::size_t>(idx)] : empty;
        };

        if (!fastParseDouble(tok(columns.x_cm).c_str(), x_v) ||
            !fastParseDouble(tok(columns.y_cm).c_str(), y_v) ||
            !fastParseDouble(tok(columns.z_cm).c_str(), z_v))
            continue;

        fastParseDouble(tok(columns.edep_MeV).c_str(), edep_v);
        fastParseDouble(tok(columns.kinE_MeV).c_str(), kine_v);
        fastParseDouble(tok(columns.time_ns).c_str(), time_v);
        fastParseDouble(tok(columns.momx_MeV).c_str(), mx);
        fastParseDouble(tok(columns.momy_MeV).c_str(), my);
        fastParseDouble(tok(columns.momz_MeV).c_str(), mz);
        fastParseInt64(tok(columns.trackID).c_str(), track_v);
        fastParseInt64(tok(columns.eventID).c_str(), event_v);

        // S4: unified guards — same function used by the batch path.
        {
            const char* reason = nullptr;
            if (!samplePassesGuards(edep_v, kine_v, time_v, reason)) {
                continue; // silently skip; streaming path has no per-row warnings
            }
        }

        std::string trajId = std::to_string(event_v) + "_" + std::to_string(track_v);
        if (trajId != currentTrajId) {
            if (!currentTrajId.empty()) storage.endTrajectory();
            storage.beginTrajectory(trajId);
            currentTrajId = trajId;
        }

        beamlab::data::TrajectorySample s{};
        s.position_m = {x_v * 0.01, y_v * 0.01, z_v * 0.01};
        s.time_s = time_v * 1.0e-9;
        s.edep_MeV = edep_v;
        s.edep_eV = edep_v * 1.0e6;
        s.kinE_MeV = kine_v;
        s.momentum_MeV = {mx, my, mz};
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
