#pragma once

#include <QWidget>

class QPushButton;
class QSplitter;

namespace beamlab::ui {

class BioSimPresenter;
class BioViewport3D;
class SlabEditor3D;
class MaterialEditorDialog;
class EnergyScaleBar;

/// Bio-simulator tab — 3D viewport + slab controls + material editing.
class BioSimView final : public QWidget {
    Q_OBJECT

public:
    explicit BioSimView(BioSimPresenter* presenter,
                        QWidget* parent = nullptr);

private:
    void setupLayout();
    void connectSignals();
    void onOpenMaterialEditor();

    BioSimPresenter* presenter_{nullptr};

    BioViewport3D*  viewport3D_{nullptr};
    SlabEditor3D*   slabEditor_{nullptr};
    EnergyScaleBar* energyScale_{nullptr};
    QPushButton*    materialsBtn_{nullptr};

    QSplitter* horizontalSplitter_{nullptr};
    QSplitter* verticalSplitter_{nullptr};
};

} // namespace beamlab::ui
