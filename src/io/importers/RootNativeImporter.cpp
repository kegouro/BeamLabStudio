#include "io/importers/RootNativeImporter.h"

#include "data/ids/SampleId.h"
#include "data/ids/TrajectoryId.h"
#include "data/model/Trajectory.h"
#include "data/model/TrajectorySample.h"

#include <TFile.h>
#include <TKey.h>
#include <TTree.h>
#include <TTreeFormula.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace beamlab::io {
namespace {

struct RootColumns {
    std::string x_cm{};
    std::string y_cm{};
    std::string z_cm{};
    std::string edep_MeV{};
    std::string kinE_MeV{};
    std::string momx_MeV{};
    std::string momy_MeV{};
    std::string momz_MeV{};
    std::string time_ns{};
    std::string trackID{};
    std::string eventID{};
    bool valid{false};
};

std::string normalize(std::string text)
{
    std::string out{};
    out.reserve(text.size());

    for (const auto c : text) {
        if (c == ' ' || c == '-' || c == '.' || c == '/') {
            out.push_back('_');
        } else {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }

    return out;
}

bool matchesAny(const std::string& normalized,
                const std::vector<std::string>& aliases)
{
    for (const auto& alias : aliases) {
        if (normalized == normalize(alias)) {
            return true;
        }
    }

    return false;
}

TTree* findFirstTree(TFile& file)
{
    TIter next(file.GetListOfKeys());
    TKey* key = nullptr;

    while ((key = static_cast<TKey*>(next())) != nullptr) {
        auto* object = key->ReadObj();

        if (auto* tree = dynamic_cast<TTree*>(object)) {
            return tree;
        }
    }

    return nullptr;
}

std::vector<std::string> branchNames(TTree& tree)
{
    std::vector<std::string> names{};

    const auto* branches = tree.GetListOfBranches();
    if (!branches) {
        return names;
    }

    for (int i = 0; i < branches->GetEntries(); ++i) {
        const auto* branch = branches->At(i);
        if (branch) {
            names.emplace_back(branch->GetName());
        }
    }

    return names;
}

std::optional<std::string> findBranch(const std::vector<std::string>& names,
                                      const std::vector<std::string>& aliases)
{
    for (const auto& name : names) {
        if (matchesAny(normalize(name), aliases)) {
            return name;
        }
    }

    return std::nullopt;
}

RootColumns resolveColumns(const std::vector<std::string>& names)
{
    RootColumns c{};

    const auto x = findBranch(names, {"x_cm", "x", "posx", "position_x", "prex", "postx"});
    const auto y = findBranch(names, {"y_cm", "y", "posy", "position_y", "prey", "posty"});
    const auto z = findBranch(names, {"z_cm", "z", "posz", "position_z", "prez", "postz"});
    const auto time = findBranch(names, {"time_ns", "time", "globaltime", "global_time", "t"});
    const auto track = findBranch(names, {"trackid", "track_id", "track"});
    const auto event = findBranch(names, {"eventid", "event_id", "event"});

    if (!x || !y || !z || !time || !track || !event) {
        c.valid = false;
        return c;
    }

    c.x_cm = *x;
    c.y_cm = *y;
    c.z_cm = *z;
    c.time_ns = *time;
    c.trackID = *track;
    c.eventID = *event;

    c.edep_MeV = findBranch(names, {"edep_mev", "edep", "energydeposit", "energy_deposit", "de"}).value_or("");
    c.kinE_MeV = findBranch(names, {"kine_mev", "kine", "kineticenergy", "kinetic_energy", "ekin"}).value_or("");
    c.momx_MeV = findBranch(names, {"momx_mev", "momx", "px", "momentum_x"}).value_or("");
    c.momy_MeV = findBranch(names, {"momy_mev", "momy", "py", "momentum_y"}).value_or("");
    c.momz_MeV = findBranch(names, {"momz_mev", "momz", "pz", "momentum_z"}).value_or("");

    c.valid = true;
    return c;
}

std::uint64_t makeUniqueTrajectoryId(const std::int64_t event_id,
                                     const std::int64_t track_id)
{
    const auto safe_event = event_id < 0 ? 0 : static_cast<std::uint64_t>(event_id);
    const auto safe_track = track_id < 0 ? 0 : static_cast<std::uint64_t>(track_id);

    return safe_event * 10000000ULL + safe_track + 1ULL;
}

double evalFormula(TTreeFormula& formula)
{
    return formula.EvalInstance();
}

} // namespace

std::string RootNativeImporter::name() const
{
    return "RootNativeImporter";
}

std::vector<std::string> RootNativeImporter::supportedExtensions() const
{
    return {".root"};
}

ImporterCapabilityScore RootNativeImporter::probe(const ProbeResult& probe_result) const
{
    ImporterCapabilityScore score{};
    score.importer_name = name();

    if (!probe_result.readable) {
        score.confidence = 0.0;
        return score;
    }

    score.confidence = probe_result.detected_format == FormatSignature::RootFile ? 0.99 : 0.0;
    return score;
}

InspectionReport RootNativeImporter::inspect(const std::string& file_path) const
{
    InspectionReport report{};
    report.file_path = file_path;
    report.readable = std::filesystem::exists(file_path);
    return report;
}

ImportSchema RootNativeImporter::buildSchema(const std::string&,
                                             const InspectionReport&) const
{
    ImportSchema schema{};
    schema.schema_name = "root_native_auto_tree";
    return schema;
}

ImportResult RootNativeImporter::import(const std::string& file_path,
                                        const ImportSchema& schema,
                                        const ImportContext& context) const
{
    ImportResult result{};

    std::unique_ptr<TFile> file(TFile::Open(file_path.c_str(), "READ"));

    if (!file || file->IsZombie()) {
        result.success = false;
        result.error = beamlab::core::Error{
            .code = beamlab::core::StatusCode::IOError,
            .severity = beamlab::core::Severity::Error,
            .message = "No se pudo abrir el archivo ROOT",
            .details = file_path
        };
        return result;
    }

    TTree* tree = findFirstTree(*file);

    if (!tree) {
        result.success = false;
        result.error = beamlab::core::Error{
            .code = beamlab::core::StatusCode::ParseError,
            .severity = beamlab::core::Severity::Error,
            .message = "No se encontró ningún TTree en el archivo ROOT",
            .details = file_path
        };
        return result;
    }

    const auto branches = branchNames(*tree);
    const auto columns = resolveColumns(branches);

    if (!columns.valid) {
        result.success = false;
        result.error = beamlab::core::Error{
            .code = beamlab::core::StatusCode::ParseError,
            .severity = beamlab::core::Severity::Error,
            .message = "No se pudieron resolver ramas ROOT requeridas",
            .details = "Se requieren x/y/z/time/trackID/eventID o alias reconocibles."
        };
        return result;
    }

    TTreeFormula fx("fx", columns.x_cm.c_str(), tree);
    TTreeFormula fy("fy", columns.y_cm.c_str(), tree);
    TTreeFormula fz("fz", columns.z_cm.c_str(), tree);
    TTreeFormula ft("ft", columns.time_ns.c_str(), tree);
    TTreeFormula ftrack("ftrack", columns.trackID.c_str(), tree);
    TTreeFormula fevent("fevent", columns.eventID.c_str(), tree);

    beamlab::data::TrajectoryDataset dataset{};
    dataset.metadata.display_name = context.preferred_display_name.empty()
        ? std::filesystem::path(file_path).stem().string()
        : context.preferred_display_name;
    dataset.metadata.source.original_path = file_path;
    dataset.metadata.source.source_type = "root_native";
    dataset.metadata.source.display_name = dataset.metadata.display_name;
    dataset.metadata.import.importer_name = name();
    dataset.metadata.import.importer_version = "0.1";
    dataset.metadata.import.schema_name = schema.schema_name;

    dataset.variables.registerVariable({"position.x", "x", "m", false});
    dataset.variables.registerVariable({"position.y", "y", "m", false});
    dataset.variables.registerVariable({"position.z", "z", "m", false});
    dataset.variables.registerVariable({"time", "time", "s", false});
    dataset.variables.registerVariable({"track_id", "trackID", "1", false});
    dataset.variables.registerVariable({"event_id", "eventID", "1", false});

    std::unordered_map<std::uint64_t, std::size_t> trajectory_index_by_id{};
    std::uint64_t sample_counter = 1;

    const auto entries = tree->GetEntries();

    for (Long64_t i = 0; i < entries; ++i) {
        tree->GetEntry(i);

        const double x_cm = evalFormula(fx);
        const double y_cm = evalFormula(fy);
        const double z_cm = evalFormula(fz);
        const double time_ns = evalFormula(ft);

        const auto track_id = static_cast<std::int64_t>(std::llround(evalFormula(ftrack)));
        const auto event_id = static_cast<std::int64_t>(std::llround(evalFormula(fevent)));

        if (!std::isfinite(x_cm) ||
            !std::isfinite(y_cm) ||
            !std::isfinite(z_cm) ||
            !std::isfinite(time_ns)) {
            continue;
        }

        const auto unique_id = makeUniqueTrajectoryId(event_id, track_id);

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
        sample.position_m = {x_cm * 0.01, y_cm * 0.01, z_cm * 0.01};
        sample.time_s = time_ns * 1.0e-9;

        trajectory.samples.push_back(sample);
    }

    if (dataset.trajectories.empty()) {
        result.success = false;
        result.error = beamlab::core::Error{
            .code = beamlab::core::StatusCode::ParseError,
            .severity = beamlab::core::Severity::Error,
            .message = "ROOT fue leído, pero no se construyó ninguna trayectoria",
            .details = file_path
        };
        return result;
    }

    result.success = true;
    result.dataset = std::move(dataset);
    return result;
}

} // namespace beamlab::io
