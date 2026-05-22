#pragma once

#include "StrategicPipelineTypes.h"

namespace strategic_nexus::strategic_pipeline {

class LightweightCabinet {
public:
    FinalStrategyPayload evaluate(
        const MinistryInputContext& input,
        const MinistryProposal& proposal,
        const ClerkInputBrief& brief,
        std::int64_t sequenceId,
        std::int64_t createdUnixMs,
        std::int64_t ttlMs) const;
};

} // namespace strategic_nexus::strategic_pipeline
