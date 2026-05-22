#pragma once

#include "StrategicPipelineTypes.h"

namespace strategic_nexus::strategic_pipeline {

class V0StrategicPipeline {
public:
    PipelineRunResult run(const PipelineRunConfig& config) const;
};

} // namespace strategic_nexus::strategic_pipeline
