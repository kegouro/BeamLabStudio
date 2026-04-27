#include "ui/qt/Scene3DWidget.h"

#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QSizeF>
#include <QStringList>
#include <QTextStream>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace beamlab::ui {
namespace {

QStringList splitObjToken(const QString& token)
{
    return token.split('/', Qt::KeepEmptyParts);
}

int parseObjIndex(const QString& token, const int vertex_count, bool& ok)
{
    const QString first = splitObjToken(token).value(0);
    const int raw = first.toInt(&ok);

    if (!ok) {
        return -1;
    }

    if (raw < 0) {
        return vertex_count + raw;
    }

    return raw - 1;
}

QColor backgroundColor()
{
    return QColor(13, 16, 22);
}

QColor gridColor()
{
    return QColor(54, 61, 76);
}

QColor textColor()
{
    return QColor(225, 231, 242);
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

Scene3DWidget::Scene3DWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(850, 580);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void Scene3DWidget::clearLayers()
{
    layers_.clear();
    updateBounds();
    update();
}

int Scene3DWidget::addObjLayer(const QString& name,
                               const QString& path,
                               const QColor& color)
{
    if (path.isEmpty()) {
        return -1;
    }

    Layer layer;
    layer.name = name;
    layer.path = path;
    layer.color = color;

    if (!parseObjIntoLayer(path, layer)) {
        return -1;
    }

    layers_.push_back(std::move(layer));
    updateBounds();
    update();

    return static_cast<int>(layers_.size()) - 1;
}

bool Scene3DWidget::removeLayer(const int layer_index)
{
    if (layer_index < 0 || layer_index >= static_cast<int>(layers_.size())) {
        return false;
    }

    layers_.erase(layers_.begin() + layer_index);
    updateBounds();
    update();
    return true;
}

bool Scene3DWidget::parseObjIntoLayer(const QString& path, Layer& layer) const
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();

        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        const QStringList parts = line.split(' ', Qt::SkipEmptyParts);

        if (parts.isEmpty()) {
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
                layer.vertices.push_back({x, y, z});
            }

            continue;
        }

        if (parts[0] == "f" && parts.size() >= 4) {
            Face face;

            for (int i = 1; i < parts.size(); ++i) {
                bool ok = false;
                const int index = parseObjIndex(
                    parts[i],
                    static_cast<int>(layer.vertices.size()),
                    ok
                );

                if (ok && index >= 0) {
                    face.indices.push_back(index);
                }
            }

            if (face.indices.size() >= 3) {
                layer.faces.push_back(std::move(face));
            }

            continue;
        }

        if (parts[0] == "l" && parts.size() >= 3) {
            Polyline polyline;

            for (int i = 1; i < parts.size(); ++i) {
                bool ok = false;
                const int index = parseObjIndex(
                    parts[i],
                    static_cast<int>(layer.vertices.size()),
                    ok
                );

                if (ok && index >= 0) {
                    polyline.indices.push_back(index);
                }
            }

            if (polyline.indices.size() >= 2) {
                layer.polylines.push_back(std::move(polyline));
            }

            continue;
        }
    }

    layer.status =
        QString("vertices=%1 faces=%2 polylines=%3")
            .arg(static_cast<qulonglong>(layer.vertices.size()))
            .arg(static_cast<qulonglong>(layer.faces.size()))
            .arg(static_cast<qulonglong>(layer.polylines.size()));

    return !layer.vertices.empty();
}

void Scene3DWidget::setLayerVisible(const int layer_index, const bool visible)
{
    if (layer_index < 0 || layer_index >= static_cast<int>(layers_.size())) {
        return;
    }

    layers_[static_cast<std::size_t>(layer_index)].visible = visible;
    updateBounds();
    update();
}

void Scene3DWidget::setLayerColor(const int layer_index, const QColor& color)
{
    if (layer_index < 0 || layer_index >= static_cast<int>(layers_.size())) {
        return;
    }

    layers_[static_cast<std::size_t>(layer_index)].color = color;
    update();
}

void Scene3DWidget::setLayerMaxPolylines(const int layer_index,
                                         const int max_polylines)
{
    if (layer_index < 0 || layer_index >= static_cast<int>(layers_.size())) {
        return;
    }

    layers_[static_cast<std::size_t>(layer_index)].max_polylines = max_polylines;
    update();
}

void Scene3DWidget::setLayerOpacity(const int layer_index, const double opacity)
{
    if (layer_index < 0 || layer_index >= static_cast<int>(layers_.size())) {
        return;
    }

    layers_[static_cast<std::size_t>(layer_index)].opacity =
        std::clamp(opacity, 0.05, 1.0);

    update();
}

void Scene3DWidget::setLayerLineWidth(const int layer_index, const double width)
{
    if (layer_index < 0 || layer_index >= static_cast<int>(layers_.size())) {
        return;
    }

    layers_[static_cast<std::size_t>(layer_index)].line_width =
        std::clamp(width, 0.2, 8.0);

    update();
}


void Scene3DWidget::setTrajectoryParameter(const double lambda)
{
    trajectory_parameter_ = std::clamp(lambda, 0.0, 1.0);
    update();
}

void Scene3DWidget::setAxesVisible(const bool visible)
{
    axes_visible_ = visible;
    update();
}

void Scene3DWidget::setMeasureGuidesVisible(const bool visible)
{
    measure_guides_visible_ = visible;
    update();
}

int Scene3DWidget::layerPolylineCount(const int layer_index) const
{
    if (layer_index < 0 || layer_index >= static_cast<int>(layers_.size())) {
        return 0;
    }

    const auto count =
        layers_[static_cast<std::size_t>(layer_index)].polylines.size();

    const auto safe_count = std::min<std::size_t>(
        count,
        static_cast<std::size_t>(std::numeric_limits<int>::max())
    );

    return static_cast<int>(safe_count);
}

bool Scene3DWidget::axesVisible() const
{
    return axes_visible_;
}

bool Scene3DWidget::measureGuidesVisible() const
{
    return measure_guides_visible_;
}

void Scene3DWidget::resetCamera()
{
    setViewPreset(0);
    zoom_ = 0.92;
    pan_ = QPointF(0.0, 0.0);
    update();
}

void Scene3DWidget::setViewPreset(const int preset)
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

void Scene3DWidget::frameLongestAxisHorizontally()
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

    int long_axis = 0;

    if (spanAt(1) > spanAt(long_axis)) {
        long_axis = 1;
    }

    if (spanAt(2) > spanAt(long_axis)) {
        long_axis = 2;
    }

    const auto axisVector = [](const int axis) {
        if (axis == 0) {
            return Vec3{1.0, 0.0, 0.0};
        }

        if (axis == 1) {
            return Vec3{0.0, 1.0, 0.0};
        }

        return Vec3{0.0, 0.0, 1.0};
    };

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

void Scene3DWidget::updateBounds()
{
    bool found = false;

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

    for (const auto& layer : layers_) {
        if (!layer.visible) {
            continue;
        }

        for (const auto& v : layer.vertices) {
            found = true;

            min_v.x = std::min(min_v.x, v.x);
            min_v.y = std::min(min_v.y, v.y);
            min_v.z = std::min(min_v.z, v.z);

            max_v.x = std::max(max_v.x, v.x);
            max_v.y = std::max(max_v.y, v.y);
            max_v.z = std::max(max_v.z, v.z);
        }
    }

    if (!found) {
        center_ = {};
        bounds_min_ = {};
        bounds_max_ = {};
        radius_ = 1.0;
        return;
    }

    bounds_min_ = min_v;
    bounds_max_ = max_v;

    center_ = {
        0.5 * (min_v.x + max_v.x),
        0.5 * (min_v.y + max_v.y),
        0.5 * (min_v.z + max_v.z)
    };

    radius_ = 1.0e-12;

    for (const auto& layer : layers_) {
        if (!layer.visible) {
            continue;
        }

        for (const auto& v : layer.vertices) {
            const double dx = v.x - center_.x;
            const double dy = v.y - center_.y;
            const double dz = v.z - center_.z;

            radius_ = std::max(
                radius_,
                std::sqrt(dx * dx + dy * dy + dz * dz)
            );
        }
    }
}

Scene3DWidget::Vec3 Scene3DWidget::rotate(const Vec3& p) const
{
    return {
        p.x * camera_.right.x + p.y * camera_.right.y + p.z * camera_.right.z,
        p.x * camera_.up.x + p.y * camera_.up.y + p.z * camera_.up.z,
        p.x * camera_.forward.x + p.y * camera_.forward.y + p.z * camera_.forward.z
    };
}

void Scene3DWidget::setCameraFromYawPitch(const double yaw, const double pitch)
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

void Scene3DWidget::normalizeCameraBasis()
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

void Scene3DWidget::applyFreeRotation(const double yaw_delta,
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

void Scene3DWidget::applyRoll(const double roll_delta)
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

void Scene3DWidget::drawAxisTriad(QPainter& painter,
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

void Scene3DWidget::drawDimensionGuides(QPainter& painter,
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

void Scene3DWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), backgroundColor());

    painter.setPen(QPen(textColor()));
    painter.drawText(
        QRectF(18.0, 10.0, static_cast<double>(width()) - 36.0, 32.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        "Combined 3D scene"
    );

    if (layers_.empty()) {
        painter.drawText(18, 60, "No 3D layers loaded.");
        return;
    }

    struct ScreenPoint {
        double x{0.0};
        double y{0.0};
        double z{0.0};
    };

    std::vector<std::vector<ScreenPoint>> projected_layers;
    projected_layers.resize(layers_.size());

    bool found = false;

    for (std::size_t layer_index = 0; layer_index < layers_.size(); ++layer_index) {
        const auto& layer = layers_[layer_index];

        if (!layer.visible) {
            continue;
        }

        auto& projected = projected_layers[layer_index];
        projected.reserve(layer.vertices.size());

        for (const auto& v : layer.vertices) {
            const Vec3 shifted{
                v.x - center_.x,
                v.y - center_.y,
                v.z - center_.z
            };

            const Vec3 r = rotate(shifted);
            projected.push_back({r.x, r.y, r.z});
            found = true;
        }
    }

    if (!found) {
        painter.drawText(18, 60, "All layers are hidden.");
        return;
    }

    const double margin = 72.0;
    const double legend_top = 52.0;
    const double legend_bottom =
        legend_top +
        22.0 * static_cast<double>(std::max<std::size_t>(1, layers_.size()));
    const double top_reserved = std::max(102.0, legend_bottom + 18.0);

    const double available_w =
        std::max(20.0, static_cast<double>(width()) - 2.0 * margin);

    const double available_h =
        std::max(20.0, static_cast<double>(height()) - top_reserved - 56.0);

    const double stable_radius = std::max(radius_, 1.0e-12);

    const double scale =
        0.48 * std::min(available_w, available_h) * zoom_ / stable_radius;
    const double center_y = top_reserved + 0.5 * available_h;

    const auto toScreen = [&](const std::size_t layer_index,
                              const int vertex_index) -> QPointF {
        const auto& layer_points = projected_layers[layer_index];

        if (vertex_index < 0 ||
            vertex_index >= static_cast<int>(layer_points.size())) {
            return QPointF{};
        }

        const auto& p = layer_points[static_cast<std::size_t>(vertex_index)];

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

    QPen grid_pen(gridColor());
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

    for (std::size_t layer_index = 0; layer_index < layers_.size(); ++layer_index) {
        const auto& layer = layers_[layer_index];

        if (!layer.visible) {
            continue;
        }

        for (std::size_t vertex_index = 0; vertex_index < layer.vertices.size(); ++vertex_index) {
            const QPointF point = toScreen(layer_index, static_cast<int>(vertex_index));

            if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
                continue;
            }

            const QRectF point_rect(point - QPointF(2.0, 2.0), QSizeF(4.0, 4.0));
            content_bounds =
                has_content_bounds ? content_bounds.united(point_rect) : point_rect;
            has_content_bounds = true;
        }
    }

    if (!has_content_bounds) {
        content_bounds = QRectF();
    } else {
        content_bounds = content_bounds.adjusted(-10.0, -10.0, 10.0, 10.0);
    }

    painter.drawRect(viewport);

    int legend_y = static_cast<int>(legend_top);

    for (std::size_t layer_index = 0; layer_index < layers_.size(); ++layer_index) {
        const auto& layer = layers_[layer_index];

        QColor color = layer.color;
        color.setAlphaF(static_cast<float>(std::clamp(layer.opacity, 0.05, 1.0)));
        const QString legend =
            QString("%1 | %2 | %3")
                .arg(layer.visible ? "ON " : "OFF")
                .arg(layer.name)
                .arg(layer.status);

        painter.setPen(QPen(color));
        painter.drawText(
            18,
            legend_y,
            painter.fontMetrics().elidedText(
                legend,
                Qt::ElideRight,
                std::max(80, width() - 36)
            )
        );

        legend_y += 22;
    }

    for (std::size_t layer_index = 0; layer_index < layers_.size(); ++layer_index) {
        const auto& layer = layers_[layer_index];

        if (!layer.visible) {
            continue;
        }

        QColor color = layer.color;
        color.setAlphaF(static_cast<float>(std::clamp(layer.opacity, 0.05, 1.0)));

        QPen pen(color);
        pen.setWidthF(layer.line_width);
        painter.setPen(pen);

        for (const auto& face : layer.faces) {
            for (std::size_t i = 0; i < face.indices.size(); ++i) {
                const int a = face.indices[i];
                const int b = face.indices[(i + 1) % face.indices.size()];

                painter.drawLine(toScreen(layer_index, a), toScreen(layer_index, b));
            }
        }

        const int total_polylines =
            static_cast<int>(std::min<std::size_t>(
                layer.polylines.size(),
                static_cast<std::size_t>(std::numeric_limits<int>::max())
            ));

        if (layer.max_polylines == 0 && total_polylines > 0) {
            continue;
        }

        int stride = 1;

        if (layer.max_polylines > 0 && total_polylines > layer.max_polylines) {
            stride = std::max(1, total_polylines / layer.max_polylines);
        }

        int drawn = 0;

        for (int i = 0; i < total_polylines; i += stride) {
            if (layer.max_polylines > 0 && drawn >= layer.max_polylines) {
                break;
            }

            const auto& polyline = layer.polylines[static_cast<std::size_t>(i)];

            const std::size_t visible_points =
                std::max<std::size_t>(
                    2,
                    static_cast<std::size_t>(
                        std::ceil(
                            static_cast<double>(polyline.indices.size()) *
                            trajectory_parameter_
                        )
                    )
                );

            const std::size_t limit =
                std::min(polyline.indices.size(), visible_points);

            for (std::size_t j = 1; j < limit; ++j) {
                painter.drawLine(
                    toScreen(layer_index, polyline.indices[j - 1]),
                    toScreen(layer_index, polyline.indices[j])
                );
            }

            ++drawn;
        }

        if (layer.faces.empty() && layer.polylines.empty()) {
            for (std::size_t i = 0; i < layer.vertices.size(); ++i) {
                const QPointF point = toScreen(layer_index, static_cast<int>(i));
                painter.drawEllipse(point, 1.5, 1.5);
            }
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

    painter.setPen(QPen(QColor(180, 190, 210)));
    const QString footer =
        QString("free camera  |  zoom=%1  |  progress=%2%  |  arrows rotate  |  Shift+arrows pan  |  Ctrl+left/right roll  |  H horizontal fit")
            .arg(zoom_, 0, 'f', 2)
            .arg(static_cast<int>(std::round(trajectory_parameter_ * 100.0)));
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

void Scene3DWidget::mousePressEvent(QMouseEvent* event)
{
    last_mouse_pos_ = event->pos();
    setFocus();
}

void Scene3DWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint delta = event->pos() - last_mouse_pos_;
    last_mouse_pos_ = event->pos();

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

void Scene3DWidget::wheelEvent(QWheelEvent* event)
{
    const double factor = std::pow(1.0015, event->angleDelta().y());
    zoom_ *= factor;
    zoom_ = std::clamp(zoom_, 0.05, 80.0);
    update();
}

void Scene3DWidget::keyPressEvent(QKeyEvent* event)
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

    if (event->key() == Qt::Key_I) {
        setViewPreset(0);
        return;
    }

    if (event->key() == Qt::Key_1) {
        setViewPreset(1);
        return;
    }

    if (event->key() == Qt::Key_2) {
        setViewPreset(2);
        return;
    }

    if (event->key() == Qt::Key_3) {
        setViewPreset(3);
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

} // namespace beamlab::ui
