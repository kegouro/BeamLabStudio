#include "io/parsing/DelimiterDetector.h"

namespace beamlab::io {

char DelimiterDetector::detect(const std::string& line)
{
    std::size_t commas = 0;
    std::size_t semicolons = 0;
    std::size_t tabs = 0;

    for (const char c : line) {
        if (c == ',') {
            ++commas;
        } else if (c == ';') {
            ++semicolons;
        } else if (c == '\t') {
            ++tabs;
        }
    }

    if (tabs >= commas && tabs >= semicolons) {
        return '\t';
    }
    if (semicolons >= commas) {
        return ';';
    }
    return ',';
}

} // namespace beamlab::io
