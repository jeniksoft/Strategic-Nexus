// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct SncGeneratedOverlayStagingStatus {
    bool ok = false;
    std::string reason;
    std::string readiness = "blocked";
    bool dryRunOnly = true;
    bool publishAllowed = false;
    bool publishesOverlay = false;
    bool validatorPassed = false;
    bool manifestVerified = false;
    bool eventsWritten = false;
    bool effectsWritten = false;
    bool triggersWritten = false;
    bool manifestWritten = false;
    std::filesystem::path sourceDslPath;
    std::filesystem::path stagedOverlayDirectory;
    std::size_t dslRuleCount = 0;
    std::size_t manifestFileCount = 0;
    std::size_t unexpectedFileCount = 0;
    std::string manifestHash;
    std::vector<std::string> warningCodes;
    std::vector<std::string> validationErrors;
};

class SncGeneratedOverlayStager {
public:
    SncGeneratedOverlayStagingStatus stage(
        const std::filesystem::path& sourceDslPath,
        const std::filesystem::path& stagedOverlayDirectory) const;
};

std::string serializeSncGeneratedOverlayStagingStatus(const SncGeneratedOverlayStagingStatus& status);

} // namespace strategic_nexus
