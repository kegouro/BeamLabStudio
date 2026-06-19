#include "ui/qt/presenters/BioSimPresenter.h"

#include <QMetaObject>
#include <Qt>
#include <QThread>

namespace beamlab::ui {

BioSimPresenter::BioSimPresenter(
    domain::simulation::SimulationEngine* engine,
    domain::materials::MaterialRegistry* materials,
    domain::physics::ParticleRegistry* particles,
    QObject* parent)
    : QObject(parent)
    , engine_(engine)
    , materials_(materials)
    , particles_(particles)
{
}

void BioSimPresenter::runSimulation(
    const domain::simulation::BioSimConfig& config)
{
    cancelled_.store(false);
    emit simulationProgress(0);

    auto* worker = QThread::create([this, config]() {
        // TODO: implement full BioSimRunner integration here.
        // For now, emit completion with an empty result.
        cancelled_.store(false);

        QMetaObject::invokeMethod(this, [this]() {
            emit simulationProgress(100);
            emit simulationCompleted(
                std::make_shared<domain::simulation::BioSimResult>());
        }, Qt::QueuedConnection);
    });
    worker->start();
}

void BioSimPresenter::stopSimulation()
{
    cancelled_.store(true);
}

void BioSimPresenter::addCustomMaterial(
    const domain::materials::Material& mat)
{
    if (!materials_) return;
    materials_->addCustom(mat);

    QStringList names;
    for (const auto& n : materials_->names())
        names << QString::fromStdString(n);
    emit materialListUpdated(names);
}

void BioSimPresenter::removeCustomMaterial(const QString& name)
{
    if (!materials_) return;
    materials_->removeCustom(name.toStdString());
}

} // namespace beamlab::ui
