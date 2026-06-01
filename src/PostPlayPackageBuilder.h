// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "AutosaveArchiveSummarizer.h"
#include "SaveEntryPointAnalyzer.h"

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct PostPlayPackageEntry {
    std::string packageEntryId;
    std::string entryPointId;
    std::string campaignKey;
    std::string sourceKind;
    std::string saveName;
    std::string saveDate;
    std::string contentHash;
    std::string analysisState;
    std::string decisionInputState;
    std::string ruleScope;
    std::string evidencePolicy;
    bool decisionInputAllowed = false;
    bool compatibleHistoryAvailable = false;
    bool futureEvidenceExcluded = false;
    std::size_t compatibleArchivedEvidenceCount = 0;
    std::size_t laterArchivedEvidenceCount = 0;
    std::vector<std::string> warningCodes;
};

struct PostPlayPackageCampaign {
    std::string campaignKey;
    std::string readiness;
    bool branchAmbiguityDetected = false;
    std::size_t entryPointCount = 0;
    std::size_t decisionReadyEntryCount = 0;
    std::size_t archivedEvidenceCount = 0;
    std::vector<std::string> warningCodes;
};

struct PostPlayPackage {
    bool ok = false;
    std::string reason;
    std::string readiness;
    bool dryRunOnly = true;
    bool publishesOverlay = false;
    std::filesystem::path sessionArchiveDirectory;
    std::string entryPointAnalysisReadiness;
    bool archiveVerified = false;
    bool branchAmbiguityDetected = false;
    std::size_t copiedSaveCount = 0;
    std::uintmax_t totalByteCount = 0;
    std::size_t entryPointCount = 0;
    std::size_t decisionReadyEntryCount = 0;
    std::size_t archivedEvidenceCount = 0;
    std::vector<std::string> warningCodes;
    std::vector<PostPlayPackageCampaign> campaigns;
    std::vector<PostPlayPackageEntry> entries;
};

class PostPlayPackageBuilder {
public:
    PostPlayPackage build(
        const AutosaveArchiveSummary& archiveSummary,
        const SaveEntryPointAnalysis& entryPointAnalysis) const;
};

std::string serializePostPlayPackage(const PostPlayPackage& package);

} // namespace strategic_nexus
