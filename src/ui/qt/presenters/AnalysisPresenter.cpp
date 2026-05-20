#include "ui/qt/presenters/AnalysisPresenter.h"

#include "core/pipeline/AnalysisPipeline.h"

namespace beamlab::ui {

AnalysisPresenter::AnalysisPresenter(QObject* parent)
    : QObject(parent)
{
}

void AnalysisPresenter::runAnalysis(const QString& csvPath,
                                     const QString& outputDir,
                                     const beamlab::core::AnalysisRunConfig& config)
{
    emit analysisStarted();
    // Run in a worker thread to not freeze the UI
    auto* worker = QThread::create([this, csvPath, outputDir, config]() {
        runOnThread(csvPath.toStdString(), outputDir.toStdString(), config);
    });
    worker->start();
}

void AnalysisPresenter::runOnThread(const std::string& csvPath,
                                     const std::string& outputDir,
                                     const beamlab::core::AnalysisRunConfig& config)
{
    pipeline_ = std::make_unique<beamlab::core::AnalysisPipeline>(config);
    auto result = pipeline_->run(csvPath, outputDir, *this);
}

void AnalysisPresenter::cancelAnalysis()
{
    ProgressTracker::cancel();
}

void AnalysisPresenter::onProgress(const beamlab::core::AnalysisProgress& p)
{
    int pct = static_cast<int>(p.fraction * 100.0);
    emit progressUpdated(pct, QString::fromStdString(p.stage));
}

void AnalysisPresenter::onComplete(bool success, const std::string& message)
{
    emit analysisFinished(success, QString::fromStdString(message));
}

void AnalysisPresenter::onLogLine(const std::string& line)
{
    emit logLineReady(QString::fromStdString(line));
}

} // namespace beamlab::ui
