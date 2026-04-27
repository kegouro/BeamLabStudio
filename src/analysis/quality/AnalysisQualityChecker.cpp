#include "analysis/quality/AnalysisQualityChecker.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace beamlab::analysis {
namespace {

std::string lower(std::string text)
{
    for (auto& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return text;
}

bool containsAnySyntheticMarker(const std::string& text)
{
    const auto s = lower(text);

    return s.find("synthetic") != std::string::npos ||
           s.find("internal_demo") != std::string::npos ||
           s.find("fake") != std::string::npos ||
           s.find("mock") != std::string::npos ||
           s.find("dummy") != std::string::npos ||
           s.find("toy") != std::string::npos;
}

void addIssue(AnalysisQualityReport& report,
              const QualitySeverity severity,
              std::string code,
              std::string title,
              std::string detail,
              std::string suggestion)
{
    report.issues.push_back(QualityIssue{
        .severity = severity,
        .code = std::move(code),
        .title = std::move(title),
        .detail = std::move(detail),
        .suggestion = std::move(suggestion)
    });

    switch (severity) {
    case QualitySeverity::Info:
        ++report.info_count;
        break;
    case QualitySeverity::Warning:
        ++report.warning_count;
        break;
    case QualitySeverity::Error:
        ++report.error_count;
        break;
    case QualitySeverity::Critical:
        ++report.critical_count;
        break;
    }
}

void finalizeVerdict(AnalysisQualityReport& report)
{
    if (report.critical_count > 0) {
        report.verdict = "FAIL";
    } else if (report.error_count > 0) {
        report.verdict = "ERROR";
    } else if (report.warning_count > 0) {
        report.verdict = "WARNING";
    } else {
        report.verdict = "PASS";
    }
}

std::size_t totalSampleCount(const beamlab::data::TrajectoryDataset& dataset)
{
    std::size_t total = 0;

    for (const auto& trajectory : dataset.trajectories) {
        total += trajectory.samples.size();
    }

    return total;
}

double median(std::vector<double> values)
{
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());

    const std::size_t n = values.size();
    if (n % 2 == 1) {
        return values[n / 2];
    }

    return 0.5 * (values[n / 2 - 1] + values[n / 2]);
}

double medianCountFromFocusCurve(const beamlab::data::FocusResult& focus)
{
    std::vector<double> counts{};
    counts.reserve(focus.curve.points.size());

    for (const auto& point : focus.curve.points) {
        counts.push_back(static_cast<double>(point.point_count));
    }

    return median(std::move(counts));
}

std::size_t validEnvelopeCount(const std::vector<beamlab::data::BeamEnvelope>& envelopes)
{
    std::size_t count = 0;

    for (const auto& envelope : envelopes) {
        if (envelope.valid) {
            ++count;
        }
    }

    return count;
}


double medianEnvelopeInputPointCount(const std::vector<beamlab::data::BeamEnvelope>& envelopes)
{
    std::vector<double> counts{};
    counts.reserve(envelopes.size());

    for (const auto& envelope : envelopes) {
        if (envelope.input_point_count > 0) {
            counts.push_back(static_cast<double>(envelope.input_point_count));
        }
    }

    return median(std::move(counts));
}

double maxEnvelopeInputPointCount(const std::vector<beamlab::data::BeamEnvelope>& envelopes)
{
    double max_count = 0.0;

    for (const auto& envelope : envelopes) {
        max_count = std::max(max_count, static_cast<double>(envelope.input_point_count));
    }

    return max_count;
}

double minPositiveEnvelopeInputPointCount(const std::vector<beamlab::data::BeamEnvelope>& envelopes)
{
    double min_count = std::numeric_limits<double>::infinity();

    for (const auto& envelope : envelopes) {
        if (envelope.input_point_count > 0) {
            min_count = std::min(min_count, static_cast<double>(envelope.input_point_count));
        }
    }

    return std::isfinite(min_count) ? min_count : 0.0;
}

double minimumEnvelopeArea(const std::vector<beamlab::data::BeamEnvelope>& envelopes)
{
    double min_area = std::numeric_limits<double>::infinity();

    for (const auto& envelope : envelopes) {
        if (envelope.valid && envelope.area_m2 > 0.0) {
            min_area = std::min(min_area, envelope.area_m2);
        }
    }

    return std::isfinite(min_area) ? min_area : 0.0;
}

const beamlab::data::BeamEnvelope* findEnvelopeBySlice(
    const std::vector<beamlab::data::BeamEnvelope>& envelopes,
    const std::size_t slice_index)
{
    for (const auto& envelope : envelopes) {
        if (envelope.slice_index == slice_index) {
            return &envelope;
        }
    }

    return nullptr;
}

bool allTrajectoriesHaveSameSampleCount(const beamlab::data::TrajectoryDataset& dataset)
{
    if (dataset.trajectories.empty()) {
        return false;
    }

    const std::size_t reference = dataset.trajectories.front().samples.size();

    for (const auto& trajectory : dataset.trajectories) {
        if (trajectory.samples.size() != reference) {
            return false;
        }
    }

    return true;
}

} // namespace

AnalysisQualityReport AnalysisQualityChecker::check(
    const beamlab::data::TrajectoryDataset& dataset,
    const beamlab::data::FocusResult& focus,
    const std::vector<beamlab::data::BeamEnvelope>& envelopes,
    const beamlab::data::LensSurfaceModel& caustic_surface,
    const beamlab::data::LensSurfaceModel& lens_disk) const
{
    AnalysisQualityReport report{};

    const auto source_blob =
        dataset.metadata.display_name + " " +
        dataset.metadata.source.original_path + " " +
        dataset.metadata.source.source_type + " " +
        dataset.metadata.import.importer_name;

    if (containsAnySyntheticMarker(source_blob)) {
        addIssue(
            report,
            QualitySeverity::Critical,
            "synthetic_source_marker",
            "La fuente parece sintética o de demostración",
            "Se detectaron marcadores como synthetic/internal_demo/fake/mock/dummy/toy en nombre, ruta, source_type o importer.",
            "Usar un archivo de simulación real exportado por COMSOL, Geant4 u otro motor externo."
        );
    } else {
        addIssue(
            report,
            QualitySeverity::Info,
            "no_synthetic_marker_detected",
            "No se detectaron marcadores obvios de data artificial",
            "La metadata no contiene etiquetas típicas de datos sintéticos o de prueba.",
            "Esto no prueba autenticidad absoluta, pero descarta señales simples de dataset artificial."
        );
    }

    if (dataset.metadata.source.original_path.empty() &&
        dataset.metadata.source.source_type != "internal_demo") {
        addIssue(
            report,
            QualitySeverity::Warning,
            "missing_source_path",
            "No hay ruta de origen registrada",
            "El dataset no conserva una ruta de archivo de entrada.",
            "Guardar siempre source_path para trazabilidad y reproducibilidad."
        );
    }

    if (!dataset.metadata.source.original_path.empty() &&
        dataset.metadata.source.source_type != "internal_demo") {
        const std::filesystem::path source_path(dataset.metadata.source.original_path);

        if (!std::filesystem::exists(source_path)) {
            addIssue(
                report,
                QualitySeverity::Warning,
                "source_file_not_found",
                "El archivo fuente ya no existe en la ruta registrada",
                "source_path=" + dataset.metadata.source.original_path,
                "Conservar el archivo original junto con los resultados para reproducibilidad."
            );
        } else {
            const auto bytes = std::filesystem::file_size(source_path);

            if (bytes < 1024) {
                addIssue(
                    report,
                    QualitySeverity::Warning,
                    "source_file_very_small",
                    "El archivo fuente es muy pequeño",
                    "file_size_bytes=" + std::to_string(bytes),
                    "Verificar que no sea un archivo de prueba, cabecera vacía o export incompleto."
                );
            } else {
                addIssue(
                    report,
                    QualitySeverity::Info,
                    "source_file_present",
                    "Archivo fuente localizado",
                    "file_size_bytes=" + std::to_string(bytes),
                    "La corrida conserva referencia a un archivo externo existente."
                );
            }
        }
    }

    if (dataset.metadata.source.source_type != "comsol_csv" &&
        dataset.metadata.source.source_type != "geant4_csv" &&
        dataset.metadata.source.source_type != "root_native" &&
        dataset.metadata.source.source_type != "internal_demo") {
        addIssue(
            report,
            QualitySeverity::Warning,
            "unknown_source_type",
            "Tipo de fuente no reconocido como COMSOL o Geant4",
            "source_type=" + dataset.metadata.source.source_type,
            "Revisar que el importador haya clasificado correctamente el archivo."
        );
    }

    const std::size_t trajectory_count = dataset.trajectories.size();
    const std::size_t sample_count = totalSampleCount(dataset);

    if (trajectory_count < 10) {
        addIssue(
            report,
            QualitySeverity::Error,
            "too_few_trajectories",
            "Muy pocas trayectorias",
            "trajectory_count=" + std::to_string(trajectory_count),
            "Usar más partículas o revisar si el importador agrupó mal las trayectorias."
        );
    }

    if (sample_count < trajectory_count * 2) {
        addIssue(
            report,
            QualitySeverity::Error,
            "too_few_samples",
            "Muy pocas muestras por trayectoria",
            "sample_count=" + std::to_string(sample_count) +
                ", trajectory_count=" + std::to_string(trajectory_count),
            "Revisar el archivo de entrada o el parser."
        );
    }

    if (sample_count > 0 && trajectory_count > 0) {
        const double mean_samples =
            static_cast<double>(sample_count) / static_cast<double>(trajectory_count);

        if (mean_samples < 5.0) {
            addIssue(
                report,
                QualitySeverity::Warning,
                "low_mean_samples_per_trajectory",
                "Pocas muestras por trayectoria",
                "mean_samples_per_trajectory=" + std::to_string(mean_samples),
                "Aumentar resolución de salida o revisar filtros del exportador."
            );
        }
    }

    if (allTrajectoriesHaveSameSampleCount(dataset) &&
        dataset.metadata.source.source_type == "geant4_csv") {
        addIssue(
            report,
            QualitySeverity::Warning,
            "geant4_perfectly_synchronized",
            "Geant4 aparece perfectamente sincronizado",
            "Todas las trayectorias tienen el mismo número de pasos. Esto puede ser válido, pero es poco típico en stepping Geant4 real.",
            "Verificar que el archivo no haya sido remuestreado artificialmente."
        );
    }

    if (dataset.axis_frame.confidence < 0.30) {
        addIssue(
            report,
            QualitySeverity::Warning,
            "low_axis_confidence",
            "Baja confianza en el eje longitudinal",
            "axis_confidence=" + std::to_string(dataset.axis_frame.confidence),
            "Permitir selección manual del eje o revisar la geometría del dataset."
        );
    } else if (dataset.axis_frame.confidence < 0.50) {
        addIssue(
            report,
            QualitySeverity::Info,
            "moderate_axis_confidence",
            "Confianza moderada en el eje longitudinal",
            "axis_confidence=" + std::to_string(dataset.axis_frame.confidence),
            "Revisar visualmente el eje o agregar configuración manual si el resultado parece rotado."
        );
    }

    if (!focus.valid) {
        addIssue(
            report,
            QualitySeverity::Critical,
            "invalid_focus",
            "No se detectó foco válido",
            "FocusResult.valid=false.",
            "Revisar curva focal, eje longitudinal y agrupamiento de muestras."
        );
    } else {
        if (focus.focus_metric_value <= 0.0 || !std::isfinite(focus.focus_metric_value)) {
            addIssue(
                report,
                QualitySeverity::Error,
                "nonphysical_focus_radius",
                "Radio focal no físico",
                "focus_metric_value=" + std::to_string(focus.focus_metric_value),
                "Un radio cero, negativo o no finito suele indicar degeneración o agrupamiento incorrecto."
            );
        }

        if (focus.focus_index < 3 ||
            focus.focus_index + 3 >= focus.curve.points.size()) {
            addIssue(
                report,
                QualitySeverity::Warning,
                "focus_near_domain_boundary",
                "El foco está cerca del borde del dominio analizado",
                "focus_index=" + std::to_string(focus.focus_index) +
                    ", curve_points=" + std::to_string(focus.curve.points.size()),
                "Extender la simulación o revisar que el mínimo no esté cortado por el rango de datos."
            );
        }

        if (focus.focus_index < focus.curve.points.size()) {
            const auto& focal_point = focus.curve.points[focus.focus_index];

            if (focal_point.point_count < 50) {
                addIssue(
                    report,
                    QualitySeverity::Error,
                    "too_few_points_at_focus",
                    "Muy pocos puntos en el corte focal",
                    "focus_point_count=" + std::to_string(focal_point.point_count),
                    "Aumentar partículas, ajustar bins axiales o revisar el exportador."
                );
            } else if (focal_point.point_count < 1000) {
                addIssue(
                    report,
                    QualitySeverity::Warning,
                    "limited_points_at_focus",
                    "Población limitada en el corte focal",
                    "focus_point_count=" + std::to_string(focal_point.point_count),
                    "El foco existe, pero la estadística local podría ser débil."
                );
            }

            const double median_count = medianCountFromFocusCurve(focus);

            if (median_count > 0.0) {
                const double ratio =
                    static_cast<double>(focal_point.point_count) / median_count;

                if (ratio < 0.05) {
                    addIssue(
                        report,
                        QualitySeverity::Warning,
                        "focus_population_extremely_low_vs_median",
                        "El corte focal tiene muy pocos puntos comparado con la curva global",
                        "focus_count=" + std::to_string(focal_point.point_count) +
                            ", median_count=" + std::to_string(median_count),
                        "Usar bins adaptativos o revisar densidad de stepping cerca del foco."
                    );
                } else if (ratio < 0.20) {
                    addIssue(
                        report,
                        QualitySeverity::Info,
                        "focus_population_low_vs_median",
                        "El corte focal tiene menos puntos que la mediana global",
                        "focus_count=" + std::to_string(focal_point.point_count) +
                            ", median_count=" + std::to_string(median_count),
                        "No invalida el resultado, pero conviene inspeccionar."
                    );
                }
            }
        }

        if (focus.confidence <= 0.0 || !std::isfinite(focus.confidence)) {
            addIssue(
                report,
                QualitySeverity::Warning,
                "weak_focus_contrast",
                "Contraste focal débil o no finito",
                "confidence=" + std::to_string(focus.confidence),
                "El mínimo podría ser plano, ambiguo o ruidoso."
            );
        }
    }

    if (envelopes.size() < 3) {
        addIssue(
            report,
            QualitySeverity::Error,
            "too_few_envelopes",
            "Muy pocos contornos para reconstrucción",
            "envelope_count=" + std::to_string(envelopes.size()),
            "Usar una ventana focal mayor o revisar SlicePlanner."
        );
    }

    const std::size_t valid_envelopes = validEnvelopeCount(envelopes);
    if (valid_envelopes != envelopes.size()) {
        addIssue(
            report,
            QualitySeverity::Warning,
            "invalid_envelopes_present",
            "Hay contornos inválidos",
            "valid_envelopes=" + std::to_string(valid_envelopes) +
                ", total_envelopes=" + std::to_string(envelopes.size()),
            "Revisar slices con pocos puntos o contornos degenerados."
        );
    }

    const auto* focal_envelope = findEnvelopeBySlice(envelopes, focus.focus_index);
    if (focus.valid && focal_envelope == nullptr) {
        addIssue(
            report,
            QualitySeverity::Warning,
            "missing_focal_envelope",
            "No se encontró contorno asociado al índice focal",
            "focus_index=" + std::to_string(focus.focus_index),
            "La lente efectiva puede haberse construido con el contorno más cercano."
        );
    } else if (focal_envelope != nullptr && !focal_envelope->valid) {
        addIssue(
            report,
            QualitySeverity::Error,
            "invalid_focal_envelope",
            "El contorno focal es inválido",
            "slice_index=" + std::to_string(focal_envelope->slice_index),
            "No confiar en la lente efectiva hasta corregir el contorno focal."
        );
    } else if (focal_envelope != nullptr) {
        const double min_area = minimumEnvelopeArea(envelopes);
        if (min_area > 0.0 && focal_envelope->area_m2 > 1.25 * min_area) {
            addIssue(
                report,
                QualitySeverity::Warning,
                "focal_envelope_not_minimum_area",
                "El contorno focal no coincide con el área mínima",
                "focal_area=" + std::to_string(focal_envelope->area_m2) +
                    ", min_area=" + std::to_string(min_area),
                "Revisar si la métrica RMS y el área de contorno apuntan al mismo foco."
            );
        }
    }


    if (!envelopes.empty()) {
        const double median_window_count = medianEnvelopeInputPointCount(envelopes);
        const double min_window_count = minPositiveEnvelopeInputPointCount(envelopes);
        const double max_window_count = maxEnvelopeInputPointCount(envelopes);

        if (median_window_count > 0.0 && max_window_count > 0.0 && min_window_count > 0.0) {
            const double imbalance = max_window_count / min_window_count;

            if (imbalance > 50.0) {
                addIssue(
                    report,
                    QualitySeverity::Warning,
                    "high_slice_population_imbalance",
                    "Los cortes focales tienen poblaciones muy desbalanceadas",
                    "min_points=" + std::to_string(min_window_count) +
                        ", median_points=" + std::to_string(median_window_count) +
                        ", max_points=" + std::to_string(max_window_count),
                    "Esto es común en stepping asíncrono tipo Geant4, pero conviene usar bins adaptativos o revisar densidad local."
                );
            } else if (imbalance > 10.0) {
                addIssue(
                    report,
                    QualitySeverity::Info,
                    "moderate_slice_population_imbalance",
                    "Los cortes focales tienen poblaciones moderadamente desbalanceadas",
                    "min_points=" + std::to_string(min_window_count) +
                        ", median_points=" + std::to_string(median_window_count) +
                        ", max_points=" + std::to_string(max_window_count),
                    "No invalida el resultado, pero conviene revisar el reporte de contornos."
                );
            }
        }

        if (focal_envelope != nullptr && median_window_count > 0.0) {
            const double focal_ratio =
                static_cast<double>(focal_envelope->input_point_count) / median_window_count;

            if (focal_ratio < 0.05) {
                addIssue(
                    report,
                    QualitySeverity::Warning,
                    "focal_slice_population_low_vs_local_window",
                    "El corte focal tiene pocos puntos comparado con la ventana focal",
                    "focal_points=" + std::to_string(focal_envelope->input_point_count) +
                        ", local_median_points=" + std::to_string(median_window_count),
                    "El foco puede ser real, pero la reconstrucción local depende de menos estadística que los cortes vecinos."
                );
            } else if (focal_ratio < 0.20) {
                addIssue(
                    report,
                    QualitySeverity::Info,
                    "focal_slice_population_moderately_low_vs_local_window",
                    "El corte focal tiene menos puntos que la mediana local",
                    "focal_points=" + std::to_string(focal_envelope->input_point_count) +
                        ", local_median_points=" + std::to_string(median_window_count),
                    "Inspeccionar visualmente la lente efectiva y la curva focal."
                );
            }
        }
    }

    if (!caustic_surface.valid || caustic_surface.mesh.empty()) {
        addIssue(
            report,
            QualitySeverity::Error,
            "invalid_caustic_surface",
            "La superficie caústica es inválida",
            "vertices=" + std::to_string(caustic_surface.mesh.vertices.size()) +
                ", faces=" + std::to_string(caustic_surface.mesh.faces.size()),
            "Revisar contornos y SurfaceBuilder."
        );
    }

    if (!lens_disk.valid || lens_disk.mesh.empty()) {
        addIssue(
            report,
            QualitySeverity::Error,
            "invalid_lens_disk",
            "El disco-lente efectivo es inválido",
            "vertices=" + std::to_string(lens_disk.mesh.vertices.size()) +
                ", faces=" + std::to_string(lens_disk.mesh.faces.size()),
            "Revisar el contorno focal y LensDiskBuilder."
        );
    }

    addIssue(
        report,
        QualitySeverity::Info,
        "authenticity_limit",
        "Límite lógico del chequeo de artificialidad",
        "El software puede detectar inconsistencias, degeneraciones y marcadores sintéticos, pero no puede demostrar que una simulación no fue fabricada externamente.",
        "Conservar archivos fuente, parámetros de simulación, versión del código Geant4/COMSOL y metadata de corrida."
    );

    finalizeVerdict(report);
    return report;
}

} // namespace beamlab::analysis
