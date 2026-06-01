// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "PostPlayPackageBuilder.h"

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct PostPlayPackageReadResult {
    bool ok = false;
    std::string reason;
    PostPlayPackage package;
};

struct SncDecisionInput {
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
    std::string evidencePolicy;
    bool modelOutputTrusted = false;
    bool validationRequired = true;
    bool futureEvidenceExcluded = false;
    std::size_t compatibleArchivedEvidenceCount = 0;
    std::size_t laterArchivedEvidenceCount = 0;
    std::vector<std::string> knownFacts;
    std::vector<std::string> uncertainties;
};

struct SncBlockedDecisionInput {
    std::string packageEntryId;
    std::string campaignKey;
    std::string ruleScope;
    std::string saveName;
    std::string saveDate;
    std::string decisionInputState;
    std::string reason;
    std::vector<std::string> warningCodes;
};

struct SncDecisionInputPackage {
    bool ok = false;
    std::string reason;
    std::string readiness;
    bool dryRunOnly = true;
    bool publishesOverlay = false;
    bool modelOutputTrusted = false;
    bool validationRequired = true;
    std::filesystem::path sourcePostPlayPackagePath;
    std::filesystem::path sessionArchiveDirectory;
    std::string sourcePostPlayReadiness;
    std::size_t entryPointCount = 0;
    std::size_t decisionInputCount = 0;
    std::size_t blockedEntryCount = 0;
    std::size_t archivedEvidenceCount = 0;
    std::vector<std::string> warningCodes;
    std::vector<SncDecisionInput> decisionInputs;
    std::vector<SncBlockedDecisionInput> blockedEntries;
};

class SncDecisionInputPackageBuilder {
public:
    SncDecisionInputPackage build(
        const PostPlayPackage& postPlayPackage,
        const std::filesystem::path& sourcePostPlayPackagePath = std::filesystem::path()) const;
};

PostPlayPackageReadResult parsePostPlayPackageJson(const std::string& json);
std::string serializeSncDecisionInputPackage(const SncDecisionInputPackage& package);

} // namespace strategic_nexus
