#include "io/exporters/ExportManager.h"
#include "ui/qt/StatsDashboardWidget.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsScene>
#include <QRegularExpression>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QWidget>

#include <fstream>

namespace beamlab::io {

namespace {

bool copyFileOverwrite(const QString& source, const QString& destination,
                       QStringList* messages)
{
    if (source.isEmpty()) return false;
    const QFileInfo source_info(source);
    if (!source_info.exists() || !source_info.isFile()) {
        if (messages != nullptr) messages->push_back("Missing: " + source);
        return false;
    }
    QFileInfo dest_info(destination);
    if (!QDir().mkpath(dest_info.absolutePath())) {
        if (messages != nullptr) messages->push_back("Could not create: " + dest_info.absolutePath());
        return false;
    }
    if (QFileInfo::exists(destination)) QFile::remove(destination);
    if (!QFile::copy(source, destination)) {
        if (messages != nullptr) messages->push_back("Could not copy: " + source);
        return false;
    }
    return true;
}

bool copyDirectoryRecursive(const QString& source_dir, const QString& dest_dir,
                            QStringList* messages)
{
    QDir source(source_dir);
    if (!source.exists()) return false;
    if (!QDir().mkpath(dest_dir)) {
        if (messages != nullptr) messages->push_back("Could not create: " + dest_dir);
        return false;
    }
    bool ok = true;
    QDirIterator it(source.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString src = it.next();
        const QString rel = source.relativeFilePath(src);
        ok = copyFileOverwrite(src, QDir(dest_dir).filePath(rel), messages) && ok;
    }
    return ok;
}

bool copyFilesBySuffix(const QString& source_dir, const QString& dest_dir,
                       const QStringList& suffixes, QStringList* messages)
{
    QDir source(source_dir);
    if (!source.exists()) return false;
    if (!QDir().mkpath(dest_dir)) {
        if (messages != nullptr) messages->push_back("Could not create: " + dest_dir);
        return false;
    }
    bool copied_any = false;
    bool ok = true;
    QDirIterator it(source.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString src = it.next();
        const QString suffix = QFileInfo(src).suffix().toLower();
        if (!suffixes.contains(suffix)) continue;
        copied_any = true;
        const QString rel = source.relativeFilePath(src);
        ok = copyFileOverwrite(src, QDir(dest_dir).filePath(rel), messages) && ok;
    }
    return copied_any && ok;
}

bool exportScenePng(QGraphicsScene* scene, const QString& path,
                    const QSize& image_size, QStringList* messages)
{
    if (scene == nullptr || scene->sceneRect().isEmpty()) {
        if (messages != nullptr) messages->push_back("No scene to export: " + path);
        return false;
    }
    QImage image(image_size, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(13, 16, 22));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    scene->render(&painter, QRectF(QPointF(0.0, 0.0), QSizeF(image_size)),
                  scene->sceneRect(), Qt::KeepAspectRatio);
    painter.end();

    QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (messages != nullptr) messages->push_back("Could not create: " + info.absolutePath());
        return false;
    }
    if (!image.save(path, "PNG")) {
        if (messages != nullptr) messages->push_back("Could not write: " + path);
        return false;
    }
    return true;
}

} // namespace

ExportManager::ExportManager(const QString& runDir,
                             const QString& manifestPath,
                             const QString& beamlineObj,
                             beamlab::ui::StatsDashboardWidget* statsDashboard)
    : runDir_(runDir)
    , manifestPath_(manifestPath)
    , beamlineObj_(beamlineObj)
    , statsDashboard_(statsDashboard)
{
}

bool ExportManager::exportAssets(const QString& outDir, QStringList* messages) const
{
    if (!QDir().mkpath(outDir)) {
        if (messages != nullptr) messages->push_back("Could not create export directory: " + outDir);
        return false;
    }

    bool ok = true;
    const QDir run(runDir_);
    const QDir out(outDir);

    ok = copyFileOverwrite(manifestPath_, out.filePath("manifest/visualization_manifest.json"), messages) && ok;

    ok = copyDirectoryRecursive(run.filePath("tables"), out.filePath("csv/tables"), messages) && ok;

    ok = copyFilesBySuffix(run.filePath("visualization"), out.filePath("csv/visualization"),
                            {"csv"}, messages) && ok;

    if (statsDashboard_ != nullptr) {
        QString stats_error;
        if (!statsDashboard_->exportBeamRadiusSpreadsheet(
                out.filePath("csv/statistics/beam_radius_profile.csv"), &stats_error)) {
            ok = false;
            if (messages != nullptr) messages->push_back(stats_error);
        }
    }

    ok = copyFilesBySuffix(run.filePath("geometry"), out.filePath("models_3d"),
                            {"obj"}, messages) && ok;

    if (!beamlineObj_.isEmpty() && QFileInfo::exists(beamlineObj_)) {
        ok = copyFileOverwrite(beamlineObj_, out.filePath("models_3d/physical_beamline_geometry.obj"),
                                messages) && ok;
    }

    copyDirectoryRecursive(run.filePath("reports"), out.filePath("reports"), messages);
    copyDirectoryRecursive(run.filePath("equations"), out.filePath("equations"), messages);

    if (messages != nullptr) messages->push_back("CSV and 3D model assets exported to: " + outDir);
    return ok;
}

bool ExportManager::exportPlots(QGraphicsScene* trajScene,
                                 QGraphicsScene* focalScene,
                                 QGraphicsScene* envelopeScene,
                                 const QString& outDir,
                                 QStringList* messages) const
{
    if (!QDir().mkpath(outDir)) {
        if (messages != nullptr) messages->push_back("Could not create plot directory: " + outDir);
        return false;
    }

    bool ok = true;
    const QSize plot_size(1900, 1300);
    const QDir out(outDir);

    ok = exportScenePng(trajScene, out.filePath("beam_trajectories_z_vs_x.png"), plot_size, messages) && ok;
    ok = exportScenePng(focalScene, out.filePath("focal_slice_u_vs_v.png"), plot_size, messages) && ok;
    ok = exportScenePng(envelopeScene, out.filePath("focal_envelope_proxy_u_vs_v.png"), plot_size, messages) && ok;

    if (statsDashboard_ != nullptr) {
        QString stats_error;
        if (!statsDashboard_->exportChartsPng(out.filePath("statistics"), &stats_error)) {
            ok = false;
            if (messages != nullptr) messages->push_back(stats_error);
        }
    }

    if (messages != nullptr) messages->push_back("PNG plots exported to: " + outDir);
    return ok;
}

bool ExportManager::exportPdf(const QString& outPath, QStringList* messages) const
{
    if (statsDashboard_ == nullptr) {
        if (messages != nullptr) messages->push_back("Statistics dashboard is not available.");
        return false;
    }

    QString error;
    const bool ok = statsDashboard_->exportStatisticsPdf(outPath, &error);

    if (messages != nullptr) {
        messages->push_back(ok ? "Statistics PDF exported to: " + outPath : error);
    }
    return ok;
}

bool ExportManager::exportMp4(QWidget* sceneWidget,
                               const QString& outPath,
                               QStringList* messages) const
{
    if (sceneWidget == nullptr) {
        if (messages != nullptr) messages->push_back("Combined 3D viewer is not available.");
        return false;
    }

    const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg.isEmpty()) {
        if (messages != nullptr)
            messages->push_back("ffmpeg was not found. Install ffmpeg to export MP4 video.");
        return false;
    }

    QFileInfo output_info(outPath);
    if (!QDir().mkpath(output_info.absolutePath())) {
        if (messages != nullptr)
            messages->push_back("Could not create video directory: " + output_info.absolutePath());
        return false;
    }

    QDir temp_root(output_info.absoluteDir());
    const QString frame_dir = temp_root.filePath(
        ".beamlab_frames_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));
    if (!QDir().mkpath(frame_dir)) {
        if (messages != nullptr)
            messages->push_back("Could not create video frame directory: " + frame_dir);
        return false;
    }

    QDir frame_dir_obj(frame_dir);
    constexpr int frame_count = 120;
    bool frames_ok = true;

    for (int frame = 0; frame < frame_count; ++frame) {
        sceneWidget->update();
        {
            QEventLoop loop;
            loop.processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        const QPixmap frame_pixmap = sceneWidget->grab();
        const QString frame_path = frame_dir_obj.filePath(
            QString("frame_%1.png").arg(frame, 4, 10, QChar('0')));

        if (frame_pixmap.isNull() || !frame_pixmap.save(frame_path, "PNG")) {
            frames_ok = false;
            break;
        }
    }

    if (!frames_ok) {
        if (messages != nullptr) messages->push_back("Could not render all trajectory video frames.");
        frame_dir_obj.removeRecursively();
        return false;
    }

    if (QFileInfo::exists(outPath)) QFile::remove(outPath);

    QStringList args;
    args << "-y" << "-framerate" << "30"
         << "-i" << frame_dir_obj.filePath("frame_%04d.png")
         << "-pix_fmt" << "yuv420p"
         << "-vf" << "scale=trunc(iw/2)*2:trunc(ih/2)*2"
         << outPath;

    QProcess process;
    process.setProgram(ffmpeg);
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForStarted(5000)) {
        if (messages != nullptr) messages->push_back("Could not start ffmpeg.");
        return false;
    }

    process.waitForFinished(-1);
    const QString log = QString::fromUtf8(process.readAllStandardOutput());

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (messages != nullptr) {
            messages->push_back("ffmpeg failed:\n" + log.right(2500));
            messages->push_back("Frames were left at: " + frame_dir);
        }
        return false;
    }

    frame_dir_obj.removeRecursively();

    if (messages != nullptr)
        messages->push_back("Trajectory MP4 exported to: " + outPath);

    return true;
}

} // namespace beamlab::io
