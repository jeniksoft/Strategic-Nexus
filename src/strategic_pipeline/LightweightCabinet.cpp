#include "LightweightCabinet.h"

#include "bridge_core/BridgeState.h"

#include <algorithm>

namespace strategic_nexus::strategic_pipeline {
namespace {

bool isValidMilitaryPosture(const std::string& value)
{
    bridge_core::MilitaryPosture parsed;
    return bridge_core::tryParseMilitaryPosture(value, parsed);
}

bool isValidResearchBias(const std::string& value)
{
    bridge_core::ResearchBias parsed;
    return bridge_core::tryParseResearchBias(value, parsed);
}

std::string fallbackIfEmpty(std::string value, const char* fallback)
{
    if (value.empty()) {
        return fallback;
    }
    return value;
}

} // namespace

FinalStrategyPayload LightweightCabinet::evaluate(
    const MinistryInputContext& input,
    const MinistryProposal& proposal,
    const ClerkInputBrief& brief,
    const std::int64_t sequenceId,
    const std::int64_t createdUnixMs,
    const std::int64_t ttlMs) const
{
    FinalStrategyPayload payload;
    payload.sequenceId = sequenceId;
    payload.createdUnixMs = createdUnixMs;
    payload.ttlMs = ttlMs;
    payload.campaignId = input.campaignId;
    payload.empireId = input.empireId;
    payload.militaryPosture = fallbackIfEmpty(input.currentMilitaryPosture, "balanced");
    payload.researchBias = fallbackIfEmpty(input.currentResearchBias, "economy");
    payload.confidence = std::clamp(proposal.confidence, 0.0, 1.0);
    payload.fallbackRequired = false;

    if (input.campaignId != proposal.campaignId || input.empireId != proposal.empireId) {
        payload.fallbackRequired = true;
        return payload;
    }
    if (input.campaignId != brief.campaignId || input.empireId != brief.empireId) {
        payload.fallbackRequired = true;
        return payload;
    }
    if (input.ministry != proposal.ministry || input.ministry != brief.ministry) {
        payload.fallbackRequired = true;
        return payload;
    }
    if (!isValidMilitaryPosture(payload.militaryPosture) || !isValidResearchBias(payload.researchBias)) {
        payload.fallbackRequired = true;
        return payload;
    }
    if (payload.confidence < 0.50) {
        return payload;
    }

    if (!proposal.requestedMilitaryPosture.empty()) {
        if (!isValidMilitaryPosture(proposal.requestedMilitaryPosture)) {
            payload.fallbackRequired = true;
            return payload;
        }
        payload.militaryPosture = proposal.requestedMilitaryPosture;
    }

    if (!proposal.requestedResearchBias.empty()) {
        if (!isValidResearchBias(proposal.requestedResearchBias)) {
            payload.fallbackRequired = true;
            return payload;
        }
        payload.researchBias = proposal.requestedResearchBias;
    }

    return payload;
}

} // namespace strategic_nexus::strategic_pipeline
