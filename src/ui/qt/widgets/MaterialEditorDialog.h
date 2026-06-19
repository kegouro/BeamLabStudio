#pragma once

#include "domain/materials/MaterialRegistry.h"
#include "domain/materials/Material.h"
#include "domain/simulation/SimulationEngine.h"

#include <QDialog>
#include <optional>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QWidget;

namespace beamlab::ui {

class MaterialEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit MaterialEditorDialog(
        domain::materials::MaterialRegistry* materialRegistry,
        domain::simulation::SimulationEngine* simulationEngine,
        QWidget* parent = nullptr);

    std::optional<domain::materials::Material> selectedMaterial() const;

signals:
    void materialsChanged();

private slots:
    void onMaterialSelected(int row);
    void onAddCustom();
    void onDeleteCustom();
    void onSaveEdits();
    void onSearchChanged(const QString& text);

private:
    void buildLayout();
    void populateList(const QString& filter = {});
    void loadMaterialToForm(const domain::materials::Material& mat);
    domain::materials::Material readMaterialFromForm() const;
    void updateDedxPlot();
    void saveCustomMaterialsToJson() const;
    void loadCustomMaterialsFromJson();

    domain::materials::MaterialRegistry* materialRegistry_;
    domain::simulation::SimulationEngine* simulationEngine_;
    std::optional<domain::materials::Material> selected_;
    domain::materials::Material editing_;

    // Widgets — left panel.
    QListWidget*  list_widget_{nullptr};
    QLineEdit*    search_edit_{nullptr};

    // Widgets — right panel (physical tab).
    QLineEdit*       edit_id_{nullptr};
    QLineEdit*       edit_name_{nullptr};
    QLineEdit*       edit_category_{nullptr};
    QDoubleSpinBox*  spin_density_{nullptr};
    QDoubleSpinBox*  spin_Z_eff_{nullptr};
    QDoubleSpinBox*  spin_A_eff_{nullptr};
    QDoubleSpinBox*  spin_I_eV_{nullptr};

    // Widgets — right panel (biological tab).
    QDoubleSpinBox*  spin_WR_{nullptr};
    QDoubleSpinBox*  spin_WT_{nullptr};
    QDoubleSpinBox*  spin_alpha_beta_{nullptr};
    QLineEdit*       edit_organ_{nullptr};

    // dE/dx preview plot widget.
    QWidget*         dedx_plot_{nullptr};
    QPushButton* btn_save_{nullptr};
    QPushButton* btn_add_{nullptr};
    QPushButton* btn_del_{nullptr};

    struct CurvePoint { double kinE_MeV; double dedx; };
    std::vector<CurvePoint> dedx_curve_{};
};

} // namespace beamlab::ui
