#pragma once

// SOBPWidget — interactive panel for Spread-Out Bragg Peak visualisation.
//
// Layout:
//   ┌──────────────────────────────────────────────────────────────────┐
//   │  [Max Energy MeV] [Proximal cm] [Distal cm] [N peaks] [Calcular] │
//   ├──────────────────────────────────────────────────────────────────┤
//   │                                                                  │
//   │   Custom QPainter plot:                                          │
//   │     · Individual Bragg curves (tenue gray)                       │
//   │     · SOBP sum (vivid blue)                                      │
//   │     · Target plateau band [proximal, distal] (shaded)            │
//   │     · X axis: depth [cm], Y axis: dose relative [0–1]           │
//   │     · Tick marks and numeric labels                              │
//   │                                                                  │
//   └──────────────────────────────────────────────────────────────────┘
//   │  [Status: plateau flatness] │
//   └────────────────────────────┘
//
// The SOBP computation (SOBPCalculator) is pure C++ with no Qt dependency.
// The widget holds the result and repaint()s the canvas on each new result.

#include "biosim/physics/SOBPCalculator.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace beamlab::biosim {

// Canvas widget that renders the SOBP plot via QPainter.
// Separated from SOBPWidget so the paint logic is self-contained.
class SOBPPlotCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit SOBPPlotCanvas(QWidget* parent = nullptr);

    void setResult(const SOBPResult& result);
    void clearResult();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    // Map data coordinates → widget pixels.
    // plot_rect_ is the axis area (excludes margins for labels).
    [[nodiscard]] QPointF toPixel(double depth_cm, double dose,
                                   const QRectF& plot_rect,
                                   double x_min, double x_max,
                                   double y_min, double y_max) const;

    void drawAxes(QPainter& p, const QRectF& plot_rect,
                  double x_min, double x_max,
                  double y_min, double y_max) const;

    void drawPlateau(QPainter& p, const QRectF& plot_rect,
                     double x_min, double x_max,
                     double y_min, double y_max,
                     double prox, double dist) const;

    void drawCurves(QPainter& p, const QRectF& plot_rect,
                    double x_min, double x_max,
                    double y_min, double y_max) const;

    SOBPResult result_{};
    bool has_result_{false};
};


// Main SOBP panel widget.
class SOBPWidget final : public QWidget {
    Q_OBJECT

public:
    explicit SOBPWidget(QWidget* parent = nullptr);

private slots:
    void onCalculate();

private:
    void setupLayout();
    void connectSignals();

    // Controls
    QDoubleSpinBox* energy_max_spin_{nullptr};   // [MeV]
    QDoubleSpinBox* proximal_spin_{nullptr};     // [cm]
    QDoubleSpinBox* distal_spin_{nullptr};       // [cm]
    QSpinBox*       n_peaks_spin_{nullptr};
    QPushButton*    calc_btn_{nullptr};

    // Plot canvas
    SOBPPlotCanvas* canvas_{nullptr};

    // Status line
    QLabel* status_label_{nullptr};

    // Calculator (owns water + proton + BraggPeakCalculator)
    SOBPCalculator  calc_;
};

} // namespace beamlab::biosim
