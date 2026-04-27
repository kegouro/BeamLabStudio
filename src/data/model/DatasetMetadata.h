#pragma once

#include "core/metadata/ImportDescriptor.h"
#include "core/metadata/SourceDescriptor.h"
#include "core/validation/ValidationReport.h"

#include <string>

namespace beamlab::data {

struct DatasetMetadata {
    std::string display_name{};
    beamlab::core::SourceDescriptor source{};
    beamlab::core::ImportDescriptor import{};
    beamlab::core::ValidationReport validation{};
};

} // namespace beamlab::data
