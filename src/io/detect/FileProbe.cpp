#include "io/detect/FileProbe.h"

#include "io/parsing/Geant4HeaderRecognizer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace beamlab::io {
namespace {

std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Run the strict tokenized recognizers on each line of the preview separately.
// The legacy implementation concatenated all preview lines into one big string
// and ran substring matches on it; that produced false positives whenever the
// individual column names happened to appear anywhere in any line (data, units
// row, comments, etc.).  Header recognition is meaningful per-line.
bool anyLineMatches(const std::vector<std::string>& lines,
                    bool (*predicate)(std::string_view))
{
    for (const auto& line : lines) {
        if (predicate(line)) return true;
    }
    return false;
}

FormatSignature guessFormat(const std::string& extension,
                            const std::vector<std::string>& preview_lines)
{
    const auto ext = toLower(extension);

    if (ext == ".csv" || ext == ".tsv" || ext == ".txt") {
        if (anyLineMatches(preview_lines, &Geant4HeaderRecognizer::looksLikeHeader)) {
            return FormatSignature::Geant4Csv;
        }

        if (anyLineMatches(preview_lines, &ComsolHeaderRecognizer::looksLikeHeader)) {
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

    for (int i = 0; i < 80 && std::getline(input, line); ++i) {
        if (i == 0) {
            result.first_line_preview = line;
        }

        result.preview_lines.push_back(line);
    }

    result.detected_format = guessFormat(result.extension, result.preview_lines);
    return result;
}

} // namespace beamlab::io
