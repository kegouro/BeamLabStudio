#include "ui/qt/presenters/ExportPresenter.h"

#include <QMetaObject>
#include <Qt>

namespace beamlab::ui {

ExportPresenter::ExportPresenter(
    services::export_::ExporterRegistry* exporters,
    services::analysis::AnalysisOrchestrator* orchestrator,
    QObject* parent)
    : QObject(parent)
    , exporters_(exporters)
    , orchestrator_(orchestrator)
{
}

QStringList ExportPresenter::availableFormats() const
{
    if (!exporters_) return {"csv", "obj", "parquet"};
    QStringList formats;
    for (const auto& f : exporters_->availableFormats())
        formats << QString::fromStdString(f);
    return formats;
}

void ExportPresenter::exportAll(
    const QString& outputDir, const QStringList& formats)
{
    if (!exporters_) return;

    std::vector<std::string> fmtList;
    for (const auto& f : formats)
        fmtList.push_back(f.toStdString());

    // TODO: run export asynchronously via QtConcurrent or std::thread.
    // For now, emit completion as a placeholder.
    emit exportProgress(0, static_cast<int>(fmtList.size()));

    QMetaObject::invokeMethod(this, [this, fmtList]() {
        emit exportProgress(static_cast<int>(fmtList.size()),
                            static_cast<int>(fmtList.size()));
        emit exportCompleted(true,
            QString("Exported %1 format(s)").arg(fmtList.size()));
    }, Qt::QueuedConnection);
}

void ExportPresenter::exportSingle(
    const QString& format, const QString& outputPath)
{
    Q_UNUSED(format);
    Q_UNUSED(outputPath);
    // TODO: delegate to exporters_->exportSingle().
    emit exportCompleted(true, "Export placeholder");
}

} // namespace beamlab::ui
