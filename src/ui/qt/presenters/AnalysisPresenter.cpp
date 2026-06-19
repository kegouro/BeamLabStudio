#include "ui/qt/presenters/AnalysisPresenter.h"

#include "app/ApplicationBootstrap.h"
#include "app/CommandLineOptions.h"

#include <QDebug>
#include <QFileInfo>
#include <iostream>
#include <sstream>


namespace beamlab::ui {

AnalysisPresenter::AnalysisPresenter(QObject* parent)
    : QObject(parent)
{
}

void AnalysisPresenter::runAnalysis(const QString& csvPath,
                                     const QString& outputDir,
                                     const beamlab::core::AnalysisRunConfig& config)
{
    qDebug() << "[AnalysisPresenter] runAnalysis called:" << csvPath << "->" << outputDir;
    emit analysisStarted();
    auto* worker = QThread::create([this, csvPath, outputDir, config]() {
        runOnThread(csvPath.toStdString(), outputDir.toStdString(), config);
    });
    worker->start();
}

void AnalysisPresenter::runOnThread(const std::string& csvPath,
                                     const std::string& outputDir,
                                     const beamlab::core::AnalysisRunConfig& config)
{
    qDebug() << "[AnalysisPresenter::worker] Starting pipeline for:" << QString::fromStdString(csvPath)
             << "output:" << QString::fromStdString(outputDir);

    // Redirect stdout/stderr to capture output for UI log
    std::ostringstream captured;
    auto* oldCout = std::cout.rdbuf();
    auto* oldCerr = std::cerr.rdbuf();
    std::cout.rdbuf(captured.rdbuf());
    std::cerr.rdbuf(captured.rdbuf());

    bool success = false;
    QString outDir = QString::fromStdString(outputDir);

    try {
        std::vector<std::string> args = {"beamlab",
            "-i", csvPath,
            "-o", outputDir,
            "--preview-trajectories",
                std::to_string(kPreviewTrajectories),
            "--preview-samples-per-trajectory",
                std::to_string(kPreviewSamplesPerTraj)
        };
        std::vector<char*> argv;
        for (auto& a : args) argv.push_back(a.data());
        int argc = static_cast<int>(argv.size());

        beamlab::app::ApplicationBootstrap bootstrap;
        int result = bootstrap.run(argc, argv.data());

        success = (result == 0);
        qDebug() << "[AnalysisPresenter::worker] ApplicationBootstrap returned:" << result;

    } catch (const std::exception& e) {
        qDebug() << "[AnalysisPresenter::worker] Exception:" << e.what();
        success = false;
    }

    // Restore stdout/stderr
    std::cout.rdbuf(oldCout);
    std::cerr.rdbuf(oldCerr);

    // Emit captured log lines
    std::string logText = captured.str();
    if (!logText.empty()) {
        auto lines = QString::fromStdString(logText).split('\n');
        for (const auto& line : lines) {
            if (!line.trimmed().isEmpty()) {
                emit logLineReady(line);
            }
        }
    }

    // Emit success with the OUTPUT DIRECTORY (not manifest path)
    emit analysisFinished(success,
        success ? "Analysis complete" : "Analysis failed",
        success ? outDir : QString());

    qDebug() << "[AnalysisPresenter::worker] Emitted analysisFinished(success=" << success
             << "outputDir=" << outDir << ")";
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
    emit analysisFinished(success, QString::fromStdString(message), QString());
}

void AnalysisPresenter::onLogLine(const std::string& line)
{
    emit logLineReady(QString::fromStdString(line));
}

} // namespace beamlab::ui
