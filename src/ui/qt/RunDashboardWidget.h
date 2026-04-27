#pragma once

#include <QString>
#include <QWidget>

class QGridLayout;
class QLabel;
class QWidget;

namespace beamlab::ui {

class RunDashboardWidget final : public QWidget {
public:
    explicit RunDashboardWidget(QWidget* parent = nullptr);

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
