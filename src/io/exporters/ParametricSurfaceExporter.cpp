#include "io/exporters/ParametricSurfaceExporter.h"

#include "analysis/surfaces/ContourResampler.h"
#include "core/foundation/Error.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numbers>

namespace beamlab::io {
namespace {

beamlab::core::Error makeError(beamlab::core::StatusCode code,
                               const std::string& message,
                               const std::string& details)
{
    return beamlab::core::Error{
        .code = code,
        .severity = beamlab::core::Severity::Error,
        .message = message,
        .details = details
    };
}

void ensureParentDirectory(const std::string& path)
{
    const std::filesystem::path p(path);
    const auto parent = p.parent_path();

    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

void writeVec3(std::ofstream& output,
               const std::string& name,
               const beamlab::core::Vec3& v)
{
    output << name << " = ("
           << std::scientific << std::setprecision(12)
           << v.x << ", " << v.y << ", " << v.z << ")\n";
}

beamlab::core::Vec3 add(const beamlab::core::Vec3& a,
                        const beamlab::core::Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

beamlab::core::Vec3 scale(const beamlab::core::Vec3& v,
                          const double factor)
{
    return {v.x * factor, v.y * factor, v.z * factor};
}

beamlab::core::Vec3 fromLocalToWorld(const beamlab::core::Vec2& uv,
                                     const double axial,
                                     const beamlab::data::AxisFrame& frame)
{
    auto position = frame.origin;
    position = add(position, scale(frame.transverse_u, uv.x));
    position = add(position, scale(frame.transverse_v, uv.y));
    position = add(position, scale(frame.longitudinal, axial));
    return position;
}

beamlab::core::Vec2 centroid2D(const std::vector<beamlab::core::Vec2>& points)
{
    beamlab::core::Vec2 center{};

    if (points.empty()) {
        return center;
    }

    for (const auto& p : points) {
        center.x += p.x;
        center.y += p.y;
    }

    center.x /= static_cast<double>(points.size());
    center.y /= static_cast<double>(points.size());

    return center;
}

double lensThickness(const double rho,
                     const beamlab::analysis::LensDiskBuildParameters& parameters)
{
    const double clamped = std::max(0.0, std::min(1.0, rho));
    const double blend = 1.0 - clamped * clamped;

    return parameters.edge_thickness_m +
           (parameters.center_thickness_m - parameters.edge_thickness_m) * blend;
}

} // namespace

std::string ParametricSurfaceExporter::name() const
{
    return "ParametricSurfaceExporter";
}

ExportResult ParametricSurfaceExporter::exportCausticParametricDescription(
    const std::vector<beamlab::data::BeamEnvelope>& envelopes,
    const beamlab::data::AxisFrame& axis_frame,
    const beamlab::analysis::SurfaceBuildParameters& parameters,
    const std::string& txt_path,
    const std::string& csv_path) const
{
    ExportResult result{};

    std::vector<beamlab::data::BeamEnvelope> valid_envelopes{};
    valid_envelopes.reserve(envelopes.size());

    for (const auto& envelope : envelopes) {
        if (envelope.valid && envelope.boundary_points.size() >= 3) {
            valid_envelopes.push_back(envelope);
        }
    }

    if (valid_envelopes.size() < 2 || parameters.resample_point_count < 3) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::InvalidArgument,
            "No hay suficientes contornos válidos para parametrizar la caústica",
            txt_path
        );
        return result;
    }

    std::sort(valid_envelopes.begin(), valid_envelopes.end(),
              [](const auto& a, const auto& b) {
                  return a.axial_position_m < b.axial_position_m;
              });

    ensureParentDirectory(txt_path);
    ensureParentDirectory(csv_path);

    std::ofstream txt(txt_path);
    std::ofstream csv(csv_path);

    if (!txt || !csv) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudieron crear los archivos de parametrización de la caústica",
            txt_path + " | " + csv_path
        );
        return result;
    }

    txt << "BeamLabStudio parametric surface description\n";
    txt << "Surface: beam_caustic_surface\n";
    txt << "Meaning: envolvente 3D del haz alrededor del foco.\n\n";

    txt << "Basis vectors in world coordinates:\n";
    writeVec3(txt, "O", axis_frame.origin);
    writeVec3(txt, "eL_longitudinal", axis_frame.longitudinal);
    writeVec3(txt, "eU_transverse", axis_frame.transverse_u);
    writeVec3(txt, "eV_transverse", axis_frame.transverse_v);

    txt << "\nDiscrete piecewise-parametric definition:\n";
    txt << "Let M be the number of valid axial rings and N the number of resampled points per ring.\n";
    txt << "Here M = " << valid_envelopes.size() << " and N = "
        << parameters.resample_point_count << ".\n\n";

    txt << "For each axial ring i, the CSV stores:\n";
    txt << "s_i = axial_position_m\n";
    txt << "C_i(theta_j) = (u_ij, v_ij), with theta_j = 2*pi*j/N\n\n";

    txt << "For lambda in [i, i+1], alpha = lambda - i:\n";
    txt << "s(lambda) = (1-alpha)*s_i + alpha*s_{i+1}\n";
    txt << "u(lambda,theta) = (1-alpha)*u_i(theta) + alpha*u_{i+1}(theta)\n";
    txt << "v(lambda,theta) = (1-alpha)*v_i(theta) + alpha*v_{i+1}(theta)\n\n";

    txt << "World-space parametrization:\n";
    txt << "R_caustic(lambda,theta) = O + s(lambda)*eL_longitudinal\n";
    txt << "                           + u(lambda,theta)*eU_transverse\n";
    txt << "                           + v(lambda,theta)*eV_transverse\n\n";

    txt << "Parameter domain:\n";
    txt << "lambda in [0, M-1]\n";
    txt << "theta in [0, 2*pi)\n";

    csv << "surface,slice_local_index,theta_index,theta_rad,axial_m,u_m,v_m,x_m,y_m,z_m\n";

    const beamlab::analysis::ContourResampler resampler{};

    for (std::size_t i = 0; i < valid_envelopes.size(); ++i) {
        const auto& envelope = valid_envelopes[i];
        const auto ring = resampler.resampleClosedContour(
            envelope.boundary_points,
            parameters.resample_point_count
        );

        for (std::size_t j = 0; j < ring.size(); ++j) {
            const double theta = 2.0 * std::numbers::pi *
                                 static_cast<double>(j) /
                                 static_cast<double>(ring.size());

            const auto world = fromLocalToWorld(
                ring[j],
                envelope.axial_position_m,
                axis_frame
            );

            csv << "beam_caustic_surface,"
                << i << ','
                << j << ','
                << std::scientific << std::setprecision(12)
                << theta << ','
                << envelope.axial_position_m << ','
                << ring[j].x << ','
                << ring[j].y << ','
                << world.x << ','
                << world.y << ','
                << world.z << '\n';
        }
    }

    result.success = true;
    result.output_path = txt_path;
    return result;
}

ExportResult ParametricSurfaceExporter::exportLensDiskParametricDescription(
    const beamlab::data::BeamEnvelope& focal_envelope,
    const beamlab::data::AxisFrame& axis_frame,
    const beamlab::analysis::LensDiskBuildParameters& parameters,
    const std::string& txt_path,
    const std::string& csv_path) const
{
    ExportResult result{};

    if (!focal_envelope.valid ||
        focal_envelope.boundary_points.size() < 3 ||
        parameters.boundary_point_count < 8) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::InvalidArgument,
            "No hay un contorno focal válido para parametrizar el disco-lente",
            txt_path
        );
        return result;
    }

    ensureParentDirectory(txt_path);
    ensureParentDirectory(csv_path);

    std::ofstream txt(txt_path);
    std::ofstream csv(csv_path);

    if (!txt || !csv) {
        result.success = false;
        result.error = makeError(
            beamlab::core::StatusCode::IOError,
            "No se pudieron crear los archivos de parametrización del disco-lente",
            txt_path + " | " + csv_path
        );
        return result;
    }

    const beamlab::analysis::ContourResampler resampler{};
    const auto boundary = resampler.resampleClosedContour(
        focal_envelope.boundary_points,
        parameters.boundary_point_count
    );

    const auto center_uv = centroid2D(boundary);

    txt << "BeamLabStudio parametric surface description\n";
    txt << "Surface: effective_lens_disk\n";
    txt << "Meaning: lente efectiva cerrada construida desde el contorno del slice focal.\n\n";

    txt << "Basis vectors in world coordinates:\n";
    writeVec3(txt, "O", axis_frame.origin);
    writeVec3(txt, "eL_longitudinal", axis_frame.longitudinal);
    writeVec3(txt, "eU_transverse", axis_frame.transverse_u);
    writeVec3(txt, "eV_transverse", axis_frame.transverse_v);

    txt << "\nFocal plane data:\n";
    txt << "s_f = " << std::scientific << std::setprecision(12)
        << focal_envelope.axial_position_m << "\n";
    txt << "center_uv = (" << center_uv.x << ", " << center_uv.y << ")\n";
    txt << "boundary point count N = " << boundary.size() << "\n";
    txt << "center_thickness_m = " << parameters.center_thickness_m << "\n";
    txt << "edge_thickness_m = " << parameters.edge_thickness_m << "\n\n";

    txt << "The CSV stores B(theta_j) = (u_j, v_j), with theta_j = 2*pi*j/N.\n\n";

    txt << "For rho in [0,1] and theta in [0,2*pi):\n";
    txt << "P_uv(rho,theta) = center_uv + rho*(B(theta) - center_uv)\n";
    txt << "h(rho) = edge_thickness + (center_thickness - edge_thickness)*(1 - rho^2)\n\n";

    txt << "Top and bottom biconvex sheets:\n";
    txt << "R_top(rho,theta) = O + (s_f + h(rho)/2)*eL_longitudinal\n";
    txt << "                   + P_uv.x(rho,theta)*eU_transverse\n";
    txt << "                   + P_uv.y(rho,theta)*eV_transverse\n\n";

    txt << "R_bottom(rho,theta) = O + (s_f - h(rho)/2)*eL_longitudinal\n";
    txt << "                      + P_uv.x(rho,theta)*eU_transverse\n";
    txt << "                      + P_uv.y(rho,theta)*eV_transverse\n\n";

    txt << "Lateral closing wall:\n";
    txt << "R_wall(theta,eta) = O + (s_f + eta*h(1)/2)*eL_longitudinal\n";
    txt << "                    + B.x(theta)*eU_transverse\n";
    txt << "                    + B.y(theta)*eV_transverse\n";
    txt << "with eta in [-1,1].\n";

    csv << "surface,theta_index,theta_rad,boundary_u_m,boundary_v_m,"
        << "top_x_m,top_y_m,top_z_m,bottom_x_m,bottom_y_m,bottom_z_m\n";

    const double boundary_thickness = lensThickness(1.0, parameters);

    for (std::size_t j = 0; j < boundary.size(); ++j) {
        const double theta = 2.0 * std::numbers::pi *
                             static_cast<double>(j) /
                             static_cast<double>(boundary.size());

        const auto top = fromLocalToWorld(
            boundary[j],
            focal_envelope.axial_position_m + 0.5 * boundary_thickness,
            axis_frame
        );

        const auto bottom = fromLocalToWorld(
            boundary[j],
            focal_envelope.axial_position_m - 0.5 * boundary_thickness,
            axis_frame
        );

        csv << "effective_lens_disk,"
            << j << ','
            << std::scientific << std::setprecision(12)
            << theta << ','
            << boundary[j].x << ','
            << boundary[j].y << ','
            << top.x << ','
            << top.y << ','
            << top.z << ','
            << bottom.x << ','
            << bottom.y << ','
            << bottom.z << '\n';
    }

    result.success = true;
    result.output_path = txt_path;
    return result;
}

} // namespace beamlab::io
