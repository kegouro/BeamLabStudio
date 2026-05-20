#pragma once

#include "biosim/materials/BioMaterial.h"
#include "biosim/materials/BioMaterialLibrary.h"
#include "biosim/physics/StoppingPowerEngine.h"

#include <QDialog>
#include <optional>

// Forward declarations.
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QWidget;

namespace beamlab::biosim {

// Material gallery + editor dialog.
//
// Left panel: searchable QListWidget with all 55+ materials grouped by category.
// Right panel: tabs for Physical params, Biological params, Composition.
// Bottom: real-time dE/dx preview plot (QPainter mini-chart).
//
// Custom materials can be added, edited, and saved persistently.
class MaterialEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit MaterialEditorDialog(QWidget* parent = nullptr);

    // Returns the material that was last selected when the dialog was closed.
    // Empty if the user cancelled or selected nothing.
    [[nodiscard]] std::optional<BioMaterial> selectedMaterial() const;

signals:
    // Emitted when a custom material is added or modified.
    void libraryChanged();

private slots:
    void onMaterialSelected(int row);
    void onAddCustom();
    void onDeleteCustom();
    void onSaveEdits();
    void onSearchChanged(const QString& text);

private:
    void buildLayout();
    void populateList(const QString& filter = {});
    void loadMaterialToForm(const BioMaterial& mat);
    BioMaterial readMaterialFromForm() const;
    void updateDedxPlot();

    BioMaterialLibrary library_{};
    StoppingPowerEngine spe_{};
    std::optional<BioMaterial> selected_{};
    BioMaterial editing_{}; // material currently shown in the form

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

    // dE/dx preview plot widget (inline QPainter widget).
    QWidget*         dedx_plot_{nullptr};

    QPushButton* btn_save_{nullptr};
    QPushButton* btn_add_{nullptr};
    QPushButton* btn_del_{nullptr};

    // Cached dE/dx curve for the preview [log-spaced kinE, dEdx].
    struct CurvePoint { double kinE_MeV; double dedx; };
    std::vector<CurvePoint> dedx_curve_{};
};

} // namespace beamlab::biosim
