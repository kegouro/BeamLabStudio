#include "io/parsing/Geant4HeaderRecognizer.h"

#include "io/parsing/DelimiterDetector.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

namespace beamlab::io {

namespace {

std::string trim(std::string s)
{
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string lower(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::vector<std::string> tokenize(std::string_view line)
{
    const std::string buffer(line);
    const char delim = DelimiterDetector::detect(buffer);

    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    for (const char c : buffer) {
        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (c == delim && !in_quotes) {
            tokens.push_back(lower(trim(current)));
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    tokens.push_back(lower(trim(current)));
    return tokens;
}

bool containsAll(const std::unordered_set<std::string>& tokens,
                 std::initializer_list<const char*> needed)
{
    for (const char* name : needed) {
        if (tokens.find(name) == tokens.end()) {
            return false;
        }
    }
    return true;
}

bool containsAny(const std::unordered_set<std::string>& tokens,
                 std::initializer_list<const char*> options)
{
    for (const char* name : options) {
        if (tokens.find(name) != tokens.end()) {
            return true;
        }
    }
    return false;
}

} // namespace

bool Geant4HeaderRecognizer::looksLikeHeader(std::string_view line)
{
    const auto tokens_vec = tokenize(line);
    if (tokens_vec.size() < 6) return false;

    const std::unordered_set<std::string> tokens(tokens_vec.begin(), tokens_vec.end());

    const bool has_position =
        containsAll(tokens, {"x_cm", "y_cm", "z_cm"});
    const bool has_step_quantities =
        containsAll(tokens, {"time_ns", "trackid", "eventid"});
    const bool has_physics_payload =
        containsAny(tokens, {"edep_mev", "kine_mev", "momx_mev"});

    return has_position && has_step_quantities && has_physics_payload;
}

bool ComsolHeaderRecognizer::looksLikeHeader(std::string_view line)
{
    const auto tokens_vec = tokenize(line);
    if (tokens_vec.size() < 4) return false;

    const std::unordered_set<std::string> tokens(tokens_vec.begin(), tokens_vec.end());

    const bool has_position =
        containsAll(tokens, {"qx", "qy", "qz"}) ||
        containsAll(tokens, {"x", "y", "z"});
    const bool has_particle_id =
        containsAny(tokens, {"pidx", "particle", "particle_id", "particleindex"});
    const bool has_time =
        containsAny(tokens, {"t", "time", "t_s", "time_s"});

    return has_position && has_particle_id && has_time;
}

} // namespace beamlab::io
