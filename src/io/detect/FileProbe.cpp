#include "io/detect/FileProbe.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace beamlab::io {
namespace {

std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool containsComsolTrajectoryHeader(const std::string& preview)
{
    const auto text = toLower(preview);

    const bool has_qx = text.find("qx") != std::string::npos;
    const bool has_qy = text.find("qy") != std::string::npos;
    const bool has_qz = text.find("qz") != std::string::npos;
    const bool has_pidx = text.find("pidx") != std::string::npos ||
                          text.find("particle") != std::string::npos;
    const bool has_time = text.find("t") != std::string::npos;

    return has_qx && has_qy && has_qz && has_pidx && has_time;
}

bool containsGeant4TrajectoryHeader(const std::string& preview)
{
    const auto text = toLower(preview);

    const bool has_position =
        text.find("x_cm") != std::string::npos &&
        text.find("y_cm") != std::string::npos &&
        text.find("z_cm") != std::string::npos;

    const bool has_step_quantities =
        text.find("time_ns") != std::string::npos &&
        text.find("trackid") != std::string::npos &&
        text.find("eventid") != std::string::npos;

    const bool has_geant4_like_payload =
        text.find("edep_mev") != std::string::npos ||
        text.find("kine_mev") != std::string::npos ||
        text.find("momx_mev") != std::string::npos;

    return has_position && has_step_quantities && has_geant4_like_payload;
}

FormatSignature guessFormat(const std::string& extension, const std::string& preview)
{
    const auto ext = toLower(extension);

    if (ext == ".csv" || ext == ".tsv" || ext == ".txt") {
        if (containsGeant4TrajectoryHeader(preview)) {
            return FormatSignature::Geant4Csv;
        }

        if (containsComsolTrajectoryHeader(preview)) {
            return FormatSignature::ComsolCsv;
        }

        return FormatSignature::TextTable;
    }

    if (ext == ".root") {
        return FormatSignature::RootFile;
    }

    if (ext == ".mph") {
        return FormatSignature::MphFile;
    }

    if (ext == ".stl" || ext == ".obj" || ext == ".ply") {
        return FormatSignature::MeshFile;
    }

    if (ext == ".beamproj") {
        return FormatSignature::ProjectFile;
    }

    return FormatSignature::Unknown;
}

} // namespace

ProbeResult FileProbe::probe(const std::string& file_path) const
{
    ProbeResult result{};
    result.file_path = file_path;

    const std::filesystem::path path(file_path);
    result.extension = path.extension().string();

    std::ifstream input(file_path);
    if (!input) {
        result.readable = false;
        result.detected_format = FormatSignature::Unknown;
        return result;
    }

    result.readable = true;

    std::string line{};
    std::string combined_preview{};

    for (int i = 0; i < 80 && std::getline(input, line); ++i) {
        if (i == 0) {
            result.first_line_preview = line;
        }

        result.preview_lines.push_back(line);
        combined_preview += line;
        combined_preview += '\n';
    }

    result.detected_format = guessFormat(result.extension, combined_preview);
    return result;
}

} // namespace beamlab::io
