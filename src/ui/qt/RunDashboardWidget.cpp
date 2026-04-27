#include "ui/qt/RunDashboardWidget.h"

#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

#include <array>

namespace beamlab::ui {
namespace {

QString formatJsonValue(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }

    if (value.isDouble()) {
        const double number = value.toDouble();
        const qint64 integer = static_cast<qint64>(number);

        if (static_cast<double>(integer) == number) {
            return QString::number(integer);
        }

        return QString::number(number, 'g', 6);
    }

    if (value.isBool()) {
        return value.toBool() ? "true" : "false";
    }

    return {};
}

QString objectValue(const QJsonObject& object, const QString& key)
{
    return formatJsonValue(object.value(key));
}

QString fileStatus(const QString& path)
{
    const QFileInfo info(path);

    if (!info.exists()) {
        return "missing";
    }

    return QString("%1 bytes").arg(info.size());
}

void clearGrid(QGridLayout* grid)
{
    if (grid == nullptr) {
        return;
    }

    while (QLayoutItem* item = grid->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }

        delete item;
    }
}

QLabel* makeKeyLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet("color: #6B7A94;");
    return label;
}

QLabel* makeValueLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.isEmpty() ? "n/a" : text, parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    label->setStyleSheet("color: #E8EEF8; font-weight: 600;");
    return label;
}

void addPair(QGridLayout* grid,
             const int row,
             const QString& key,
             const QString& value,
             QWidget* parent)
{
    grid->addWidget(makeKeyLabel(key, parent), row, 0);
    grid->addWidget(makeValueLabel(value, parent), row, 1);
}

void addFilePair(QGridLayout* grid,
                 const int row,
                 const QString& key,
                 const QString& path,
                 QWidget* parent,
                 QStringList* warnings)
{
    const QString status = fileStatus(path);
    const bool missing = status == "missing";

    auto* key_label = makeKeyLabel(key, parent);
    auto* path_label = makeValueLabel(path.isEmpty() ? "n/a" : path, parent);
    auto* status_label = new QLabel(
        missing ? QString("x %1").arg(status) : QString("ok %1").arg(status),
        parent
    );

    status_label->setStyleSheet(
        missing
            ? "color: #FF6B6B; font-weight: 700;"
            : "color: #7CF2D2; font-weight: 700;"
    );
    path_label->setToolTip(path);

    grid->addWidget(key_label, row, 0);
    grid->addWidget(path_label, row, 1);
    grid->addWidget(status_label, row, 2);

    if (missing && warnings != nullptr) {
        warnings->push_back(key + " is missing: " + path);
    }
}

} // namespace

RunDashboardWidget::RunDashboardWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    run_title_ = new QLabel("BeamLabStudio run dashboard", content);
    run_title_->setStyleSheet(
        "color: #E8EEF8; font-size: 18px; font-weight: 700; padding-bottom: 4px;"
    );
    layout->addWidget(run_title_);

    auto* config_box = new QGroupBox("Configuration", content);
    config_grid_ = new QGridLayout(config_box);
    config_grid_->setColumnStretch(1, 1);
    config_grid_->setHorizontalSpacing(16);
    config_grid_->setVerticalSpacing(8);
    layout->addWidget(config_box);

    auto* files_box = new QGroupBox("Loaded files", content);
    files_grid_ = new QGridLayout(files_box);
    files_grid_->setColumnStretch(1, 1);
    files_grid_->setHorizontalSpacing(16);
    files_grid_->setVerticalSpacing(8);
    layout->addWidget(files_box);

    auto* warnings_group = new QGroupBox("Warnings", content);
    warnings_group->setStyleSheet(
        "QGroupBox { border-color: #C06020; color: #FFBE78; }"
        "QGroupBox::title { color: #FFBE78; }"
    );
    auto* warnings_layout = new QVBoxLayout(warnings_group);
    warnings_label_ = new QLabel(warnings_group);
    warnings_label_->setWordWrap(true);
    warnings_label_->setStyleSheet("color: #FFBE78;");
    warnings_layout->addWidget(warnings_label_);
    warnings_box_ = warnings_group;
    layout->addWidget(warnings_box_);

    layout->addStretch(1);
    scroll->setWidget(content);
    root_layout->addWidget(scroll);
}

void RunDashboardWidget::loadFromManifest(const QString& manifest_text,
                                          const QString& trajectories_csv,
                                          const QString& focal_slice_csv,
                                          const QString& envelope_csv,
                                          const QString& caustic_obj,
                                          const QString& lens_obj)
{
    clearGrid(config_grid_);
    clearGrid(files_grid_);

    QJsonParseError parse_error{};
    const QJsonDocument document =
        QJsonDocument::fromJson(manifest_text.toUtf8(), &parse_error);
    const QJsonObject root = document.isObject() ? document.object() : QJsonObject{};
    const QJsonObject configuration = root.value("configuration").toObject();
    const QJsonObject row_counts = root.value("row_counts").toObject();

    run_title_->setText(
        parse_error.error == QJsonParseError::NoError
            ? "BeamLabStudio run dashboard"
            : "BeamLabStudio run dashboard (manifest parse warning)"
    );

    int row = 0;
    addPair(config_grid_, row++, "Axis mode", objectValue(configuration, "axis_mode"), this);
    addPair(config_grid_, row++, "Reference mode", objectValue(configuration, "reference_mode"), this);
    addPair(config_grid_, row++, "Binning mode", objectValue(configuration, "binning_mode"), this);
    addPair(config_grid_, row++, "Axial bins", objectValue(configuration, "axial_bin_count"), this);
    addPair(config_grid_, row++, "Window", objectValue(configuration, "half_window_frames"), this);
    addPair(config_grid_, row++, "Trajectories preview", objectValue(row_counts, "trajectories_preview"), this);
    addPair(config_grid_, row++, "Focal slice points", objectValue(row_counts, "focal_slice_points"), this);
    addPair(config_grid_, row++, "Envelope rings", objectValue(row_counts, "envelope_rings"), this);

    QStringList warnings;

    if (parse_error.error != QJsonParseError::NoError) {
        warnings.push_back("Manifest JSON could not be parsed: " + parse_error.errorString());
    }

    row = 0;
    addFilePair(files_grid_, row++, "Trajectories CSV", trajectories_csv, this, &warnings);
    addFilePair(files_grid_, row++, "Focal slice CSV", focal_slice_csv, this, &warnings);
    addFilePair(files_grid_, row++, "Envelope rings CSV", envelope_csv, this, &warnings);
    addFilePair(files_grid_, row++, "Focal envelope proxy OBJ", caustic_obj, this, &warnings);
    addFilePair(files_grid_, row++, "Effective lens OBJ", lens_obj, this, &warnings);

    const bool has_warnings = !warnings.isEmpty();
    warnings_box_->setVisible(has_warnings);
    warnings_label_->setText(warnings.join("\n"));
}

} // namespace beamlab::ui
