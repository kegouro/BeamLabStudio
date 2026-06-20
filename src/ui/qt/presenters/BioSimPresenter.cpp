#include "ui/qt/presenters/BioSimPresenter.h"

#include "biosim/core/BioSimRunner.h"

#include <QFuture>
#include <QMetaObject>
#include <Qt>
#include <QtConcurrent/QtConcurrent>

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
    watcher_ = new QFutureWatcher<beamlab::biosim::BioSimResult>(this);
    connect(watcher_, &QFutureWatcher<beamlab::biosim::BioSimResult>::finished,
            this, &BioSimPresenter::onSimulationFinished);
}

void BioSimPresenter::runSimulation(
    const beamlab::biosim::BioSimConfig& config)
{
    if (watcher_->isRunning()) return;  // a run is already in flight

    cancelled_.store(false);
    emit simulationProgress(0);

    // Run BioSimRunner on a worker thread — it is stateless / thread-safe.
    // The progress callback (1) marshals percent back to the Qt thread and
    // (2) returns false once stopSimulation() flips cancelled_, which makes
    // BioSimRunner::run abort and return result.valid = false. This is the
    // real mid-run cancellation BioSimWidget::onStop could not perform.
    QFuture<beamlab::biosim::BioSimResult> future =
        QtConcurrent::run([this, config]() -> beamlab::biosim::BioSimResult {
            beamlab::biosim::BioSimRunner runner;
            return runner.run(config, [this](int percent) -> bool {
                QMetaObject::invokeMethod(
                    this,
                    [this, percent]() { emit simulationProgress(percent); },
                    Qt::QueuedConnection);
                return !cancelled_.load();
            });
        });

    watcher_->setFuture(future);
}

void BioSimPresenter::stopSimulation()
{
    // The atomic flag is the real cancellation: the progress callback returns
    // false on the next tick, BioSimRunner::run aborts, the future finishes and
    // onSimulationFinished() discards the abandoned result.
    // ponytail: QtConcurrent::run futures aren't cancellable mid-compute, so we
    //   don't call watcher_->cancel(); the flag + finished-time guard is enough.
    cancelled_.store(true);
}

void BioSimPresenter::onSimulationFinished()
{
    // The future always completes — either with a real result or, if the user
    // hit Stop, with an aborted (valid == false) one we discard.
    if (cancelled_.load()) {
        cancelled_.store(false);
        return;
    }

    emit simulationProgress(100);
    emit simulationCompleted(
        std::make_shared<beamlab::biosim::BioSimResult>(watcher_->result()));
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
