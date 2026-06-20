#pragma once

#include "domain/simulation/SimulationEngine.h"
#include "domain/materials/MaterialRegistry.h"
#include "domain/physics/ParticleRegistry.h"

// The real, working biosim types live in src/biosim/core
// (namespace beamlab::biosim).  The previously-included
// domain/simulation/BioSim{Config,Result}.h headers never existed —
// BioSimRunner is the engine this presenter drives.
#include "biosim/core/BioSimConfig.h"
#include "biosim/core/BioSimResult.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>

namespace beamlab::ui {

/// Bridges biological simulation (C++17) with the Qt BioSimView.
class BioSimPresenter : public QObject {
    Q_OBJECT

public:
    BioSimPresenter(
        domain::simulation::SimulationEngine* engine,
        domain::materials::MaterialRegistry* materials,
        domain::physics::ParticleRegistry* particles,
        QObject* parent = nullptr);

    domain::materials::MaterialRegistry* materialRegistry() const
        { return materials_; }
    domain::simulation::SimulationEngine* simulationEngine() const
        { return engine_; }

public slots:
    void runSimulation(const beamlab::biosim::BioSimConfig& config);
    void stopSimulation();
    void addCustomMaterial(const domain::materials::Material& mat);
    void removeCustomMaterial(const QString& name);

signals:
    void simulationProgress(int percent);
    void simulationCompleted(
        std::shared_ptr<beamlab::biosim::BioSimResult> result);
    void materialListUpdated(const QStringList& materialNames);

private:
    void onSimulationFinished();

    domain::simulation::SimulationEngine* engine_;
    domain::materials::MaterialRegistry* materials_;
    domain::physics::ParticleRegistry* particles_;

    // Async run via QtConcurrent; the watcher lets onStop() cancel and lets
    // us pull the BioSimResult on the Qt thread when the future finishes.
    QFutureWatcher<beamlab::biosim::BioSimResult>* watcher_{nullptr};

    // Set by stopSimulation(); checked in the progress callback (real
    // mid-run cancellation) and in onSimulationFinished() (ignore a late
    // result the user already abandoned).
    std::atomic<bool> cancelled_{false};
};

} // namespace beamlab::ui
