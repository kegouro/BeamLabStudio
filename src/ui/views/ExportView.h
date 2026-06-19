#pragma once

#include <QWidget>

class QPushButton;
class QListWidget;
class QTextEdit;

namespace beamlab::ui {

class ExportPresenter;

/// Export tab — format selection, preview, and batch export controls.
///
/// Layout:
///   ┌───────────────────────────────────────────────────┐
///   │  Available Formats                                │
///   │  ┌───────────────────────┐                        │
///   │  │ □ CSV                 │                        │
///   │  │ □ OBJ                 │                        │
///   │  │ □ Parquet (CSV stub)  │                        │
///   │  └───────────────────────┘                        │
///   │                                                   │
///   │  [ Export Selected ]  [ Export All ]              │
///   ├───────────────────────────────────────────────────┤
///   │  Last export log                                  │
///   │  ┌───────────────────────────────────────────────┐│
///   │  │ [timestamp] Exported 3/3 formats (42 KB)      ││
///   │  └───────────────────────────────────────────────┘│
///   └───────────────────────────────────────────────────┘
class ExportView final : public QWidget {
    Q_OBJECT

public:
    explicit ExportView(ExportPresenter* presenter,
                        QWidget* parent = nullptr);

    void refreshFormats();

signals:
    void requestExport(const std::vector<std::string>& formats);

private:
    void setupLayout();
    void connectSignals();
    void onExportSelected();
    void onExportAll();

    ExportPresenter* presenter_{nullptr};

    QListWidget*  formatList_{nullptr};
    QPushButton*  exportSelectedBtn_{nullptr};
    QPushButton*  exportAllBtn_{nullptr};
    QTextEdit*    logView_{nullptr};
};

} // namespace beamlab::ui
