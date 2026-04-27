#pragma once

#include "app/ApplicationContext.h"

namespace beamlab::app {

class ApplicationBootstrap {
public:
    ApplicationBootstrap() = default;
    int run(int argc, char** argv);

private:
    ApplicationContext m_context{};
};

} // namespace beamlab::app
