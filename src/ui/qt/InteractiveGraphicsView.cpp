#include "ui/qt/InteractiveGraphicsView.h"

#include <QGraphicsScene>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>

namespace beamlab::ui {

InteractiveGraphicsView::InteractiveGraphicsView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void InteractiveGraphicsView::resetView()
{
    QGraphicsScene* current_scene = scene();

    if (current_scene == nullptr) {
        return;
    }

    QRectF bounds = current_scene->sceneRect();

    if (!bounds.isValid() || bounds.isEmpty()) {
        bounds = current_scene->itemsBoundingRect();

        if (bounds.isValid() && !bounds.isEmpty()) {
            const double pad_x = std::max(25.0, bounds.width() * 0.08);
            const double pad_y = std::max(25.0, bounds.height() * 0.08);
            bounds = bounds.adjusted(-pad_x, -pad_y, pad_x, pad_y);
        }
    }

    if (!bounds.isValid() || bounds.isEmpty()) {
        return;
    }

    current_scene->setSceneRect(bounds);
    resetTransform();
    fitInView(bounds, Qt::KeepAspectRatio);
}

void InteractiveGraphicsView::wheelEvent(QWheelEvent* event)
{
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    const double factor =
        event->angleDelta().y() > 0
            ? 1.15
            : 1.0 / 1.15;

    scale(factor, factor);
    event->accept();
}

void InteractiveGraphicsView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        panning_ = true;
        last_pan_pos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void InteractiveGraphicsView::mouseMoveEvent(QMouseEvent* event)
{
    if (panning_) {
        const QPoint delta = event->pos() - last_pan_pos_;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        last_pan_pos_ = event->pos();
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void InteractiveGraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && panning_) {
        panning_ = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

} // namespace beamlab::ui
