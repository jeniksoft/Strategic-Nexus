// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct SncGeneratedOverlayPublishGateRequest {
    std::filesystem::path stagingStatusPath;
    std::filesystem::path activeOverlayDirectory;
    std::filesystem::path backupRootDirectory;
    std::string ownerApprovalToken;
    bool stellarisRunning = false;
};

struct SncGeneratedOverlayPublishGateResult {
    bool ok = false;
    std::string reason;
    std::string readiness = "blocked";
    bool ownerApproved = false;
    bool backupCreated = false;
    bool published = false;
    bool stellarisRunning = false;
    std::filesystem::path stagingStatusPath;
    std::filesystem::path stagedOverlayDirectory;
    std::filesystem::path activeOverlayDirectory;
    std::filesystem::path backupDirectory;
    std::string sourceManifestHash;
    std::string publishedManifestHash;
    std::size_t publishedFileCount = 0;
    std::vector<std::string> warningCodes;
    std::vector<std::string> validationErrors;
};

class SncGeneratedOverlayPublishGate {
public:
    SncGeneratedOverlayPublishGateResult publish(
        const SncGeneratedOverlayPublishGateRequest& request) const;
};

std::string serializeSncGeneratedOverlayPublishGateResult(
    const SncGeneratedOverlayPublishGateResult& result);

} // namespace strategic_nexus
