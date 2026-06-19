#include "services/import/Geant4CsvImporter.h"

#include "data/model/TrajectorySample.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace beamlab::services::import {

// ── Sanitise line ────────────────────────────────────────────────

void Geant4CsvImporter::sanitiseLine(std::string& line)
{
    // Strip BOM (UTF-8: 0xEF 0xBB 0xBF).
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);
    }
    // Strip trailing \r (Windows CRLF).
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

// ── Detect delimiter ─────────────────────────────────────────────

char Geant4CsvImporter::detectDelimiter(const std::string& line)
{
    auto commas = std::count(line.begin(), line.end(), ',');
    auto semis  = std::count(line.begin(), line.end(), ';');
    auto tabs   = std::count(line.begin(), line.end(), '\t');

    if (tabs > commas && tabs > semis) return '\t';
    if (semis > commas)               return ';';
    return ',';
}

// ── Split line ───────────────────────────────────────────────────

std::vector<std::string> Geant4CsvImporter::splitLine(
    const std::string& line, char delim)
{
    std::vector<std::string> tokens;
    std::string token;
    bool inQuote = false;

    for (char c : line) {
        if (c == '"') { inQuote = !inQuote; continue; }
        if (c == delim && !inQuote) {
            // Trim whitespace from token.
            auto s = token.find_first_not_of(" \t\r");
            if (s != std::string::npos) {
                auto e = token.find_last_not_of(" \t\r");
                tokens.push_back(token.substr(s, e - s + 1));
            } else {
                tokens.push_back("");
            }
            token.clear();
        } else {
            token.push_back(c);
        }
    }
    // Last token.
    auto s = token.find_first_not_of(" \t\r");
    if (s != std::string::npos) {
        auto e = token.find_last_not_of(" \t\r");
        tokens.push_back(token.substr(s, e - s + 1));
    } else {
        tokens.push_back("");
    }
    return tokens;
}

// ── Token at index ───────────────────────────────────────────────

const std::string& Geant4CsvImporter::tokenAt(
    const std::vector<std::string>& tokens, int col)
{
    static const std::string kEmpty;
    if (col < 0 || static_cast<std::size_t>(col) >= tokens.size())
        return kEmpty;
    return tokens[static_cast<std::size_t>(col)];
}

// ── Probe ────────────────────────────────────────────────────────

ImporterCapabilityScore Geant4CsvImporter::probe(const std::string& filePath)
{
    ImporterCapabilityScore result;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[Geant4CsvImporter] Cannot open: " << filePath << "\n";
        return result;
    }

    std::string line;
    if (!std::getline(file, line)) return result;

    sanitiseLine(line);
    if (line.empty()) return result;

    // Detect delimiter.
    char delim = detectDelimiter(line);
    auto tokens = splitLine(line, delim);

    if (tokens.empty()) return result;

    // Check if this looks like a header (has letters, underscores, brackets).
    bool isHeader = false;
    for (const auto& t : tokens) {
        for (char c : t) {
            if (std::isalpha(static_cast<unsigned char>(c))
                || c == '_' || c == '(' || c == ')' || c == '[' || c == ']') {
                isHeader = true;
                break;
            }
        }
        if (isHeader) break;
    }

    if (!isHeader) {
        // If the first line is all numbers, it's data — assume 3-column x,y,z,edep.
        if (tokens.size() >= 3) {
            result.value = 0.30;
            result.matchedHeader = "generic-numeric-" + std::to_string(tokens.size()) + "cols";
        }
        return result;
    }

    // Header line — score based on recognised Geant4 columns.
    double score = 0.0;
    int hits = 0;
    for (const auto& t : tokens) {
        std::string lower;
        for (char c : t) lower += static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));

        if (lower.find("x") == 0 || lower == "x") { score += 0.18; ++hits; continue; }
        if (lower.find("y") == 0 || lower == "y") { score += 0.18; ++hits; continue; }
        if (lower.find("z") == 0 || lower == "z") { score += 0.18; ++hits; continue; }
        if (lower.find("edep") != std::string::npos
            || lower.find("energy") != std::string::npos
            || lower.find("de") == 0) { score += 0.18; ++hits; continue; }
        if (lower.find("track") != std::string::npos
            || lower.find("trk") != std::string::npos
            || lower.find("id") == 0) { score += 0.08; ++hits; continue; }
        if (lower.find("mom") != std::string::npos
            || lower.find("kin") != std::string::npos
            || lower.find("time") != std::string::npos) { score += 0.06; ++hits; continue; }
    }

    result.value = std::min(score, 1.0);

    if (hits >= 1) {
        std::stringstream ss;
        ss << "geant4-header," << hits << "hits";
        result.matchedHeader = ss.str();
    }

    std::cerr << "[Geant4CsvImporter] probe(" << filePath << "): "
              << "score=" << result.value
              << " header=" << isHeader
              << " cols=" << tokens.size()
              << " delim='" << delim << "'\n";

    return result;
}

// ── Import ───────────────────────────────────────────────────────

void Geant4CsvImporter::import(
    const std::string& filePath,
    storage::IStorageBackend& storage,
    ImportProgressCallback onProgress)
{
    auto fileSize = fs::file_size(filePath);
    std::cerr << "[Geant4CsvImporter] Opening: " << filePath
              << " (" << (fileSize / (1024.0 * 1024.0)) << " MB)\n";

    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }

    // ── Read & parse header ──────────────────────────────────────
    std::string headerLine;
    if (!std::getline(file, headerLine)) {
        throw std::runtime_error("Empty file: " + filePath);
    }
    sanitiseLine(headerLine);

    char delim = detectDelimiter(headerLine);
    std::cerr << "[Geant4CsvImporter] Detected delimiter: '" << delim << "'\n";

    auto headerTokens = splitLine(headerLine, delim);
    auto mapping = HeaderAnalyzer::analyse(headerLine);

    std::cerr << "[Geant4CsvImporter] Columns: " << headerTokens.size()
              << " (recognised: " << mapping.recognisedCount << ")\n";
    for (std::size_t i = 0; i < headerTokens.size() && i < 20; ++i) {
        std::cerr << "  [" << i << "] " << headerTokens[i] << "\n";
    }

    bool hasHeader = mapping.recognisedCount >= 3;

    // If no header was recognised, treat the first line as data.
    if (!hasHeader) {
        std::cerr << "[Geant4CsvImporter] No header recognised — treating as raw CSV.\n";
        // Rewind and parse without header.
        file.seekg(0);
    }

    // ── Begin write batch ────────────────────────────────────────
    storage.beginWriteBatch();
    struct BatchGuard {
        storage::IStorageBackend& b;
        bool committed = false;
        ~BatchGuard() { if (!committed) try { b.endWriteBatch(); } catch (...) {} }
    };
    BatchGuard guard{storage};

    // ── Parse rows ───────────────────────────────────────────────
    // Build fast index lookup for the mapping.
    int idxX     = mapping.index(HeaderAnalyzer::Column::X_cm);
    int idxY     = mapping.index(HeaderAnalyzer::Column::Y_cm);
    int idxZ     = mapping.index(HeaderAnalyzer::Column::Z_cm);
    int idxEdep  = mapping.index(HeaderAnalyzer::Column::Edep_MeV);
    int idxKinE  = mapping.index(HeaderAnalyzer::Column::KinE_MeV);
    int idxTrack = mapping.index(HeaderAnalyzer::Column::TrackID);
    int idxEvent = mapping.index(HeaderAnalyzer::Column::EventID);

    // If no mapping, assume first 3 columns = x, y, z, and try to find edep.
    if (!mapping.isValid()) {
        std::cerr << "[Geant4CsvImporter] No valid column mapping — using"
                  << " columns 0=x, 1=y, 2=z, 3=edep as fallback.\n";
        idxX = 0; idxY = 1; idxZ = 2; idxEdep = 3;
    }

    // Packing must match io/importers/Geant4CsvImporter:
    //   unique_id = event_id * kMaxTracksPerEvent + track_id + 1
    static constexpr uint64_t kMaxTracksPerEvent = 10'000'000ULL;

    std::string line;
    uint64_t sampleCount = 0;
    uint64_t skippedCount = 0;
    static constexpr uint64_t kNotifyInterval = 100000;

    while (std::getline(file, line)) {
        sanitiseLine(line);
        if (line.empty()) continue;

        auto tokens = splitLine(line, delim);
        if (tokens.size() < 3u) { ++skippedCount; continue; }

        try {
            beamlab::data::TrajectorySample sample;

            // x, y, z (required) — Geant4 exports cm; model uses m.
            sample.position_m.x = std::stod(tokenAt(tokens, idxX)) / 100.0;
            sample.position_m.y = std::stod(tokenAt(tokens, idxY)) / 100.0;
            sample.position_m.z = std::stod(tokenAt(tokens, idxZ)) / 100.0;

            // edep (fill 0 if column absent).
            auto& edepStr = tokenAt(tokens, idxEdep);
            sample.edep_MeV = edepStr.empty() ? 0.0 : std::stod(edepStr);

            // kinE (optional).
            auto& kineStr = tokenAt(tokens, idxKinE);
            sample.kinE_MeV = kineStr.empty() ? 0.0 : std::stod(kineStr);

            // Trajectory ID — pack (eventID, trackID) into a single uint64.
            // Same formula as io/importers/Geant4CsvImporter so both pipelines
            // produce identical IDs for the same Geant4 data.
            {
                auto& evStr = tokenAt(tokens, idxEvent);
                auto& tkStr = tokenAt(tokens, idxTrack);
                const uint64_t ev = (evStr.empty() ? 0ULL
                                                    : static_cast<uint64_t>(std::stoull(evStr)));
                const uint64_t tk = (tkStr.empty() ? 0ULL
                                                    : static_cast<uint64_t>(std::stoull(tkStr)));
                sample.trajectory_id = beamlab::data::TrajectoryId(
                    ev * kMaxTracksPerEvent + tk + 1ULL);
            }

            storage.writeSample(sample);
            ++sampleCount;

        } catch (const std::exception& e) {
            ++skippedCount;
            continue;
        }

        if ((sampleCount % kNotifyInterval) == 0 && onProgress) {
            auto pos = file.tellg();
            onProgress(static_cast<uint64_t>(pos),
                       static_cast<uint64_t>(fileSize));
        }
    }

    guard.committed = true;
    storage.endWriteBatch();

    if (onProgress) {
        onProgress(fileSize, fileSize);
    }

    std::cerr << "[Geant4CsvImporter] Done: " << sampleCount << " samples"
              << (skippedCount > 0 ? ", " + std::to_string(skippedCount) + " skipped" : "")
              << "\n";
}

} // namespace beamlab::services::import
