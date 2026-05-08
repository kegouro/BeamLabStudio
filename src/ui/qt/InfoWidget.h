#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTextBrowser;

namespace beamlab::ui {

// Pestaña "Info" — muestra la documentación matemática generada automáticamente
// a partir de los bloques //!@math-begin en el código fuente.
//
// Al cambiar una fórmula en el código y recompilar, el script
// scripts/generate_math_docs.py re-genera assets/math_docs.html y CMake lo
// recompila en el QRC, por lo que al abrir la nueva versión del binario el
// contenido aquí es siempre consistente con la implementación.
class InfoWidget final : public QWidget {
    Q_OBJECT

public:
    explicit InfoWidget(QWidget* parent = nullptr);

private slots:
    void jumpToModule(int index);

private:
    void buildUi();
    void loadDocs();

    QTextBrowser* browser_{nullptr};
    QComboBox*    module_jump_{nullptr};
    QLabel*       source_label_{nullptr};
};

} // namespace beamlab::ui
