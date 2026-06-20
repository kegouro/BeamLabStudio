#pragma once

#include <memory>

#include <QString>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QSplitter;
class QTabWidget;

namespace beamlab {
namespace biosim {
struct BioSimResult;
class BioViewport3D;
class SlabEditor3D;
class MaterialEditorDialog;
class EnergyScaleBar;
class SOBPWidget;
}
namespace ui {

class BioSimPresenter;

/// Bio-simulator tab — 3D viewport + slab controls + material editing
/// + SOBP planning panel.
///
/// MVP view: owns no business logic.  Run/Stop are forwarded to the
/// BioSimPresenter, which drives the real BioSimRunner; the resulting
/// BioSimResult is pushed back into the viewport for rendering.
class BioSimView final : public QWidget {
    Q_OBJECT

public:
    explicit BioSimView(BioSimPresenter* presenter,
                        QWidget* parent = nullptr);

    /// Pre-load a Geant4 energy_step_profile.csv path (e.g. after an analysis
    /// run finishes).  Does not start the simulation.
    void setCsvPath(const QString& path);

private slots:
    void onBrowseCsv();
    void onRun();
    void onStop();
    void onSimulationProgress(int percent);
    void onSimulationCompleted(
        std::shared_ptr<beamlab::biosim::BioSimResult> result);

private:
    void setupLayout();
    void connectSignals();
    void onOpenMaterialEditor();

    BioSimPresenter* presenter_{nullptr};

    biosim::BioViewport3D*  viewport3D_{nullptr};
    biosim::SlabEditor3D*   slabEditor_{nullptr};
    biosim::EnergyScaleBar* energyScale_{nullptr};
    biosim::SOBPWidget*     sobpWidget_{nullptr};
    QTabWidget*             biosimTabs_{nullptr};
    QPushButton*    materialsBtn_{nullptr};
    QPushButton*    runBtn_{nullptr};
    QPushButton*    stopBtn_{nullptr};
    QLabel*         csvLabel_{nullptr};
    QLabel*         statusLabel_{nullptr};
    QProgressBar*   progressBar_{nullptr};

    QSplitter* horizontalSplitter_{nullptr};
    QSplitter* verticalSplitter_{nullptr};

    QString csvPath_{};
    bool    running_{false};
};

} // namespace ui
} // namespace beamlab
