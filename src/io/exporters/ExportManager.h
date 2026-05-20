#pragma once

#include <QString>
#include <QStringList>

class QGraphicsScene;
class QWidget;

namespace beamlab::ui {
class StatsDashboardWidget;
} // namespace beamlab::ui

namespace beamlab::io {

class ExportManager {
public:
    ExportManager(const QString& runDir,
                  const QString& manifestPath,
                  const QString& beamlineObj,
                  beamlab::ui::StatsDashboardWidget* statsDashboard = nullptr);

    bool exportAssets(const QString& outDir, QStringList* messages) const;
    bool exportPlots(QGraphicsScene* trajScene,
                     QGraphicsScene* focalScene,
                     QGraphicsScene* envelopeScene,
                     const QString& outDir,
                     QStringList* messages) const;
    bool exportPdf(const QString& outPath, QStringList* messages) const;
    bool exportMp4(QWidget* sceneWidget,
                   const QString& outPath,
                   QStringList* messages) const;

private:
    QString runDir_;
    QString manifestPath_;
    QString beamlineObj_;
    beamlab::ui::StatsDashboardWidget* statsDashboard_{nullptr};
};

} // namespace beamlab::io
