#pragma once

#include "biosim/ui/qt/EnergyColorMapper.h"

#include <QPoint>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <utility>
#include <vector>

class QPainter;

namespace beamlab::ui {

class ObjViewerWidget final : public QWidget {
    Q_OBJECT
public:
    explicit ObjViewerWidget(QWidget* parent = nullptr);

    bool loadObj(const QString& path);
    // Load per-vertex kinE_MeV from trajectories_preview.csv (row order must
    // match OBJ vertex order, which holds when both are exported with the same
    // max_trajectories / max_samples parameters).
    void loadEnergyCSV(const QString& path);
    void resetCamera();
    void setViewPreset(int preset);
    void frameLongestAxisHorizontally();
    void setAxesVisible(bool visible);
    void setMeasureGuidesVisible(bool visible);
    bool axesVisible() const;
    bool measureGuidesVisible() const;

    void setEnergyGradientEnabled(bool enabled);
    bool energyGradientEnabled() const;

    // Select the active energy colour palette (default: BraggPeak).
    void setActivePalette(beamlab::biosim::EnergyColorMapper::Palette palette);

    // Limit the number of polylines ('l' OBJ entries) rendered.
    // -1 = render all (default). 0 = render none.
    void setMaxPolylines(int max);
    int polylineCount() const;

signals:
    // Emitted on a click (no drag) when energy gradient is active.
    void energyPicked(double kinE_MeV, QPointF screen_pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    struct Vec3 {
        double x{0.0};
        double y{0.0};
        double z{0.0};
    };

    struct Face {
        std::vector<int> indices{};
    };

    struct CameraBasis {
        Vec3 right{1.0, 0.0, 0.0};
        Vec3 up{0.0, 1.0, 0.0};
        Vec3 forward{0.0, 0.0, 1.0};
    };

    QPointF project(const Vec3& p) const;
    Vec3 rotate(const Vec3& p) const;
    void updateBounds();
    void applyFreeRotation(double yaw_delta, double pitch_delta);
    void applyRoll(double roll_delta);
    void setCameraFromYawPitch(double yaw, double pitch);
    void normalizeCameraBasis();
    void drawAxisTriad(QPainter& painter,
                       const QRectF& viewport,
                       const QRectF& content_bounds) const;
    void drawDimensionGuides(QPainter& painter,
                             const std::vector<Vec3>& bounds_corners,
                             double scale,
                             double center_y,
                             const QRectF& content_bounds) const;
    void drawEnergyScaleBar(QPainter& painter,
                            double top_reserved,
                            double available_h) const;
    [[nodiscard]] QColor energyToColor(double t) const;

    struct Polyline {
        std::vector<int> indices{};
    };

    std::vector<Vec3> vertices_{};
    std::vector<Face> faces_{};
    std::vector<Polyline> polylines_{};
    int max_polylines_{-1};

    // Per-vertex kinetic energy for gradient coloring (parallel to vertices_).
    std::vector<double> vertex_energies_{};
    double energy_min_{0.0};
    double energy_max_{1.0};
    bool energy_gradient_{false};

    beamlab::biosim::EnergyColorMapper color_mapper_{};
    beamlab::biosim::EnergyColorMapper::Palette active_palette_{
        beamlab::biosim::EnergyColorMapper::Palette::BraggPeak};

    Vec3 center_{};
    Vec3 bounds_min_{};
    Vec3 bounds_max_{};
    double radius_{1.0};

    CameraBasis camera_{};
    double zoom_{1.0};
    QPointF pan_{0.0, 0.0};

    QPoint last_mouse_pos_{};
    bool mouse_moved_{false};
    QString loaded_path_{};
    QString status_text_{"No OBJ loaded"};
    bool axes_visible_{true};
    bool measure_guides_visible_{true};
};

} // namespace beamlab::ui
