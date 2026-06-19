// ═════════════════════════════════════════════════════════════════════
//  BeamLabStudio Python bindings — pybind11 module
//
//  The compiled .so is named _core.so and imported by
//  src/scripting/__init__.py as:
//      from . import _core
//
//  This keeps the C++ module name separate from the Python package name,
//  allowing __init__.py to add Python-side helpers.
// ═════════════════════════════════════════════════════════════════════

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "domain/materials/Material.h"
#include "domain/materials/MaterialRegistry.h"
#include "domain/physics/Particle.h"
#include "domain/physics/ParticleRegistry.h"
#include "domain/simulation/SimulationEngine.h"
#include "core/config/ProfileManager.h"

namespace py = pybind11;
using namespace beamlab::domain::materials;
using namespace beamlab::domain::physics;
using namespace beamlab::domain::simulation;
using namespace beamlab::core;

// ── Helper: wrap import ProgressCallback as py::function ──────────
// Releases the GIL so Python threads can run during long imports.

void importWithProgress(AnalysisOrchestrator& self,
                        const std::string& path,
                        const AnalysisRunConfig& cfg,
                        py::function on_progress)
{
    // TODO: implement when AnalysisOrchestrator is available.
    // For now, placeholder that logs the path.
    py::print("[beamlab] import:", path);
}

// ═════════════════════════════════════════════════════════════════════
//  Python Module Definition
// ═════════════════════════════════════════════════════════════════════

PYBIND11_MODULE(_core, m)
{
    m.doc() = "BeamLabStudio — scientific analysis library for muon beam trajectory data";

    // ── Material struct ────────────────────────────────────────────
    py::class_<Element>(m, "Element")
        .def(py::init<>())
        .def_readwrite("symbol", &Element::symbol)
        .def_readwrite("Z", &Element::Z)
        .def_readwrite("massFraction", &Element::massFraction);

    py::class_<Material>(m, "Material")
        .def(py::init<>())
        .def_readwrite("id", &Material::id)
        .def_readwrite("name", &Material::name)
        .def_readwrite("density_g_cm3", &Material::density_g_cm3)
        .def_readwrite("Z_eff", &Material::Z_eff)
        .def_readwrite("A_eff", &Material::A_eff)
        .def_readwrite("meanExcitationEnergy_eV", &Material::meanExcitationEnergy_eV)
        .def_readwrite("radiationLength_cm", &Material::radiationLength_cm)
        .def_readwrite("composition", &Material::composition)
        .def("is_gas", &Material::isGas)
        .def("is_valid", &Material::isValid)
        .def("__repr__", [](const Material& m) {
            return "<Material: " + m.name + " (" + m.id + ")>";
        });

    // ── MaterialRegistry ──────────────────────────────────────────
    py::class_<MaterialRegistry>(m, "MaterialRegistry")
        .def(py::init<>())
        .def("load_builtin", &MaterialRegistry::loadBuiltinMaterials)
        .def("get", &MaterialRegistry::get,
             py::return_value_policy::reference_internal)
        .def("find", [](const MaterialRegistry& self, const std::string& id) {
            auto result = self.find(id);
            if (result.has_value())
                return py::cast(&result->get(),
                                 py::return_value_policy::reference_internal);
            return py::cast<py::none>(Py_None);
        })
        .def("names", &MaterialRegistry::names)
        .def("count", &MaterialRegistry::count)
        .def("add_custom", &MaterialRegistry::addCustom)
        .def("remove_custom", &MaterialRegistry::removeCustom)
        .def("__len__", &MaterialRegistry::count)
        .def("__getitem__", [](const MaterialRegistry& self,
                                const std::string& id) -> const Material& {
            return self.get(id);
        }, py::return_value_policy::reference_internal)
        .def("__contains__", [](const MaterialRegistry& self,
                                 const std::string& id) {
            return self.find(id).has_value();
        });

    // ── Particle struct ───────────────────────────────────────────
    py::class_<Particle>(m, "Particle")
        .def(py::init<>())
        .def_readwrite("name", &Particle::name)
        .def_readwrite("pdgCode", &Particle::pdgCode)
        .def_readwrite("mass_MeV", &Particle::mass_MeV)
        .def_readwrite("charge_e", &Particle::charge_e)
        .def_readwrite("spin", &Particle::spin)
        .def_readwrite("WR", &Particle::WR)
        .def("__repr__", [](const Particle& p) {
            return "<Particle: " + p.name + " (PDG " + std::to_string(p.pdgCode) + ")>";
        });

    // ── ParticleRegistry ──────────────────────────────────────────
    py::class_<ParticleRegistry>(m, "ParticleRegistry")
        .def(py::init<>())
        .def("load_builtin", &ParticleRegistry::loadBuiltinParticles)
        .def("get", py::overload_cast<const std::string&>(
                &ParticleRegistry::get, py::const_),
             py::return_value_policy::reference_internal)
        .def("get_by_pdg", py::overload_cast<int>(
                &ParticleRegistry::getByPdgCode, py::const_),
             py::return_value_policy::reference_internal)
        .def("names", &ParticleRegistry::names)
        .def("count", &ParticleRegistry::count)
        .def("__len__", &ParticleRegistry::count)
        .def("__getitem__", py::overload_cast<const std::string&>(
                &ParticleRegistry::get, py::const_),
             py::return_value_policy::reference_internal);

    // ── StepResult ────────────────────────────────────────────────
    py::class_<SimulationEngine::StepResult>(m, "StepResult")
        .def_readonly("dEdx_MeV_cm", &SimulationEngine::StepResult::dEdx_MeV_cm)
        .def_readonly("energyLoss_MeV", &SimulationEngine::StepResult::energyLoss_MeV)
        .def_readonly("mcsAngle_rad", &SimulationEngine::StepResult::mcsAngle_rad)
        .def_readonly("stragglingSigma_MeV",
                       &SimulationEngine::StepResult::stragglingSigma_MeV)
        .def("__repr__", [](const SimulationEngine::StepResult& s) {
            return "<StepResult: dE/dx=" + std::to_string(s.dEdx_MeV_cm) + " MeV/cm>";
        });

    // ── BraggCurve ────────────────────────────────────────────────
    py::class_<SimulationEngine::BraggCurve>(m, "BraggCurve")
        .def_readonly("depth_cm", &SimulationEngine::BraggCurve::depth_cm)
        .def_readonly("dEdx_MeV_cm", &SimulationEngine::BraggCurve::dEdx_MeV_cm)
        .def_readonly("peakDepth_cm", &SimulationEngine::BraggCurve::peakDepth_cm)
        .def_readonly("peakDEdx_MeV_cm",
                       &SimulationEngine::BraggCurve::peakDEdx_MeV_cm);

    // ── SimulationEngine ──────────────────────────────────────────
    py::class_<SimulationEngine>(m, "SimulationEngine")
        .def(py::init<const MaterialRegistry&, const ParticleRegistry&>(),
             py::keep_alive<1, 2>(), py::keep_alive<1, 3>())
        .def("compute_step", &SimulationEngine::computeStep,
             py::arg("kinE_MeV"), py::arg("stepLength_cm") = 0.01,
             py::arg("material_name"), py::arg("particle_name"))
        .def("compute_bragg_curve", &SimulationEngine::computeBraggCurve,
             py::arg("initialE_MeV"), py::arg("material_name"),
             py::arg("particle_name"), py::arg("n_steps") = 1000)
        .def("validate_against_nist", &SimulationEngine::validateAgainstNist,
             py::arg("material_name"), py::arg("particle_name"));

    // ── ValidationReport ──────────────────────────────────────────
    py::class_<SimulationEngine::ValidationReport>(m, "ValidationReport")
        .def_readonly("passed", &SimulationEngine::ValidationReport::passed)
        .def_readonly("referenceSource",
                       &SimulationEngine::ValidationReport::referenceSource)
        .def("__repr__", [](const SimulationEngine::ValidationReport& r) {
            return "<ValidationReport: " + std::string(r.passed ? "PASS" : "FAIL")
                   + " (" + r.referenceSource + ")>";
        });

    // ── ProfileManager ────────────────────────────────────────────
    py::class_<ProfileManager>(m, "ProfileManager")
        .def(py::init<>())
        .def("load_profile", &ProfileManager::loadProfile)
        .def("available_profiles", &ProfileManager::availableProfiles)
        .def("resolve", &ProfileManager::resolve,
             py::arg("profile_name"),
             py::arg("cli_overrides") = nlohmann::json::object());

    // ── Example usage ─────────────────────────────────────────────
    m.def("demo", []() {
        py::print("  BeamLabStudio Python module loaded successfully.");
        py::print("  Try: mats = beamlab.MaterialRegistry()");
        py::print("       mats.load_builtin()");
        py::print("       water = mats['water_icru']");
    });
}
