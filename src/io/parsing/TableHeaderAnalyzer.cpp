#include "io/parsing/TableHeaderAnalyzer.h"

#include <sstream>

namespace beamlab::io {

std::vector<std::string> TableHeaderAnalyzer::splitHeader(const std::string& line, char delimiter)
{
    std::vector<std::string> tokens{};
    std::stringstream stream(line);
    std::string token{};

    while (std::getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

} // namespace beamlab::io
