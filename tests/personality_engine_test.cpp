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

    strategic_nexus::StrategicSummary lowPressureSummary = quietSummary;
    lowPressureSummary.instability = 0.09;

    const auto lowPressureConsolidateReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        lowPressureSummary,
        proposedConsolidate);
    requireCondition(
        lowPressureConsolidateReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak fearful empire under low pressure should still reject consolidate in favor of defense");
    requireCondition(
        lowPressureConsolidateReject.rationale ==
            "Rejected consolidate: weak capability, fear, and low pressure make passive consolidation unsafe.",
        "low-pressure consolidate reject should explain the contradiction");
    requireCondition(
        lowPressureConsolidateReject.confidence == 0.43,
        "low-pressure consolidate reject should clamp confidence");
    const auto lowPressureConsolidateRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedConsolidate,
        lowPressureConsolidateReject);
    requireCondition(
        lowPressureConsolidateRejectNote.find("rejected consolidate in favor of defensive_posture") != std::string::npos,
        "low-pressure consolidate reject note should explain the explicit contradiction rejection");
    requireCondition(
        lowPressureConsolidateRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "low-pressure consolidate reject note should still expose the personality bias");

    strategic_nexus::StrategicSummary moderatePressureSummary = quietSummary;
    moderatePressureSummary.instability = 0.22;

    const auto moderatePressureExpansionReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        moderatePressureSummary,
        proposedExpansion);
    requireCondition(
        moderatePressureExpansionReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak fearful empire under moderate pressure should reject opportunistic expansion in favor of defense");
    requireCondition(
        moderatePressureExpansionReject.rationale ==
            "Rejected opportunistic expansion: weak capability, fear, and moderate pressure do not support it yet.",
        "moderate-pressure opportunistic expansion reject should explain the contradiction");
    requireCondition(
        moderatePressureExpansionReject.confidence == 0.44,
        "moderate-pressure opportunistic expansion reject should clamp confidence");
    const auto moderatePressureExpansionRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedExpansion,
        moderatePressureExpansionReject);
    requireCondition(
        moderatePressureExpansionRejectNote.find("rejected opportunistic_expansion in favor of defensive_posture") != std::string::npos,
        "moderate-pressure opportunistic expansion reject note should explain the explicit contradiction rejection");
    requireCondition(
        moderatePressureExpansionRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "moderate-pressure opportunistic expansion reject note should still expose the personality bias");

    const auto moderatePressureConsolidateReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        moderatePressureSummary,
        proposedConsolidate);
    requireCondition(
        moderatePressureConsolidateReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak fearful empire under moderate pressure should reject consolidate in favor of defense");
    requireCondition(
        moderatePressureConsolidateReject.rationale ==
            "Rejected consolidate: weak capability, fear, and moderate pressure still favor immediate defense.",
        "moderate-pressure consolidate reject should explain the contradiction");
    requireCondition(
        moderatePressureConsolidateReject.confidence == 0.46,
        "moderate-pressure consolidate reject should clamp confidence");
    const auto moderatePressureConsolidateRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedConsolidate,
        moderatePressureConsolidateReject);
    requireCondition(
        moderatePressureConsolidateRejectNote.find("rejected consolidate in favor of defensive_posture") != std::string::npos,
        "moderate-pressure consolidate reject note should explain the explicit contradiction rejection");
    requireCondition(
        moderatePressureConsolidateRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "moderate-pressure consolidate reject note should still expose the personality bias");

    strategic_nexus::StrategicSummary elevatedPressureSummary = quietSummary;
    elevatedPressureSummary.instability = 0.52;

    const auto elevatedPressureConsolidateReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        elevatedPressureSummary,
        proposedConsolidate);
    requireCondition(
        elevatedPressureConsolidateReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak fearful empire under elevated pressure should reject consolidate in favor of defense");
    requireCondition(
        elevatedPressureConsolidateReject.rationale ==
            "Rejected consolidate: weak capability, fear, and elevated pressure still require immediate defense.",
        "elevated-pressure consolidate reject should explain the contradiction");
    requireCondition(
        elevatedPressureConsolidateReject.confidence == 0.42,
        "elevated-pressure consolidate reject should clamp confidence");
    const auto elevatedPressureConsolidateRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedConsolidate,
        elevatedPressureConsolidateReject);
    requireCondition(
        elevatedPressureConsolidateRejectNote.find("rejected consolidate in favor of defensive_posture") != std::string::npos,
        "elevated-pressure consolidate reject note should explain the explicit contradiction rejection");
    requireCondition(
        elevatedPressureConsolidateRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "elevated-pressure consolidate reject note should still expose the personality bias");

    strategic_nexus::StrategicSummary hegemonicPressureSummary = quietSummary;
    hegemonicPressureSummary.hegemonyDetected = true;
    hegemonicPressureSummary.instability = 0.09;

    const auto hegemonicReject = engine.refineDoctrineDecision(fearfulEmpire, hegemonicPressureSummary, proposedExpansion);
    requireCondition(
        hegemonicReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "hegemony pressure should still resolve weak fearful opportunism to defense");
    requireCondition(
        hegemonicReject.rationale ==
            "Rejected opportunistic expansion: a detected hegemon, capability limits, and fear do not support it yet.",
        "hegemony-aware reject should explain the contradiction");
    requireCondition(
        hegemonicReject.confidence == 0.39,
        "hegemony-aware reject path should clamp confidence lower than the other reject paths");
    const auto hegemonicRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedExpansion,
        hegemonicReject);
    requireCondition(
        hegemonicRejectNote.find("rejected opportunistic_expansion in favor of defensive_posture") != std::string::npos,
        "hegemony-aware reject note should explain the explicit contradiction rejection");
    requireCondition(
        hegemonicRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "hegemony-aware reject note should still expose the personality bias");

    const auto proposedBalanceAgainstHegemon = strategic_nexus::DoctrineDecision{
        strategic_nexus::DoctrineType::BalanceAgainstHegemon,
        "Balance against the hegemon while the pressure point is active.",
        0.72};
    const auto balanceAgainstHegemonReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        hegemonicPressureSummary,
        proposedBalanceAgainstHegemon);
    requireCondition(
        balanceAgainstHegemonReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak fearful empire under hegemonic pressure should reject balance-against-hegemon in favor of defense");
    requireCondition(
        balanceAgainstHegemonReject.rationale ==
            "Rejected balance against hegemon: capability limits, fear, and hegemonic pressure require immediate defense.",
        "balance-against-hegemon reject should explain the contradiction");
    requireCondition(
        balanceAgainstHegemonReject.confidence == 0.47,
        "balance-against-hegemon reject should clamp confidence");
    const auto balanceAgainstHegemonRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedBalanceAgainstHegemon,
        balanceAgainstHegemonReject);
    requireCondition(
        balanceAgainstHegemonRejectNote.find("rejected balance_against_hegemon in favor of defensive_posture") != std::string::npos,
        "balance-against-hegemon reject note should explain the explicit contradiction rejection");
    requireCondition(
        balanceAgainstHegemonRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "balance-against-hegemon reject note should still expose the personality bias");

    const auto hegemonicConsolidate = strategic_nexus::DoctrineDecision{
        strategic_nexus::DoctrineType::Consolidate,
        "Consolidate while external pressure remains elevated.",
        0.67};
    const auto consolidateReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        hegemonicPressureSummary,
        hegemonicConsolidate);
    requireCondition(
        consolidateReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "hegemony pressure should reject fragile consolidation in favor of defense");
    requireCondition(
        consolidateReject.rationale ==
            "Rejected consolidate: a detected hegemon, capability limits, and fear require immediate defense.",
        "consolidate reject should explain the contradiction");
    requireCondition(
        consolidateReject.confidence == 0.44,
        "consolidate reject should clamp confidence");
    const auto consolidateRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        hegemonicConsolidate,
        consolidateReject);
    requireCondition(
        consolidateRejectNote.find("rejected consolidate in favor of defensive_posture") != std::string::npos,
        "consolidate reject note should explain the explicit contradiction rejection");
    requireCondition(
        consolidateRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "consolidate reject note should still expose the personality bias");

    strategic_nexus::EmpireState hegemonicButCapableEmpire = boldEmpire;
    hegemonicButCapableEmpire.id = "empire_hegemonic_but_capable";
    hegemonicButCapableEmpire.personality.paranoia = 0.78;
    hegemonicButCapableEmpire.adaptiveState.fearOfPlayer = 0.74;

    const auto hegemonicCapableConsolidateReject = engine.refineDoctrineDecision(
        hegemonicButCapableEmpire,
        hegemonicPressureSummary,
        proposedConsolidate);
    requireCondition(
        hegemonicCapableConsolidateReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "hegemonic capable empire should still reject fragile consolidation in favor of defense");
    requireCondition(
        hegemonicCapableConsolidateReject.rationale ==
            "Rejected consolidate: a detected hegemon and fear require immediate defense even when capability is adequate.",
        "hegemonic capable consolidate reject should explain the contradiction");
    requireCondition(
        hegemonicCapableConsolidateReject.confidence == 0.48,
        "hegemonic capable consolidate reject should clamp confidence");
    const auto hegemonicCapableConsolidateRejectNote = engine.buildDoctrineAlignmentNote(
        hegemonicButCapableEmpire,
        proposedConsolidate,
        hegemonicCapableConsolidateReject);
    requireCondition(
        hegemonicCapableConsolidateRejectNote.find("rejected consolidate in favor of defensive_posture") != std::string::npos,
        "hegemonic capable consolidate reject note should explain the explicit contradiction rejection");
    requireCondition(
        hegemonicCapableConsolidateRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "hegemonic capable consolidate reject note should still expose the personality bias");

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
