// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CampaignSaveScanner.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct CampaignLibraryPinSet {
    bool present = false;
    bool schemaSupported = false;
    std::filesystem::path path;
    std::string state = "unavailable";
    std::string reason =
        "user-pinned campaign exceptions are not yet available; keep the local save root present or restore it before broader coverage tests";
    std::string nextStep =
        "keep the local save root present or restore it before broader coverage tests";
    std::size_t pinnedCount = 0;
    std::vector<std::string> pinnedCampaignKeys;
};

struct CampaignLibraryPlanEntry {
    std::string campaignKey;
    std::string displayName;
    std::string relativePath;
    std::string anchorSaveContentHash;
    bool pinned = false;
    bool locallyPlayable = true;
    std::string status;
    std::string reason;
};

struct CampaignLibraryPlan {
    bool saveRootAvailable = false;
    bool limitReached = false;
    std::size_t maxIncludedCampaigns = 0;
    std::size_t includedCount = 0;
    std::size_t skippedCount = 0;
    std::size_t skippedDueToLimitCount = 0;
    std::size_t pinnedCount = 0;
    std::size_t pinnedMissingLocalSaveCount = 0;
    std::vector<CampaignLibraryPlanEntry> entries;
};

class CampaignLibraryPlanner {
public:
    CampaignLibraryPlan build(const CampaignSaveInventory& inventory, std::size_t maxIncludedCampaigns) const;
    CampaignLibraryPlan build(
        const CampaignSaveInventory& inventory,
        std::size_t maxIncludedCampaigns,
        const CampaignLibraryPinSet& pinSet) const;
};

CampaignLibraryPinSet loadCampaignLibraryPins(const std::filesystem::path& path);
bool writeCampaignLibraryPins(
    const std::filesystem::path& path,
    const std::vector<std::string>& pinnedCampaignKeys);

std::string serializeCampaignLibraryPlan(const CampaignLibraryPlan& plan);

} // namespace strategic_nexus

