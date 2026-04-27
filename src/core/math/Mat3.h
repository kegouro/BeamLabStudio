#pragma once

namespace beamlab::core {

struct Mat3 {
    double m[3][3]{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    };
};

} // namespace beamlab::core
