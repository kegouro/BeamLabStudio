#pragma once

namespace beamlab::app {

class ServiceRegistry {
public:
    ServiceRegistry() = default;
    ~ServiceRegistry() = default;

    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;
    ServiceRegistry(ServiceRegistry&&) = default;
    ServiceRegistry& operator=(ServiceRegistry&&) = default;
};

} // namespace beamlab::app
