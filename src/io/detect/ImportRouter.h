#pragma once

#include "io/detect/ProbeResult.h"
#include "io/importers/IImporter.h"

#include <memory>
#include <vector>

namespace beamlab::io {

class ImportRouter {
public:
    void registerImporter(std::shared_ptr<IImporter> importer);

    [[nodiscard]] std::shared_ptr<IImporter> chooseBestImporter(const ProbeResult& probe_result) const;

private:
    std::vector<std::shared_ptr<IImporter>> m_importers{};
};

} // namespace beamlab::io
