// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PersonalityEngine.h"

#include "DoctrinePlanner.h"

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

std::string PersonalityEngine::buildDoctrineAlignmentNote(
    const EmpireState& empire,
    const DoctrineDecision& proposedDecision,
    const DoctrineDecision& refinedDecision) const
{
    const std::string bias = describeStrategicBias(empire);

    if (refinedDecision.type == proposedDecision.type) {
        std::string note = "Personality alignment: no doctrine adjustment; bias = " + bias;
        if (!refinedDecision.rationale.empty()) {
            note += "; rationale = " + refinedDecision.rationale;
        }
        note += '.';
        return note;
    }

    std::string note = "Personality alignment: ";
    if (proposedDecision.type == DoctrineType::Consolidate &&
        refinedDecision.type == DoctrineType::OpportunisticExpansion) {
        note += "upgraded from consolidate to opportunistic_expansion";
    } else if (proposedDecision.type == DoctrineType::Consolidate &&
               refinedDecision.type == DoctrineType::DefensivePosture) {
        if (refinedDecision.rationale.rfind("Rejected consolidate:", 0) == 0) {
            note += "rejected consolidate in favor of defensive_posture";
        } else {
            note += "downgraded from consolidate to defensive_posture";
        }
    } else if (proposedDecision.type == DoctrineType::OpportunisticExpansion &&
               refinedDecision.type == DoctrineType::DefensivePosture) {
        if (refinedDecision.rationale.rfind("Rejected opportunistic expansion:", 0) == 0) {
            note += "rejected opportunistic_expansion in favor of defensive_posture";
        } else {
            note += "downgraded from opportunistic_expansion to defensive_posture";
        }
    } else {
        note += "adjusted from ";
        note += toString(proposedDecision.type);
        note += " to ";
        note += toString(refinedDecision.type);
    }

    note += "; bias = " + bias;
    if (!refinedDecision.rationale.empty()) {
        note += "; rationale = " + refinedDecision.rationale;
    }
    note += '.';
    return note;
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

    if (decision.type == DoctrineType::OpportunisticExpansion && weakCapability && fearful && summary.hegemonyDetected) {
        decision.type = DoctrineType::DefensivePosture;
        decision.rationale =
            "Rejected opportunistic expansion: a detected hegemon, capability limits, and fear do not support it yet.";
        decision.confidence = 0.39;
        return decision;
    }

    if (decision.type == DoctrineType::OpportunisticExpansion && weakCapability && fearful && summary.instability < 0.15) {
        decision.type = DoctrineType::DefensivePosture;
        decision.rationale = "Rejected opportunistic expansion: capability, fear, and low pressure do not support it yet.";
        decision.confidence = 0.41;
        return decision;
    }

    if (decision.type == DoctrineType::Consolidate && weakCapability && fearful && summary.hegemonyDetected) {
        decision.type = DoctrineType::DefensivePosture;
        decision.rationale =
            "Rejected consolidate: a detected hegemon, capability limits, and fear require immediate defense.";
        decision.confidence = 0.44;
        return decision;
    }

    if (decision.type == DoctrineType::Consolidate && weakCapability && fearful && summary.instability < 0.15) {
        decision.type = DoctrineType::DefensivePosture;
        decision.rationale =
            "Rejected consolidate: weak capability, fear, and low pressure make passive consolidation unsafe.";
        decision.confidence = 0.43;
        return decision;
    }

    if (decision.type == DoctrineType::Consolidate && weakCapability && fearful && summary.instability < 0.30) {
        decision.type = DoctrineType::DefensivePosture;
        decision.rationale =
            "Rejected consolidate: weak capability, fear, and moderate pressure still favor immediate defense.";
        decision.confidence = 0.46;
        return decision;
    }

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

