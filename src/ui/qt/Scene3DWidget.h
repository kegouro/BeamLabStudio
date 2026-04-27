#pragma once

#include <QColor>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <vector>

class QPainter;

namespace beamlab::ui {

class Scene3DWidget final : public QWidget {
public:
    explicit Scene3DWidget(QWidget* parent = nullptr);

    void clearLayers();

    int addObjLayer(const QString& name,
                    const QString& path,
                    const QColor& color);

    bool removeLayer(int layer_index);
    void setLayerVisible(int layer_index, bool visible);
    void setLayerColor(int layer_index, const QColor& color);
    void setLayerMaxPolylines(int layer_index, int max_polylines);
    void setLayerOpacity(int layer_index, double opacity);
    void setLayerLineWidth(int layer_index, double width);
    void setTrajectoryParameter(double lambda);
    void setAxesVisible(bool visible);
    void setMeasureGuidesVisible(bool visible);

    int layerPolylineCount(int layer_index) const;
    bool axesVisible() const;
    bool measureGuidesVisible() const;

    void resetCamera();
    void setViewPreset(int preset);
    void frameLongestAxisHorizontally();

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

    struct Polyline {
        std::vector<int> indices{};
    };

    struct CameraBasis {
        Vec3 right{1.0, 0.0, 0.0};
        Vec3 up{0.0, 1.0, 0.0};
        Vec3 forward{0.0, 0.0, 1.0};
    };

    struct Layer {
        QString name{};
        QString path{};
        QString status{};
        std::vector<Vec3> vertices{};
        std::vector<Face> faces{};
        std::vector<Polyline> polylines{};
        bool visible{true};
        QColor color{120, 170, 255};
        double opacity{1.0};
        double line_width{1.0};
        int max_polylines{-1};
    };

    bool parseObjIntoLayer(const QString& path, Layer& layer) const;
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

    std::vector<Layer> layers_{};

    Vec3 center_{};
    Vec3 bounds_min_{};
    Vec3 bounds_max_{};
    double radius_{1.0};

    CameraBasis camera_{};
    double zoom_{0.92};
    QPointF pan_{0.0, 0.0};

    QPoint last_mouse_pos_{};
    double trajectory_parameter_{1.0};
    bool axes_visible_{true};
    bool measure_guides_visible_{true};
};

} // namespace beamlab::ui
