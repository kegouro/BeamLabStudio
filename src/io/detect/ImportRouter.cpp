#include "io/detect/ImportRouter.h"

namespace beamlab::io {

void ImportRouter::registerImporter(std::shared_ptr<IImporter> importer)
{
    if (importer) {
        m_importers.push_back(std::move(importer));
    }
}

std::shared_ptr<IImporter> ImportRouter::chooseBestImporter(const ProbeResult& probe_result) const
{
    std::shared_ptr<IImporter> best{};
    double best_confidence = -1.0;

    for (const auto& importer : m_importers) {
        const auto score = importer->probe(probe_result);
        if (score.confidence > best_confidence) {
            best_confidence = score.confidence;
            best = importer;
        }
    }

    if (best_confidence < 0.20) {
        return {};
    }

    return best;
}

} // namespace beamlab::io
