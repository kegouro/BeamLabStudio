#pragma once

#include "domain/simulation/SimulationEngine.h"
#include "domain/materials/MaterialRegistry.h"
#include "domain/physics/ParticleRegistry.h"
#include "domain/simulation/BioSimConfig.h"
#include "domain/simulation/BioSimResult.h"

#include <QObject>
#include <QString>
#include <QStringList>

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
    void runSimulation(const domain::simulation::BioSimConfig& config);
    void stopSimulation();
    void addCustomMaterial(const domain::materials::Material& mat);
    void removeCustomMaterial(const QString& name);

signals:
    void simulationProgress(int percent);
    void simulationCompleted(
        std::shared_ptr<domain::simulation::BioSimResult> result);
    void materialListUpdated(const QStringList& materialNames);

private:
    domain::simulation::SimulationEngine* engine_;
    domain::materials::MaterialRegistry* materials_;
    domain::physics::ParticleRegistry* particles_;

    std::atomic<bool> cancelled_{false};
};

} // namespace beamlab::ui
