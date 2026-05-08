//!@math-begin module="FocusDetection" title="Detección del Foco — Mínimo de la Curva Métrica"
//!@math El foco se define como la posición axial s* donde la métrica M(s)
//!@math alcanza su mínimo global:
//!@formula s* = argmin_s { M(s) }   con M(s) = r_rms(s)  [por defecto]
//!@section Confianza del foco (contrast proxy)
//!@math Se mide el contraste alrededor del bin óptimo i*:
//!@formula C = [ M(i*−1) + M(i*+1) ] / 2 − M(i*)
//!@note C > 0 indica un mínimo real (pozo); C ≈ 0 indica curva plana (sin foco claro).
//!@note Para datos Geant4 de muones cósmicos (sin lente) el foco es el punto de
//!@note menor dispersión transversal, que puede estar en la entrada del volumen.
//!@section Métricas disponibles
//!@formula r_rms = √(σ_u² + σ_v²)   [radio RMS total — por defecto]
//!@formula σ_u   [desviación estándar en la dirección transversal u]
//!@formula σ_v   [desviación estándar en la dirección transversal v]
//!@math-end

#include "analysis/focus/FocusDetector.h"

#include <cmath>
#include <limits>

namespace beamlab::analysis {

beamlab::data::FocusResult FocusDetector::detect(const beamlab::data::FocusCurve& curve,
                                                 const FocusParameters&) const
{
    beamlab::data::FocusResult result{};
    result.curve = curve;
    result.detection_method = "minimum_metric";

    if (curve.points.empty()) {
        result.valid = false;
        result.confidence = 0.0;
        return result;
    }

    std::size_t best_index = 0;
    double best_value = std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < curve.points.size(); ++i) {
        const double metric_value = curve.points[i].metric_value;

        if (!std::isfinite(metric_value)) {
            continue;
        }

        if (metric_value < best_value) {
            best_value = curve.points[i].metric_value;
            best_index = i;
        }
    }

    if (!std::isfinite(best_value)) {
        result.valid = false;
        result.confidence = 0.0;
        return result;
    }

    result.focus_index = best_index;
    result.focus_reference_value = curve.points[best_index].reference_value;
    result.focus_metric_value = curve.points[best_index].metric_value;
    result.valid = true;

    if (curve.points.size() >= 3 && best_index > 0 && best_index + 1 < curve.points.size()) {
        const double left = curve.points[best_index - 1].metric_value;
        const double center = curve.points[best_index].metric_value;
        const double right = curve.points[best_index + 1].metric_value;

        if (std::isfinite(left) && std::isfinite(center) && std::isfinite(right)) {
            const double contrast = ((left - center) + (right - center)) * 0.5;
            result.confidence = contrast > 0.0 ? contrast : 0.0;
        } else {
            result.confidence = 0.1;
        }
    } else {
        result.confidence = 0.1;
    }

    return result;
}

} // namespace beamlab::analysis
