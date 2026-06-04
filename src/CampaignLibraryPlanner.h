// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CampaignSaveScanner.h"

#include <cstddef>
#include <string>
#include <vector>

namespace strategic_nexus {

struct CampaignLibraryPlanEntry {
    std::string campaignKey;
    std::string displayName;
    std::string relativePath;
    std::string anchorSaveContentHash;
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
    std::vector<CampaignLibraryPlanEntry> entries;
};

class CampaignLibraryPlanner {
public:
    CampaignLibraryPlan build(const CampaignSaveInventory& inventory, std::size_t maxIncludedCampaigns) const;
};

std::string serializeCampaignLibraryPlan(const CampaignLibraryPlan& plan);

} // namespace strategic_nexus

