#pragma once

#include <QGraphicsView>
#include <QPoint>

class QGraphicsScene;

namespace beamlab::ui {

class InteractiveGraphicsView final : public QGraphicsView {
public:
    explicit InteractiveGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr);

    void resetView();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPoint last_pan_pos_{};
    bool panning_{false};
};

} // namespace beamlab::ui
