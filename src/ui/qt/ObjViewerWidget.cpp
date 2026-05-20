#include "ui/qt/ObjViewerWidget.h"

#include <QColor>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QSizeF>
#include <QTextStream>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <tuple>
#include <unordered_map>

namespace beamlab::ui {
namespace {

QStringList splitObjFaceToken(const QString& token)
{
    return token.split('/', Qt::KeepEmptyParts);
}

QColor panelBackground()
{
    return QColor(13, 16, 22);
}

QColor panelGrid()
{
    return QColor(54, 61, 76);
}

QColor panelText()
{
    return QColor(225, 231, 242);
}

QColor panelAccent()
{
    return QColor(118, 170, 255);
}

QString formatDistance(const double meters)
{
    const double value = std::abs(meters);

    if (value >= 1.0) {
        return QString("%1 m").arg(meters, 0, 'g', 4);
    }

    if (value >= 1.0e-2) {
        return QString("%1 cm").arg(meters * 100.0, 0, 'g', 4);
    }

    return QString("%1 mm").arg(meters * 1000.0, 0, 'g', 4);
}

double overlapArea(const QRectF& a, const QRectF& b)
{
    const QRectF intersection = a.intersected(b);

    if (intersection.isEmpty()) {
        return 0.0;
    }

    return intersection.width() * intersection.height();
}

} // namespace

ObjViewerWidget::ObjViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(800, 560);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

bool ObjViewerWidget::loadObj(const QString& path)
{
    vertices_.clear();
    faces_.clear();
    polylines_.clear();
    loaded_path_ = path;

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        status_text_ = "Could not open OBJ: " + path;
        update();
        return false;
    }

    QTextStream stream(&file);

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();

        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);

        if (parts.empty()) {
            continue;
        }

        if (parts[0] == "v" && parts.size() >= 4) {
            bool ok_x = false;
            bool ok_y = false;
            bool ok_z = false;

            const double x = parts[1].toDouble(&ok_x);
            const double y = parts[2].toDouble(&ok_y);
            const double z = parts[3].toDouble(&ok_z);

            if (ok_x && ok_y && ok_z) {
                vertices_.push_back({x, y, z});
            }
        } else if (parts[0] == "f" && parts.size() >= 4) {
            Face face{};

            for (int i = 1; i < parts.size(); ++i) {
                const QStringList face_parts = splitObjFaceToken(parts[i]);
                bool ok = false;
                const int index = face_parts[0].toInt(&ok);

                if (ok) {
                    face.indices.push_back(index - 1);
                }
            }

            if (face.indices.size() >= 3) {
                faces_.push_back(std::move(face));
            }
        } else if (parts[0] == "l" && parts.size() >= 3) {
            Polyline polyline{};

            for (int i = 1; i < parts.size(); ++i) {
                const QStringList line_parts = splitObjFaceToken(parts[i]);
                bool ok = false;
                const int index = line_parts[0].toInt(&ok);

                if (ok) {
                    polyline.indices.push_back(index - 1);
                }
            }

            if (polyline.indices.size() >= 2) {
                polylines_.push_back(std::move(polyline));
            }
        }
    }

    updateBounds();
    resetCamera();
    vertex_energies_.clear();  // invalidate stale energy data on new OBJ load

    status_text_ =
        QString("Loaded OBJ | vertices: %1 | faces: %2 | polylines: %3")
            .arg(vertices_.size())
            .arg(faces_.size())
            .arg(polylines_.size());

    update();
    return !vertices_.empty();
}

void ObjViewerWidget::loadEnergyCSV(const QString& path)
{
    vertex_energies_.clear();
    if (vertices_.empty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream stream(&file);

    // --- Pass 1: build coordinate → kinE_MeV map from CSV ---
    // CSV columns: trajectory_index(0), sample_index(1), time_s(2),
    //              x_m(3), y_m(4), z_m(5), edep_MeV(6), edep_eV(7), kinE_MeV(8)
    // Both OBJ and CSV use the same source values with the same precision, so
    // parsed doubles compare exactly. Trajectory skip mismatches (<2 samples)
    // are rare; coordinate lookup handles them correctly in all cases.
    struct CoordHash {
        std::size_t operator()(const std::tuple<double,double,double>& k) const {
            // Bit-cast each double to uint64 and mix.
            auto h = [](double v) -> std::size_t {
                std::uint64_t bits{};
                static_assert(sizeof(bits) == sizeof(v));
                std::memcpy(&bits, &v, sizeof(v));
                return std::hash<std::uint64_t>{}(bits);
            };
            auto seed = h(std::get<0>(k));
            seed ^= h(std::get<1>(k)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h(std::get<2>(k)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
    std::unordered_map<std::tuple<double,double,double>, double, CoordHash> coord_map;
    coord_map.reserve(vertices_.size() * 2);

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') ||
            line.startsWith("trajectory")) {
            continue;
        }
        const QStringList parts = line.split(',');
        if (parts.size() < 9) continue;

        bool ok_x{}, ok_y{}, ok_z{}, ok_e{};
        const double x = parts[3].toDouble(&ok_x);
        const double y = parts[4].toDouble(&ok_y);
        const double z = parts[5].toDouble(&ok_z);
        const double e = parts[8].toDouble(&ok_e);
        if (!ok_x || !ok_y || !ok_z || !ok_e) continue;

        coord_map.emplace(std::make_tuple(x, y, z), e);
    }

    if (coord_map.empty()) return;

    // --- Pass 2: assign energy to each OBJ vertex via coordinate lookup ---
    double e_min = std::numeric_limits<double>::infinity();
    double e_max = -std::numeric_limits<double>::infinity();
    int matched = 0;

    vertex_energies_.resize(vertices_.size(), 0.0);
    std::vector<double> matched_energies;
    matched_energies.reserve(vertices_.size());
    for (std::size_t i = 0; i < vertices_.size(); ++i) {
        const auto& v = vertices_[i];
        auto it = coord_map.find(std::make_tuple(v.x, v.y, v.z));
        if (it != coord_map.end()) {
            vertex_energies_[i] = it->second;
            e_min = std::min(e_min, it->second);
            e_max = std::max(e_max, it->second);
            matched_energies.push_back(it->second);
            ++matched;
        }
    }

    if (matched == 0) {
        vertex_energies_.clear();
        return;
    }

    // Use P1–P99 percentiles for the colour scale so a few outlier samples
    // (e.g. a particle that fully stops) do not flatten the visible gradient.
    double p_lo = e_min;
    double p_hi = e_max;
    if (matched_energies.size() >= 100) {
        const std::size_t lo_idx = static_cast<std::size_t>(0.01 * static_cast<double>(matched_energies.size()));
        const std::size_t hi_idx = std::min(matched_energies.size() - 1,
                                            static_cast<std::size_t>(0.99 * static_cast<double>(matched_energies.size())));
        std::nth_element(matched_energies.begin(),
                         matched_energies.begin() + static_cast<std::ptrdiff_t>(lo_idx),
                         matched_energies.end());
        p_lo = matched_energies[lo_idx];
        std::nth_element(matched_energies.begin() + static_cast<std::ptrdiff_t>(lo_idx) + 1,
                         matched_energies.begin() + static_cast<std::ptrdiff_t>(hi_idx),
                         matched_energies.end());
        p_hi = matched_energies[hi_idx];
    }
    if (!(p_hi > p_lo)) {
        p_hi = p_lo + 0.5;
        p_lo -= 0.5;
    }

    energy_min_ = p_lo;
    energy_max_ = p_hi;

    // Update status to reflect energy range.
    status_text_ = QString("OBJ loaded | Energy P1–P99: [%1, %2] MeV | full [%3, %4] | %5/%6 vertices matched")
        .arg(p_lo, 0, 'f', 2)
        .arg(p_hi, 0, 'f', 2)
        .arg(e_min, 0, 'g', 4)
        .arg(e_max, 0, 'g', 4)
        .arg(matched)
        .arg(static_cast<int>(vertices_.size()));

    update();
}

void ObjViewerWidget::setEnergyGradientEnabled(const bool enabled)
{
    energy_gradient_ = enabled;
    update();
}

bool ObjViewerWidget::energyGradientEnabled() const
{
    return energy_gradient_;
}

// static
QColor ObjViewerWidget::energyToColor(const double t)
{
    // Temperature gradient: blue(cold) → cyan → green → yellow → red(hot)
    struct Stop { double pos; int r, g, b; };
    constexpr std::array<Stop, 5> stops{{
        {0.00,   0,   0, 230},
        {0.25,   0, 200, 255},
        {0.50,   0, 230,  80},
        {0.75, 255, 210,   0},
        {1.00, 255,  40,   0}
    }};
    const double tc = std::clamp(t, 0.0, 1.0);
    int seg = 0;
    for (; seg < 3; ++seg) {
        if (tc <= stops[static_cast<std::size_t>(seg + 1)].pos) break;
    }
    const auto& s0 = stops[static_cast<std::size_t>(seg)];
    const auto& s1 = stops[static_cast<std::size_t>(seg + 1)];
    const double f = (tc - s0.pos) / (s1.pos - s0.pos);
    return QColor(
        static_cast<int>(s0.r + f * (s1.r - s0.r)),
        static_cast<int>(s0.g + f * (s1.g - s0.g)),
        static_cast<int>(s0.b + f * (s1.b - s0.b))
    );
}

void ObjViewerWidget::drawEnergyScaleBar(QPainter& painter,
                                         const double top_reserved,
                                         const double available_h) const
{
    if (vertex_energies_.empty()) return;

    const int bar_w  = 14;
    const int bar_h  = static_cast<int>(available_h) - 40;
    const int bar_x  = width() - bar_w - 56;
    const int bar_y  = static_cast<int>(top_reserved) + 20;

    if (bar_h <= 0) return;

    painter.save();

    // Gradient strip (one line per pixel row).
    for (int y = 0; y < bar_h; ++y) {
        const double t = 1.0 - static_cast<double>(y) / static_cast<double>(bar_h);
        painter.setPen(energyToColor(t));
        painter.drawLine(bar_x, bar_y + y, bar_x + bar_w - 1, bar_y + y);
    }
    painter.setPen(QColor(150, 165, 190));
    painter.drawRect(bar_x, bar_y, bar_w - 1, bar_h - 1);

    // Tick marks and labels (5 ticks).  Pick precision dynamically so a narrow
    // range over a large absolute (e.g. 2899–3000 MeV) does not collapse all
    // tick labels to the same scientific-notation digits.
    QFont f = painter.font();
    f.setPointSizeF(7.0);
    painter.setFont(f);

    const double e_span = energy_max_ - energy_min_;
    const double tick_step = e_span / 4.0;
    int decimals = 0;
    if (tick_step > 0.0) {
        const double mag = std::log10(tick_step);
        decimals = std::clamp(1 - static_cast<int>(std::floor(mag)), 0, 6);
    }

    for (int ti = 0; ti <= 4; ++ti) {
        const double frac = static_cast<double>(ti) / 4.0;
        const int y = bar_y + static_cast<int>((1.0 - frac) * static_cast<double>(bar_h));
        const double e = energy_min_ + frac * e_span;

        painter.setPen(QColor(150, 165, 190));
        painter.drawLine(bar_x - 4, y, bar_x, y);

        painter.setPen(QColor(210, 220, 235));
        painter.drawText(
            bar_x - 52, y - 9, 46, 18,
            Qt::AlignRight | Qt::AlignVCenter,
            QString::number(e, 'f', decimals)
        );
    }

    // Rotated unit label to the right of the bar.
    painter.translate(bar_x + bar_w + 14, bar_y + bar_h / 2.0);
    painter.rotate(90);
    painter.setPen(QColor(160, 175, 200));
    f.setPointSizeF(7.0);
    painter.setFont(f);
    painter.drawText(QRect(-36, -9, 72, 18), Qt::AlignCenter,
                     QStringLiteral("kinE [MeV]"));

    painter.restore();
}

void ObjViewerWidget::resetCamera()
{
    setViewPreset(0);
    zoom_ = 0.92;
    pan_ = QPointF(0.0, 0.0);
    update();
}

void ObjViewerWidget::updateBounds()
{
    if (vertices_.empty()) {
        center_ = {};
        bounds_min_ = {};
        bounds_max_ = {};
        radius_ = 1.0;
        return;
    }

    Vec3 min_v{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    };

    Vec3 max_v{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const auto& v : vertices_) {
        min_v.x = std::min(min_v.x, v.x);
        min_v.y = std::min(min_v.y, v.y);
        min_v.z = std::min(min_v.z, v.z);

        max_v.x = std::max(max_v.x, v.x);
        max_v.y = std::max(max_v.y, v.y);
        max_v.z = std::max(max_v.z, v.z);
    }

    center_ = {
        0.5 * (min_v.x + max_v.x),
        0.5 * (min_v.y + max_v.y),
        0.5 * (min_v.z + max_v.z)
    };
    bounds_min_ = min_v;
    bounds_max_ = max_v;

    radius_ = 1.0e-12;

    for (const auto& v : vertices_) {
        const double dx = v.x - center_.x;
        const double dy = v.y - center_.y;
        const double dz = v.z - center_.z;
        radius_ = std::max(radius_, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
}

ObjViewerWidget::Vec3 ObjViewerWidget::rotate(const Vec3& p) const
{
    return {
        p.x * camera_.right.x + p.y * camera_.right.y + p.z * camera_.right.z,
        p.x * camera_.up.x + p.y * camera_.up.y + p.z * camera_.up.z,
        p.x * camera_.forward.x + p.y * camera_.forward.y + p.z * camera_.forward.z
    };
}

void ObjViewerWidget::setCameraFromYawPitch(const double yaw, const double pitch)
{
    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);

    camera_.right = {cy, 0.0, sy};
    camera_.up = {sp * sy, cp, -sp * cy};
    camera_.forward = {-cp * sy, sp, cp * cy};
    normalizeCameraBasis();
}

void ObjViewerWidget::normalizeCameraBasis()
{
    const auto dot = [](const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };

    const auto cross = [](const Vec3& a, const Vec3& b) {
        return Vec3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    };

    const auto normalized = [](const Vec3& v) {
        const double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

        if (length <= 1.0e-15) {
            return Vec3{1.0, 0.0, 0.0};
        }

        return Vec3{v.x / length, v.y / length, v.z / length};
    };

    camera_.right = normalized(camera_.right);

    const double up_projection = dot(camera_.up, camera_.right);
    camera_.up = normalized({
        camera_.up.x - up_projection * camera_.right.x,
        camera_.up.y - up_projection * camera_.right.y,
        camera_.up.z - up_projection * camera_.right.z
    });

    camera_.forward = normalized(cross(camera_.right, camera_.up));
}

void ObjViewerWidget::applyFreeRotation(const double yaw_delta,
                                        const double pitch_delta)
{
    const double cy = std::cos(yaw_delta);
    const double sy = std::sin(yaw_delta);
    const double cp = std::cos(pitch_delta);
    const double sp = std::sin(pitch_delta);

    const Vec3 right0 = camera_.right;
    const Vec3 up0 = camera_.up;
    const Vec3 forward0 = camera_.forward;

    const Vec3 right_y{
        cy * right0.x + sy * forward0.x,
        cy * right0.y + sy * forward0.y,
        cy * right0.z + sy * forward0.z
    };

    const Vec3 forward_y{
        -sy * right0.x + cy * forward0.x,
        -sy * right0.y + cy * forward0.y,
        -sy * right0.z + cy * forward0.z
    };

    camera_.right = right_y;
    camera_.up = {
        cp * up0.x - sp * forward_y.x,
        cp * up0.y - sp * forward_y.y,
        cp * up0.z - sp * forward_y.z
    };
    camera_.forward = {
        sp * up0.x + cp * forward_y.x,
        sp * up0.y + cp * forward_y.y,
        sp * up0.z + cp * forward_y.z
    };

    normalizeCameraBasis();
}

void ObjViewerWidget::applyRoll(const double roll_delta)
{
    const double cr = std::cos(roll_delta);
    const double sr = std::sin(roll_delta);

    const Vec3 right0 = camera_.right;
    const Vec3 up0 = camera_.up;

    camera_.right = {
        cr * right0.x - sr * up0.x,
        cr * right0.y - sr * up0.y,
        cr * right0.z - sr * up0.z
    };
    camera_.up = {
        sr * right0.x + cr * up0.x,
        sr * right0.y + cr * up0.y,
        sr * right0.z + cr * up0.z
    };

    normalizeCameraBasis();
}

void ObjViewerWidget::setViewPreset(const int preset)
{
    if (preset == 1) {
        camera_.right = {1.0, 0.0, 0.0};
        camera_.up = {0.0, 1.0, 0.0};
        camera_.forward = {0.0, 0.0, 1.0};
    } else if (preset == 2) {
        camera_.right = {0.0, 0.0, 1.0};
        camera_.up = {0.0, 1.0, 0.0};
        camera_.forward = {-1.0, 0.0, 0.0};
    } else if (preset == 3) {
        camera_.right = {1.0, 0.0, 0.0};
        camera_.up = {0.0, 0.0, 1.0};
        camera_.forward = {0.0, -1.0, 0.0};
    } else {
        setCameraFromYawPitch(0.85, -0.48);
        update();
        return;
    }

    normalizeCameraBasis();
    update();
}

void ObjViewerWidget::frameLongestAxisHorizontally()
{
    const Vec3 spans{
        bounds_max_.x - bounds_min_.x,
        bounds_max_.y - bounds_min_.y,
        bounds_max_.z - bounds_min_.z
    };

    const auto spanAt = [&](const int axis) {
        if (axis == 0) {
            return spans.x;
        }

        if (axis == 1) {
            return spans.y;
        }

        return spans.z;
    };

    const auto axisVector = [](const int axis) {
        if (axis == 0) {
            return Vec3{1.0, 0.0, 0.0};
        }

        if (axis == 1) {
            return Vec3{0.0, 1.0, 0.0};
        }

        return Vec3{0.0, 0.0, 1.0};
    };

    int long_axis = 0;

    if (spanAt(1) > spanAt(long_axis)) {
        long_axis = 1;
    }

    if (spanAt(2) > spanAt(long_axis)) {
        long_axis = 2;
    }

    int up_axis = long_axis == 2 ? 1 : 2;
    const int alternate_axis = 3 - long_axis - up_axis;

    if (spanAt(alternate_axis) > spanAt(up_axis)) {
        up_axis = alternate_axis;
    }

    camera_.right = axisVector(long_axis);
    camera_.up = axisVector(up_axis);
    normalizeCameraBasis();
    zoom_ = 0.74;
    pan_ = QPointF(0.0, 0.0);
    update();
}

void ObjViewerWidget::setAxesVisible(const bool visible)
{
    axes_visible_ = visible;
    update();
}

void ObjViewerWidget::setMeasureGuidesVisible(const bool visible)
{
    measure_guides_visible_ = visible;
    update();
}

bool ObjViewerWidget::axesVisible() const
{
    return axes_visible_;
}

bool ObjViewerWidget::measureGuidesVisible() const
{
    return measure_guides_visible_;
}

QPointF ObjViewerWidget::project(const Vec3& p) const
{
    const Vec3 shifted{
        p.x - center_.x,
        p.y - center_.y,
        p.z - center_.z
    };

    const Vec3 r = rotate(shifted);

    const double margin = 72.0;
    const double top_reserved = 88.0;
    const double available_w =
        std::max(20.0, static_cast<double>(width()) - 2.0 * margin);
    const double available_h =
        std::max(20.0, static_cast<double>(height()) - top_reserved - 56.0);
    const double stable_radius = std::max(radius_, 1.0e-12);
    const double scale =
        0.48 * std::min(available_w, available_h) * zoom_ / stable_radius;

    const double center_y = top_reserved + 0.5 * available_h;
    const double sx = 0.5 * width() + pan_.x() + r.x * scale;
    const double sy = center_y + pan_.y() - r.y * scale;

    return QPointF(sx, sy);
}

void ObjViewerWidget::drawAxisTriad(QPainter& painter,
                                    const QRectF& viewport,
                                    const QRectF& content_bounds) const
{
    const QSizeF panel_size(126.0, 112.0);
    const double gap = 14.0;

    const std::array<QRectF, 4> candidates{
        QRectF(viewport.right() - panel_size.width() - gap,
               viewport.bottom() - panel_size.height() - gap,
               panel_size.width(),
               panel_size.height()),
        QRectF(viewport.left() + gap,
               viewport.bottom() - panel_size.height() - gap,
               panel_size.width(),
               panel_size.height()),
        QRectF(viewport.right() - panel_size.width() - gap,
               viewport.top() + gap,
               panel_size.width(),
               panel_size.height()),
        QRectF(viewport.left() + gap,
               viewport.top() + gap,
               panel_size.width(),
               panel_size.height())
    };

    QRectF panel = candidates[0];
    double best_score = std::numeric_limits<double>::infinity();

    for (const QRectF& candidate : candidates) {
        const double score =
            overlapArea(candidate, content_bounds) +
            (candidate.contains(QPointF(18.0, static_cast<double>(height()) - 22.0))
                 ? 4000.0
                 : 0.0);

        if (score < best_score) {
            best_score = score;
            panel = candidate;
        }
    }

    const QPointF origin(panel.center().x(), panel.center().y() + 8.0);
    const double length = 36.0;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QPen(QColor(74, 88, 112, 190), 1.0));
    painter.setBrush(QColor(9, 13, 20, 190));
    painter.drawRoundedRect(panel, 8.0, 8.0);

    QFont label_font = painter.font();
    label_font.setPointSizeF(std::max(8.5, label_font.pointSizeF()));
    label_font.setBold(true);
    painter.setFont(label_font);

    struct AxisDraw {
        Vec3 axis{};
        QColor color{};
        QString label{};
        double depth{0.0};
    };

    std::array<AxisDraw, 3> axes{{
        {{1.0, 0.0, 0.0}, QColor(255, 118, 118), "X", rotate({1.0, 0.0, 0.0}).z},
        {{0.0, 1.0, 0.0}, QColor(118, 232, 166), "Y", rotate({0.0, 1.0, 0.0}).z},
        {{0.0, 0.0, 1.0}, QColor(132, 180, 255), "Z", rotate({0.0, 0.0, 1.0}).z}
    }};

    std::sort(
        axes.begin(),
        axes.end(),
        [](const AxisDraw& a, const AxisDraw& b) {
            return a.depth < b.depth;
        }
    );

    const auto drawAxis = [&](const Vec3& axis,
                              const QColor& color,
                              const QString& label) {
        const Vec3 r = rotate(axis);
        const QPointF end(
            origin.x() + r.x * length,
            origin.y() - r.y * length
        );
        const QPointF dir = end - origin;
        const double dir_len = std::hypot(dir.x(), dir.y());
        const QPointF unit =
            dir_len > 1.0e-9
                ? QPointF(dir.x() / dir_len, dir.y() / dir_len)
                : QPointF(1.0, 0.0);
        const QPointF normal(-unit.y(), unit.x());

        QPen pen(color);
        pen.setWidthF(2.4);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(origin, end);

        const QPointF arrow_a = end - unit * 8.0 + normal * 4.0;
        const QPointF arrow_b = end - unit * 8.0 - normal * 4.0;
        painter.drawLine(end, arrow_a);
        painter.drawLine(end, arrow_b);
        painter.drawText(end + unit * 7.0 + QPointF(1.0, -2.0), label);
    };

    painter.setPen(QPen(QColor(150, 164, 188), 1.4));
    painter.setBrush(QColor(220, 228, 240));
    painter.drawEllipse(origin, 2.6, 2.6);

    for (const auto& axis : axes) {
        drawAxis(axis.axis, axis.color, axis.label);
    }

    QFont caption_font = painter.font();
    caption_font.setBold(false);
    caption_font.setPointSizeF(std::max(7.5, caption_font.pointSizeF() - 1.0));
    painter.setFont(caption_font);
    painter.setPen(QColor(185, 198, 218));
    painter.drawText(
        panel.adjusted(8.0, 6.0, -8.0, -6.0),
        Qt::AlignLeft | Qt::AlignTop,
        "world axes"
    );

    painter.restore();
}

void ObjViewerWidget::drawDimensionGuides(QPainter& painter,
                                          const std::vector<Vec3>& bounds_corners,
                                          const double scale,
                                          const double center_y,
                                          const QRectF& content_bounds) const
{
    if (bounds_corners.empty() || content_bounds.isEmpty()) {
        return;
    }

    const auto projectWorld = [&](const Vec3& point) {
        const Vec3 shifted{
            point.x - center_.x,
            point.y - center_.y,
            point.z - center_.z
        };
        const Vec3 r = rotate(shifted);
        return QPointF(
            0.5 * static_cast<double>(width()) + pan_.x() + r.x * scale,
            center_y + pan_.y() - r.y * scale
        );
    };

    const auto normalized = [](const QPointF& vector) {
        const double length = std::hypot(vector.x(), vector.y());

        if (length <= 1.0e-9) {
            return QPointF(1.0, 0.0);
        }

        return QPointF(vector.x() / length, vector.y() / length);
    };

    struct Guide {
        QString label{};
        Vec3 a{};
        Vec3 b{};
        QColor color{};
        double length_m{0.0};
    };

    const std::array<Guide, 3> guides{{
        {
            "X",
            {bounds_min_.x, bounds_min_.y, bounds_min_.z},
            {bounds_max_.x, bounds_min_.y, bounds_min_.z},
            QColor(255, 118, 118, 220),
            bounds_max_.x - bounds_min_.x
        },
        {
            "Y",
            {bounds_max_.x, bounds_min_.y, bounds_min_.z},
            {bounds_max_.x, bounds_max_.y, bounds_min_.z},
            QColor(118, 232, 166, 220),
            bounds_max_.y - bounds_min_.y
        },
        {
            "Z",
            {bounds_min_.x, bounds_max_.y, bounds_min_.z},
            {bounds_min_.x, bounds_max_.y, bounds_max_.z},
            QColor(132, 180, 255, 220),
            bounds_max_.z - bounds_min_.z
        }
    }};

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont guide_font = painter.font();
    guide_font.setPointSizeF(std::max(8.0, guide_font.pointSizeF() - 0.5));
    guide_font.setBold(true);
    painter.setFont(guide_font);

    for (const Guide& guide : guides) {
        if (std::abs(guide.length_m) <= 1.0e-15) {
            continue;
        }

        const QPointF p1 = projectWorld(guide.a);
        const QPointF p2 = projectWorld(guide.b);
        const QPointF mid = 0.5 * (p1 + p2);
        const QPointF outward = normalized(mid - content_bounds.center());
        QPointF offset = outward * 28.0;

        QRectF label_rect;
        const QString label =
            QString("%1 %2").arg(guide.label).arg(formatDistance(guide.length_m));

        for (int attempt = 0; attempt < 4; ++attempt) {
            const QPointF label_pos = mid + offset;
            const QRect text_bounds =
                painter.fontMetrics().boundingRect(label);
            label_rect = QRectF(
                label_pos.x() - 0.5 * static_cast<double>(text_bounds.width()) - 6.0,
                label_pos.y() - 0.5 * static_cast<double>(text_bounds.height()) - 4.0,
                static_cast<double>(text_bounds.width()) + 12.0,
                static_cast<double>(text_bounds.height()) + 8.0
            );

            if (overlapArea(label_rect, content_bounds) <= 1.0) {
                break;
            }

            offset += outward * 18.0;
        }

        const QPointF q1 = p1 + offset;
        const QPointF q2 = p2 + offset;
        const QPointF direction = normalized(q2 - q1);
        const QPointF tick(-direction.y(), direction.x());

        QPen guide_pen(guide.color);
        guide_pen.setWidthF(1.35);
        guide_pen.setCapStyle(Qt::RoundCap);
        painter.setPen(guide_pen);
        painter.drawLine(q1, q2);
        painter.drawLine(q1 - tick * 5.0, q1 + tick * 5.0);
        painter.drawLine(q2 - tick * 5.0, q2 + tick * 5.0);
        painter.drawLine(p1, q1);
        painter.drawLine(p2, q2);

        painter.setPen(QPen(QColor(7, 11, 18, 210), 1.0));
        painter.setBrush(QColor(7, 11, 18, 210));
        painter.drawRoundedRect(label_rect, 5.0, 5.0);
        painter.setPen(guide.color);
        painter.drawText(label_rect, Qt::AlignCenter, label);
    }

    painter.restore();
}

void ObjViewerWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), panelBackground());

    painter.setPen(QPen(panelText()));
    painter.drawText(
        QRectF(18.0, 10.0, static_cast<double>(width()) - 36.0, 38.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(
            status_text_,
            Qt::ElideMiddle,
            std::max(40, width() - 36)
        )
    );

    if (vertices_.empty()) {
        painter.drawText(18, 60, "No OBJ loaded.");
        return;
    }

    struct ScreenPoint {
        double x{0.0};
        double y{0.0};
        double depth{0.0};
    };

    std::vector<ScreenPoint> raw_points;
    raw_points.reserve(vertices_.size());

    for (const auto& v : vertices_) {
        const Vec3 shifted{
            v.x - center_.x,
            v.y - center_.y,
            v.z - center_.z
        };

        const Vec3 r = rotate(shifted);
        raw_points.push_back({r.x, r.y, r.z});
    }

    const double margin = 72.0;
    const double top_reserved = 88.0;
    const double available_w =
        std::max(20.0, static_cast<double>(width()) - 2.0 * margin);

    const double available_h =
        std::max(20.0, static_cast<double>(height()) - top_reserved - 56.0);

    const double stable_radius = std::max(radius_, 1.0e-12);

    const double scale =
        0.48 * std::min(available_w, available_h) * zoom_ / stable_radius;
    const double center_y = top_reserved + 0.5 * available_h;

    const auto toScreen = [&](const int index) -> QPointF {
        const auto& p = raw_points[static_cast<std::size_t>(index)];

        const double sx =
            0.5 * static_cast<double>(width()) +
            pan_.x() +
            p.x * scale;

        const double sy =
            center_y +
            pan_.y() -
            p.y * scale;

        return QPointF(sx, sy);
    };

    QPen grid_pen(panelGrid());
    grid_pen.setWidthF(1.0);
    painter.setPen(grid_pen);

    const QRectF viewport(
        margin,
        top_reserved,
        available_w,
        available_h
    );

    QRectF content_bounds;
    bool has_content_bounds = false;

    for (int i = 0; i < static_cast<int>(raw_points.size()); ++i) {
        const QPointF point = toScreen(i);

        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
            continue;
        }

        const QRectF point_rect(point - QPointF(2.0, 2.0), QSizeF(4.0, 4.0));
        content_bounds =
            has_content_bounds ? content_bounds.united(point_rect) : point_rect;
        has_content_bounds = true;
    }

    if (!has_content_bounds) {
        content_bounds = QRectF();
    } else {
        content_bounds = content_bounds.adjusted(-10.0, -10.0, 10.0, 10.0);
    }

    painter.drawRect(viewport);

    QPen wire_pen(panelAccent());
    wire_pen.setWidthF(1.05);
    painter.setPen(wire_pen);

    if (!faces_.empty()) {
        for (const auto& face : faces_) {
            for (std::size_t i = 0; i < face.indices.size(); ++i) {
                const int a = face.indices[i];
                const int b = face.indices[(i + 1) % face.indices.size()];

                if (a < 0 || b < 0 ||
                    a >= static_cast<int>(vertices_.size()) ||
                    b >= static_cast<int>(vertices_.size())) {
                    continue;
                }

                painter.drawLine(toScreen(a), toScreen(b));
            }
        }
    }

    if (!polylines_.empty()) {
        const int n_total = static_cast<int>(polylines_.size());
        const int n_draw = (max_polylines_ < 0)
            ? n_total
            : std::min(max_polylines_, n_total);

        const bool use_gradient =
            energy_gradient_ &&
            vertex_energies_.size() == vertices_.size();
        const double e_span = energy_max_ - energy_min_;
        const int nv = static_cast<int>(vertices_.size());

        if (!use_gradient) {
            QPen line_pen(QColor(120, 255, 210, 120));
            line_pen.setWidthF(0.9);
            painter.setPen(line_pen);
        }

        for (int pi = 0; pi < n_draw; ++pi) {
            const auto& poly = polylines_[static_cast<std::size_t>(pi)];
            for (std::size_t si = 1; si < poly.indices.size(); ++si) {
                const int a = poly.indices[si - 1];
                const int b = poly.indices[si];

                if (a < 0 || b < 0 || a >= nv || b >= nv) continue;

                if (use_gradient) {
                    const double ea = vertex_energies_[static_cast<std::size_t>(a)];
                    const double eb = vertex_energies_[static_cast<std::size_t>(b)];
                    const double t = 0.5 * (ea + eb - 2.0 * energy_min_) / e_span;
                    QPen gpen(energyToColor(t));
                    gpen.setWidthF(1.2);
                    painter.setPen(gpen);
                }

                painter.drawLine(toScreen(a), toScreen(b));
            }
        }
    }

    if (faces_.empty() && polylines_.empty()) {
        painter.setPen(QPen(panelAccent(), 2.0));

        for (std::size_t i = 0; i < vertices_.size(); ++i) {
            const QPointF p = toScreen(static_cast<int>(i));
            painter.drawEllipse(p, 1.5, 1.5);
        }
    }

    const std::vector<Vec3> bounds_corners{
        {bounds_min_.x, bounds_min_.y, bounds_min_.z},
        {bounds_max_.x, bounds_min_.y, bounds_min_.z},
        {bounds_min_.x, bounds_max_.y, bounds_min_.z},
        {bounds_max_.x, bounds_max_.y, bounds_min_.z},
        {bounds_min_.x, bounds_min_.y, bounds_max_.z},
        {bounds_max_.x, bounds_min_.y, bounds_max_.z},
        {bounds_min_.x, bounds_max_.y, bounds_max_.z},
        {bounds_max_.x, bounds_max_.y, bounds_max_.z}
    };
    if (measure_guides_visible_) {
        drawDimensionGuides(
            painter,
            bounds_corners,
            scale,
            center_y,
            content_bounds
        );
    }

    if (energy_gradient_ && !vertex_energies_.empty()) {
        drawEnergyScaleBar(painter, top_reserved, available_h);
    }

    painter.setPen(QPen(QColor(180, 190, 210)));
    const QString footer =
        QString("free camera  |  zoom=%1  |  arrows rotate  |  Shift+arrows pan  |  Ctrl+left/right roll  |  H horizontal fit")
            .arg(zoom_, 0, 'f', 2);
    painter.drawText(
        18,
        height() - 22,
        painter.fontMetrics().elidedText(
            footer,
            Qt::ElideRight,
            std::max(80, width() - 36)
        )
    );

    if (axes_visible_) {
        drawAxisTriad(painter, viewport, content_bounds);
    }
}

void ObjViewerWidget::mousePressEvent(QMouseEvent* event)
{
    last_mouse_pos_ = event->pos();
    mouse_moved_ = false;
    setFocus();
}

void ObjViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (mouse_moved_) return;
    if (event->button() != Qt::LeftButton) return;
    if (!energy_gradient_ || vertex_energies_.size() != vertices_.size()) return;

    // Find nearest projected vertex within 16 px.
    const QPointF click(event->pos());
    int best_idx = -1;
    double best_dist2 = 16.0 * 16.0;

    for (int i = 0; i < static_cast<int>(vertices_.size()); ++i) {
        const QPointF sp = project(vertices_[static_cast<std::size_t>(i)]);
        const double dx = sp.x() - click.x();
        const double dy = sp.y() - click.y();
        const double d2 = dx * dx + dy * dy;
        if (d2 < best_dist2) {
            best_dist2 = d2;
            best_idx = i;
        }
    }

    if (best_idx >= 0) {
        emit energyPicked(
            vertex_energies_[static_cast<std::size_t>(best_idx)],
            project(vertices_[static_cast<std::size_t>(best_idx)])
        );
    }
}

void ObjViewerWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint delta = event->pos() - last_mouse_pos_;
    last_mouse_pos_ = event->pos();

    if (event->buttons() != Qt::NoButton) {
        mouse_moved_ = true;
    }

    if (event->buttons() & Qt::LeftButton) {
        applyFreeRotation(
            static_cast<double>(delta.x()) * 0.0085,
            static_cast<double>(delta.y()) * 0.0085
        );
        update();
    } else if (event->buttons() & Qt::RightButton ||
               event->buttons() & Qt::MiddleButton) {
        pan_ += QPointF(delta.x(), delta.y());
        update();
    }
}

void ObjViewerWidget::wheelEvent(QWheelEvent* event)
{
    const double factor = std::pow(1.0015, event->angleDelta().y());
    zoom_ *= factor;
    zoom_ = std::clamp(zoom_, 0.05, 80.0);
    update();
}

void ObjViewerWidget::keyPressEvent(QKeyEvent* event)
{
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    const bool control = event->modifiers().testFlag(Qt::ControlModifier);
    constexpr double rotation_step = 0.085;
    constexpr double roll_step = 0.10;
    constexpr double pan_step = 34.0;

    if (event->key() == Qt::Key_Left ||
        event->key() == Qt::Key_Right ||
        event->key() == Qt::Key_Up ||
        event->key() == Qt::Key_Down) {
        if (shift) {
            if (event->key() == Qt::Key_Left) {
                pan_ += QPointF(-pan_step, 0.0);
            } else if (event->key() == Qt::Key_Right) {
                pan_ += QPointF(pan_step, 0.0);
            } else if (event->key() == Qt::Key_Up) {
                pan_ += QPointF(0.0, -pan_step);
            } else {
                pan_ += QPointF(0.0, pan_step);
            }
        } else if (control && (event->key() == Qt::Key_Left ||
                               event->key() == Qt::Key_Right)) {
            applyRoll(event->key() == Qt::Key_Left ? -roll_step : roll_step);
        } else {
            if (event->key() == Qt::Key_Left) {
                applyFreeRotation(-rotation_step, 0.0);
            } else if (event->key() == Qt::Key_Right) {
                applyFreeRotation(rotation_step, 0.0);
            } else if (event->key() == Qt::Key_Up) {
                applyFreeRotation(0.0, -rotation_step);
            } else {
                applyFreeRotation(0.0, rotation_step);
            }
        }

        update();
        return;
    }

    if (event->key() == Qt::Key_H) {
        frameLongestAxisHorizontally();
        return;
    }

    if (event->key() == Qt::Key_R) {
        resetCamera();
        return;
    }

    if (event->key() == Qt::Key_1) {
        setViewPreset(1);
        update();
        return;
    }

    if (event->key() == Qt::Key_2) {
        setViewPreset(2);
        update();
        return;
    }

    if (event->key() == Qt::Key_3) {
        setViewPreset(3);
        update();
        return;
    }

    if (event->key() == Qt::Key_I) {
        setViewPreset(0);
        update();
        return;
    }

    if (event->key() == Qt::Key_Q) {
        applyRoll(-0.12);
        update();
        return;
    }

    if (event->key() == Qt::Key_E) {
        applyRoll(0.12);
        update();
        return;
    }

    QWidget::keyPressEvent(event);
}

void ObjViewerWidget::setMaxPolylines(const int max)
{
    max_polylines_ = max;
    update();
}

int ObjViewerWidget::polylineCount() const
{
    return static_cast<int>(polylines_.size());
}

} // namespace beamlab::ui
