#pragma once

#include "analysis/envelope/IEnvelopeExtractor.h"

namespace beamlab::analysis {

class ConvexHullEnvelopeExtractor final : public IEnvelopeExtractor {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] beamlab::data::BeamEnvelope extract(const beamlab::data::BeamSlice& slice,
                                                      const EnvelopeParameters& parameters) const override;
};

} // namespace beamlab::analysis
