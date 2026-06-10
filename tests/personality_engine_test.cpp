// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "LlmClient.h"
#include "ModIntegration.h"
#include "PersonalityEngine.h"

#include <iostream>
#include <string>

namespace {

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

} // namespace

int main()
{
    const strategic_nexus::PersonalityEngine engine;

    strategic_nexus::EmpireState boldEmpire;
    boldEmpire.id = "empire_bold";
    boldEmpire.power.fleetPower = 120;
    boldEmpire.power.economicRank = 4;
    boldEmpire.power.technologyRank = 4;
    boldEmpire.personality.boldness = 0.82;
    boldEmpire.personality.paranoia = 0.18;
    boldEmpire.personality.honor = 0.55;
    boldEmpire.personality.opportunism = 0.77;

    strategic_nexus::StrategicSummary quietSummary;
    quietSummary.year = 2301;
    quietSummary.dominantEmpireId = "empire_bold";
    quietSummary.instability = 0.18;
    quietSummary.hegemonyDetected = false;

    const auto proposedConsolidate = strategic_nexus::DoctrineDecision{
        strategic_nexus::DoctrineType::Consolidate,
        "No immediate hegemonic or crisis pressure detected.",
        0.58};
    const auto upgradedDecision = engine.refineDoctrineDecision(boldEmpire, quietSummary, proposedConsolidate);
    requireCondition(
        upgradedDecision.type == strategic_nexus::DoctrineType::OpportunisticExpansion,
        "bold, capable empire should upgrade consolidation to opportunistic expansion");
    requireCondition(
        upgradedDecision.rationale.find("bounded expansion bias") != std::string::npos,
        "upgrade should explain the personality-driven expansion bias");
    requireCondition(
        upgradedDecision.confidence == 0.63,
        "upgrade should set the bounded personality confidence");
    requireCondition(
        engine.describeStrategicBias(boldEmpire) == "assertive expansion",
        "bold empire should be described as assertive expansion");
    const auto upgradedNote = engine.buildDoctrineAlignmentNote(
        boldEmpire,
        proposedConsolidate,
        upgradedDecision);
    requireCondition(
        upgradedNote.find("upgraded from consolidate to opportunistic_expansion") != std::string::npos,
        "upgrade note should explain the personality alignment shift");
    requireCondition(
        upgradedNote.find("bias = assertive expansion") != std::string::npos,
        "upgrade note should expose the personality bias");

    strategic_nexus::EmpireState fearfulEmpire;
    fearfulEmpire.id = "empire_fearful";
    fearfulEmpire.power.fleetPower = 22;
    fearfulEmpire.power.economicRank = 1;
    fearfulEmpire.power.technologyRank = 1;
    fearfulEmpire.personality.boldness = 0.24;
    fearfulEmpire.personality.paranoia = 0.81;
    fearfulEmpire.personality.honor = 0.43;
    fearfulEmpire.personality.opportunism = 0.74;
    fearfulEmpire.adaptiveState.fearOfPlayer = 0.78;

    const auto proposedExpansion = strategic_nexus::DoctrineDecision{
        strategic_nexus::DoctrineType::OpportunisticExpansion,
        "Aggressive expansion is available.",
        0.76};
    const auto downgradedDecision = engine.refineDoctrineDecision(fearfulEmpire, quietSummary, proposedExpansion);
    requireCondition(
        downgradedDecision.type == strategic_nexus::DoctrineType::DefensivePosture,
        "fearful, weak empire should downgrade opportunistic expansion to defense");
    requireCondition(
        downgradedDecision.rationale == "Personality and capability do not support opportunistic expansion yet.",
        "downgrade should explain the contradiction");
    requireCondition(
        downgradedDecision.confidence == 0.52,
        "downgrade should clamp confidence");
    requireCondition(
        engine.describeStrategicBias(fearfulEmpire) == "risk-sensitive balancing",
        "fearful empire should be described as risk-sensitive balancing");
    const auto downgradedNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedExpansion,
        downgradedDecision);
    requireCondition(
        downgradedNote.find("downgraded from opportunistic_expansion to defensive_posture") != std::string::npos,
        "downgrade note should explain the personality alignment shift");
    requireCondition(
        downgradedNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "downgrade note should expose the personality bias");

    strategic_nexus::StrategicSummary severePressureSummary = quietSummary;
    severePressureSummary.instability = 0.08;

    const auto rejectedDecision = engine.refineDoctrineDecision(fearfulEmpire, severePressureSummary, proposedExpansion);
    requireCondition(
        rejectedDecision.type == strategic_nexus::DoctrineType::DefensivePosture,
        "severely contradictory opportunistic expansion should still resolve to defense");
    requireCondition(
        rejectedDecision.rationale ==
            "Rejected opportunistic expansion: capability, fear, and low pressure do not support it yet.",
        "severe contradiction should use the explicit reject rationale");
    requireCondition(
        rejectedDecision.confidence == 0.41,
        "severe contradiction reject path should clamp confidence lower than the downgrade path");
    const auto rejectedNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedExpansion,
        rejectedDecision);
    requireCondition(
        rejectedNote.find("rejected opportunistic_expansion in favor of defensive_posture") != std::string::npos,
        "reject note should explain the explicit contradiction rejection");
    requireCondition(
        rejectedNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "reject note should still expose the personality bias");

    strategic_nexus::EmpireState capabilityConstrainedEmpire;
    capabilityConstrainedEmpire.id = "empire_capability_constrained";
    capabilityConstrainedEmpire.power.fleetPower = 34;
    capabilityConstrainedEmpire.power.economicRank = 3;
    capabilityConstrainedEmpire.power.technologyRank = 3;
    capabilityConstrainedEmpire.personality.boldness = 0.57;
    capabilityConstrainedEmpire.personality.paranoia = 0.28;
    capabilityConstrainedEmpire.personality.honor = 0.38;
    capabilityConstrainedEmpire.personality.opportunism = 0.76;
    capabilityConstrainedEmpire.adaptiveState.fearOfPlayer = 0.18;

    const auto capabilityDrivenDowngrade = engine.refineDoctrineDecision(
        capabilityConstrainedEmpire,
        quietSummary,
        proposedExpansion);
    requireCondition(
        capabilityDrivenDowngrade.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak capability alone should downgrade opportunistic expansion to defense");
    requireCondition(
        capabilityDrivenDowngrade.rationale == "Personality and capability do not support opportunistic expansion yet.",
        "capability-driven downgrade should explain the contradiction");
    requireCondition(
        capabilityDrivenDowngrade.confidence == 0.52,
        "capability-driven downgrade should clamp confidence");
    const auto capabilityDrivenNote = engine.buildDoctrineAlignmentNote(
        capabilityConstrainedEmpire,
        proposedExpansion,
        capabilityDrivenDowngrade);
    requireCondition(
        capabilityDrivenNote.find("downgraded from opportunistic_expansion to defensive_posture") != std::string::npos,
        "capability-driven downgrade note should explain the alignment shift");
    requireCondition(
        capabilityDrivenNote.find("bias = opportunistic pressure") != std::string::npos,
        "capability-driven downgrade note should expose the opportunistic bias");

    strategic_nexus::EmpireState steadyEmpire;
    steadyEmpire.id = "empire_steady";
    steadyEmpire.power.fleetPower = 88;
    steadyEmpire.power.economicRank = 3;
    steadyEmpire.power.technologyRank = 3;
    steadyEmpire.personality.boldness = 0.48;
    steadyEmpire.personality.paranoia = 0.35;
    steadyEmpire.personality.honor = 0.58;
    steadyEmpire.personality.opportunism = 0.46;

    const auto steadyDecision = engine.refineDoctrineDecision(steadyEmpire, quietSummary, proposedConsolidate);
    requireCondition(
        steadyDecision.type == strategic_nexus::DoctrineType::Consolidate,
        "steady empire should keep the consolidate doctrine");
    const auto neutralNote = engine.buildDoctrineAlignmentNote(
        steadyEmpire,
        proposedConsolidate,
        steadyDecision);
    requireCondition(
        neutralNote.find("no doctrine adjustment") != std::string::npos,
        "neutral note should explain the absence of an adjustment");
    requireCondition(
        neutralNote.find("bias = cautious consolidation") != std::string::npos,
        "neutral note should expose the cautious bias");

    const strategic_nexus::LlmClient llmClient;
    auto promptDecision = upgradedDecision;
    promptDecision.personalityAlignmentNote = upgradedNote;
    const auto prompt = llmClient.buildPrompt(quietSummary, promptDecision, "assertive expansion");
    requireCondition(
        prompt.find("Personality bias: assertive expansion") != std::string::npos,
        "LLM prompt should surface the personality bias");
    requireCondition(
        prompt.find("Personality alignment note") != std::string::npos,
        "LLM prompt should surface the personality alignment note when present");

    const strategic_nexus::ModIntegration modIntegration;
    const auto doctrineJson = modIntegration.renderDoctrineJson(promptDecision);
    requireCondition(
        doctrineJson.find("\"personality_alignment_note\": \"") != std::string::npos,
        "doctrine JSON should surface the personality alignment note field");
    requireCondition(
        doctrineJson.find("assertive expansion") != std::string::npos,
        "doctrine JSON should carry the alignment note content");

    std::cout << "personality engine tests passed.\n";
    return 0;
}
