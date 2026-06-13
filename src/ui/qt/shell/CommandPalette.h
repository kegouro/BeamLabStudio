#pragma once

#include <QDialog>

#include "ui/qt/shell/ActionRegistry.h"

class QLineEdit;
class QListWidget;

namespace beamlab::ui {

/// Paleta de comandos estilo ⌘K: campo de búsqueda + lista filtrada en vivo.
/// Enter ejecuta la acción seleccionada; Esc cierra.
class CommandPalette final : public QDialog {
    Q_OBJECT
public:
    explicit CommandPalette(const ActionRegistry* registry, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void refresh(const QString& query);
    void runSelected();

    const ActionRegistry* registry_;
    QLineEdit* search_{nullptr};
    QListWidget* list_{nullptr};
};

} // namespace beamlab::ui
