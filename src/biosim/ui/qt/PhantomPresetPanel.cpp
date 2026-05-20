#include "biosim/ui/qt/PhantomPresetPanel.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace beamlab::biosim {

// ---------------------------------------------------------------------------
// Axial diagram widget
// ---------------------------------------------------------------------------

class PhantomPresetPanel::DiagramWidget : public QWidget {
public:
    explicit DiagramWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(120);
    }

    void setPreset(const PhantomPreset* preset)
    {
        preset_ = preset;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(12, 12, 22));

        if (!preset_ || preset_->slabs.empty()) {
            p.setPen(QColor(80, 80, 80));
            p.drawText(rect(), Qt::AlignCenter,
                       QStringLiteral("Select a phantom to preview"));
            return;
        }

        // Find total axial span.
        double z_min = preset_->slabs.front().axial_start_m;
        double z_max = z_min;
        for (const auto& s : preset_->slabs) {
            z_min = std::min(z_min, s.axial_start_m);
            z_max = std::max(z_max, s.axial_start_m + s.thickness_m);
        }
        const double span = (z_max > z_min) ? (z_max - z_min) : 1.0;

        const int W = width()  - 40;  // leave margins for labels
        const int H = height() - 30;
        const int ox = 20, oy = 8;

        // Draw beam arrow.
        p.setPen(QPen(QColor(100, 200, 255, 120), 1, Qt::DashLine));
        p.drawLine(ox, oy + H / 2, ox + W, oy + H / 2);
        p.setPen(QColor(100, 200, 255, 180));
        QFont f; f.setPointSizeF(7.0); f.setBold(true); p.setFont(f);
        p.drawText(QRect(ox + W - 28, oy + H / 2 - 8, 28, 10),
                   Qt::AlignLeft, QStringLiteral("→ z"));

        // Draw slabs as coloured horizontal bars.
        const int slab_h = static_cast<int>(H * 0.6);
        const int slab_y = oy + (H - slab_h) / 2;

        for (const auto& slab : preset_->slabs) {
            if (!slab.enabled) continue;
            const double frac_start = (slab.axial_start_m - z_min) / span;
            const double frac_end   = (slab.axial_start_m + slab.thickness_m - z_min) / span;

            const int sx = ox + static_cast<int>(frac_start * W);
            const int ex = ox + static_cast<int>(frac_end   * W);
            const int sw = std::max(ex - sx, 2);

            // Unpack colour.
            const unsigned int rgba = slab.color_rgba;
            const int r = static_cast<int>((rgba >> 24) & 0xFFu);
            const int g = static_cast<int>((rgba >> 16) & 0xFFu);
            const int b = static_cast<int>((rgba >>  8) & 0xFFu);

            p.fillRect(sx, slab_y, sw, slab_h, QColor(r, g, b, 160));
            p.setPen(QColor(r, g, b));
            p.drawRect(sx, slab_y, sw - 1, slab_h - 1);

            // Label (material name, rotated if narrow).
            QFont lf; lf.setPointSizeF(6.0); p.setFont(lf);
            p.setPen(Qt::white);
            const QString mat_label =
                QString::fromStdString(slab.material.name).left(8);
            if (sw > 20) {
                p.save();
                p.translate(sx + sw / 2, slab_y + slab_h / 2);
                if (sw < 40) {
                    p.rotate(-90);
                    p.drawText(QRect(-30, -8, 60, 16), Qt::AlignCenter, mat_label);
                } else {
                    p.drawText(QRect(-30, -8, 60, 16), Qt::AlignCenter, mat_label);
                }
                p.restore();
            }
        }

        // Axis ticks at start/end.
        p.setPen(QColor(120, 120, 120));
        QFont axf; axf.setPointSizeF(6.5); p.setFont(axf);
        p.drawText(QRect(ox - 10, oy + H + 2, 50, 12),
                   Qt::AlignLeft, QString::number(z_min * 100.0, 'f', 1) + " cm");
        p.drawText(QRect(ox + W - 50, oy + H + 2, 60, 12),
                   Qt::AlignRight, QString::number(z_max * 100.0, 'f', 1) + " cm");
    }

private:
    const PhantomPreset* preset_{nullptr};
};

// ---------------------------------------------------------------------------
// PhantomPresetPanel
// ---------------------------------------------------------------------------

PhantomPresetPanel::PhantomPresetPanel(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Phantom Presets"));
    setMinimumSize(700, 420);
    buildLayout();
    populateList();
}

void PhantomPresetPanel::buildLayout()
{
    auto* root = new QHBoxLayout(this);

    // ── Left: list ───────────────────────────────────────────────────────
    auto* left = new QVBoxLayout();
    auto* search = new QLineEdit(this);
    search->setPlaceholderText(QStringLiteral("Search phantoms…"));
    left->addWidget(search);

    list_ = new QListWidget(this);
    list_->setAlternatingRowColors(true);
    list_->setStyleSheet(QStringLiteral(
        "QListWidget { font-size: 9px; background: #0d0d1a; color: #ddd; }"
        "QListWidget::item:selected { background: #1a3a6e; }"));
    connect(list_, &QListWidget::currentRowChanged,
            this, &PhantomPresetPanel::onPresetSelected);
    left->addWidget(list_, 1);

    // Filter.
    connect(search, &QLineEdit::textChanged, this,
            [this](const QString& t) {
                for (int i = 0; i < list_->count(); ++i) {
                    auto* item = list_->item(i);
                    const bool match = t.isEmpty() ||
                        item->text().contains(t, Qt::CaseInsensitive);
                    item->setHidden(!match);
                }
            });

    // ── Right: preview ───────────────────────────────────────────────────
    auto* right = new QVBoxLayout();

    auto* diag_w = new DiagramWidget(this);
    diagram_widget_ = diag_w;
    right->addWidget(diagram_widget_, 1);

    desc_label_ = new QLabel(QStringLiteral("Select a preset from the list."), this);
    desc_label_->setWordWrap(true);
    desc_label_->setStyleSheet(QStringLiteral("color: #aaa; font-size: 9px; padding: 4px;"));
    right->addWidget(desc_label_);

    // Buttons.
    btn_apply_ = new QPushButton(QStringLiteral("Apply Preset"), this);
    btn_apply_->setEnabled(false);
    connect(btn_apply_, &QPushButton::clicked, this, &PhantomPresetPanel::onApply);

    auto* btn_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(btn_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* btn_row = new QHBoxLayout();
    btn_row->addWidget(btn_apply_);
    btn_row->addWidget(btn_box);
    right->addLayout(btn_row);

    // Assemble.
    auto* left_w = new QWidget(this);
    left_w->setLayout(left);
    left_w->setMaximumWidth(240);
    root->addWidget(left_w);

    auto* right_w = new QWidget(this);
    right_w->setLayout(right);
    root->addWidget(right_w, 1);
}

void PhantomPresetPanel::populateList()
{
    const auto& all = library_.all();
    QString last_cat;
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        const auto& preset = all[static_cast<std::size_t>(i)];
        const QString cat = QString::fromStdString(preset.category);

        if (cat != last_cat) {
            auto* sep = new QListWidgetItem(QStringLiteral("── ") + cat +
                                            QStringLiteral(" ──"));
            sep->setFlags(Qt::NoItemFlags);
            sep->setForeground(QColor(100, 160, 255));
            QFont f; f.setBold(true); f.setPointSizeF(8.0); sep->setFont(f);
            list_->addItem(sep);
            last_cat = cat;
        }

        auto* item = new QListWidgetItem(
            QStringLiteral("  ") + QString::fromStdString(preset.name));
        item->setData(Qt::UserRole, QString::fromStdString(preset.id));
        item->setToolTip(QString::fromStdString(preset.description));
        list_->addItem(item);
    }
}

void PhantomPresetPanel::onPresetSelected(int row)
{
    const auto* item = list_->item(row);
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;

    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    const auto preset = library_.findById(id);
    if (!preset) return;

    selected_idx_ = row;
    updatePreview(*preset);
    btn_apply_->setEnabled(true);
}

void PhantomPresetPanel::onApply()
{
    const auto* item = list_->currentItem();
    if (!item) return;
    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    const auto preset = library_.findById(id);
    if (!preset) return;
    emit phantomSelected(*preset);
    accept();
}

void PhantomPresetPanel::updatePreview(const PhantomPreset& preset)
{
    // Update axial diagram.
    if (auto* dw = static_cast<DiagramWidget*>(diagram_widget_))
        dw->setPreset(&preset);

    // Update description label.
    const QString text = QString(
        "<b>%1</b><br>"
        "<small>Category: %2 &nbsp;|&nbsp; Slabs: %3</small><br>"
        "<small>%4</small><br>"
        "<small><i>Ref: %5</i></small>")
        .arg(QString::fromStdString(preset.name))
        .arg(QString::fromStdString(preset.category))
        .arg(preset.slabs.size())
        .arg(QString::fromStdString(preset.description))
        .arg(QString::fromStdString(preset.reference));
    desc_label_->setText(text);
}

} // namespace beamlab::biosim
