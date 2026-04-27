#include "io/parsing/ColumnTypeInferer.h"

#include <charconv>

namespace beamlab::io {

ColumnType ColumnTypeInferer::infer(const std::string& sample)
{
    if (sample.empty()) {
        return ColumnType::Unknown;
    }

    int int_value = 0;
    const auto* begin = sample.data();
    const auto* end = sample.data() + sample.size();

    if (const auto [ptr, ec] = std::from_chars(begin, end, int_value);
        ec == std::errc{} && ptr == end) {
        return ColumnType::Integer;
    }

    try {
        std::size_t processed = 0;
        (void)std::stod(sample, &processed);
        if (processed == sample.size()) {
            return ColumnType::Floating;
        }
    } catch (...) {
    }

    return ColumnType::Text;
}

} // namespace beamlab::io
