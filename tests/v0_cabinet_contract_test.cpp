// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "strategic_pipeline/LightweightCabinet.h"

#include <iostream>
#include <string>

namespace {

using strategic_nexus::strategic_pipeline::ClerkInputBrief;
using strategic_nexus::strategic_pipeline::FinalStrategyPayload;
using strategic_nexus::strategic_pipeline::LightweightCabinet;
using strategic_nexus::strategic_pipeline::MinistryInputContext;
using strategic_nexus::strategic_pipeline::MinistryProposal;

MinistryInputContext makeInput()
{
    MinistryInputContext input;
    input.schemaVersion = 1;
    input.contextId = "context_cabinet_contract";
    input.campaignId = "campaign_cabinet_contract";
    input.empireId = "empire_cabinet_contract";
    input.ministry = "military";
    input.currentMilitaryPosture = "defensive";
    input.currentResearchBias = "economy";
    return input;
}

MinistryProposal makeProposal(const MinistryInputContext& input)
{
    MinistryProposal proposal;
    proposal.campaignId = input.campaignId;
    proposal.empireId = input.empireId;
    proposal.ministry = input.ministry;
    proposal.proposalId = "proposal_cabinet_contract";
    proposal.requestedMilitaryPosture = "aggressive";
    proposal.confidence = 0.75;
    return proposal;
}

ClerkInputBrief makeBrief(const MinistryInputContext& input)
{
    ClerkInputBrief brief;
    brief.campaignId = input.campaignId;
    brief.empireId = input.empireId;
    brief.ministry = input.ministry;
    brief.sourceContextId = input.contextId;
    return brief;
}

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

FinalStrategyPayload evaluate(
    const MinistryInputContext& input,
    const MinistryProposal& proposal,
    const ClerkInputBrief& brief)
{
    const LightweightCabinet cabinet;
    return cabinet.evaluate(input, proposal, brief, 501, 10000, 30000);
}

} // namespace

int main()
{
    const MinistryInputContext input = makeInput();

    {
        const FinalStrategyPayload payload = evaluate(input, makeProposal(input), makeBrief(input));
        requireCondition(!payload.fallbackRequired, "matching ministry identity should be accepted");
        requireCondition(payload.militaryPosture == "aggressive", "accepted proposal should apply requested military posture");
        requireCondition(payload.researchBias == "economy", "accepted proposal should preserve unchanged research bias");
    }

    {
        MinistryProposal proposal = makeProposal(input);
        proposal.ministry = "research";
        const FinalStrategyPayload payload = evaluate(input, proposal, makeBrief(input));
        requireCondition(payload.fallbackRequired, "proposal ministry mismatch should require fallback");
    }

    {
        ClerkInputBrief brief = makeBrief(input);
        brief.ministry = "research";
        const FinalStrategyPayload payload = evaluate(input, makeProposal(input), brief);
        requireCondition(payload.fallbackRequired, "clerk brief ministry mismatch should require fallback");
    }

    {
        MinistryProposal proposal = makeProposal(input);
        proposal.confidence = 0.25;
        const FinalStrategyPayload payload = evaluate(input, proposal, makeBrief(input));
        requireCondition(!payload.fallbackRequired, "low confidence with valid current agenda should preserve safely");
        requireCondition(payload.militaryPosture == "defensive", "low confidence should preserve current military posture");
    }

    std::cout << "v0 cabinet contract tests passed.\n";
    return 0;
}

