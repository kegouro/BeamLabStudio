#include "biosim/ui/qt/EnergyScaleBar.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <cmath>

namespace beamlab::biosim {

EnergyScaleBar::EnergyScaleBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(kTotalWidth);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void EnergyScaleBar::setRange(double min_val, double max_val,
                               const QString& unit)
{
    min_val_    = min_val;
    max_val_    = max_val;
    unit_       = unit;
    cache_dirty_ = true;
    update();
}

void EnergyScaleBar::setPalette(EnergyColorMapper::Palette palette)
{
    palette_     = palette;
    cache_dirty_ = true;
    update();
}

QSize EnergyScaleBar::sizeHint()     const { return {kTotalWidth, 300}; }
QSize EnergyScaleBar::minimumSizeHint() const { return {kTotalWidth, 120}; }

// ---------------------------------------------------------------------------
// Cache
// ---------------------------------------------------------------------------

void EnergyScaleBar::rebuildCache(int bar_h)
{
    gradient_cache_ = QPixmap(kBarWidth, bar_h);
    QPainter p(&gradient_cache_);

    // Draw each pixel row with the corresponding palette colour (top = max).
    for (int y = 0; y < bar_h; ++y) {
        const double t = 1.0 - (y / double(bar_h - 1));
        p.setPen(mapper_.map(t, palette_));
        p.drawLine(0, y, kBarWidth - 1, y);
    }
    cache_dirty_ = false;
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void EnergyScaleBar::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect r = rect();
    p.fillRect(r, QColor(30, 30, 30));

    const int bar_y = kPadTop;
    const int bar_h = r.height() - kPadTop - kPadBottom;
    if (bar_h <= 0) return;

    if (cache_dirty_ || gradient_cache_.height() != bar_h)
        rebuildCache(bar_h);

    // Gradient strip.
    p.drawPixmap(0, bar_y, gradient_cache_);

    // Thin border around the strip.
    p.setPen(QColor(80, 80, 80));
    p.drawRect(0, bar_y, kBarWidth - 1, bar_h - 1);

    // Unit label at top.
    if (!unit_.isEmpty()) {
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPointSizeF(7.5);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 2, r.width(), kPadTop - 2),
                   Qt::AlignHCenter | Qt::AlignVCenter, unit_);
    }

    drawTicks(p, bar_y, bar_h);
}

void EnergyScaleBar::drawTicks(QPainter& p, int bar_y, int bar_h) const
{
    const auto ticks = EnergyColorMapper::niceTicks(min_val_, max_val_, 6);

    QFont f = p.font();
    f.setPointSizeF(7.0);
    f.setBold(false);
    p.setFont(f);
    p.setPen(Qt::white);

    const double span = max_val_ - min_val_;

    for (double v : ticks) {
        const double t = (span > 0.0) ? (v - min_val_) / span : 0.5;
        // y=bar_y is max (t=1), y=bar_y+bar_h is min (t=0)
        const int y = bar_y + static_cast<int>((1.0 - t) * (bar_h - 1));

        // Short tick mark on the right edge of the gradient strip.
        p.setPen(QColor(200, 200, 200));
        p.drawLine(kBarWidth, y, kBarWidth + 3, y);

        // Value label.
        QString label;
        if (std::abs(v) >= 1e4 || (std::abs(v) < 0.01 && v != 0.0))
            label = QString::number(v, 'e', 2);
        else
            label = QString::number(v, 'g', 4);

        p.setPen(Qt::white);
        const QRect tr(kBarWidth + 5, y - 8, kLabelWidth - 5, 16);
        p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter, label);
    }
}

} // namespace beamlab::biosim
