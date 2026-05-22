#pragma once

#include "StrategicPipelineTypes.h"

namespace strategic_nexus::strategic_pipeline {

class ProcessingPriorityScorer {
public:
    ProcessingPriorityScore score(const MinistryInputContext& input) const;
};

} // namespace strategic_nexus::strategic_pipeline
