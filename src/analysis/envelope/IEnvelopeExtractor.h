#pragma once

#include "analysis/envelope/EnvelopeParameters.h"
#include "data/model/BeamEnvelope.h"
#include "data/model/BeamSlice.h"

#include <string>

namespace beamlab::analysis {

class IEnvelopeExtractor {
public:
    virtual ~IEnvelopeExtractor() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    [[nodiscard]] virtual beamlab::data::BeamEnvelope extract(const beamlab::data::BeamSlice& slice,
                                                              const EnvelopeParameters& parameters) const = 0;
};

} // namespace beamlab::analysis
