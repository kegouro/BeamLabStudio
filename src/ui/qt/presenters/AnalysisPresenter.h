#pragma once

#include "core/config/AnalysisConfig.h"
#include "core/pipeline/AnalysisPipeline.h"
#include "core/pipeline/ProgressTracker.h"

#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>
#include <memory>

namespace beamlab::ui {

class AnalysisPresenter : public QObject,
                          public beamlab::core::ProgressTracker
{
    Q_OBJECT

public:
    explicit AnalysisPresenter(QObject* parent = nullptr);

    void runAnalysis(const QString& csvPath,
                     const QString& outputDir,
                     const beamlab::core::AnalysisRunConfig& config);

    void cancelAnalysis();

    // ProgressTracker interface
    void onProgress(const beamlab::core::AnalysisProgress& p) override;
    void onComplete(bool success, const std::string& message) override;
    void onLogLine(const std::string& line) override;

signals:
    void progressUpdated(int percent, const QString& stage);
    void logLineReady(const QString& line);
    void analysisFinished(bool success, const QString& message,
                          const QString& outputDir);
    void analysisStarted();

private:
    void runOnThread(const std::string& csvPath,
                     const std::string& outputDir,
                     const beamlab::core::AnalysisRunConfig& config);

    std::unique_ptr<beamlab::core::AnalysisPipeline> pipeline_;
    QThread workerThread_;
};

} // namespace beamlab::ui
