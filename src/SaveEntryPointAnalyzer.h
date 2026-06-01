// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct SaveEmpireStateSummary {
    bool parsed = false;
    std::string parseStatus;
    std::string parserReason;
    std::string playerCountryId;
    std::string empireName;
    std::string government;
    std::string authority;
    std::string founderSpeciesName;
    std::string capitalPlanetName;
    std::string homeSystemName;
    std::size_t ownedFleetCount = 0;
    std::size_t activeWarCount = 0;
    std::vector<std::string> missingFields;
    std::vector<std::string> uncertainties;
};

struct SaveEntryPoint {
    std::string id;
    std::string campaignKey;
    std::string sourceRoot;
    std::string relativePath;
    std::string sourceKind;
    std::string saveName;
    std::string saveDate;
    std::string hashAlgorithm;
    std::string contentHash;
    std::uintmax_t byteCount = 0;
    std::string parseStatus;
    std::string playerCountryId;
    std::string empireName;
    SaveEmpireStateSummary empireState;
    std::size_t compatibleArchivedEvidenceCount = 0;
    std::size_t laterArchivedEvidenceCount = 0;
    std::string analysisState;
    std::vector<std::string> warningCodes;
};

struct ArchivedSaveEvidence {
    std::string campaignKey;
    std::string archivedPath;
    std::string saveName;
    std::string saveDate;
    std::string hashAlgorithm;
    std::string contentHash;
    std::uintmax_t byteCount = 0;
};

struct SaveEntryPointCampaignAnalysis {
    std::string campaignKey;
    std::size_t entryPointCount = 0;
    std::size_t archivedEvidenceCount = 0;
    bool branchAmbiguityDetected = false;
    std::vector<std::string> warningCodes;
};

struct SaveEntryPointAnalysis {
    bool ok = false;
    std::string reason;
    bool archiveVerified = false;
    std::filesystem::path sessionArchiveDirectory;
    std::vector<std::filesystem::path> saveRoots;
    std::size_t existingRootCount = 0;
    std::size_t entryPointCount = 0;
    std::size_t archivedEvidenceCount = 0;
    bool branchAmbiguityDetected = false;
    std::string readiness;
    std::vector<std::string> warningCodes;
    std::vector<SaveEntryPoint> entryPoints;
    std::vector<ArchivedSaveEvidence> archivedEvidence;
    std::vector<SaveEntryPointCampaignAnalysis> campaigns;
};

class SaveEntryPointAnalyzer {
public:
    SaveEntryPointAnalysis analyze(
        const std::filesystem::path& sessionArchiveDirectory,
        const std::vector<std::filesystem::path>& saveRoots) const;
};

std::string serializeSaveEntryPointAnalysis(const SaveEntryPointAnalysis& analysis);

} // namespace strategic_nexus
