#include "app/ApplicationContext.h"

namespace beamlab::app {

ServiceRegistry& ApplicationContext::services()
{
    return m_services;
}

const ServiceRegistry& ApplicationContext::services() const
{
    return m_services;
}

} // namespace beamlab::app
