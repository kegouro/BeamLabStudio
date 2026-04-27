#pragma once

#include "geometry/model/MeshFace.h"
#include "geometry/model/MeshVertex.h"

#include <string>
#include <vector>

namespace beamlab::geometry {

struct Mesh {
    std::string name{};
    std::vector<MeshVertex> vertices{};
    std::vector<MeshFace> faces{};

    [[nodiscard]] bool empty() const
    {
        return vertices.empty() || faces.empty();
    }
};

} // namespace beamlab::geometry
