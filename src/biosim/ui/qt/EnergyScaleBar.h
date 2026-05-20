#pragma once

#include "biosim/ui/qt/EnergyColorMapper.h"

#include <QWidget>
#include <QString>

namespace beamlab::biosim {

// Vertical colour scale bar widget.
// Displays the active palette gradient with annotated tick labels.
// Width is fixed at 72 px; height is flexible.
class EnergyScaleBar : public QWidget {
    Q_OBJECT

public:
    explicit EnergyScaleBar(QWidget* parent = nullptr);

    // Set the data range and redraw. Unit string appears at the top.
    void setRange(double min_val, double max_val,
                  const QString& unit = QString());

    // Switch the active colour palette and redraw.
    void setPalette(EnergyColorMapper::Palette palette);

    // Current palette.
    [[nodiscard]] EnergyColorMapper::Palette palette() const { return palette_; }

    // QWidget overrides.
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    EnergyColorMapper mapper_;
    EnergyColorMapper::Palette palette_{EnergyColorMapper::Palette::Viridis};
    double min_val_{0.0};
    double max_val_{1.0};
    QString unit_{};

    // Cached gradient pixmap; rebuilt on resize / palette / range change.
    QPixmap gradient_cache_{};
    bool cache_dirty_{true};

    void rebuildCache(int bar_h);
    void drawTicks(QPainter& p, int bar_y, int bar_h) const;

    static constexpr int kBarWidth   = 18; // gradient strip width (px)
    static constexpr int kLabelWidth = 50; // label area width (px)
    static constexpr int kPadTop     = 20; // room for unit label
    static constexpr int kPadBottom  = 8;
    static constexpr int kTotalWidth = kBarWidth + kLabelWidth + 4;
};

} // namespace beamlab::biosim
