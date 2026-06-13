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
    fearfulEmpire.adaptiveState.warTrauma = 0.83;
    fearfulEmpire.adaptiveState.trustInFederations = 0.17;

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

    const auto proposedHegemonBalance = strategic_nexus::DoctrineDecision{
        strategic_nexus::DoctrineType::BalanceAgainstHegemon,
        "Coalition balancing is available.",
        0.68};
    strategic_nexus::StrategicSummary traumaBalanceSummary = quietSummary;
    traumaBalanceSummary.instability = 0.24;
    const auto traumaBalanceReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        traumaBalanceSummary,
        proposedHegemonBalance);
    requireCondition(
        traumaBalanceReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "traumatized fearful empire under moderate pressure should reject balance-against-hegemon without confirmed hegemon");
    requireCondition(
        traumaBalanceReject.rationale ==
            "Rejected balance against hegemon: war trauma and low federation trust make coalition balancing unreliable even without confirmed hegemonic pressure.",
        "trauma-guarded balance reject should explain the contradiction");
    requireCondition(
        traumaBalanceReject.confidence == 0.43,
        "trauma-guarded balance reject should clamp confidence");
    const auto traumaBalanceRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedHegemonBalance,
        traumaBalanceReject);
    requireCondition(
        traumaBalanceRejectNote.find("rejected balance_against_hegemon in favor of defensive_posture") != std::string::npos,
        "trauma-guarded balance reject note should explain the explicit contradiction rejection");
    requireCondition(
        traumaBalanceRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "trauma-guarded balance reject note should still expose the personality bias");

    strategic_nexus::EmpireState lowTrustEmpire = boldEmpire;
    lowTrustEmpire.id = "empire_low_trust";
    lowTrustEmpire.power.fleetPower = 82;
    lowTrustEmpire.power.economicRank = 3;
    lowTrustEmpire.power.technologyRank = 3;
    lowTrustEmpire.personality.boldness = 0.43;
    lowTrustEmpire.personality.paranoia = 0.24;
    lowTrustEmpire.personality.honor = 0.55;
    lowTrustEmpire.personality.opportunism = 0.39;
    lowTrustEmpire.adaptiveState.fearOfPlayer = 0.21;
    lowTrustEmpire.adaptiveState.warTrauma = 0.18;
    lowTrustEmpire.adaptiveState.trustInFederations = 0.24;

    const auto lowTrustBalanceReject = engine.refineDoctrineDecision(
        lowTrustEmpire,
        traumaBalanceSummary,
        proposedHegemonBalance);
    requireCondition(
        lowTrustBalanceReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "low-trust empire under low pressure should reject balance-against-hegemon in favor of defense");
    requireCondition(
        lowTrustBalanceReject.rationale ==
            "Rejected balance against hegemon: low federation trust and low pressure do not justify coalition balancing yet.",
        "low-trust balance reject should explain the contradiction");
    requireCondition(
        lowTrustBalanceReject.confidence == 0.42,
        "low-trust balance reject should clamp confidence");
    const auto lowTrustBalanceRejectNote = engine.buildDoctrineAlignmentNote(
        lowTrustEmpire,
        proposedHegemonBalance,
        lowTrustBalanceReject);
    requireCondition(
        lowTrustBalanceRejectNote.find("rejected balance_against_hegemon in favor of defensive_posture") != std::string::npos,
        "low-trust balance reject note should explain the explicit contradiction rejection");
    requireCondition(
        lowTrustBalanceRejectNote.find("bias = trauma-guarded caution") != std::string::npos,
        "low-trust balance reject note should still expose the personality bias");

    strategic_nexus::EmpireState traumaTrustExpansionEmpire = boldEmpire;
    traumaTrustExpansionEmpire.id = "empire_trauma_trust_expansion";
    traumaTrustExpansionEmpire.power.fleetPower = 94;
    traumaTrustExpansionEmpire.power.economicRank = 3;
    traumaTrustExpansionEmpire.power.technologyRank = 3;
    traumaTrustExpansionEmpire.personality.boldness = 0.46;
    traumaTrustExpansionEmpire.personality.paranoia = 0.28;
    traumaTrustExpansionEmpire.personality.honor = 0.49;
    traumaTrustExpansionEmpire.personality.opportunism = 0.53;
    traumaTrustExpansionEmpire.adaptiveState.fearOfPlayer = 0.23;
    traumaTrustExpansionEmpire.adaptiveState.warTrauma = 0.79;
    traumaTrustExpansionEmpire.adaptiveState.trustInFederations = 0.22;

    strategic_nexus::StrategicSummary traumaTrustExpansionSummary = quietSummary;
    traumaTrustExpansionSummary.instability = 0.22;

    const auto traumaTrustExpansionReject = engine.refineDoctrineDecision(
        traumaTrustExpansionEmpire,
        traumaTrustExpansionSummary,
        proposedExpansion);
    requireCondition(
        traumaTrustExpansionReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "trauma-guarded low-trust empire should reject opportunistic expansion in favor of defense");
    requireCondition(
        traumaTrustExpansionReject.rationale ==
            "Rejected opportunistic expansion: war trauma and low federation trust do not support expansion yet.",
        "trauma-trust opportunistic reject should explain the contradiction");
    requireCondition(
        traumaTrustExpansionReject.confidence == 0.47,
        "trauma-trust opportunistic reject should clamp confidence");
    const auto traumaTrustExpansionRejectNote = engine.buildDoctrineAlignmentNote(
        traumaTrustExpansionEmpire,
        proposedExpansion,
        traumaTrustExpansionReject);
    requireCondition(
        traumaTrustExpansionRejectNote.find("rejected opportunistic_expansion in favor of defensive_posture") !=
            std::string::npos,
        "trauma-trust opportunistic reject note should explain the explicit contradiction rejection");
    requireCondition(
        traumaTrustExpansionRejectNote.find("bias = trauma-guarded caution") != std::string::npos,
        "trauma-trust opportunistic reject note should still expose the personality bias");

    strategic_nexus::EmpireState lowTrustHegemonEmpire = boldEmpire;
    lowTrustHegemonEmpire.id = "empire_low_trust_hegemon";
    lowTrustHegemonEmpire.power.fleetPower = 86;
    lowTrustHegemonEmpire.power.economicRank = 3;
    lowTrustHegemonEmpire.power.technologyRank = 3;
    lowTrustHegemonEmpire.personality.boldness = 0.44;
    lowTrustHegemonEmpire.personality.paranoia = 0.26;
    lowTrustHegemonEmpire.personality.honor = 0.51;
    lowTrustHegemonEmpire.personality.opportunism = 0.37;
    lowTrustHegemonEmpire.adaptiveState.fearOfPlayer = 0.2;
    lowTrustHegemonEmpire.adaptiveState.warTrauma = 0.16;
    lowTrustHegemonEmpire.adaptiveState.trustInFederations = 0.23;

    strategic_nexus::StrategicSummary lowTrustHegemonSummary = quietSummary;
    lowTrustHegemonSummary.hegemonyDetected = true;
    lowTrustHegemonSummary.instability = 0.09;

    const auto lowTrustHegemonReject = engine.refineDoctrineDecision(
        lowTrustHegemonEmpire,
        lowTrustHegemonSummary,
        proposedHegemonBalance);
    requireCondition(
        lowTrustHegemonReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "low-trust empire under hegemonic pressure should reject balance-against-hegemon in favor of defense");
    requireCondition(
        lowTrustHegemonReject.rationale ==
            "Rejected balance against hegemon: low federation trust and hegemonic pressure do not justify coalition balancing yet.",
        "low-trust hegemon reject should explain the contradiction");
    requireCondition(
        lowTrustHegemonReject.confidence == 0.46,
        "low-trust hegemon reject should clamp confidence");
    const auto lowTrustHegemonRejectNote = engine.buildDoctrineAlignmentNote(
        lowTrustHegemonEmpire,
        proposedHegemonBalance,
        lowTrustHegemonReject);
    requireCondition(
        lowTrustHegemonRejectNote.find("rejected balance_against_hegemon in favor of defensive_posture") != std::string::npos,
        "low-trust hegemon reject note should explain the explicit contradiction rejection");
    requireCondition(
        lowTrustHegemonRejectNote.find("bias = trauma-guarded caution") != std::string::npos,
        "low-trust hegemon reject note should still expose the personality bias");

    strategic_nexus::EmpireState lowTrustRisingPressureEmpire = boldEmpire;
    lowTrustRisingPressureEmpire.id = "empire_low_trust_rising_pressure";
    lowTrustRisingPressureEmpire.power.fleetPower = 84;
    lowTrustRisingPressureEmpire.power.economicRank = 3;
    lowTrustRisingPressureEmpire.power.technologyRank = 3;
    lowTrustRisingPressureEmpire.personality.boldness = 0.41;
    lowTrustRisingPressureEmpire.personality.paranoia = 0.23;
    lowTrustRisingPressureEmpire.personality.honor = 0.53;
    lowTrustRisingPressureEmpire.personality.opportunism = 0.36;
    lowTrustRisingPressureEmpire.adaptiveState.fearOfPlayer = 0.22;
    lowTrustRisingPressureEmpire.adaptiveState.warTrauma = 0.18;
    lowTrustRisingPressureEmpire.adaptiveState.trustInFederations = 0.24;

    strategic_nexus::StrategicSummary risingPressureBalanceSummary = quietSummary;
    risingPressureBalanceSummary.instability = 0.34;
    const auto risingPressureBalanceReject = engine.refineDoctrineDecision(
        lowTrustRisingPressureEmpire,
        risingPressureBalanceSummary,
        proposedHegemonBalance);
    requireCondition(
        risingPressureBalanceReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "low-trust empire under rising pressure should reject balance-against-hegemon in favor of defense");
    requireCondition(
        risingPressureBalanceReject.rationale ==
            "Rejected balance against hegemon: low federation trust and rising pressure do not justify coalition balancing yet.",
        "rising-pressure low-trust balance reject should explain the contradiction");
    requireCondition(
        risingPressureBalanceReject.confidence == 0.44,
        "rising-pressure low-trust balance reject should clamp confidence");
    const auto risingPressureBalanceRejectNote = engine.buildDoctrineAlignmentNote(
        lowTrustRisingPressureEmpire,
        proposedHegemonBalance,
        risingPressureBalanceReject);
    requireCondition(
        risingPressureBalanceRejectNote.find("rejected balance_against_hegemon in favor of defensive_posture") != std::string::npos,
        "rising-pressure low-trust balance reject note should explain the explicit contradiction rejection");
    requireCondition(
        risingPressureBalanceRejectNote.find("bias = trauma-guarded caution") != std::string::npos,
        "rising-pressure low-trust balance reject note should still expose the personality bias");

    strategic_nexus::EmpireState traumaLowTrustRisingBalanceEmpire = traumaTrustExpansionEmpire;
    traumaLowTrustRisingBalanceEmpire.id = "empire_trauma_low_trust_rising_balance";
    traumaLowTrustRisingBalanceEmpire.power.fleetPower = 92;
    traumaLowTrustRisingBalanceEmpire.power.economicRank = 3;
    traumaLowTrustRisingBalanceEmpire.power.technologyRank = 3;
    traumaLowTrustRisingBalanceEmpire.personality.boldness = 0.45;
    traumaLowTrustRisingBalanceEmpire.personality.paranoia = 0.29;
    traumaLowTrustRisingBalanceEmpire.personality.honor = 0.5;
    traumaLowTrustRisingBalanceEmpire.personality.opportunism = 0.41;
    traumaLowTrustRisingBalanceEmpire.adaptiveState.fearOfPlayer = 0.24;
    traumaLowTrustRisingBalanceEmpire.adaptiveState.warTrauma = 0.8;
    traumaLowTrustRisingBalanceEmpire.adaptiveState.trustInFederations = 0.23;

    strategic_nexus::StrategicSummary traumaLowTrustRisingBalanceSummary = quietSummary;
    traumaLowTrustRisingBalanceSummary.instability = 0.36;

    const auto traumaLowTrustRisingBalanceReject = engine.refineDoctrineDecision(
        traumaLowTrustRisingBalanceEmpire,
        traumaLowTrustRisingBalanceSummary,
        proposedHegemonBalance);
    requireCondition(
        traumaLowTrustRisingBalanceReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "traumatized low-trust empire under rising pressure should reject balance-against-hegemon in favor of defense");
    requireCondition(
        traumaLowTrustRisingBalanceReject.rationale ==
            "Rejected balance against hegemon: war trauma, low federation trust, and rising pressure do not justify coalition balancing yet.",
        "traumatized low-trust rising-pressure balance reject should explain the contradiction");
    requireCondition(
        traumaLowTrustRisingBalanceReject.confidence == 0.44,
        "traumatized low-trust rising-pressure balance reject should clamp confidence");
    const auto traumaLowTrustRisingBalanceRejectNote = engine.buildDoctrineAlignmentNote(
        traumaLowTrustRisingBalanceEmpire,
        proposedHegemonBalance,
        traumaLowTrustRisingBalanceReject);
    requireCondition(
        traumaLowTrustRisingBalanceRejectNote.find("rejected balance_against_hegemon in favor of defensive_posture") !=
            std::string::npos,
        "traumatized low-trust rising-pressure balance reject note should explain the explicit contradiction rejection");
    requireCondition(
        traumaLowTrustRisingBalanceRejectNote.find("bias = trauma-guarded caution") != std::string::npos,
        "traumatized low-trust rising-pressure balance reject note should still expose the personality bias");

    strategic_nexus::EmpireState steadyNoHegemonEmpire = boldEmpire;
    steadyNoHegemonEmpire.id = "empire_steady_no_hegemon";
    steadyNoHegemonEmpire.power.fleetPower = 88;
    steadyNoHegemonEmpire.power.economicRank = 3;
    steadyNoHegemonEmpire.power.technologyRank = 3;
    steadyNoHegemonEmpire.personality.boldness = 0.49;
    steadyNoHegemonEmpire.personality.paranoia = 0.29;
    steadyNoHegemonEmpire.personality.honor = 0.58;
    steadyNoHegemonEmpire.personality.opportunism = 0.44;
    steadyNoHegemonEmpire.adaptiveState.fearOfPlayer = 0.22;
    steadyNoHegemonEmpire.adaptiveState.warTrauma = 0.14;
    steadyNoHegemonEmpire.adaptiveState.trustInFederations = 0.52;

    strategic_nexus::StrategicSummary noHegemonBalanceSummary = quietSummary;
    noHegemonBalanceSummary.instability = 0.19;

    const auto noHegemonBalanceReject = engine.refineDoctrineDecision(
        steadyNoHegemonEmpire,
        noHegemonBalanceSummary,
        proposedHegemonBalance);
    requireCondition(
        noHegemonBalanceReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "balance-against-hegemon without a detected hegemon should resolve to defense");
    requireCondition(
        noHegemonBalanceReject.rationale ==
            "Rejected balance against hegemon: no confirmed hegemonic pressure yet.",
        "no-hegemon balance reject should explain the contradiction");
    requireCondition(
        noHegemonBalanceReject.confidence == 0.41,
        "no-hegemon balance reject should clamp confidence");
    const auto noHegemonBalanceRejectNote = engine.buildDoctrineAlignmentNote(
        steadyNoHegemonEmpire,
        proposedHegemonBalance,
        noHegemonBalanceReject);
    requireCondition(
        noHegemonBalanceRejectNote.find("rejected balance_against_hegemon in favor of defensive_posture") != std::string::npos,
        "no-hegemon balance reject note should explain the explicit contradiction rejection");
    requireCondition(
        noHegemonBalanceRejectNote.find("bias = cautious consolidation") != std::string::npos,
        "no-hegemon balance reject note should still expose the personality bias");

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

    strategic_nexus::EmpireState weakCalmEmpire = boldEmpire;
    weakCalmEmpire.id = "empire_weak_calm";
    weakCalmEmpire.power.fleetPower = 28;
    weakCalmEmpire.power.economicRank = 1;
    weakCalmEmpire.power.technologyRank = 1;
    weakCalmEmpire.personality.boldness = 0.34;
    weakCalmEmpire.personality.paranoia = 0.18;
    weakCalmEmpire.personality.honor = 0.56;
    weakCalmEmpire.personality.opportunism = 0.31;
    weakCalmEmpire.adaptiveState.fearOfPlayer = 0.19;
    weakCalmEmpire.adaptiveState.warTrauma = 0.12;
    weakCalmEmpire.adaptiveState.trustInFederations = 0.57;

    strategic_nexus::StrategicSummary weakCalmSummary = quietSummary;
    weakCalmSummary.instability = 0.11;

    const auto weakCalmConsolidateReject = engine.refineDoctrineDecision(
        weakCalmEmpire,
        weakCalmSummary,
        proposedConsolidate);
    requireCondition(
        weakCalmConsolidateReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak calm empire under low pressure should reject consolidate in favor of defense");
    requireCondition(
        weakCalmConsolidateReject.rationale ==
            "Rejected consolidate: weak capability and low pressure make passive consolidation unsafe.",
        "weak calm consolidate reject should explain the contradiction");
    requireCondition(
        weakCalmConsolidateReject.confidence == 0.45,
        "weak calm consolidate reject should clamp confidence");
    const auto weakCalmConsolidateRejectNote = engine.buildDoctrineAlignmentNote(
        weakCalmEmpire,
        proposedConsolidate,
        weakCalmConsolidateReject);
    requireCondition(
        weakCalmConsolidateRejectNote.find("rejected consolidate in favor of defensive_posture") != std::string::npos,
        "weak calm consolidate reject note should explain the explicit contradiction rejection");
    requireCondition(
        weakCalmConsolidateRejectNote.find("bias = cautious consolidation") != std::string::npos,
        "weak calm consolidate reject note should still expose the personality bias");

    strategic_nexus::StrategicSummary moderatePressureSummary = quietSummary;
    moderatePressureSummary.instability = 0.22;

    const auto moderatePressureNoHegemonConsolidateReject = engine.refineDoctrineDecision(
        weakCalmEmpire,
        moderatePressureSummary,
        proposedConsolidate);
    requireCondition(
        moderatePressureNoHegemonConsolidateReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak calm empire under moderate pressure should reject consolidate in favor of defense");
    requireCondition(
        moderatePressureNoHegemonConsolidateReject.rationale ==
            "Rejected consolidate: weak capability and moderate pressure still require immediate defense.",
        "moderate-pressure no-hegemon consolidate reject should explain the contradiction");
    requireCondition(
        moderatePressureNoHegemonConsolidateReject.confidence == 0.44,
        "moderate-pressure no-hegemon consolidate reject should clamp confidence");
    const auto moderatePressureNoHegemonConsolidateRejectNote = engine.buildDoctrineAlignmentNote(
        weakCalmEmpire,
        proposedConsolidate,
        moderatePressureNoHegemonConsolidateReject);
    requireCondition(
        moderatePressureNoHegemonConsolidateRejectNote.find("rejected consolidate in favor of defensive_posture") !=
            std::string::npos,
        "moderate-pressure no-hegemon consolidate reject note should explain the explicit contradiction rejection");
    requireCondition(
        moderatePressureNoHegemonConsolidateRejectNote.find("bias = cautious consolidation") != std::string::npos,
        "moderate-pressure no-hegemon consolidate reject note should still expose the personality bias");

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

    strategic_nexus::StrategicSummary risingPressureExpansionSummary = quietSummary;
    risingPressureExpansionSummary.instability = 0.34;

    const auto risingPressureExpansionReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        risingPressureExpansionSummary,
        proposedExpansion);
    requireCondition(
        risingPressureExpansionReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak fearful empire under rising pressure should reject opportunistic expansion in favor of defense");
    requireCondition(
        risingPressureExpansionReject.rationale ==
            "Rejected opportunistic expansion: weak capability, fear, and rising pressure still do not support it yet.",
        "rising-pressure opportunistic expansion reject should explain the contradiction");
    requireCondition(
        risingPressureExpansionReject.confidence == 0.45,
        "rising-pressure opportunistic expansion reject should clamp confidence");
    const auto risingPressureExpansionRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedExpansion,
        risingPressureExpansionReject);
    requireCondition(
        risingPressureExpansionRejectNote.find("rejected opportunistic_expansion in favor of defensive_posture") != std::string::npos,
        "rising-pressure opportunistic expansion reject note should explain the explicit contradiction rejection");
    requireCondition(
        risingPressureExpansionRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "rising-pressure opportunistic expansion reject note should still expose the personality bias");

    strategic_nexus::StrategicSummary elevatedPressureExpansionSummary = quietSummary;
    elevatedPressureExpansionSummary.instability = 0.52;

    const auto elevatedPressureExpansionReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        elevatedPressureExpansionSummary,
        proposedExpansion);
    requireCondition(
        elevatedPressureExpansionReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak fearful empire under elevated pressure should reject opportunistic expansion in favor of defense");
    requireCondition(
        elevatedPressureExpansionReject.rationale ==
            "Rejected opportunistic expansion: weak capability, fear, and elevated pressure still do not support it yet.",
        "elevated-pressure opportunistic expansion reject should explain the contradiction");
    requireCondition(
        elevatedPressureExpansionReject.confidence == 0.42,
        "elevated-pressure opportunistic expansion reject should clamp confidence");
    const auto elevatedPressureExpansionRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedExpansion,
        elevatedPressureExpansionReject);
    requireCondition(
        elevatedPressureExpansionRejectNote.find("rejected opportunistic_expansion in favor of defensive_posture") != std::string::npos,
        "elevated-pressure opportunistic expansion reject note should explain the explicit contradiction rejection");
    requireCondition(
        elevatedPressureExpansionRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "elevated-pressure opportunistic expansion reject note should still expose the personality bias");

    strategic_nexus::EmpireState lowTrustExpansionEmpire = boldEmpire;
    lowTrustExpansionEmpire.id = "empire_low_trust_expansion";
    lowTrustExpansionEmpire.power.fleetPower = 90;
    lowTrustExpansionEmpire.power.economicRank = 4;
    lowTrustExpansionEmpire.power.technologyRank = 4;
    lowTrustExpansionEmpire.personality.boldness = 0.46;
    lowTrustExpansionEmpire.personality.paranoia = 0.23;
    lowTrustExpansionEmpire.personality.honor = 0.54;
    lowTrustExpansionEmpire.personality.opportunism = 0.45;
    lowTrustExpansionEmpire.adaptiveState.fearOfPlayer = 0.20;
    lowTrustExpansionEmpire.adaptiveState.warTrauma = 0.14;
    lowTrustExpansionEmpire.adaptiveState.trustInFederations = 0.24;

    const auto lowTrustExpansionReject = engine.refineDoctrineDecision(
        lowTrustExpansionEmpire,
        quietSummary,
        proposedExpansion);
    requireCondition(
        lowTrustExpansionReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "low-trust empire under low pressure should reject opportunistic expansion in favor of defense");
    requireCondition(
        lowTrustExpansionReject.rationale ==
            "Rejected opportunistic expansion: low federation trust and low pressure do not support it yet.",
        "low-trust opportunistic expansion reject should explain the contradiction");
    requireCondition(
        lowTrustExpansionReject.confidence == 0.43,
        "low-trust opportunistic expansion reject should clamp confidence");
    const auto lowTrustExpansionRejectNote = engine.buildDoctrineAlignmentNote(
        lowTrustExpansionEmpire,
        proposedExpansion,
        lowTrustExpansionReject);
    requireCondition(
        lowTrustExpansionRejectNote.find("rejected opportunistic_expansion in favor of defensive_posture") != std::string::npos,
        "low-trust opportunistic expansion reject note should explain the explicit contradiction rejection");
    requireCondition(
        lowTrustExpansionRejectNote.find("bias = trauma-guarded caution") != std::string::npos,
        "low-trust opportunistic expansion reject note should still expose the personality bias");

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

    strategic_nexus::StrategicSummary risingPressureSummary = quietSummary;
    risingPressureSummary.instability = 0.34;

    const auto risingPressureConsolidateReject = engine.refineDoctrineDecision(
        fearfulEmpire,
        risingPressureSummary,
        proposedConsolidate);
    requireCondition(
        risingPressureConsolidateReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak fearful empire under rising pressure should reject consolidate in favor of defense");
    requireCondition(
        risingPressureConsolidateReject.rationale ==
            "Rejected consolidate: weak capability, fear, and rising pressure still require immediate defense.",
        "rising-pressure consolidate reject should explain the contradiction");
    requireCondition(
        risingPressureConsolidateReject.confidence == 0.45,
        "rising-pressure consolidate reject should clamp confidence");
    const auto risingPressureConsolidateRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulEmpire,
        proposedConsolidate,
        risingPressureConsolidateReject);
    requireCondition(
        risingPressureConsolidateRejectNote.find("rejected consolidate in favor of defensive_posture") != std::string::npos,
        "rising-pressure consolidate reject note should explain the explicit contradiction rejection");
    requireCondition(
        risingPressureConsolidateRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "rising-pressure consolidate reject note should still expose the personality bias");

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

    strategic_nexus::EmpireState traumatizedEmpire = boldEmpire;
    traumatizedEmpire.id = "empire_traumatized";
    traumatizedEmpire.power.fleetPower = 112;
    traumatizedEmpire.power.economicRank = 4;
    traumatizedEmpire.power.technologyRank = 4;
    traumatizedEmpire.personality.boldness = 0.46;
    traumatizedEmpire.personality.paranoia = 0.29;
    traumatizedEmpire.personality.honor = 0.52;
    traumatizedEmpire.personality.opportunism = 0.41;
    traumatizedEmpire.adaptiveState.fearOfPlayer = 0.19;
    traumatizedEmpire.adaptiveState.warTrauma = 0.83;
    traumatizedEmpire.adaptiveState.trustInFederations = 0.17;

    const auto traumatizedBalanceReject = engine.refineDoctrineDecision(
        traumatizedEmpire,
        hegemonicPressureSummary,
        proposedBalanceAgainstHegemon);
    requireCondition(
        traumatizedBalanceReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "traumatized empire under hegemonic pressure should reject balance-against-hegemon in favor of defense");
    requireCondition(
        traumatizedBalanceReject.rationale ==
            "Rejected balance against hegemon: war trauma and low federation trust make coalition balancing unreliable.",
        "trauma-guarded balance reject should explain the contradiction");
    requireCondition(
        traumatizedBalanceReject.confidence == 0.45,
        "trauma-guarded balance reject should clamp confidence");
    requireCondition(
        engine.describeStrategicBias(traumatizedEmpire) == "trauma-guarded caution",
        "traumatized empire should be described as trauma-guarded caution");
    const auto traumatizedBalanceRejectNote = engine.buildDoctrineAlignmentNote(
        traumatizedEmpire,
        proposedBalanceAgainstHegemon,
        traumatizedBalanceReject);
    requireCondition(
        traumatizedBalanceRejectNote.find("rejected balance_against_hegemon in favor of defensive_posture") !=
            std::string::npos,
        "trauma-guarded balance reject note should explain the explicit contradiction rejection");
    requireCondition(
        traumatizedBalanceRejectNote.find("bias = trauma-guarded caution") != std::string::npos,
        "trauma-guarded balance reject note should still expose the personality bias");

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

    strategic_nexus::EmpireState calmHegemonEmpire = boldEmpire;
    calmHegemonEmpire.id = "empire_calm_hegemon";
    calmHegemonEmpire.power.fleetPower = 44;
    calmHegemonEmpire.power.economicRank = 1;
    calmHegemonEmpire.power.technologyRank = 2;
    calmHegemonEmpire.personality.paranoia = 0.28;
    calmHegemonEmpire.personality.boldness = 0.31;
    calmHegemonEmpire.personality.opportunism = 0.37;
    calmHegemonEmpire.adaptiveState.fearOfPlayer = 0.22;
    calmHegemonEmpire.adaptiveState.warTrauma = 0.18;
    calmHegemonEmpire.adaptiveState.trustInFederations = 0.61;

    const auto calmHegemonConsolidateReject = engine.refineDoctrineDecision(
        calmHegemonEmpire,
        hegemonicPressureSummary,
        hegemonicConsolidate);
    requireCondition(
        calmHegemonConsolidateReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "weak calm empire under hegemonic pressure should reject consolidate in favor of defense");
    requireCondition(
        calmHegemonConsolidateReject.rationale ==
            "Rejected consolidate: a detected hegemon and capability limits require immediate defense.",
        "calm hegemon consolidate reject should explain the contradiction");
    requireCondition(
        calmHegemonConsolidateReject.confidence == 0.46,
        "calm hegemon consolidate reject should clamp confidence");
    const auto calmHegemonConsolidateRejectNote = engine.buildDoctrineAlignmentNote(
        calmHegemonEmpire,
        hegemonicConsolidate,
        calmHegemonConsolidateReject);
    requireCondition(
        calmHegemonConsolidateRejectNote.find("rejected consolidate in favor of defensive_posture") != std::string::npos,
        "calm hegemon consolidate reject note should explain the explicit contradiction rejection");
    requireCondition(
        calmHegemonConsolidateRejectNote.find("bias = cautious consolidation") != std::string::npos,
        "calm hegemon consolidate reject note should still expose the personality bias");

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

    strategic_nexus::EmpireState fearfulCapableExpansionEmpire = boldEmpire;
    fearfulCapableExpansionEmpire.id = "empire_fearful_capable_expansion";
    fearfulCapableExpansionEmpire.power.fleetPower = 92;
    fearfulCapableExpansionEmpire.power.economicRank = 4;
    fearfulCapableExpansionEmpire.power.technologyRank = 4;
    fearfulCapableExpansionEmpire.personality.boldness = 0.33;
    fearfulCapableExpansionEmpire.personality.paranoia = 0.79;
    fearfulCapableExpansionEmpire.personality.honor = 0.47;
    fearfulCapableExpansionEmpire.personality.opportunism = 0.71;
    fearfulCapableExpansionEmpire.adaptiveState.fearOfPlayer = 0.73;
    fearfulCapableExpansionEmpire.adaptiveState.warTrauma = 0.19;
    fearfulCapableExpansionEmpire.adaptiveState.trustInFederations = 0.48;

    strategic_nexus::StrategicSummary fearfulCapableExpansionSummary = quietSummary;
    fearfulCapableExpansionSummary.instability = 0.12;

    const auto fearfulCapableExpansionReject = engine.refineDoctrineDecision(
        fearfulCapableExpansionEmpire,
        fearfulCapableExpansionSummary,
        proposedExpansion);
    requireCondition(
        fearfulCapableExpansionReject.type == strategic_nexus::DoctrineType::DefensivePosture,
        "capable fearful empire under low pressure should still reject opportunistic expansion in favor of defense");
    requireCondition(
        fearfulCapableExpansionReject.rationale ==
            "Personality and capability do not support opportunistic expansion yet.",
        "capable fearful opportunistic fallback should explain the contradiction");
    requireCondition(
        fearfulCapableExpansionReject.confidence == 0.52,
        "capable fearful opportunistic fallback should clamp confidence");
    const auto fearfulCapableExpansionRejectNote = engine.buildDoctrineAlignmentNote(
        fearfulCapableExpansionEmpire,
        proposedExpansion,
        fearfulCapableExpansionReject);
    requireCondition(
        fearfulCapableExpansionRejectNote.find("downgraded from opportunistic_expansion to defensive_posture") !=
            std::string::npos,
        "capable fearful opportunistic fallback note should explain the alignment shift");
    requireCondition(
        fearfulCapableExpansionRejectNote.find("bias = risk-sensitive balancing") != std::string::npos,
        "capable fearful opportunistic fallback note should still expose the personality bias");

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
