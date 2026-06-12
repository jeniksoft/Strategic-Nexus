// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CoreTypes.h"
#include "SeasonEmpireBriefBuilder.h"

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct IntegratedEmpireFieldAvailability {
    std::string fieldGroup;
    std::string sourceQuality;
    bool available = false;
    std::vector<std::string> missingReasons;
};

struct EmpireInternalPressureProfile {
    double securityAnxiety = 0.0;
    double economicPressure = 0.0;
    double publicTrauma = 0.0;
    double eliteAgenda = 0.0;
    double prestigeNeed = 0.0;
    double diplomaticFlexibility = 0.0;
    double warExhaustionMemory = 0.0;
    std::string summary;
};

struct StrategicReputationProfile {
    double privateSelfImage = 0.0;
    double observerAudiencePerception = 0.0;
    std::string privateSelfImageSummary;
    std::string observerAudiencePerceptionSummary;
};

struct DoctrineInertiaProfile {
    double inertia = 0.0;
    double reformCost = 0.0;
    std::string inertiaSummary;
    std::string reformCostSummary;
};

struct IntegratedEmpireState {
    bool ok = false;
    std::string reason;
    std::string campaignId;
    std::string empireId;
    std::string sourceBriefQuality;
    std::string sourceEmpireStateQuality;
    bool personalityProfileApplied = false;
    int personalityProfileSourceSchemaVersion = 0;
    std::string personalityProfileSchemaCompatibilityState;
    std::string personalityProfileSchemaCompatibilityNote;
    std::string personalityProfileValidatedUpdateSummary;
    std::string personalityProfilePromptOutputNote;
    std::string personalityProfileSourceSaveDate;
    bool personalityProfileZeroHistoryBootstrap = false;
    double confidence = 0.0;
    bool summaryOnly = true;
    std::vector<std::string> evidenceReferences;
    EmpireInternalPressureProfile internalPressure;
    StrategicReputationProfile strategicReputation;
    DoctrineInertiaProfile doctrineInertia;
    std::vector<IntegratedEmpireFieldAvailability> fieldAvailability;
    std::size_t fieldAvailabilityAvailableCount = 0;
    std::size_t fieldAvailabilityMissingCount = 0;
    std::string fieldAvailabilitySummary;
    std::vector<std::string> missingInformation;
    std::vector<std::string> compressionNotes;
};

class IntegratedEmpireStateBuilder {
public:
    explicit IntegratedEmpireStateBuilder(
        std::filesystem::path personalityProfileRoot = "dist/private_reports/personality_profiles");

    IntegratedEmpireState build(
        const SeasonEmpireBrief& brief,
        const EmpireState& empire,
        double confidence) const;

private:
    std::filesystem::path personalityProfileRoot_;
};

std::string serializeIntegratedEmpireState(const IntegratedEmpireState& state);

} // namespace strategic_nexus
