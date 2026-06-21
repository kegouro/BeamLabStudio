#pragma once

// ValidationView — Qt widget for the C4 NIST Validation Panel.
//
// Displays a QTableWidget comparing stopping-power output from:
//   (1) domain SimulationEngine
//   (2) biosim StoppingPowerEngine
// against an external NIST PSTAR reference (proton/water only).
//
// Layout:
//   [ Partícula: ▼ ] [ Material: ▼ ] [ Validar ]
//   ──────────────────────────────────────────
//   QTableWidget (native, no QtCharts)
//   ───────────────────���──────────────────────
//   Footer note (QLabel)
//
// Columns — proton/water:
//   E [MeV] | NIST [MeV/cm] | Dominio [MeV/cm] | BioSim [MeV/cm] |
//   dev dom % | dev bio % | ✓/✗
//
// Columns — other combinations:
//   E [MeV] | Dominio [MeV/cm] | BioSim [MeV/cm] | dif inter-motor %
//
// MVP: this view holds no business logic. NistValidator does all computation.
// The view owns its own registries (loaded from builtins) — no injection needed.

#include "domain/materials/MaterialRegistry.h"
#include "domain/physics/ParticleRegistry.h"

#include <QWidget>

#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace beamlab::ui {

class ValidationView final : public QWidget {
    Q_OBJECT

public:
    explicit ValidationView(QWidget* parent = nullptr);

private slots:
    void onValidate();

private:
    void buildUi();
    void populateSelectors();

    // Owned registries — loaded from builtins on construction.
    // ponytail: mirrors test SetUp() pattern; no shared ownership needed here.
    std::unique_ptr<domain::materials::MaterialRegistry> matReg_;
    std::unique_ptr<domain::physics::ParticleRegistry>   partReg_;

    QComboBox*    particle_combo_{nullptr};
    QComboBox*    material_combo_{nullptr};
    QPushButton*  validate_btn_{nullptr};
    QTableWidget* table_{nullptr};
    QLabel*       footer_label_{nullptr};
};

} // namespace beamlab::ui
