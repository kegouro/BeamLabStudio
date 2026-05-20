#include "analysis/envelope/FullEnvelopePreviewBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace beamlab::analysis {

namespace {

struct Sample {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct Bin {
    int count{0};
    double sum_longitudinal{0.0};
    double sum_a{0.0};
    double sum_b{0.0};
    double center_longitudinal{0.0};
    double center_a{0.0};
    double center_b{0.0};
    double max_radius{0.0};
    double fallback_radius{0.0};
    std::vector<std::vector<double>> angular_radii{};
    std::vector<double> angular_radius{};
};

std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> result;
    std::string token;
    bool in_quotes = false;

    for (const auto ch : line) {
        if (ch == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (ch == ',' && !in_quotes) {
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            result.push_back(token);
            token.clear();
            continue;
        }
        token.push_back(ch);
    }
    token.erase(0, token.find_first_not_of(" \t\r\n"));
    token.erase(token.find_last_not_of(" \t\r\n") + 1);
    result.push_back(token);
    return result;
}

int columnIndex(const std::vector<std::string>& header, const std::string& name)
{
    for (int i = 0; i < static_cast<int>(header.size()); ++i) {
        if (header[static_cast<std::size_t>(i)] == name) {
            return i;
        }
    }
    return -1;
}

double quantileRadius(std::vector<double>& values, const double q)
{
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t index = std::min<std::size_t>(
        values.size() - 1,
        static_cast<std::size_t>(std::floor(q * static_cast<double>(values.size() - 1)))
    );
    return values[index];
}

} // namespace

bool FullEnvelopePreviewBuilder::buildPreview(const std::string& csv_path,
                                               const std::string& output_obj,
                                               std::string* error)
{
    std::ifstream file(csv_path);
    if (!file) {
        if (error != nullptr) {
            *error = "Could not open trajectories CSV:\n" + csv_path;
        }
        return false;
    }

    std::vector<Sample> samples;
    samples.reserve(60000);

    std::vector<std::string> header;
    int x_col = -1;
    int y_col = -1;
    int z_col = -1;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (header.empty()) {
            header = splitCsvLine(line);
            x_col = columnIndex(header, "x_m");
            y_col = columnIndex(header, "y_m");
            z_col = columnIndex(header, "z_m");
            continue;
        }
        const auto cols = splitCsvLine(line);
        if (x_col < 0 || y_col < 0 || z_col < 0 ||
            x_col >= static_cast<int>(cols.size()) ||
            y_col >= static_cast<int>(cols.size()) ||
            z_col >= static_cast<int>(cols.size())) {
            continue;
        }

        char* end_x = nullptr;
        char* end_y = nullptr;
        char* end_z = nullptr;
        const double x = std::strtod(cols[static_cast<std::size_t>(x_col)].c_str(), &end_x);
        const double y = std::strtod(cols[static_cast<std::size_t>(y_col)].c_str(), &end_y);
        const double z = std::strtod(cols[static_cast<std::size_t>(z_col)].c_str(), &end_z);

        if (end_x != cols[static_cast<std::size_t>(x_col)].c_str() &&
            end_y != cols[static_cast<std::size_t>(y_col)].c_str() &&
            end_z != cols[static_cast<std::size_t>(z_col)].c_str() &&
            std::isfinite(x) && std::isfinite(y) && std::isfinite(z)) {
            samples.push_back({x, y, z});
        }
    }

    if (samples.size() < 16) {
        if (error != nullptr) {
            *error = "Not enough trajectory samples to build a full envelope preview.";
        }
        return false;
    }

    std::array<double, 3> min_coord{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    };
    std::array<double, 3> max_coord{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    const auto coord = [](const Sample& sample, const int axis) {
        if (axis == 0) return sample.x;
        if (axis == 1) return sample.y;
        return sample.z;
    };

    for (const auto& sample : samples) {
        min_coord[0] = std::min(min_coord[0], sample.x);
        min_coord[1] = std::min(min_coord[1], sample.y);
        min_coord[2] = std::min(min_coord[2], sample.z);
        max_coord[0] = std::max(max_coord[0], sample.x);
        max_coord[1] = std::max(max_coord[1], sample.y);
        max_coord[2] = std::max(max_coord[2], sample.z);
    }

    const std::array<double, 3> span{
        max_coord[0] - min_coord[0],
        max_coord[1] - min_coord[1],
        max_coord[2] - min_coord[2]
    };

    int longitudinal_axis = 0;
    if (span[1] > span[static_cast<std::size_t>(longitudinal_axis)]) {
        longitudinal_axis = 1;
    }
    if (span[2] > span[static_cast<std::size_t>(longitudinal_axis)]) {
        longitudinal_axis = 2;
    }

    std::array<int, 2> transverse_axes{0, 1};
    int transverse_slot = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (axis != longitudinal_axis) {
            transverse_axes[static_cast<std::size_t>(transverse_slot)] = axis;
            ++transverse_slot;
        }
    }

    const double longitudinal_min = min_coord[static_cast<std::size_t>(longitudinal_axis)];
    const double longitudinal_span = span[static_cast<std::size_t>(longitudinal_axis)];

    if (longitudinal_span <= 1.0e-15) {
        if (error != nullptr) {
            *error = "Trajectory preview has no usable longitudinal range for a full envelope preview.";
        }
        return false;
    }

    constexpr int angle_count = 128;
    constexpr double pi = 3.141592653589793238462643383279502884;
    const int bin_count = std::clamp(
        static_cast<int>(samples.size() / 500),
        24,
        72
    );

    std::vector<Bin> bins(static_cast<std::size_t>(bin_count));
    for (auto& bin : bins) {
        bin.angular_radii.resize(static_cast<std::size_t>(angle_count));
        bin.angular_radius.assign(static_cast<std::size_t>(angle_count), 0.0);
    }

    const auto binIndexForLongitudinal = [&](const double longitudinal) {
        const double t =
            std::clamp((longitudinal - longitudinal_min) / longitudinal_span, 0.0, 1.0);
        return std::clamp(
            static_cast<int>(std::floor(t * static_cast<double>(bin_count - 1))),
            0,
            bin_count - 1
        );
    };

    for (const auto& sample : samples) {
        const double longitudinal = coord(sample, longitudinal_axis);
        const double a = coord(sample, transverse_axes[0]);
        const double b = coord(sample, transverse_axes[1]);
        auto& bin = bins[static_cast<std::size_t>(binIndexForLongitudinal(longitudinal))];
        ++bin.count;
        bin.sum_longitudinal += longitudinal;
        bin.sum_a += a;
        bin.sum_b += b;
    }

    for (auto& bin : bins) {
        if (bin.count <= 0) continue;
        const double inv_count = 1.0 / static_cast<double>(bin.count);
        bin.center_longitudinal = bin.sum_longitudinal * inv_count;
        bin.center_a = bin.sum_a * inv_count;
        bin.center_b = bin.sum_b * inv_count;
    }

    for (const auto& sample : samples) {
        const double longitudinal = coord(sample, longitudinal_axis);
        const double a = coord(sample, transverse_axes[0]);
        const double b = coord(sample, transverse_axes[1]);
        auto& bin = bins[static_cast<std::size_t>(binIndexForLongitudinal(longitudinal))];
        if (bin.count <= 0) continue;

        const double da = a - bin.center_a;
        const double db = b - bin.center_b;
        const double radius = std::hypot(da, db);
        double angle = std::atan2(db, da);
        if (angle < 0.0) angle += 2.0 * pi;

        const int angle_index = std::clamp(
            static_cast<int>(std::floor(angle * static_cast<double>(angle_count) / (2.0 * pi))),
            0,
            angle_count - 1
        );
        bin.angular_radii[static_cast<std::size_t>(angle_index)].push_back(radius);
        bin.max_radius = std::max(bin.max_radius, radius);
    }

    for (auto& bin : bins) {
        if (bin.count <= 0) continue;

        std::vector<double> all_radii;
        for (const auto& sector : bin.angular_radii) {
            all_radii.insert(all_radii.end(), sector.begin(), sector.end());
        }
        bin.fallback_radius = quantileRadius(all_radii, 0.90);
        if (bin.fallback_radius <= 0.0) {
            bin.fallback_radius = bin.max_radius;
        }

        for (int angle_index = 0; angle_index < angle_count; ++angle_index) {
            auto& sector = bin.angular_radii[static_cast<std::size_t>(angle_index)];
            double radius = quantileRadius(sector, 0.90);
            if (radius <= 0.0) radius = bin.fallback_radius;
            bin.angular_radius[static_cast<std::size_t>(angle_index)] = radius;
        }

        for (int smooth_pass = 0; smooth_pass < 2; ++smooth_pass) {
            std::vector<double> smoothed = bin.angular_radius;
            for (int angle_index = 0; angle_index < angle_count; ++angle_index) {
                const int previous = (angle_index + angle_count - 1) % angle_count;
                const int next = (angle_index + 1) % angle_count;
                smoothed[static_cast<std::size_t>(angle_index)] =
                    0.25 * bin.angular_radius[static_cast<std::size_t>(previous)] +
                    0.50 * bin.angular_radius[static_cast<std::size_t>(angle_index)] +
                    0.25 * bin.angular_radius[static_cast<std::size_t>(next)];
            }
            bin.angular_radius = std::move(smoothed);
        }
    }

    std::vector<int> valid_bins;
    for (int i = 0; i < bin_count; ++i) {
        const auto& bin = bins[static_cast<std::size_t>(i)];
        if (bin.count >= 6 && bin.fallback_radius > 0.0) {
            valid_bins.push_back(i);
        }
    }

    if (valid_bins.size() < 2) {
        if (error != nullptr) {
            *error = "The trajectory preview did not produce enough valid envelope slices.";
        }
        return false;
    }

    {
        std::ofstream output(output_obj);
        if (!output) {
            if (error != nullptr) {
                *error = "Could not write OBJ:\n" + output_obj;
            }
            return false;
        }

        output << "# BeamLabStudio full beam envelope preview\n";
        output << "# Generated from trajectories_preview.csv; use as a visual proxy only.\n";
        output << "# longitudinal_axis: " << (longitudinal_axis == 0 ? "x" : longitudinal_axis == 1 ? "y" : "z") << "\n";
        output << "# vertices: x_m y_m z_m\n";

        const auto makeWorldPoint = [&](const double longitudinal,
                                        const double a,
                                        const double b) {
            std::array<double, 3> values{0.0, 0.0, 0.0};
            values[static_cast<std::size_t>(longitudinal_axis)] = longitudinal;
            values[static_cast<std::size_t>(transverse_axes[0])] = a;
            values[static_cast<std::size_t>(transverse_axes[1])] = b;
            return Sample{values[0], values[1], values[2]};
        };

        for (const int bin_index : valid_bins) {
            const Bin& bin = bins[static_cast<std::size_t>(bin_index)];
            for (int angle_index = 0; angle_index < angle_count; ++angle_index) {
                const double angle = 2.0 * pi * static_cast<double>(angle_index)
                                   / static_cast<double>(angle_count);
                double radius = bin.angular_radius[static_cast<std::size_t>(angle_index)];
                if (radius <= 0.0) radius = bin.fallback_radius;

                const Sample world = makeWorldPoint(
                    bin.center_longitudinal,
                    bin.center_a + radius * std::cos(angle),
                    bin.center_b + radius * std::sin(angle)
                );
                output << "v " << world.x << ' ' << world.y << ' ' << world.z << '\n';
            }
        }

        for (std::size_t ring = 1; ring < valid_bins.size(); ++ring) {
            const int previous_base =
                static_cast<int>((ring - 1) * static_cast<std::size_t>(angle_count)) + 1;
            const int current_base =
                static_cast<int>(ring * static_cast<std::size_t>(angle_count)) + 1;

            for (int angle_index = 0; angle_index < angle_count; ++angle_index) {
                const int next = (angle_index + 1) % angle_count;
                output << "f "
                       << previous_base + angle_index << ' '
                       << previous_base + next << ' '
                       << current_base + next << ' '
                       << current_base + angle_index << '\n';
            }
        }
    }

    return true;
}

} // namespace beamlab::analysis
