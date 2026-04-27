#include "app/CommandLineOptions.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace beamlab::app {
namespace {

std::size_t parseSize(const std::string& value, const std::string& option_name)
{
    try {
        const auto parsed = std::stoull(value);
        return static_cast<std::size_t>(parsed);
    } catch (...) {
        throw std::runtime_error("Valor inválido para " + option_name + ": " + value);
    }
}

double parseDouble(const std::string& value, const std::string& option_name)
{
    try {
        return std::stod(value);
    } catch (...) {
        throw std::runtime_error("Valor inválido para " + option_name + ": " + value);
    }
}

std::string requireValue(int& i, int argc, char** argv, const std::string& option_name)
{
    if (i + 1 >= argc) {
        throw std::runtime_error("Falta valor para " + option_name);
    }

    ++i;
    return argv[i];
}

bool isAllowed(const std::string& value, const std::initializer_list<const char*> allowed)
{
    for (const auto* item : allowed) {
        if (value == item) {
            return true;
        }
    }

    return false;
}

} // namespace

CommandLineOptions CommandLineParser::parse(int argc, char** argv)
{
    CommandLineOptions options{};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
            return options;
        }

        if (arg == "--input" || arg == "-i") {
            options.input_file = requireValue(i, argc, argv, arg);
        } else if (arg == "--output" || arg == "-o") {
            options.output_directory = requireValue(i, argc, argv, arg);
        } else if (arg == "--axis") {
            options.axis_mode = requireValue(i, argc, argv, arg);

            if (!isAllowed(options.axis_mode, {"auto", "x", "y", "z", "-x", "-y", "-z"})) {
                throw std::runtime_error("Valor inválido para --axis: " + options.axis_mode);
            }
        } else if (arg == "--reference-mode") {
            options.reference_mode = requireValue(i, argc, argv, arg);

            if (!isAllowed(options.reference_mode, {"auto", "synchronized", "axial-bins"})) {
                throw std::runtime_error("Valor inválido para --reference-mode: " + options.reference_mode);
            }
        } else if (arg == "--binning") {
            options.binning_mode = requireValue(i, argc, argv, arg);

            if (!isAllowed(options.binning_mode, {"uniform", "equal-count"})) {
                throw std::runtime_error("Valor inválido para --binning: " + options.binning_mode);
            }
        } else if (arg == "--axial-bins") {
            options.axial_bin_count = parseSize(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--window") {
            options.half_window_frames = parseSize(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--caustic-points") {
            options.caustic_resample_points = parseSize(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--caustic-preview-scale") {
            options.caustic_preview_scale = parseDouble(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--lens-points") {
            options.lens_boundary_points = parseSize(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--lens-layers") {
            options.lens_radial_layers = parseSize(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--lens-center-thickness") {
            options.lens_center_thickness_m = parseDouble(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--lens-edge-thickness") {
            options.lens_edge_thickness_m = parseDouble(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--lens-preview-scale") {
            options.lens_preview_scale = parseDouble(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--preview-trajectories") {
            options.preview_trajectory_count = parseSize(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--preview-samples-per-trajectory") {
            options.preview_samples_per_trajectory = parseSize(requireValue(i, argc, argv, arg), arg);
        } else if (arg == "--preview-slice-points") {
            options.preview_slice_point_count = parseSize(requireValue(i, argc, argv, arg), arg);
        } else if (!arg.empty() && arg.front() == '-') {
            throw std::runtime_error("Opción desconocida: " + arg);
        } else {
            options.input_file = arg;
        }
    }

    return options;
}

void CommandLineParser::printHelp(const std::string& executable_name)
{
    std::cout
        << "BeamLabStudio\n\n"
        << "Uso:\n"
        << "  " << executable_name << " [archivo] [opciones]\n"
        << "  " << executable_name << " --input archivo.csv --output outputs/mi_corrida\n\n"
        << "Entrada:\n"
        << "  -i, --input <ruta>                  CSV COMSOL, CSV Geant4 o ROOT si está habilitado\n"
        << "  -o, --output <carpeta>              Carpeta de salida\n\n"
        << "Referencia y eje:\n"
        << "      --axis <modo>                   auto, x, y, z, -x, -y, -z\n"
        << "      --reference-mode <modo>         auto, synchronized, axial-bins\n"
        << "      --binning <modo>                uniform, equal-count\n"
        << "      --axial-bins <N>                Número de bins axiales\n"
        << "      --window <N>                    Frames/bins antes y después del foco\n\n"
        << "Superficie caústica del haz:\n"
        << "      --caustic-points <N>            Puntos por anillo axial\n"
        << "      --caustic-preview-scale <x>     Escala visual del preview\n\n"
        << "Disco-lente efectivo:\n"
        << "      --lens-points <N>               Puntos del borde del disco\n"
        << "      --lens-layers <N>               Capas radiales del disco\n"
        << "      --lens-center-thickness <m>     Espesor central en metros\n"
        << "      --lens-edge-thickness <m>       Espesor del borde en metros\n"
        << "      --lens-preview-scale <x>        Escala visual del preview\n\n"
        << "Datos livianos para UI:\n"
        << "      --preview-trajectories <N>       Trayectorias máximas en preview\n"
        << "      --preview-samples-per-trajectory <N>  Muestras máximas por trayectoria\n"
        << "      --preview-slice-points <N>       Puntos máximos del slice focal\n\n"
        << "Ayuda:\n"
        << "  -h, --help\n\n";
}

} // namespace beamlab::app
