// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "SeasonEmpireBriefBuilder.h"

#include <string>
#include <vector>

namespace strategic_nexus {

struct ObserverTargetRelationshipDelta {
    double generalTrust = 0.0;
    double predictedFutureBehavior = 0.0;
    std::string generalTrustSummary;
    std::string predictedFutureBehaviorSummary;
};

struct ObserverTargetFieldAvailability {
    std::string fieldGroup;
    std::string sourceQuality;
    bool available = false;
    std::vector<std::string> missingReasons;
};

struct ObserverTargetProfile {
    bool ok = false;
    std::string reason;
    std::string campaignId;
    std::string observerEmpireId;
    std::string targetEmpireId;
    std::string sourceBriefQuality;
    double confidence = 0.0;
    bool summaryOnly = true;
    std::vector<std::string> evidenceReferences;
    std::vector<std::string> allowedRuleDomains;
    std::vector<std::string> targetSpecificRuleCandidates;
    ObserverTargetRelationshipDelta relationshipDelta;
    std::vector<ObserverTargetFieldAvailability> fieldAvailability;
    std::vector<std::string> missingInformation;
    std::vector<std::string> compressionNotes;
};

class ObserverTargetProfileBuilder {
public:
    ObserverTargetProfile build(
        const SeasonEmpireBrief& observerBrief,
        const std::string& targetEmpireId,
        double confidence) const;
};

std::string serializeObserverTargetProfile(const ObserverTargetProfile& profile);

} // namespace strategic_nexus
