#pragma once

#include "StrategicPipelineTypes.h"

namespace strategic_nexus::strategic_pipeline {

class StatelessClerk {
public:
    ClerkInputBrief reduce(const MinistryInputContext& input) const;
};

} // namespace strategic_nexus::strategic_pipeline
