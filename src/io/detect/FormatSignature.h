#pragma once

namespace beamlab::io {

enum class FormatSignature {
    Unknown,
    TextTable,
    ComsolCsv,
    Geant4Csv,
    RootFile,
    MphFile,
    MeshFile,
    ProjectFile
};

} // namespace beamlab::io
