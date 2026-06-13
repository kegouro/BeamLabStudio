#pragma once

#include <QWidget>

class QButtonGroup;
class QVBoxLayout;

namespace beamlab::ui {

/// Riel lateral de navegación. Cada ítem corresponde a una página del
/// QStackedWidget del workspace. Emite sectionChanged(index) al activarse.
class NavigationRail final : public QWidget {
    Q_OBJECT
public:
    explicit NavigationRail(QWidget* parent = nullptr);

    /// Añade un ítem (icono textual + etiqueta). Devuelve su índice.
    int addItem(const QString& glyph, const QString& label);
    /// Añade un encabezado de grupo no seleccionable ("ANÁLISIS").
    void addGroupHeader(const QString& text);
    /// Empuja los ítems siguientes hacia abajo (stretch flexible).
    void addStretch();

    void setCurrentIndex(int index);
    int currentIndex() const;

signals:
    void sectionChanged(int index);

private:
    QVBoxLayout* layout_{nullptr};
    QButtonGroup* group_{nullptr};
    int next_index_{0};
};

} // namespace beamlab::ui
