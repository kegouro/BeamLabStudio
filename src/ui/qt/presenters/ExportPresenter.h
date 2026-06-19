#pragma once

#include "services/export/ExporterRegistry.h"
#include "services/analysis/AnalysisOrchestrator.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace beamlab::ui {

/// Bridges ExporterRegistry (C++17) with the Qt ExportView.
///
/// Caches the last AnalysisResult so exports can be triggered
/// independently of the analysis pipeline.
class ExportPresenter : public QObject {
    Q_OBJECT

public:
    ExportPresenter(
        services::export_::ExporterRegistry* exporters,
        services::analysis::AnalysisOrchestrator* orchestrator,
        QObject* parent = nullptr);

    [[nodiscard]] QStringList availableFormats() const;

public slots:
    void exportAll(const QString& outputDir, const QStringList& formats);
    void exportSingle(const QString& format, const QString& outputPath);

signals:
    void exportProgress(int fileIndex, int totalFiles);
    void exportCompleted(bool success, const QString& message);
    void availableFormatsChanged(const QStringList& formats);

private:
    services::export_::ExporterRegistry* exporters_;
    services::analysis::AnalysisOrchestrator* orchestrator_;
};

} // namespace beamlab::ui
