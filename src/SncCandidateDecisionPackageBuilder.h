// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "SncDecisionInputPackageBuilder.h"

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct SncCandidateDecision {
    std::string candidateDecisionId;
    std::string decisionInputId;
    std::string packageEntryId;
    std::string campaignKey;
    std::string ruleScope;
    std::string sourceKind;
    std::string saveName;
    std::string saveDate;
    std::string contentHash;
    std::string parseStatus;
    std::string playerCountryId;
    std::string empireName;
    SaveEmpireStateSummary empireState;
    std::string candidateSource = "deterministic_v0_stub";
    std::string recommendedAction = "observe_only";
    std::string militaryPosture = "preserve";
    std::string researchBias = "preserve";
    std::string decisionBoundary = "dry_run_no_overlay_publish";
    std::string validationState = "validated_dry_run_candidate";
    int confidencePercent = 25;
    bool modelOutputUsed = false;
    bool modelOutputTrusted = false;
    bool validationRequired = true;
    bool publishesOverlay = false;
    std::vector<ArchivedSaveEvidenceReference> compatibleArchivedEvidenceSamples;
    std::vector<ArchivedSaveEvidenceReference> laterArchivedEvidenceSamples;
    std::vector<std::string> knownFacts;
    std::vector<std::string> uncertainties;
    std::vector<std::string> validationWarnings;
};

struct SncCandidateDecisionPackage {
    bool ok = false;
    std::string reason;
    std::string readiness;
    bool dryRunOnly = true;
    bool publishesOverlay = false;
    bool modelOutputUsed = false;
    bool modelOutputTrusted = false;
    bool validationRequired = true;
    bool validatorPassed = false;
    std::string candidateSource = "deterministic_v0_stub";
    std::filesystem::path sourceDecisionInputPackagePath;
    std::string sourceDecisionInputReadiness;
    std::size_t entryPointCount = 0;
    std::size_t decisionInputCount = 0;
    std::size_t candidateDecisionCount = 0;
    std::size_t acceptedCandidateCount = 0;
    std::size_t rejectedCandidateCount = 0;
    std::size_t blockedSourceEntryCount = 0;
    std::size_t archivedEvidenceCount = 0;
    std::vector<std::string> warningCodes;
    std::vector<std::string> validationErrors;
    std::vector<SncCandidateDecision> candidateDecisions;
    std::vector<SncBlockedDecisionInput> blockedSourceEntries;
};

struct SncCandidateDecisionPackageReadResult {
    bool ok = false;
    std::string reason;
    SncCandidateDecisionPackage package;
};

class SncCandidateDecisionPackageBuilder {
public:
    SncCandidateDecisionPackage build(
        const SncDecisionInputPackage& decisionInputPackage,
        const std::filesystem::path& sourceDecisionInputPackagePath = std::filesystem::path()) const;
};

SncCandidateDecisionPackageReadResult parseSncCandidateDecisionPackageJson(const std::string& json);
std::string serializeSncCandidateDecisionPackage(const SncCandidateDecisionPackage& package);

} // namespace strategic_nexus
