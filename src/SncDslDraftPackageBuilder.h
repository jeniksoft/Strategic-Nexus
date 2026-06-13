// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "SncCandidateDecisionPackageBuilder.h"

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct SncDslDraftPackage {
    bool ok = false;
    std::string reason;
    std::string readiness;
    bool dryRunOnly = true;
    bool publishesOverlay = false;
    bool modelOutputUsed = false;
    bool modelOutputTrusted = false;
    bool validationRequired = true;
    bool validatorPassed = false;
    bool overlayCompileAllowed = false;
    std::filesystem::path sourceCandidateDecisionPackagePath;
    std::string sourceCandidateReadiness;
    std::size_t sourceCandidateSchemaVersion = 0;
    std::string schemaCompatibilityState = "current";
    std::string schemaCompatibilityNote;
    std::size_t candidateDecisionCount = 0;
    std::size_t eligibleCandidateCount = 0;
    std::size_t skippedCandidateCount = 0;
    std::size_t dslRuleCount = 0;
    std::vector<std::string> warningCodes;
    std::vector<std::string> validationErrors;
    std::vector<std::string> skippedCandidateIds;
    std::string dslText;
};

class SncDslDraftPackageBuilder {
public:
    SncDslDraftPackage build(
        const SncCandidateDecisionPackage& candidatePackage,
        const std::filesystem::path& sourceCandidateDecisionPackagePath = std::filesystem::path()) const;
};

SncCandidateDecisionPackageReadResult parseSncCandidateDecisionPackageJson(const std::string& json);
std::string serializeSncDslDraftPackage(const SncDslDraftPackage& package);

} // namespace strategic_nexus
