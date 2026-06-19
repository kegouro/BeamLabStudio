#pragma once

#include "ui/qt/dockable/IDockableWidget.h"

#include <QString>
#include <QWidget>

class QGridLayout;
class QLabel;
class QWidget;

namespace beamlab::ui {

class RunDashboardWidget final : public QWidget, public qt::IDockableWidget {
public:
    explicit RunDashboardWidget(QWidget* parent = nullptr);

    // ── IDockableWidget ───────────────────────────────────────────
    QString title() const override { return QStringLiteral("Runs"); }
    QString id() const override { return QStringLiteral("runs"); }
    QWidget* widget() override { return this; }
    Qt::DockWidgetArea preferredArea() const override
        { return Qt::RightDockWidgetArea; }

    void loadFromManifest(const QString& manifest_text,
                          const QString& trajectories_csv,
                          const QString& focal_slice_csv,
                          const QString& envelope_csv,
                          const QString& caustic_obj,
                          const QString& lens_obj);

private:
    QLabel* run_title_{nullptr};
    QGridLayout* config_grid_{nullptr};
    QGridLayout* files_grid_{nullptr};
    QWidget* warnings_box_{nullptr};
    QLabel* warnings_label_{nullptr};
};

} // namespace beamlab::ui
