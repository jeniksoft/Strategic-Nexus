// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "CampaignLibraryPlanner.h"

#include "common/FileUtil.h"

#include <filesystem>
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

    const auto pinManifestPath = std::filesystem::temp_directory_path() / "strategic_nexus_campaign_library_pins_test.json";
    requireCondition(
        strategic_nexus::writeCampaignLibraryPins(pinManifestPath, {"gamma", "delta", "gamma"}),
        "pin manifest should be writable");
    const auto pinSet = strategic_nexus::loadCampaignLibraryPins(pinManifestPath);
    requireCondition(pinSet.present, "pin manifest should be detected as present");
    requireCondition(pinSet.schemaSupported, "pin manifest should use schema version 1");
    requireCondition(pinSet.pinnedCount == 2, "pin manifest should deduplicate campaign keys");
    requireCondition(pinSet.state == "ready", "pin manifest should surface ready state");

    strategic_nexus::CampaignSaveInventory pinnedInventory;
    pinnedInventory.rootExists = false;
    pinnedInventory.entries.push_back(makeEntry("alpha", "hash_alpha"));
    pinnedInventory.entries.push_back(makeEntry("beta", "hash_beta"));

    const auto pinnedPlan = planner.build(pinnedInventory, 3, pinSet);
    requireCondition(pinnedPlan.pinnedCount == 2, "pinned plan should count included pinned campaigns");
    requireCondition(pinnedPlan.pinnedMissingLocalSaveCount == 2, "pinned plan should count synthetic pinned campaigns");
    requireCondition(pinnedPlan.includedCount == 3, "pinned plan should include pinned campaigns ahead of unpinned campaigns");
    requireCondition(pinnedPlan.limitReached, "pinned plan should still respect the active library limit");
    requireCondition(pinnedPlan.skippedDueToLimitCount == 1, "pinned plan should still count overflow skipped by the limit");
    requireCondition(
        pinnedPlan.entries[0].status == "included_pinned_missing_local_save",
        "missing pinned campaigns should stay available as synthetic included entries");
    requireCondition(
        pinnedPlan.entries[0].pinned,
        "missing pinned campaigns should be marked pinned");
    requireCondition(
        pinnedPlan.entries[2].status == "included",
        "unpinned local campaigns should still be included when capacity remains");
    requireCondition(
        pinnedPlan.entries[3].reason == "active_library_limit",
        "overflow local campaigns should still be skipped by the limit");

    const auto pinnedJson = strategic_nexus::serializeCampaignLibraryPlan(pinnedPlan);
    requireCondition(pinnedJson.find("\"pinned_count\": 2") != std::string::npos, "pinned plan JSON should include pinned count");
    requireCondition(
        pinnedJson.find("\"pinned_missing_local_save_count\": 2") != std::string::npos,
        "pinned plan JSON should include missing local save count");
    requireCondition(
        pinnedJson.find("\"status\": \"included_pinned_missing_local_save\"") != std::string::npos,
        "pinned plan JSON should include synthetic pinned status");

    const auto legacyPinManifestPath = std::filesystem::temp_directory_path() / "strategic_nexus_campaign_library_pins_legacy_test.json";
    requireCondition(
        strategic_nexus::writeCampaignLibraryPins(legacyPinManifestPath, {"legacy_alpha", "legacy_beta"}),
        "legacy pin manifest fixture should be writable");
    std::string legacyPinJson = strategic_nexus::common::readTextFile(legacyPinManifestPath);
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(
            legacyPinManifestPath,
            legacyPinJson.replace(
                legacyPinJson.find("\"schema_version\": 1"),
                std::string("\"schema_version\": 1").size(),
                "\"schema_version\": 0")),
        "legacy pin manifest fixture should be rewritten to schema_version 0");
    const auto legacyPinSet = strategic_nexus::loadCampaignLibraryPins(legacyPinManifestPath);
    requireCondition(legacyPinSet.present, "legacy pin manifest should still be detected as present");
    requireCondition(legacyPinSet.schemaSupported, "legacy pin manifest should be treated as degraded compatibility");
    requireCondition(legacyPinSet.state == "degraded", "legacy pin manifest should surface degraded state");
    requireCondition(
        legacyPinSet.reason == "pinned campaign exception manifest loaded from legacy schema_version 0",
        "legacy pin manifest should explain the degraded compatibility");
    requireCondition(
        legacyPinSet.nextStep.find("schema_version 1 manifest") != std::string::npos,
        "legacy pin manifest should recommend regeneration");
    requireCondition(legacyPinSet.pinnedCount == 2, "legacy pin manifest should still preserve pinned keys");

    std::error_code cleanupError;
    std::filesystem::remove(pinManifestPath, cleanupError);
    std::filesystem::remove(legacyPinManifestPath, cleanupError);

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

