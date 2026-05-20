#pragma once

#include "biosim/geometry/PhantomLibrary.h"
#include "biosim/geometry/PhantomPreset.h"

#include <QDialog>

// Forward declarations.
class QLabel;
class QListWidget;
class QPushButton;
class QWidget;

namespace beamlab::biosim {

// Modal dialog for selecting a phantom preset.
//
// Left: filterable list grouped by category.
// Right: axial diagram (QPainter) + description label.
// OK button emits phantomSelected(preset) and closes.
class PhantomPresetPanel : public QDialog {
    Q_OBJECT

public:
    explicit PhantomPresetPanel(QWidget* parent = nullptr);

signals:
    // Emitted when the user clicks "Apply" with a valid selection.
    void phantomSelected(const PhantomPreset& preset);

private slots:
    void onPresetSelected(int row);
    void onApply();

private:
    void buildLayout();
    void populateList();
    void updatePreview(const PhantomPreset& preset);

    PhantomLibrary library_{};
    int selected_idx_{-1};

    QListWidget* list_{nullptr};
    QLabel*      desc_label_{nullptr};
    QWidget*     diagram_widget_{nullptr};
    QPushButton* btn_apply_{nullptr};

    // Mini QPainter diagram widget (shows slab stack axially).
    class DiagramWidget;
};

} // namespace beamlab::biosim
