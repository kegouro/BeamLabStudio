#include "biosim/ui/qt/BioViewport3D.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace beamlab::biosim {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

BioViewport3D::BioViewport3D(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(400, 300);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);

    spark_timer_ = new QTimer(this);
    spark_timer_->setInterval(16); // ~60 fps
    connect(spark_timer_, &QTimer::timeout, this, [this] {
        tickSparks();
        update();
    });
}

// ---------------------------------------------------------------------------
// Public setters
// ---------------------------------------------------------------------------

void BioViewport3D::setResult(const BioSimResult& result)
{
    result_     = result;
    has_result_ = !result_.tracks.empty();

    // Cap total rendered segments to ~40 k to keep repaints under ~16 ms.
    // With 1 M+ steps the QPainter loop would lock the UI without this.
    constexpr int kMaxRenderSegments = 40000;
    int total_steps = 0;
    for (const auto& t : result_.tracks)
        total_steps += static_cast<int>(t.steps.size());
    render_stride_ = std::max(1, total_steps / kMaxRenderSegments);

    computeSceneBounds();
    recomputeColorRange();
    update();
}

void BioViewport3D::setSlabs(const std::vector<BioSlab>& slabs)
{
    slabs_ = slabs;
    update();
}

void BioViewport3D::setColorBy(ColorBy field)
{
    color_by_ = field;
    recomputeColorRange();
    update();
}

void BioViewport3D::setPalette(EnergyColorMapper::Palette p)
{
    palette_ = p;
    update();
}

void BioViewport3D::resetCamera()
{
    cam_ = Camera{};
    update();
}

std::pair<double, double> BioViewport3D::colorRange() const
{
    return {color_min_, color_max_};
}

QString BioViewport3D::colorUnit() const
{
    switch (color_by_) {
    case ColorBy::EdepMeV:    return QStringLiteral("MeV");
    case ColorBy::DoseGy:     return QStringLiteral("Gy");
    case ColorBy::LET:        return QStringLiteral("MeV/cm");
    case ColorBy::KineticMeV: return QStringLiteral("MeV");
    case ColorBy::SVmSv:      return QStringLiteral("mSv");
    }
    return {};
}

// ---------------------------------------------------------------------------
// Scene bounds
// ---------------------------------------------------------------------------

void BioViewport3D::computeSceneBounds()
{
    if (!has_result_) return;

    float xmin = std::numeric_limits<float>::max();
    float xmax = -xmin, ymin = xmin, ymax = -xmin, zmin = xmin, zmax = -xmin;

    for (const auto& track : result_.tracks) {
        for (const auto& step : track.steps) {
            const float x = static_cast<float>(step.x_m * 1000.0);
            const float y = static_cast<float>(step.y_m * 1000.0);
            const float z = static_cast<float>(step.z_m * 1000.0);
            xmin = std::min(xmin, x); xmax = std::max(xmax, x);
            ymin = std::min(ymin, y); ymax = std::max(ymax, y);
            zmin = std::min(zmin, z); zmax = std::max(zmax, z);
        }
    }

    scene_cx_ = (xmin + xmax) * 0.5f;
    scene_cy_ = (ymin + ymax) * 0.5f;
    scene_cz_ = (zmin + zmax) * 0.5f;
    const float dx = xmax - xmin, dy = ymax - ymin, dz = zmax - zmin;
    scene_radius_ = std::max({dx, dy, dz, 1.0f}) * 0.5f;
}

// ---------------------------------------------------------------------------
// Colour helpers
// ---------------------------------------------------------------------------

double BioViewport3D::scalarForStep(const BioStep& step) const
{
    switch (color_by_) {
    case ColorBy::EdepMeV:    return step.energy.edep_MeV_original;
    case ColorBy::DoseGy:     return step.energy.dose_Gy;
    case ColorBy::LET:        return step.energy.LET_MeV_cm;
    case ColorBy::KineticMeV: return step.energy.kinE_MeV_simulated;
    case ColorBy::SVmSv:      return step.energy.E_mSv;
    }
    return 0.0;
}

void BioViewport3D::recomputeColorRange()
{
    // Sample up to kMaxSamples values and use 1st–99th percentile so that
    // Bragg-peak outliers or muon delta-ray spikes don't compress the palette
    // for the 99% of steps that carry the interesting variation.
    constexpr int kMaxSamples = 50000;

    int total_steps = 0;
    for (const auto& t : result_.tracks)
        total_steps += static_cast<int>(t.steps.size());
    const int sample_stride = std::max(1, total_steps / kMaxSamples);

    std::vector<double> vals;
    vals.reserve(static_cast<std::size_t>(std::min(total_steps, kMaxSamples)));

    int idx = 0;
    for (const auto& track : result_.tracks)
        for (const auto& step : track.steps) {
            if (idx % sample_stride == 0)
                vals.push_back(scalarForStep(step));
            ++idx;
        }

    if (vals.empty()) { color_min_ = 0.0; color_max_ = 1.0; return; }

    std::sort(vals.begin(), vals.end());
    const auto n = static_cast<int>(vals.size());
    const auto lo = static_cast<std::size_t>(std::max(0, static_cast<int>(n * 0.01)));
    const auto hi = static_cast<std::size_t>(std::min(n - 1, static_cast<int>(n * 0.99)));
    color_min_ = vals[lo];
    color_max_ = vals[hi];

    if (color_min_ >= color_max_) { color_min_ = vals.front(); color_max_ = vals.back(); }
    if (color_min_ >= color_max_) { color_min_ = 0.0; color_max_ = 1.0; }

    emit colorRangeChanged(color_min_, color_max_, colorUnit());
}

// ---------------------------------------------------------------------------
// Camera / projection
//
// Orthographic projection after rotating by azimuth (Y) and elevation (X).
// World coords are in mm; projection maps to widget pixels with zoom.
// ---------------------------------------------------------------------------

QPointF BioViewport3D::project(float x_mm, float y_mm, float z_mm) const
{
    // Centre the scene.
    x_mm -= scene_cx_;
    y_mm -= scene_cy_;
    z_mm -= scene_cz_;

    const float az  = cam_.azimuth  * static_cast<float>(M_PI) / 180.0f;
    const float el  = cam_.elevation * static_cast<float>(M_PI) / 180.0f;

    // Rotate about Y (azimuth).
    const float x1 =  x_mm * std::cos(az) + z_mm * std::sin(az);
    const float y1 =  y_mm;
    const float z1 = -x_mm * std::sin(az) + z_mm * std::cos(az);

    // Rotate about X (elevation).
    const float x2 =  x1;
    const float y2 =  y1 * std::cos(el) - z1 * std::sin(el);

    // Scale to widget.
    const float W  = static_cast<float>(width());
    const float H  = static_cast<float>(height());
    const float scale = cam_.zoom * (std::min(W, H) * 0.5f) / scene_radius_;

    return {
        W * 0.5f + x2 * scale + cam_.pan_x,
        H * 0.5f - y2 * scale + cam_.pan_y
    };
}

// ---------------------------------------------------------------------------
// Picking
// ---------------------------------------------------------------------------

std::optional<BioViewport3D::PickResult> BioViewport3D::pick(
    const QPoint& pixel) const
{
    if (!has_result_) return std::nullopt;

    constexpr float kPickRadius = 12.0f;
    float best_dist = kPickRadius * kPickRadius;
    int best_t = -1, best_s = -1;

    for (int ti = 0; ti < static_cast<int>(result_.tracks.size()); ++ti) {
        const auto& track = result_.tracks[static_cast<std::size_t>(ti)];
        for (int si = 0; si < static_cast<int>(track.steps.size()); ++si) {
            const auto& step = track.steps[static_cast<std::size_t>(si)];
            const QPointF p = project(
                static_cast<float>(step.x_m * 1000.0),
                static_cast<float>(step.y_m * 1000.0),
                static_cast<float>(step.z_m * 1000.0));
            const float dx = static_cast<float>(p.x()) - static_cast<float>(pixel.x());
            const float dy = static_cast<float>(p.y()) - static_cast<float>(pixel.y());
            const float d2 = dx*dx + dy*dy;
            if (d2 < best_dist) { best_dist = d2; best_t = ti; best_s = si; }
        }
    }

    if (best_t < 0) return std::nullopt;
    return PickResult{best_t, best_s, std::sqrt(best_dist)};
}

// ---------------------------------------------------------------------------
// Spark particles
// ---------------------------------------------------------------------------

static uint32_t xorshift32(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void BioViewport3D::spawnSparks(const QPointF& origin, const QColor& color)
{
    static uint32_t rng_state = 0xDEADBEEF;
    constexpr int kCount = 20;
    sparks_.reserve(sparks_.size() + kCount);
    for (int i = 0; i < kCount; ++i) {
        const float angle = static_cast<float>(xorshift32(rng_state)) /
                            4294967295.0f * 2.0f * static_cast<float>(M_PI);
        const float speed = 1.5f + static_cast<float>(xorshift32(rng_state)) /
                            4294967295.0f * 4.0f;
        sparks_.push_back({
            origin,
            {std::cos(angle) * speed, std::sin(angle) * speed},
            1.0f,
            color
        });
    }
    spark_timer_->start();
}

void BioViewport3D::tickSparks()
{
    for (auto& sp : sparks_) {
        sp.pos   += sp.vel;
        sp.vel.ry() += 0.12;  // gravity
        sp.life  -= 0.035f;
    }
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(),
                                  [](const Spark& s){ return s.life <= 0.0f; }),
                  sparks_.end());
    if (sparks_.empty()) spark_timer_->stop();
}

void BioViewport3D::drawSparks(QPainter& p) const
{
    for (const auto& sp : sparks_) {
        QColor c = sp.color;
        c.setAlphaF(static_cast<float>(sp.life));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const double r = 3.0 * sp.life;
        p.drawEllipse(sp.pos, r, r);
    }
}

// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------

void BioViewport3D::drawAxes(QPainter& p) const
{
    const float len = scene_radius_ * 0.3f;
    const QPointF O = project(0, 0, 0);
    struct Axis { float dx, dy, dz; QColor col; const char* label; };
    const Axis axes[] = {
        {len, 0, 0, {220, 60, 60},  "X"},
        {0, len, 0, {60, 200, 60},  "Y"},
        {0, 0, len, {60, 120, 220}, "Z"},
    };
    for (const auto& ax : axes) {
        const QPointF tip = project(ax.dx, ax.dy, ax.dz);
        p.setPen(QPen(ax.col, 2));
        p.drawLine(O, tip);
        p.setPen(ax.col);
        QFont f = p.font(); f.setBold(true); f.setPointSizeF(8); p.setFont(f);
        p.drawText(tip + QPointF(4, -4), QString::fromLatin1(ax.label));
    }
}

void BioViewport3D::drawTrajectories(QPainter& p) const
{
    if (!has_result_) return;

    const double span = (color_max_ > color_min_)
                        ? (color_max_ - color_min_) : 1.0;

    for (int ti = 0; ti < static_cast<int>(result_.tracks.size()); ++ti) {
        const auto& track = result_.tracks[static_cast<std::size_t>(ti)];
        if (track.steps.size() < 2) continue;

        const bool is_picked = (ti == picked_track_);
        const bool dim = (picked_track_ >= 0 && !is_picked);
        const float base_alpha = dim ? 0.15f : 1.0f;

        for (int si = render_stride_; si < static_cast<int>(track.steps.size()); si += render_stride_) {
            const auto& s0 = track.steps[static_cast<std::size_t>(si - render_stride_)];
            const auto& s1 = track.steps[static_cast<std::size_t>(si)];

            const QPointF p0 = project(
                static_cast<float>(s0.x_m * 1000.0),
                static_cast<float>(s0.y_m * 1000.0),
                static_cast<float>(s0.z_m * 1000.0));
            const QPointF p1 = project(
                static_cast<float>(s1.x_m * 1000.0),
                static_cast<float>(s1.y_m * 1000.0),
                static_cast<float>(s1.z_m * 1000.0));

            const double t = (scalarForStep(s1) - color_min_) / span;
            QColor col = mapper_.map(t, palette_);
            col.setAlphaF(static_cast<float>(base_alpha));

            const float lw = is_picked ? 2.5f : 1.5f;
            p.setPen(QPen(col, lw, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(p0, p1);
        }

        // Picked step highlight dot.
        if (is_picked && picked_step_ >= 0 &&
            picked_step_ < static_cast<int>(track.steps.size()))
        {
            const auto& step = track.steps[static_cast<std::size_t>(picked_step_)];
            const QPointF pp = project(
                static_cast<float>(step.x_m * 1000.0),
                static_cast<float>(step.y_m * 1000.0),
                static_cast<float>(step.z_m * 1000.0));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 100, 220));
            p.drawEllipse(pp, 5.0, 5.0);
        }
    }
}

void BioViewport3D::drawSlabs(QPainter& p) const
{
    // Draw each slab as a translucent quad.
    // For simplicity we project the 8 corners of the bounding box.
    for (const auto& slab : slabs_) {
        if (!slab.enabled) continue;

        const float z0_mm = static_cast<float>(slab.axial_start_m * 1000.0);
        const float z1_mm = static_cast<float>((slab.axial_start_m +
                                                 slab.thickness_m) * 1000.0);

        // Half-widths (use transverse dims; fallback to scene radius for Infinite).
        float hx = (slab.shape != BioSlab::Shape::Infinite)
                   ? static_cast<float>(slab.width_m * 500.0)   // width_m → half in mm
                   : scene_radius_ * 0.8f;
        float hy = (slab.shape != BioSlab::Shape::Infinite)
                   ? static_cast<float>(slab.height_m * 500.0)
                   : scene_radius_ * 0.8f;

        // Clamp to avoid giant quads.
        hx = std::min(hx, scene_radius_ * 1.5f);
        hy = std::min(hy, scene_radius_ * 1.5f);

        const float cx = static_cast<float>(slab.center_x_m * 1000.0);
        const float cy = static_cast<float>(slab.center_y_m * 1000.0);

        // 8 corners.
        const float xs[2] = {cx - hx, cx + hx};
        const float ys[2] = {cy - hy, cy + hy};
        const float zs[2] = {z0_mm, z1_mm};

        // Project front and back faces (z0 and z1).
        // Unpack 0xRRGGBBAA colour.
        const auto rgba = slab.color_rgba;
        const int cr = static_cast<int>((rgba >> 24) & 0xFFu);
        const int cg = static_cast<int>((rgba >> 16) & 0xFFu);
        const int cb = static_cast<int>((rgba >>  8) & 0xFFu);

        for (int face = 0; face < 2; ++face) {
            const float fz = zs[face];
            QPolygonF quad;
            quad << project(xs[0], ys[0], fz)
                 << project(xs[1], ys[0], fz)
                 << project(xs[1], ys[1], fz)
                 << project(xs[0], ys[1], fz);

            QColor fill(cr, cg, cb, 40);
            QColor edge(cr, cg, cb, 180);
            p.setBrush(fill);
            p.setPen(QPen(edge, 1));
            p.drawPolygon(quad);
        }

        // Four lateral edges.
        const int corners[4][2] = {{0,0},{1,0},{1,1},{0,1}};
        QColor edge(cr, cg, cb, 150);
        p.setPen(QPen(edge, 1, Qt::DashLine));
        for (const auto& c : corners) {
            p.drawLine(project(xs[c[0]], ys[c[1]], z0_mm),
                       project(xs[c[0]], ys[c[1]], z1_mm));
        }

        // Label at the front face top-right.
        p.setPen(QColor(cr, cg, cb));
        QFont f = p.font(); f.setPointSizeF(7.5); p.setFont(f);
        const QPointF label_pt = project(xs[1], ys[1], z0_mm) + QPointF(4, -4);
        p.drawText(label_pt, QString::fromStdString(slab.label));
    }
}

void BioViewport3D::drawGlow(QPainter& p)
{
    // Skip the glow pass when data is decimated: it would look wrong with
    // wide-stroke lines spanning many skipped steps, and it's the most
    // expensive pass (thick lines at half-res for every segment).
    if (!has_result_ || render_stride_ > 1) return;

    // Render trajectories into an off-screen buffer at half res, then paint
    // back with Lighten composition for the glow effect.
    const int W = width() / 2;
    const int H = height() / 2;
    if (glow_buffer_.width() != W || glow_buffer_.height() != H)
        glow_buffer_ = QImage(W, H, QImage::Format_ARGB32_Premultiplied);

    glow_buffer_.fill(Qt::transparent);
    QPainter gp(&glow_buffer_);
    gp.setRenderHint(QPainter::Antialiasing, false);
    gp.scale(0.5, 0.5);

    const double span = (color_max_ > color_min_) ? (color_max_ - color_min_) : 1.0;
    for (const auto& track : result_.tracks) {
        for (int si = render_stride_; si < static_cast<int>(track.steps.size()); si += render_stride_) {
            const auto& s0 = track.steps[static_cast<std::size_t>(si - render_stride_)];
            const auto& s1 = track.steps[static_cast<std::size_t>(si)];
            const double t = (scalarForStep(s1) - color_min_) / span;
            QColor col = mapper_.map(t, palette_);
            col.setAlpha(80);
            // Wide strokes for glow appearance.
            gp.setPen(QPen(col, 6, Qt::SolidLine, Qt::RoundCap));
            gp.drawLine(
                project(static_cast<float>(s0.x_m * 1000.0),
                         static_cast<float>(s0.y_m * 1000.0),
                         static_cast<float>(s0.z_m * 1000.0)),
                project(static_cast<float>(s1.x_m * 1000.0),
                         static_cast<float>(s1.y_m * 1000.0),
                         static_cast<float>(s1.z_m * 1000.0)));
        }
    }
    gp.end();

    p.setCompositionMode(QPainter::CompositionMode_Lighten);
    p.drawImage(QRectF(0, 0, width(), height()),
                glow_buffer_, glow_buffer_.rect());
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void BioViewport3D::drawHUD(QPainter& p) const
{
    const QString info = QString("Az: %1°  El: %2°  Zoom: %3×  | %4 | %5")
        .arg(static_cast<int>(cam_.azimuth))
        .arg(static_cast<int>(cam_.elevation))
        .arg(cam_.zoom, 0, 'f', 2)
        .arg(EnergyColorMapper::paletteName(palette_))
        .arg(colorUnit());

    QFont f = p.font();
    f.setPointSizeF(8.0);
    p.setFont(f);
    p.setPen(QColor(180, 180, 180, 200));
    p.drawText(QRect(8, height() - 22, width() - 16, 18),
               Qt::AlignLeft | Qt::AlignVCenter, info);
}

// ---------------------------------------------------------------------------
// paintEvent
// ---------------------------------------------------------------------------

void BioViewport3D::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Background gradient.
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(12, 12, 24));
    bg.setColorAt(1.0, QColor(5, 5, 10));
    p.fillRect(rect(), bg);

    if (!has_result_) {
        p.setPen(QColor(100, 100, 100));
        QFont f; f.setPointSizeF(11); p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("No simulation result loaded.\n"
                                  "Run BioSimRunner and call setResult()."));
        return;
    }

    drawSlabs(p);
    drawGlow(p);
    drawTrajectories(p);
    drawAxes(p);
    drawSparks(p);
    drawHUD(p);
}

// ---------------------------------------------------------------------------
// Mouse / wheel
// ---------------------------------------------------------------------------

void BioViewport3D::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // First try to pick a trajectory.
        auto hit = pick(event->pos());
        if (hit) {
            picked_track_ = hit->track_idx;
            picked_step_  = hit->step_idx;

            const auto& step =
                result_.tracks[static_cast<std::size_t>(hit->track_idx)]
                        .steps[static_cast<std::size_t>(hit->step_idx)];
            const QPointF pp = project(
                static_cast<float>(step.x_m * 1000.0),
                static_cast<float>(step.y_m * 1000.0),
                static_cast<float>(step.z_m * 1000.0));

            const double t = (color_max_ > color_min_)
                ? (scalarForStep(step) - color_min_) / (color_max_ - color_min_)
                : 0.5;
            spawnSparks(pp, mapper_.map(t, palette_));
            emit stepPicked(hit->track_idx, hit->step_idx);
        } else {
            // Begin orbit drag.
            dragging_     = true;
            drag_start_   = event->pos();
            drag_az_start_ = cam_.azimuth;
            drag_el_start_ = cam_.elevation;
        }
    } else if (event->button() == Qt::MiddleButton) {
        dragging_     = true;
        drag_start_   = event->pos();
        drag_az_start_ = cam_.pan_x;
        drag_el_start_ = cam_.pan_y;
    }
    update();
}

void BioViewport3D::mouseMoveEvent(QMouseEvent* event)
{
    if (!dragging_) return;

    const QPoint delta = event->pos() - drag_start_;
    if (event->buttons() & Qt::LeftButton) {
        cam_.azimuth   = drag_az_start_ + static_cast<float>(delta.x()) * 0.5f;
        cam_.elevation = std::clamp(drag_el_start_ + static_cast<float>(delta.y()) * 0.3f,
                                    -89.0f, 89.0f);
    } else if (event->buttons() & Qt::MiddleButton) {
        cam_.pan_x = drag_az_start_ + static_cast<float>(delta.x());
        cam_.pan_y = drag_el_start_ + static_cast<float>(delta.y());
    }
    update();
}

void BioViewport3D::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    dragging_ = false;
}

void BioViewport3D::wheelEvent(QWheelEvent* event)
{
    const float factor = (event->angleDelta().y() > 0) ? 1.1f : 0.9f;
    cam_.zoom = std::clamp(cam_.zoom * factor, 0.05f, 50.0f);
    update();
}

void BioViewport3D::resizeEvent(QResizeEvent* /*event*/)
{
    glow_buffer_ = QImage{}; // invalidate glow cache on resize
}

} // namespace beamlab::biosim
