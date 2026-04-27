#pragma once

#include "app/ServiceRegistry.h"

namespace beamlab::app {

class ApplicationContext {
public:
    ApplicationContext() = default;

    ServiceRegistry& services();
    const ServiceRegistry& services() const;

private:
    ServiceRegistry m_services{};
};

} // namespace beamlab::app
