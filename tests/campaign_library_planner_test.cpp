// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "CampaignLibraryPlanner.h"

#include <iostream>
#include <string>

namespace {

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

strategic_nexus::CampaignSaveInventoryEntry makeEntry(
    const std::string& key,
    const std::string& hash)
{
    strategic_nexus::CampaignSaveInventoryEntry entry;
    entry.campaignKey = key;
    entry.displayName = key;
    entry.relativePath = key + ".sav";
    entry.sourceKind = "loose_save";
    entry.anchorSaveContentHash = hash;
    entry.anchorSaveHashAlgorithm = "fnv1a64";
    entry.anchorSaveName = key + ".sav";
    entry.saveFileCount = 1;
    entry.anchorSaveByteCount = hash.empty() ? 0 : 7;
    return entry;
}

} // namespace

int main()
{
    strategic_nexus::CampaignSaveInventory inventory;
    inventory.rootExists = true;
    inventory.entries.push_back(makeEntry("alpha", "hash_alpha"));
    inventory.entries.push_back(makeEntry("beta", ""));
    inventory.entries.push_back(makeEntry("gamma", "hash_gamma"));

    const strategic_nexus::CampaignLibraryPlanner planner;
    const auto plan = planner.build(inventory, 1);

    requireCondition(plan.saveRootAvailable, "planner should preserve save-root availability state");
    requireCondition(plan.limitReached, "planner should surface when the active library limit truncates local campaigns");
    requireCondition(plan.includedCount == 1, "planner should include up to the campaign limit");
    requireCondition(plan.skippedCount == 2, "planner should skip missing fingerprints and overflow");
    requireCondition(plan.skippedDueToLimitCount == 1, "planner should count campaigns skipped specifically because of the active library limit");
    requireCondition(plan.entries[0].status == "included", "planner should include first safe local campaign");
    requireCondition(plan.entries[1].reason == "missing_anchor_fingerprint", "planner should skip campaigns without fingerprints");
    requireCondition(plan.entries[2].reason == "active_library_limit", "planner should skip campaigns over the active library limit");

    const auto json = strategic_nexus::serializeCampaignLibraryPlan(plan);
    requireCondition(json.find("\"save_root_available\": true") != std::string::npos, "plan JSON should include save-root availability");
    requireCondition(json.find("\"limit_reached\": true") != std::string::npos, "plan JSON should include limit-reached state");
    requireCondition(json.find("\"included_count\": 1") != std::string::npos, "plan JSON should include included count");
    requireCondition(json.find("\"skipped_due_to_limit_count\": 1") != std::string::npos, "plan JSON should include skipped-due-to-limit count");
    requireCondition(json.find("\"reason\": \"active_library_limit\"") != std::string::npos, "plan JSON should include limit reason");
    requireCondition(json.find("\"reason\": \"missing_anchor_fingerprint\"") != std::string::npos, "plan JSON should include fingerprint reason");

    strategic_nexus::CampaignSaveInventory missingRootInventory;
    const auto missingRootPlan = planner.build(missingRootInventory, 2);
    requireCondition(!missingRootPlan.saveRootAvailable, "planner should preserve unavailable save-root state");
    requireCondition(!missingRootPlan.limitReached, "missing root without entries should not surface a spurious limit-reached state");
    requireCondition(missingRootPlan.includedCount == 0, "missing root should not include campaigns");
    requireCondition(missingRootPlan.skippedCount == 0, "missing root without entries should not synthesize skipped campaigns");
    requireCondition(missingRootPlan.skippedDueToLimitCount == 0, "missing root without entries should not synthesize limit-skipped campaigns");

    const auto missingRootJson = strategic_nexus::serializeCampaignLibraryPlan(missingRootPlan);
    requireCondition(missingRootJson.find("\"save_root_available\": false") != std::string::npos, "missing-root plan JSON should include unavailable state");
    requireCondition(missingRootJson.find("\"limit_reached\": false") != std::string::npos, "missing-root plan JSON should include non-limit state");

    std::cout << "campaign library planner tests passed.\n";
    return 0;
}

