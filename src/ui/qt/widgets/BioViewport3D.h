#pragma once

#include "biosim/core/BioSimResult.h"
#include "biosim/core/BioSimConfig.h"
#include "biosim/geometry/BioSlab.h"
#include "biosim/ui/qt/EnergyColorMapper.h"
#include "ui/qt/dockable/IDockableWidget.h"

#include <QImage>
#include <QPointF>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <optional>
#include <vector>

namespace beamlab::biosim {

// QPainter-based 3D trajectory viewer with:
//   • Orthographic camera (orbit via drag, zoom via wheel)
//   • Coloured trajectories from BioSimResult (per-step EnergyScaleSet)
//   • Translucent slab boxes drawn from BioSimConfig
//   • Click-to-pick: nearest trajectory step highlighted
//   • Glow pass (off-screen blurred overlay) and click-spark particles
//   • Hovering a trajectory dims all others
class BioViewport3D : public QWidget,
                       public beamlab::ui::qt::IDockableWidget {
    Q_OBJECT

public:
    explicit BioViewport3D(QWidget* parent = nullptr);

    // ── IDockableWidget ───────────────────────────────────────────
    QString title() const override
        { return QStringLiteral("Bio Simulator 3D"); }
    QString id() const override
        { return QStringLiteral("bio_viewport3d"); }
    QWidget* widget() override { return this; }
    Qt::DockWidgetArea preferredArea() const override
        { return Qt::CenterDockWidgetArea; }

    // Load simulation result and rebuild display lists.
    void setResult(const BioSimResult& result);

    // Update slab geometry (for live preview while editing slabs).
    void setSlabs(const std::vector<BioSlab>& slabs);

    // Choose which energy field colours the trajectories.
    void setColorBy(ColorBy field);

    // Switch colour palette.
    void setPalette(EnergyColorMapper::Palette palette);

    // Reset camera to default isometric view.
    void resetCamera();

    // Current colour range (min, max) for the active field — drives scale bar.
    [[nodiscard]] std::pair<double, double> colorRange() const;

    // Unit string for the current ColorBy field.
    [[nodiscard]] QString colorUnit() const;

    // Current palette.
    [[nodiscard]] EnergyColorMapper::Palette palette() const { return palette_; }

signals:
    // Emitted when the user clicks a trajectory step.
    // track_idx and step_idx index into BioSimResult::tracks.
    void stepPicked(int track_idx, int step_idx);

    // Emitted when the colour range changes (new result loaded or field changed).
    void colorRangeChanged(double min_val, double max_val, const QString& unit);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // ------------------------------------------------------------------
    // Camera / projection
    // ------------------------------------------------------------------
    struct Camera {
        float azimuth{-30.0f};   // degrees around Y
        float elevation{25.0f};  // degrees up from XZ plane
        float zoom{1.0f};        // scale factor
        float pan_x{0.0f};       // world-space pan offset X
        float pan_y{0.0f};
    } cam_;

    // Project a 3D world point [mm] to widget pixel coordinates.
    QPointF project(float x_mm, float y_mm, float z_mm) const;

    // Unproject a pixel ray to world-Z plane — used for picking.
    // Returns the 3-D point on the nearest track step.
    struct PickResult { int track_idx; int step_idx; float dist_px; };
    std::optional<PickResult> pick(const QPoint& pixel) const;

    // ------------------------------------------------------------------
    // Scene data
    // ------------------------------------------------------------------
    BioSimResult result_{};
    std::vector<BioSlab> slabs_{};
    bool has_result_{false};

    // Bounding box of all trajectory points [mm].
    float scene_cx_{0}, scene_cy_{0}, scene_cz_{0};
    float scene_radius_{1.0f};

    void computeSceneBounds();

    // ------------------------------------------------------------------
    // Rendering
    // ------------------------------------------------------------------
    EnergyColorMapper mapper_;
    EnergyColorMapper::Palette palette_{EnergyColorMapper::Palette::BraggPeak};
    ColorBy color_by_{ColorBy::EdepMeV};

    double color_min_{0.0};
    double color_max_{1.0};

    // Render stride: every N-th step is drawn.  1 = all steps.
    // Raised automatically when total step count would make repaints too slow.
    int render_stride_{1};

    // Get the scalar value for a given step / color_by_ field.
    double scalarForStep(const BioStep& step) const;

    // Recompute color_min_ / color_max_ from current result + color_by_.
    void recomputeColorRange();

    // Draw world axes.
    void drawAxes(QPainter& p) const;

    // Draw all trajectory lines.
    void drawTrajectories(QPainter& p) const;

    // Draw slab boxes (translucent rectangles in 3D).
    void drawSlabs(QPainter& p) const;

    // Draw glow overlay (blurred copy painted with Lighten composition).
    void drawGlow(QPainter& p);
    QImage glow_buffer_{};

    // Draw HUD: camera info + palette name.
    void drawHUD(QPainter& p) const;

    // ------------------------------------------------------------------
    // Interaction state
    // ------------------------------------------------------------------
    bool dragging_{false};
    QPoint drag_start_{};
    float drag_az_start_{0};
    float drag_el_start_{0};

    // Highlighted track/step (-1 = none).
    int hover_track_{-1};
    int picked_track_{-1};
    int picked_step_{-1};

    // ------------------------------------------------------------------
    // Click-spark particle system
    // ------------------------------------------------------------------
    struct Spark {
        QPointF pos;    // widget px
        QPointF vel;    // px/frame
        float life;     // [0,1]
        QColor color;
    };
    std::vector<Spark> sparks_{};
    QTimer* spark_timer_{nullptr};

    void spawnSparks(const QPointF& origin, const QColor& color);
    void tickSparks();
    void drawSparks(QPainter& p) const;
};

} // namespace beamlab::biosim
