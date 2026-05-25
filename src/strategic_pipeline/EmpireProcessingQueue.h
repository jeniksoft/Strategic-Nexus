// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "ProcessingPriorityScorer.h"
#include "StrategicPipelineTypes.h"

#include <vector>

namespace strategic_nexus::strategic_pipeline {

struct EmpireProcessingQueueEntry {
    MinistryInputContext input;
    ProcessingPriorityScore priorityScore;
    std::size_t originalIndex = 0;
};

class EmpireProcessingQueue {
public:
    std::vector<EmpireProcessingQueueEntry> build(std::vector<MinistryInputContext> inputs) const;
};

} // namespace strategic_nexus::strategic_pipeline

