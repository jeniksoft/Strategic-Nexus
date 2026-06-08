// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>

namespace strategic_nexus {

struct SncPostPlayArtifactBackfillConfig {
    std::filesystem::path dslDraftPath;
    std::filesystem::path dslDraftAuditPath;
    std::filesystem::path candidateDecisionPackagePath;
    std::filesystem::path generatedOverlayStagingStatusPath;
    std::filesystem::path generatedOverlayStagingDirectory;
    std::filesystem::path mpOverlayPackageDirectory;
    std::filesystem::path mpOverlayPackageZipPath;
    std::string mpOverlayVersion = "overlay_v0_session";
    std::string mpGameVersion = "stellaris_4.x";
    std::string mpModVersion = "strategic_nexus_v0";
};

struct SncPostPlayArtifactBackfillResult {
    bool attempted = false;
    bool stagedOverlayStatusWritten = false;
    bool stagedOverlayReady = false;
    bool mpOverlayPackageExported = false;
    std::string mpPackageRefreshState = "not_attempted";
    std::string mpPackageRefreshReason = "waiting_for_stellaris_session";
};

class SncPostPlayArtifactBackfiller {
public:
    SncPostPlayArtifactBackfillResult backfill(const SncPostPlayArtifactBackfillConfig& config) const;
};

} // namespace strategic_nexus
