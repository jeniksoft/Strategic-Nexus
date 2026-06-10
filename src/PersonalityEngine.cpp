// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PersonalityEngine.h"

namespace strategic_nexus {

std::string PersonalityEngine::describeStrategicBias(const EmpireState& empire) const
{
    if (empire.personality.paranoia > 0.65 || empire.adaptiveState.fearOfPlayer > 0.65) {
        return "risk-sensitive balancing";
    }

    if (empire.personality.opportunism > 0.6 && empire.personality.honor < 0.45) {
        return "opportunistic pressure";
    }

    if (empire.personality.boldness > 0.65) {
        return "assertive expansion";
    }

    return "cautious consolidation";
}

DoctrineDecision PersonalityEngine::refineDoctrineDecision(
    const EmpireState& empire,
    const StrategicSummary& summary,
    const DoctrineDecision& proposedDecision) const
{
    DoctrineDecision decision = proposedDecision;

    const bool weakCapability = empire.power.fleetPower < 60
        || empire.power.economicRank < 2
        || empire.power.technologyRank < 2;
    const bool fearful = empire.personality.paranoia > 0.65 || empire.adaptiveState.fearOfPlayer > 0.65;
    const bool boldAndOpportunistic = empire.personality.boldness > 0.7 && empire.personality.opportunism > 0.6;
    const bool pressureIsLow = !summary.hegemonyDetected && summary.instability < 0.45;

    if (decision.type == DoctrineType::OpportunisticExpansion && (weakCapability || fearful)) {
        decision.type = DoctrineType::DefensivePosture;
        decision.rationale = "Personality and capability do not support opportunistic expansion yet.";
        decision.confidence = 0.52;
        return decision;
    }

    if (decision.type == DoctrineType::Consolidate && boldAndOpportunistic && !weakCapability && pressureIsLow) {
        decision.type = DoctrineType::OpportunisticExpansion;
        decision.rationale = "Bold opportunism and strong capability support a bounded expansion bias.";
        decision.confidence = 0.63;
        return decision;
    }

    return decision;
}

} // namespace strategic_nexus

