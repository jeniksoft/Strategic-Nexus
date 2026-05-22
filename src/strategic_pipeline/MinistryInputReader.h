#pragma once

#include "StrategicPipelineTypes.h"

namespace strategic_nexus::strategic_pipeline {

struct MinistryInputReadResult {
    bool ok = false;
    std::string error;
    MinistryInputContext input;
};

class MinistryInputReader {
public:
    MinistryInputReadResult read(const std::filesystem::path& path) const;
};

} // namespace strategic_nexus::strategic_pipeline
