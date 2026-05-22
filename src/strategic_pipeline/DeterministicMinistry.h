#pragma once

#include "StrategicPipelineTypes.h"

namespace strategic_nexus::strategic_pipeline {

class DeterministicMinistry {
public:
    MinistryProposal propose(const MinistryInputContext& input) const;
};

} // namespace strategic_nexus::strategic_pipeline
