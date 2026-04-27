#pragma once

#include <QPoint>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <utility>
#include <vector>

class QPainter;

namespace beamlab::ui {

class ObjViewerWidget final : public QWidget {
public:
    explicit ObjViewerWidget(QWidget* parent = nullptr);

    bool loadObj(const QString& path);
    void resetCamera();
    void setViewPreset(int preset);
    void frameLongestAxisHorizontally();
    void setAxesVisible(bool visible);
    void setMeasureGuidesVisible(bool visible);
    bool axesVisible() const;
    bool measureGuidesVisible() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
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

    std::vector<Vec3> vertices_{};
    std::vector<Face> faces_{};
    std::vector<std::pair<int, int>> lines_{};

    Vec3 center_{};
    Vec3 bounds_min_{};
    Vec3 bounds_max_{};
    double radius_{1.0};

    CameraBasis camera_{};
    double zoom_{1.0};
    QPointF pan_{0.0, 0.0};

    QPoint last_mouse_pos_{};
    QString loaded_path_{};
    QString status_text_{"No OBJ loaded"};
    bool axes_visible_{true};
    bool measure_guides_visible_{true};
};

} // namespace beamlab::ui
