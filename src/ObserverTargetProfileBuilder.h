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

struct ObserverTargetRuleCandidateValidation {
    bool ready = false;
    std::vector<std::string> candidateDomains;
    std::vector<std::string> requiredSignals;
    std::vector<std::string> blockedReasons;

    bool allowsDomain(const std::string& domain) const;
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
    std::string targetMemorySummary;
    double targetMemorySummaryConfidence = 0.0;
    std::string targetMemorySummaryConfidenceBand;
    std::vector<std::string> predictiveDimensions;
    std::string predictiveDimensionsSummary;
    std::vector<std::string> allowedRuleDomains;
    std::vector<std::string> targetSpecificRuleCandidates;
    ObserverTargetRuleCandidateValidation ruleCandidateValidation;
    ObserverTargetRelationshipDelta relationshipDelta;
    std::vector<ObserverTargetFieldAvailability> fieldAvailability;
    std::size_t fieldAvailabilityAvailableCount = 0;
    std::size_t fieldAvailabilityMissingCount = 0;
    std::string fieldAvailabilitySummary;
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
