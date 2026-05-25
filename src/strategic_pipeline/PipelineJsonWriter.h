// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "EmpireProcessingQueue.h"
#include "StrategicPipelineTypes.h"

#include <string>
#include <vector>

namespace strategic_nexus::strategic_pipeline {

std::string serializeFinalStrategyPayload(const FinalStrategyPayload& payload);
std::string serializeMinistryInputContext(const MinistryInputContext& input);
std::string serializePipelineAuditRecord(const PipelineRunResult& result);
std::string serializeProcessingQueue(
    const std::vector<EmpireProcessingQueueEntry>& entries,
    const std::vector<std::string>& invalidInputs);

} // namespace strategic_nexus::strategic_pipeline

