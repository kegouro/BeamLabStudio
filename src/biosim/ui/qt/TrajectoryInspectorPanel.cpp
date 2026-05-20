#include "biosim/ui/qt/TrajectoryInspectorPanel.h"

#include <QComboBox>
#include <QFont>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <cmath>

namespace beamlab::biosim {

// ---------------------------------------------------------------------------
// Mini energy-profile plot widget (inner implementation)
// ---------------------------------------------------------------------------

class EnergyProfilePlot : public QWidget {
public:
    explicit EnergyProfilePlot(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(90);
        setMaximumHeight(130);
    }

    void setData(const BioTrack* track,
                 std::function<double(const EnergyScaleSet&)> getter,
                 int highlight_step,
                 const QString& unit,
                 EnergyColorMapper* mapper,
                 EnergyColorMapper::Palette palette)
    {
        track_           = track;
        getter_          = std::move(getter);
        highlight_step_  = highlight_step;
        unit_            = unit;
        mapper_          = mapper;
        palette_         = palette;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(18, 18, 28));

        if (!track_ || track_->steps.empty()) return;

        // Collect values.
        std::vector<double> vals;
        vals.reserve(track_->steps.size());
        for (const auto& s : track_->steps)
            vals.push_back(getter_(s.energy));

        const double vmin = *std::min_element(vals.begin(), vals.end());
        const double vmax = *std::max_element(vals.begin(), vals.end());
        const double span = (vmax > vmin) ? (vmax - vmin) : 1.0;

        const int W = width() - 4;
        const int H = height() - 18;  // leave 18 px for x-axis label
        const int N = static_cast<int>(vals.size());

        auto toX = [&](int i) { return 2 + i * W / (N - 1); };
        auto toY = [&](double v) {
            return 2 + static_cast<int>((1.0 - (v - vmin) / span) * (H - 4));
        };

        // Draw curve with colour gradient.
        for (int i = 1; i < N; ++i) {
            const double t = (vals[static_cast<std::size_t>(i)] - vmin) / span;
            QColor col = mapper_->map(t, palette_);
            p.setPen(QPen(col, 1.5));
            p.drawLine(toX(i-1), toY(vals[static_cast<std::size_t>(i-1)]),
                       toX(i),   toY(vals[static_cast<std::size_t>(i)]));
        }

        // Highlighted step vertical line.
        if (highlight_step_ >= 0 && highlight_step_ < N) {
            p.setPen(QPen(QColor(255, 255, 80, 200), 1, Qt::DashLine));
            p.drawLine(toX(highlight_step_), 0,
                       toX(highlight_step_), H);
        }

        // Y-axis labels.
        QFont f; f.setPointSizeF(6.5); p.setFont(f); p.setPen(QColor(160, 160, 160));
        p.drawText(QRect(2, 0, 60, 12), Qt::AlignLeft, QString::number(vmax, 'g', 3));
        p.drawText(QRect(2, H - 10, 60, 12), Qt::AlignLeft, QString::number(vmin, 'g', 3));

        // Unit label bottom-right.
        p.drawText(QRect(W - 50, H + 4, 54, 12), Qt::AlignRight, unit_);
    }

private:
    const BioTrack* track_{nullptr};
    std::function<double(const EnergyScaleSet&)> getter_;
    int highlight_step_{-1};
    QString unit_;
    EnergyColorMapper* mapper_{nullptr};
    EnergyColorMapper::Palette palette_{};
};

// ---------------------------------------------------------------------------
// TrajectoryInspectorPanel
// ---------------------------------------------------------------------------

TrajectoryInspectorPanel::TrajectoryInspectorPanel(QWidget* parent)
    : QWidget(parent)
{
    buildLayout();
}

void TrajectoryInspectorPanel::buildLayout()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // --- Header ---
    header_label_ = new QLabel(QStringLiteral("No track selected"), this);
    header_label_->setStyleSheet(QStringLiteral(
        "QLabel { background: #1a1a2e; color: #aaa; padding: 4px; "
        "border-radius: 3px; font-size: 10px; }"));
    header_label_->setWordWrap(true);
    root->addWidget(header_label_);

    // --- Scale group selector ---
    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(QStringLiteral("Scale:"), this));
    scale_combo_ = new QComboBox(this);
    scale_combo_->addItem(QStringLiteral("Physical  (MeV, J, erg)"),
                          QVariant::fromValue(int(ScaleGroup::Physical)));
    scale_combo_->addItem(QStringLiteral("Biological (Gy, Sv, rem)"),
                          QVariant::fromValue(int(ScaleGroup::Biological)));
    scale_combo_->addItem(QStringLiteral("LET / RBE / BED"),
                          QVariant::fromValue(int(ScaleGroup::LET_RBE)));
    scale_combo_->addItem(QStringLiteral("All scales"),
                          QVariant::fromValue(int(ScaleGroup::All)));
    connect(scale_combo_, &QComboBox::currentIndexChanged, this,
            [this](int idx) {
                scale_group_ = static_cast<ScaleGroup>(
                    scale_combo_->itemData(idx).toInt());
                if (has_result_ && current_track_ >= 0)
                    populateStepTable(current_track_);
                emit scaleGroupChanged(scale_group_);
            });
    row->addWidget(scale_combo_);
    root->addLayout(row);

    // --- Step table ---
    step_table_ = new QTableWidget(this);
    step_table_->setAlternatingRowColors(true);
    step_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    step_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    step_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    step_table_->verticalHeader()->setDefaultSectionSize(18);
    step_table_->setStyleSheet(QStringLiteral(
        "QTableWidget { font-size: 9px; background: #0d0d1a; color: #ccc; }"
        "QHeaderView::section { background: #1a1a2e; color: #88aaff; font-size: 9px; }"));
    root->addWidget(step_table_, 2);

    // --- Energy profile mini-plot ---
    energy_plot_ = new EnergyProfilePlot(this);
    root->addWidget(energy_plot_, 0);

    // --- Per-slab summary ---
    auto* slab_group = new QGroupBox(QStringLiteral("Per-slab summary"), this);
    auto* sg_layout  = new QVBoxLayout(slab_group);
    sg_layout->setContentsMargins(2, 4, 2, 2);

    slab_table_ = new QTableWidget(this);
    slab_table_->setAlternatingRowColors(true);
    slab_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    slab_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    slab_table_->verticalHeader()->setDefaultSectionSize(18);
    slab_table_->setStyleSheet(step_table_->styleSheet());
    sg_layout->addWidget(slab_table_);
    root->addWidget(slab_group, 1);
}

// ---------------------------------------------------------------------------
// Column definitions
// ---------------------------------------------------------------------------

std::vector<TrajectoryInspectorPanel::ColumnDef>
TrajectoryInspectorPanel::columnsForGroup(ScaleGroup g) const
{
    std::vector<ColumnDef> cols;
    cols.push_back({"Step",    [](const EnergyScaleSet&) { return 0.0; }}); // placeholder

    auto add = [&](const QString& h, std::function<double(const EnergyScaleSet&)> fn) {
        cols.push_back({h, std::move(fn)});
    };

    const bool phys = (g == ScaleGroup::Physical  || g == ScaleGroup::All);
    const bool bio  = (g == ScaleGroup::Biological || g == ScaleGroup::All);
    const bool let  = (g == ScaleGroup::LET_RBE    || g == ScaleGroup::All);

    if (phys) {
        add("dE/dx [MeV]", [](const EnergyScaleSet& s){ return s.edep_MeV_original; });
        add("KinE [MeV]",  [](const EnergyScaleSet& s){ return s.kinE_MeV_simulated; });
        add("dE [keV]",    [](const EnergyScaleSet& s){ return s.edep_keV; });
        add("dE [eV]",     [](const EnergyScaleSet& s){ return s.edep_eV; });
        add("dE [J]",      [](const EnergyScaleSet& s){ return s.edep_J; });
        add("dE [erg]",    [](const EnergyScaleSet& s){ return s.edep_erg; });
    }
    if (bio) {
        add("Dose [Gy]",   [](const EnergyScaleSet& s){ return s.dose_Gy; });
        add("Dose [mGy]",  [](const EnergyScaleSet& s){ return s.dose_mGy; });
        add("H [Sv]",      [](const EnergyScaleSet& s){ return s.H_Sv; });
        add("H [mSv]",     [](const EnergyScaleSet& s){ return s.H_mSv; });
        add("H [rem]",     [](const EnergyScaleSet& s){ return s.H_rem; });
        add("D [rad]",     [](const EnergyScaleSet& s){ return s.dose_rad; });
    }
    if (let) {
        add("LET [MeV/cm]",[](const EnergyScaleSet& s){ return s.LET_MeV_cm; });
        add("RBE",         [](const EnergyScaleSet& s){ return s.RBE; });
        add("BED [Gy]",    [](const EnergyScaleSet& s){ return s.BED_Gy; });
        add("Eff.H [mSv]", [](const EnergyScaleSet& s){ return s.E_mSv; });
    }

    return cols;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void TrajectoryInspectorPanel::setResult(const BioSimResult& result)
{
    result_     = result;
    has_result_ = true;
    populateSlabSummary();
}

void TrajectoryInspectorPanel::showStep(int track_idx, int step_idx)
{
    if (!has_result_) return;
    if (track_idx < 0 || track_idx >= static_cast<int>(result_.tracks.size())) return;

    current_track_ = track_idx;
    current_step_  = step_idx;

    populateHeader(track_idx);
    populateStepTable(track_idx);
    highlightRow(step_idx);
    populateEnergyPlot(track_idx);
}

void TrajectoryInspectorPanel::clear()
{
    header_label_->setText(QStringLiteral("No track selected"));
    step_table_->clearContents();
    step_table_->setRowCount(0);
    slab_table_->clearContents();
    slab_table_->setRowCount(0);
    current_track_ = -1;
    current_step_  = -1;
}

// ---------------------------------------------------------------------------
// Populate helpers
// ---------------------------------------------------------------------------

void TrajectoryInspectorPanel::populateHeader(int track_idx) const
{
    const auto& track = result_.tracks[static_cast<std::size_t>(track_idx)];
    const QString text = QString(
        "<b>Track %1</b> &nbsp; event=%2 &nbsp; particle=%3<br>"
        "Steps: %4 &nbsp; Total ΔE: <b>%5 MeV</b> &nbsp; "
        "Total dose: <b>%6 mGy</b>")
        .arg(track.track_id)
        .arg(track.event_id)
        .arg(QString::fromStdString(track.particle_type))
        .arg(track.steps.size())
        .arg(track.total_edep_MeV_original, 0, 'f', 4)
        .arg(track.total_dose_Gy * 1000.0, 0, 'f', 4);
    header_label_->setText(text);
}

void TrajectoryInspectorPanel::populateStepTable(int track_idx) const
{
    const auto& track = result_.tracks[static_cast<std::size_t>(track_idx)];
    const auto cols = columnsForGroup(scale_group_);

    step_table_->setRowCount(static_cast<int>(track.steps.size()));
    step_table_->setColumnCount(static_cast<int>(cols.size()) + 2); // +depth +slab

    // Headers.
    QStringList headers;
    headers << QStringLiteral("Step") << QStringLiteral("Depth[cm]");
    for (const auto& c : cols) headers << c.header;
    headers << QStringLiteral("Slab");
    // Replace first placeholder header.
    headers[2] = cols[0].header; // "Step" already set

    // Rebuild.
    QStringList real_headers;
    real_headers << QStringLiteral("Step") << QStringLiteral("Depth[cm]");
    for (std::size_t ci = 1; ci < cols.size(); ++ci)
        real_headers << cols[ci].header;
    real_headers << QStringLiteral("Slab");
    step_table_->setHorizontalHeaderLabels(real_headers);

    for (int si = 0; si < static_cast<int>(track.steps.size()); ++si) {
        const auto& step = track.steps[static_cast<std::size_t>(si)];
        int col = 0;
        auto setItem = [&](const QString& txt) {
            auto* item = new QTableWidgetItem(txt);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            step_table_->setItem(si, col++, item);
        };
        setItem(QString::number(step.step_index));
        setItem(QString::number(step.energy.depth_in_slab_cm, 'f', 3));

        for (std::size_t ci = 1; ci < cols.size(); ++ci)
            setItem(QString::number(cols[ci].getter(step.energy), 'g', 4));

        // Slab column.
        const QString slab_str = (step.energy.slab_index >= 0)
            ? QString::number(step.energy.slab_index)
            : QStringLiteral("—");
        auto* item = new QTableWidgetItem(slab_str);
        item->setTextAlignment(Qt::AlignCenter);
        step_table_->setItem(si, col, item);
    }
}

void TrajectoryInspectorPanel::highlightRow(int step_idx) const
{
    if (step_idx < 0 || step_idx >= step_table_->rowCount()) return;
    step_table_->selectRow(step_idx);
    step_table_->scrollTo(step_table_->model()->index(step_idx, 0),
                          QAbstractItemView::PositionAtCenter);
}

void TrajectoryInspectorPanel::populateEnergyPlot(int track_idx) const
{
    auto* plot = static_cast<EnergyProfilePlot*>(energy_plot_);
    if (!plot) return;

    const auto& track = result_.tracks[static_cast<std::size_t>(track_idx)];

    // Use the first non-step column as the plotted quantity.
    const auto cols = columnsForGroup(scale_group_);
    if (cols.size() <= 1) return;

    const auto& col = cols[1]; // first actual data column
    plot->setData(&track, col.getter, current_step_, col.header,
                  const_cast<EnergyColorMapper*>(&mapper_), palette_);
}

void TrajectoryInspectorPanel::populateSlabSummary() const
{
    const auto& stats = result_.slab_stats;
    slab_table_->setRowCount(static_cast<int>(stats.size()));
    slab_table_->setColumnCount(6);
    slab_table_->setHorizontalHeaderLabels({
        "Slab", "Material", "ΔE[MeV]", "Dose[mGy]", "Tracks", "Steps"
    });

    for (int i = 0; i < static_cast<int>(stats.size()); ++i) {
        const auto& st = stats[static_cast<std::size_t>(i)];
        int c = 0;
        auto setI = [&](const QString& s) {
            auto* it = new QTableWidgetItem(s);
            it->setTextAlignment(Qt::AlignCenter);
            slab_table_->setItem(i, c++, it);
        };
        setI(QString::fromStdString(st.slab_id));
        setI(QString::fromStdString(st.material_name));
        setI(QString::number(st.total_edep_MeV_original, 'g', 5));
        setI(QString::number(st.mean_dose_Gy * 1000.0,   'g', 5));
        setI(QString::number(st.tracks_crossing));
        setI(QString::number(st.total_steps_in_slab));
    }
}

} // namespace beamlab::biosim
