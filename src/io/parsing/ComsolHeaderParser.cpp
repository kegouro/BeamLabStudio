#include "io/parsing/ComsolHeaderParser.h"
#include "io/parsing/TableHeaderAnalyzer.h"

namespace beamlab::io {

bool ComsolHeaderParser::looksLikeComsolHeader(const std::string& line)
{
    return line.find("qx") != std::string::npos ||
           line.find("qy") != std::string::npos ||
           line.find("qz") != std::string::npos;
}

std::vector<std::string> ComsolHeaderParser::extractTokens(const std::string& line, char delimiter)
{
    return TableHeaderAnalyzer::splitHeader(line, delimiter);
}

} // namespace beamlab::io
