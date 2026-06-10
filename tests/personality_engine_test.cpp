// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "LlmClient.h"
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

    const strategic_nexus::LlmClient llmClient;
    const auto prompt = llmClient.buildPrompt(quietSummary, upgradedDecision, "assertive expansion");
    requireCondition(
        prompt.find("Personality bias: assertive expansion") != std::string::npos,
        "LLM prompt should surface the personality bias");

    std::cout << "personality engine tests passed.\n";
    return 0;
}
