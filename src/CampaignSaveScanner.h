// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace strategic_nexus {

struct CampaignSaveInventoryEntry {
    std::string campaignKey;
    std::string displayName;
    std::string relativePath;
    std::string sourceKind;
    std::string anchorSaveName;
    std::string anchorSaveHashAlgorithm;
    std::string anchorSaveContentHash;
    std::size_t saveFileCount = 0;
    std::uintmax_t anchorSaveByteCount = 0;
};

struct CampaignSaveInventory {
    bool rootExists = false;
    std::filesystem::path rootPath;
    std::vector<CampaignSaveInventoryEntry> entries;
};

struct CampaignSaveInventoryChange {
    std::string changeKind;
    CampaignSaveInventoryEntry previousEntry;
    CampaignSaveInventoryEntry currentEntry;
};

struct CampaignSaveInventoryDiff {
    std::size_t addedCount = 0;
    std::size_t removedCount = 0;
    std::size_t renamedCount = 0;
    std::size_t changedCount = 0;
    std::size_t unchangedCount = 0;
    std::vector<CampaignSaveInventoryChange> changes;
};

class CampaignSaveScanner {
public:
    CampaignSaveInventory scan(const std::filesystem::path& saveRoot) const;
};

CampaignSaveInventoryDiff diffCampaignSaveInventories(
    const CampaignSaveInventory& previous,
    const CampaignSaveInventory& current);

std::string serializeCampaignSaveInventory(const CampaignSaveInventory& inventory);
std::string serializeCampaignSaveInventoryDiff(const CampaignSaveInventoryDiff& diff);

} // namespace strategic_nexus

