#pragma once

#include "geometry/model/Mesh.h"
#include "io/common/ExportResult.h"
#include "io/exporters/IExporter.h"

#include <string>

namespace beamlab::io {

class MeshExporter final : public IExporter {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] ExportResult exportObj(const beamlab::geometry::Mesh& mesh,
                                         const std::string& output_path) const;

    [[nodiscard]] ExportResult exportObjPreview(const beamlab::geometry::Mesh& mesh,
                                                const std::string& output_path,
                                                double scale) const;
};

} // namespace beamlab::io
