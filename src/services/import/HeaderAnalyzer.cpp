#include "services/import/HeaderAnalyzer.h"

#include <cstring>
#include <sstream>

namespace beamlab::services::import {

// ── Normalise ─────────────────────────────────────────────────────

std::string HeaderAnalyzer::normalise(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\r') continue;
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

// ── Column name → enum ───────────────────────────────────────────

HeaderAnalyzer::Column HeaderAnalyzer::matchColumnName(const std::string& raw)
{
    auto n = normalise(raw);

    // Exact or fuzzy matches for known Geant4 column names.
    // Priority: most specific first.

    // x
    if (n == "x_cm" || n == "x_cm." || n == "x[cm]" || n == "x" || n == "pos_x")
        return Column::X_cm;
    // y
    if (n == "y_cm" || n == "y_cm." || n == "y[cm]" || n == "y" || n == "pos_y")
        return Column::Y_cm;
    // z
    if (n == "z_cm" || n == "z_cm." || n == "z[cm]" || n == "z" || n == "pos_z")
        return Column::Z_cm;

    // edep
    if (n == "edep_mev" || n == "edep" || n == "e_dep" || n == "edep_mev." || n == "dose")
        return Column::Edep_MeV;
    // kinetic energy
    if (n == "kine_mev" || n == "kine" || n == "e_kin" || n == "kine_mev." || n == "kinetic_energy")
        return Column::KinE_MeV;

    // momentum
    if (n == "momx_mev" || n == "px_mev" || n == "px")
        return Column::Momx_MeV;
    if (n == "momy_mev" || n == "py_mev" || n == "py")
        return Column::Momy_MeV;
    if (n == "momz_mev" || n == "pz_mev" || n == "pz")
        return Column::Momz_MeV;

    // time
    if (n == "time_ns" || n == "time" || n == "t_ns" || n == "t")
        return Column::Time_ns;

    // IDs
    if (n == "trackid" || n == "track_id" || n == "track" || n == "trkid")
        return Column::TrackID;
    if (n == "parentid" || n == "parent_id" || n == "parent")
        return Column::ParentID;
    if (n == "eventid" || n == "event_id" || n == "event")
        return Column::EventID;

    // PDG & particle
    if (n == "pdg" || n == "pdg_code" || n == "pdgcode" || n == "pdgcode")
        return Column::Pdg;
    if (n == "particlename" || n == "particle_name" || n == "particle" || n == "name")
        return Column::ParticleName;
    if (n == "source_file" || n == "source" || n == "file" || n == "filename")
        return Column::SourceFile;

    return Column::Unknown;
}

// ── Analyse header line ──────────────────────────────────────────

HeaderAnalyzer::Mapping HeaderAnalyzer::analyse(const std::string& headerLine)
{
    Mapping map;

    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(headerLine);

    while (std::getline(ss, token, ',')) {
        // Trim leading/trailing whitespace.
        auto start = token.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) { tokens.push_back(""); continue; }
        auto end = token.find_last_not_of(" \t\r\n");
        tokens.push_back(token.substr(start, end - start + 1));
    }

    for (int i = 0; i < static_cast<int>(tokens.size()); ++i) {
        Column col = matchColumnName(tokens[static_cast<std::size_t>(i)]);
        if (col != Column::Unknown) {
            map.indices[col] = i;
            map.foundColumns.push_back(col);
            ++map.recognisedCount;
        }
    }

    return map;
}

// ── Fetch value from token row ───────────────────────────────────

const std::string& HeaderAnalyzer::Mapping::value(
    Column c, const std::vector<std::string>& tokens) const
{
    auto it = indices.find(c);
    if (it == indices.end())
        throw std::out_of_range("Column not in mapping");

    auto idx = static_cast<std::size_t>(it->second);
    if (idx >= tokens.size())
        throw std::out_of_range("Row has fewer tokens than column index");

    return tokens[idx];
}

} // namespace beamlab::services::import
