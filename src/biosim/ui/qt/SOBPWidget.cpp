// SOBPWidget — Spread-Out Bragg Peak interactive panel.
//
// Rendering uses QPainter directly (no QtCharts dependency).
// The physics (SOBPCalculator) is pure C++ — see SOBPCalculator.h.

#include "biosim/ui/qt/SOBPWidget.h"

#include <QDoubleSpinBox>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

// ── SOBPPlotCanvas ────────────────────────────────────────────────────────────

SOBPPlotCanvas::SOBPPlotCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(400, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(false);
}

void SOBPPlotCanvas::setResult(const SOBPResult& result)
{
    result_    = result;
    has_result_ = result.valid && !result.depth_grid_cm.empty();
    update();
}

void SOBPPlotCanvas::clearResult()
{
    has_result_ = false;
    update();
}

QPointF SOBPPlotCanvas::toPixel(const double depth_cm, const double dose,
                                  const QRectF& pr,
                                  const double x_min, const double x_max,
                                  const double y_min, const double y_max) const
{
    const double dx = (x_max > x_min) ? (depth_cm - x_min) / (x_max - x_min) : 0.0;
    const double dy = (y_max > y_min) ? (dose     - y_min) / (y_max - y_min) : 0.0;
    return {
        pr.left() + dx * pr.width(),
        pr.bottom() - dy * pr.height()  // Y flipped: dose 0 at bottom
    };
}

void SOBPPlotCanvas::drawAxes(QPainter& p, const QRectF& pr,
                               const double x_min, const double x_max,
                               const double y_min, const double y_max) const
{
    const QPen axis_pen(QColor(130, 145, 165), 1);
    const QPen tick_pen(QColor(100, 115, 135), 1);
    const QFont label_font = []{
        QFont f;
        f.setPointSizeF(8.5);
        return f;
    }();
    const QFontMetrics fm(label_font);

    p.setPen(axis_pen);
    p.setFont(label_font);

    // Background fill
    p.fillRect(pr, QColor(13, 17, 26));

    // Axes lines
    p.drawLine(pr.bottomLeft(), pr.bottomRight());  // X axis
    p.drawLine(pr.bottomLeft(), pr.topLeft());       // Y axis

    // ── X ticks ────────────────────────────────────────────────────────────
    // Choose ~6 ticks in [x_min, x_max] at "nice" intervals.
    const int n_x_ticks = 6;
    const double x_step = (x_max - x_min) / static_cast<double>(n_x_ticks);
    p.setPen(tick_pen);

    for (int i = 0; i <= n_x_ticks; ++i) {
        const double x_val = x_min + static_cast<double>(i) * x_step;
        const QPointF pt = toPixel(x_val, y_min, pr, x_min, x_max, y_min, y_max);
        // Tick mark
        p.drawLine(QPointF(pt.x(), pr.bottom()),
                   QPointF(pt.x(), pr.bottom() + 4));
        // Light grid line
        p.setPen(QPen(QColor(25, 32, 46), 1, Qt::DotLine));
        p.drawLine(QPointF(pt.x(), pr.top()),
                   QPointF(pt.x(), pr.bottom()));
        p.setPen(tick_pen);
        // Tick label
        const QString lbl = QString::number(x_val, 'f', 1);
        const QRect tr = fm.boundingRect(lbl);
        p.setPen(QColor(140, 155, 175));
        p.drawText(QPointF(pt.x() - tr.width() / 2.0, pr.bottom() + 14), lbl);
        p.setPen(tick_pen);
    }

    // X axis label
    p.setPen(QColor(180, 195, 210));
    {
        QFont axis_label_font = label_font;
        axis_label_font.setPointSizeF(9.0);
        p.setFont(axis_label_font);
        const QString xl = "Depth [cm]";
        const QRect xl_r = QFontMetrics(axis_label_font).boundingRect(xl);
        p.drawText(QPointF(pr.center().x() - xl_r.width() / 2.0,
                           pr.bottom() + 28), xl);
    }

    // ── Y ticks ────────────────────────────────────────────────────────────
    p.setFont(label_font);
    const int n_y_ticks = 5;
    const double y_step = (y_max - y_min) / static_cast<double>(n_y_ticks);

    for (int i = 0; i <= n_y_ticks; ++i) {
        const double y_val = y_min + static_cast<double>(i) * y_step;
        const QPointF pt = toPixel(x_min, y_val, pr, x_min, x_max, y_min, y_max);
        p.setPen(tick_pen);
        p.drawLine(QPointF(pr.left() - 4, pt.y()),
                   QPointF(pr.left(),     pt.y()));
        p.setPen(QPen(QColor(25, 32, 46), 1, Qt::DotLine));
        p.drawLine(QPointF(pr.left(),  pt.y()),
                   QPointF(pr.right(), pt.y()));
        p.setPen(QColor(140, 155, 175));
        const QString lbl = QString::number(y_val, 'f', 1);
        const QRect lr    = fm.boundingRect(lbl);
        p.drawText(QPointF(pr.left() - 6 - lr.width(), pt.y() + lr.height() / 3.0), lbl);
    }

    // Y axis label (rotated)
    {
        p.save();
        QFont axis_label_font = label_font;
        axis_label_font.setPointSizeF(9.0);
        p.setFont(axis_label_font);
        p.setPen(QColor(180, 195, 210));
        p.translate(10.0, pr.center().y());
        p.rotate(-90.0);
        const QString yl = "Relative Dose";
        const QRect yl_r = QFontMetrics(axis_label_font).boundingRect(yl);
        p.drawText(QPointF(-yl_r.width() / 2.0, 0), yl);
        p.restore();
    }
}

void SOBPPlotCanvas::drawPlateau(QPainter& p, const QRectF& pr,
                                   const double x_min, const double x_max,
                                   const double y_min, const double y_max,
                                   const double prox, const double dist) const
{
    // Shaded band from dose=0 to dose=1.05 in [proximal, distal].
    const QPointF tl = toPixel(prox, 1.05, pr, x_min, x_max, y_min, y_max);
    const QPointF br = toPixel(dist, 0.0,  pr, x_min, x_max, y_min, y_max);
    QRectF band(tl, br);
    band = band.normalized();
    band = band.intersected(pr);
    if (!band.isValid()) return;

    // Semi-transparent teal fill
    p.fillRect(band, QColor(40, 200, 160, 30));

    // Dashed boundary lines
    QPen border_pen(QColor(60, 210, 170, 160), 1, Qt::DashLine);
    p.setPen(border_pen);
    // Proximal vertical
    const QPointF pt_lo = toPixel(prox, y_max, pr, x_min, x_max, y_min, y_max);
    const QPointF pb_lo = toPixel(prox, y_min, pr, x_min, x_max, y_min, y_max);
    p.drawLine(pt_lo, pb_lo);
    // Distal vertical
    const QPointF pt_hi = toPixel(dist, y_max, pr, x_min, x_max, y_min, y_max);
    const QPointF pb_hi = toPixel(dist, y_min, pr, x_min, x_max, y_min, y_max);
    p.drawLine(pt_hi, pb_hi);
}

void SOBPPlotCanvas::drawCurves(QPainter& p, const QRectF& pr,
                                  const double x_min, const double x_max,
                                  const double y_min, const double y_max) const
{
    if (!has_result_ || result_.depth_grid_cm.empty()) return;

    const std::size_t n = result_.depth_grid_cm.size();

    auto buildPath = [&](const std::vector<double>& dose) {
        QPainterPath path;
        bool started = false;
        for (std::size_t i = 0; i < n; ++i) {
            const QPointF pt = toPixel(result_.depth_grid_cm[i], dose[i],
                                        pr, x_min, x_max, y_min, y_max);
            if (!started) { path.moveTo(pt); started = true; }
            else            path.lineTo(pt);
        }
        return path;
    };

    // ── Individual Bragg curves (tenue gray) ─────────────────────────────
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPen indiv_pen(QColor(110, 125, 145, 130), 1);
    p.setPen(indiv_pen);

    for (const auto& comp : result_.components) {
        if (comp.weighted_dose.size() != n) continue;
        p.drawPath(buildPath(comp.weighted_dose));
    }

    // ── SOBP sum (vivid blue) ─────────────────────────────────────────────
    const QPen sobp_pen(QColor(74, 158, 255), 2);
    p.setPen(sobp_pen);
    p.drawPath(buildPath(result_.sobp_dose));
}

void SOBPPlotCanvas::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Whole-widget background
    p.fillRect(rect(), QColor(13, 17, 26));

    if (!has_result_) {
        // Empty state
        p.setPen(QColor(80, 95, 115));
        QFont f;
        f.setPointSizeF(10.5);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter,
                   "Configure parameters and press Calcular");
        return;
    }

    // Margins: left=46, right=14, top=16, bottom=44 (room for labels).
    constexpr double M_L = 52.0;
    constexpr double M_R = 16.0;
    constexpr double M_T = 16.0;
    constexpr double M_B = 44.0;

    const QRectF plot_rect(M_L,
                            M_T,
                            static_cast<double>(width())  - M_L - M_R,
                            static_cast<double>(height()) - M_T - M_B);

    if (!plot_rect.isValid()) return;

    // Data range
    const double x_max_val = result_.depth_grid_cm.empty() ? 1.0 :
                              result_.depth_grid_cm.back() * 1.02;
    const double x_min_val = 0.0;
    const double y_min_val = 0.0;
    const double y_max_val = 1.15;

    drawPlateau(p, plot_rect,
                x_min_val, x_max_val, y_min_val, y_max_val,
                result_.proximal_cm, result_.distal_cm);

    drawAxes(p, plot_rect,
             x_min_val, x_max_val, y_min_val, y_max_val);

    drawCurves(p, plot_rect,
               x_min_val, x_max_val, y_min_val, y_max_val);

    // ── Legend ─────────────────────────────────────────────────────────────
    {
        QFont lf;
        lf.setPointSizeF(8.5);
        p.setFont(lf);

        const double lx = M_L + 8.0;
        double ly = M_T + 12.0;
        const double swatch_w = 14.0;
        const double swatch_h = 3.0;

        // Individual peaks
        p.fillRect(QRectF(lx, ly - 1.5, swatch_w, swatch_h),
                   QColor(110, 125, 145, 160));
        p.setPen(QColor(140, 155, 175));
        p.drawText(QPointF(lx + swatch_w + 4.0, ly + 4.0),
                   QString("Individual peaks (%1)").arg(
                       static_cast<int>(result_.components.size())));
        ly += 14.0;

        // SOBP
        p.fillRect(QRectF(lx, ly - 1.5, swatch_w, 2.5), QColor(74, 158, 255));
        p.setPen(QColor(74, 158, 255));
        p.drawText(QPointF(lx + swatch_w + 4.0, ly + 4.0), "SOBP");
        ly += 14.0;

        // Plateau
        p.fillRect(QRectF(lx, ly - 4.0, swatch_w, 8.0),
                   QColor(40, 200, 160, 50));
        p.setPen(QColor(60, 210, 170, 200));
        p.drawText(QPointF(lx + swatch_w + 4.0, ly + 4.0), "Target plateau");
    }
}

// ── SOBPWidget ────────────────────────────────────────────────────────────────

SOBPWidget::SOBPWidget(QWidget* parent)
    : QWidget(parent)
{
    setupLayout();
    connectSignals();
}

void SOBPWidget::setupLayout()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    // ── Controls row ─────────────────────────────────────────────────────────
    auto* ctrl_row = new QHBoxLayout();
    ctrl_row->setSpacing(8);

    // Max energy
    auto* e_label = new QLabel("E_max [MeV]", this);
    e_label->setStyleSheet("color:#B8C8DC; font-size:9px;");
    energy_max_spin_ = new QDoubleSpinBox(this);
    energy_max_spin_->setRange(50.0, 350.0);
    energy_max_spin_->setSingleStep(10.0);
    energy_max_spin_->setValue(150.0);
    energy_max_spin_->setDecimals(1);
    energy_max_spin_->setToolTip("Maximum proton kinetic energy [MeV]. "
                                  "Sets the depth of the deepest Bragg peak.");

    // Proximal
    auto* prox_label = new QLabel("Proximal [cm]", this);
    prox_label->setStyleSheet("color:#B8C8DC; font-size:9px;");
    proximal_spin_ = new QDoubleSpinBox(this);
    proximal_spin_->setRange(0.1, 35.0);
    proximal_spin_->setSingleStep(0.5);
    proximal_spin_->setValue(10.0);
    proximal_spin_->setDecimals(1);
    proximal_spin_->setToolTip("Proximal edge of the target plateau [cm].");

    // Distal
    auto* dist_label = new QLabel("Distal [cm]", this);
    dist_label->setStyleSheet("color:#B8C8DC; font-size:9px;");
    distal_spin_ = new QDoubleSpinBox(this);
    distal_spin_->setRange(0.2, 40.0);
    distal_spin_->setSingleStep(0.5);
    distal_spin_->setValue(15.0);
    distal_spin_->setDecimals(1);
    distal_spin_->setToolTip("Distal edge of the target plateau [cm]. "
                              "Should not exceed CSDA range of E_max.");

    // N peaks
    auto* n_label = new QLabel("N peaks", this);
    n_label->setStyleSheet("color:#B8C8DC; font-size:9px;");
    n_peaks_spin_ = new QSpinBox(this);
    n_peaks_spin_->setRange(2, 30);
    n_peaks_spin_->setValue(10);
    n_peaks_spin_->setToolTip("Number of mono-energetic Bragg peaks to superpose. "
                               "More peaks give a flatter plateau.");

    // Button
    calc_btn_ = new QPushButton("Calcular", this);
    calc_btn_->setToolTip("Compute and display the SOBP.");
    calc_btn_->setObjectName("PrimaryButton");

    ctrl_row->addWidget(e_label);
    ctrl_row->addWidget(energy_max_spin_);
    ctrl_row->addWidget(prox_label);
    ctrl_row->addWidget(proximal_spin_);
    ctrl_row->addWidget(dist_label);
    ctrl_row->addWidget(distal_spin_);
    ctrl_row->addWidget(n_label);
    ctrl_row->addWidget(n_peaks_spin_);
    ctrl_row->addWidget(calc_btn_);
    ctrl_row->addStretch(1);

    root->addLayout(ctrl_row);

    // ── Plot canvas ──────────────────────────────────────────────────────────
    canvas_ = new SOBPPlotCanvas(this);
    root->addWidget(canvas_, 1);

    // ── Status bar ───────────────────────────────────────────────────────────
    status_label_ = new QLabel("", this);
    status_label_->setStyleSheet("color:#8899AA; font-size:9px; padding:2px 4px;");
    root->addWidget(status_label_);
}

void SOBPWidget::connectSignals()
{
    connect(calc_btn_, &QPushButton::clicked, this, &SOBPWidget::onCalculate);
}

void SOBPWidget::onCalculate()
{
    const double e_max  = energy_max_spin_->value();
    const double prox   = proximal_spin_->value();
    const double dist   = distal_spin_->value();
    const int    n_peak = n_peaks_spin_->value();

    SOBPParams params;
    params.energy_max_MeV = e_max;
    params.proximal_cm    = prox;
    params.distal_cm      = dist;
    params.n_peaks        = n_peak;
    params.depth_step_cm  = 0.02;
    params.curve_n_points = 300;
    params.sigma_E_rel    = 0.01;

    calc_btn_->setEnabled(false);
    status_label_->setText("Computing…");
    status_label_->repaint();

    const SOBPResult result = calc_.compute(params);
    calc_btn_->setEnabled(true);

    if (!result.valid) {
        status_label_->setText(QString("Error: %1").arg(
            QString::fromStdString(result.error)));
        canvas_->clearResult();
        return;
    }

    canvas_->setResult(result);

    const double dev_pct = result.plateau_max_deviation * 100.0;
    const QString flatness_str = dev_pct < 10.0
        ? QString("Plateau flatness: ±%1% — PASS").arg(dev_pct, 0, 'f', 1)
        : QString("Plateau flatness: ±%1% — FAIL (>10%)").arg(dev_pct, 0, 'f', 1);

    status_label_->setText(
        QString("%1  |  N=%2 peaks  |  Emax=%3 MeV  |  [%4–%5 cm]")
            .arg(flatness_str)
            .arg(n_peak)
            .arg(e_max, 0, 'f', 1)
            .arg(prox, 0, 'f', 1)
            .arg(dist, 0, 'f', 1));
}

} // namespace beamlab::biosim
