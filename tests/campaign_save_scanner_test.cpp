// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "CampaignSaveScanner.h"

#include <filesystem>
#include <fstream>
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

void writeFile(const std::filesystem::path& path, const std::string& content = "fixture")
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << content;
}

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/campaign_save_scanner_fixture");
    std::filesystem::remove_all(root);
    writeFile(root / "Alpha Campaign" / "autosave_2230.sav", "alpha-anchor");
    writeFile(root / "Alpha Campaign" / "ironman.sav", "alpha-ironman");
    writeFile(root / "Beta.sav", "beta-loose");
    writeFile(root / "Ignored Folder" / "note.txt");
    writeFile(root / "not_a_save.txt");

    const strategic_nexus::CampaignSaveScanner scanner;
    const auto inventory = scanner.scan(root);
    requireCondition(inventory.rootExists, "scanner should mark existing root");
    requireCondition(inventory.entries.size() == 2, "scanner should find one campaign directory and one loose save");
    requireCondition(inventory.entries[0].campaignKey == "alpha_campaign", "scanner should stabilize directory campaign key");
    requireCondition(inventory.entries[0].saveFileCount == 2, "scanner should count directory save files");
    requireCondition(inventory.entries[0].anchorSaveName == "autosave_2230.sav", "scanner should choose stable directory anchor save");
    requireCondition(!inventory.entries[0].anchorSaveContentHash.empty(), "scanner should hash directory anchor save");
    requireCondition(inventory.entries[1].campaignKey == "beta", "scanner should stabilize loose save campaign key");
    requireCondition(inventory.entries[1].sourceKind == "loose_save", "scanner should classify loose save");
    requireCondition(inventory.entries[1].anchorSaveByteCount == std::string("beta-loose").size(), "scanner should record loose save byte count");

    const auto json = strategic_nexus::serializeCampaignSaveInventory(inventory);
    requireCondition(json.find("\"campaign_count\": 2") != std::string::npos, "inventory JSON should include campaign count");
    requireCondition(json.find("\"source_kind\": \"campaign_directory\"") != std::string::npos, "inventory JSON should include directory source kind");
    requireCondition(json.find("\"source_kind\": \"loose_save\"") != std::string::npos, "inventory JSON should include loose save source kind");
    requireCondition(json.find("\"anchor_save_hash_algorithm\": \"fnv1a64\"") != std::string::npos, "inventory JSON should include anchor save hash metadata");

    const auto nextRoot = std::filesystem::path("dist/campaign_save_scanner_fixture_next");
    std::filesystem::remove_all(nextRoot);
    writeFile(nextRoot / "Alpha Campaign" / "autosave_2230.sav", "alpha-anchor");
    writeFile(nextRoot / "Alpha Campaign" / "autosave_2231.sav", "alpha-mid");
    writeFile(nextRoot / "Alpha Campaign" / "autosave_2232.sav", "alpha-late");
    writeFile(nextRoot / "Gamma.sav", "gamma-loose");

    const auto nextInventory = scanner.scan(nextRoot);
    const auto diff = strategic_nexus::diffCampaignSaveInventories(inventory, nextInventory);
    requireCondition(diff.addedCount == 1, "inventory diff should detect added campaigns");
    requireCondition(diff.removedCount == 1, "inventory diff should detect removed campaigns");
    requireCondition(diff.changedCount == 1, "inventory diff should detect changed save counts");
    requireCondition(diff.unchangedCount == 0, "inventory diff should not mark changed entries unchanged");

    const auto diffJson = strategic_nexus::serializeCampaignSaveInventoryDiff(diff);
    requireCondition(diffJson.find("\"change_kind\": \"added\"") != std::string::npos, "diff JSON should include added changes");
    requireCondition(diffJson.find("\"change_kind\": \"removed\"") != std::string::npos, "diff JSON should include removed changes");
    requireCondition(diffJson.find("\"change_kind\": \"changed\"") != std::string::npos, "diff JSON should include changed changes");
    requireCondition(diffJson.find("\"anchor_save_content_hash\":") != std::string::npos, "diff JSON should include anchor save hashes");

    const auto renamedRoot = std::filesystem::path("dist/campaign_save_scanner_fixture_renamed");
    std::filesystem::remove_all(renamedRoot);
    writeFile(renamedRoot / "Alpha Campaign Renamed" / "autosave_2230.sav", "alpha-anchor");
    writeFile(renamedRoot / "Alpha Campaign Renamed" / "ironman.sav", "alpha-ironman");
    writeFile(renamedRoot / "Gamma.sav", "gamma-loose");

    const auto renamedInventory = scanner.scan(renamedRoot);
    const auto renamedDiff = strategic_nexus::diffCampaignSaveInventories(inventory, renamedInventory);
    requireCondition(renamedDiff.addedCount == 1, "inventory diff should still detect newly added campaigns alongside rename");
    requireCondition(renamedDiff.removedCount == 1, "inventory diff should still detect removed campaigns alongside rename");
    requireCondition(renamedDiff.renamedCount == 1, "inventory diff should detect renamed campaigns by anchor fingerprint");
    requireCondition(renamedDiff.restoredCount == 0, "inventory rename diff should not misclassify rename as restored continuity");
    requireCondition(renamedDiff.changedCount == 0, "inventory rename diff should not misclassify rename as changed");
    requireCondition(renamedDiff.unchangedCount == 0, "inventory rename diff should not mark renamed entries unchanged");

    const auto renamedDiffJson = strategic_nexus::serializeCampaignSaveInventoryDiff(renamedDiff);
    requireCondition(renamedDiffJson.find("\"renamed_count\": 1") != std::string::npos, "diff JSON should include renamed count");
    requireCondition(renamedDiffJson.find("\"change_kind\": \"renamed\"") != std::string::npos, "diff JSON should include renamed changes");
    requireCondition(
        renamedDiffJson.find("\"previous_relative_path\": \"Alpha Campaign\"") != std::string::npos &&
        renamedDiffJson.find("\"current_relative_path\": \"Alpha Campaign Renamed\"") != std::string::npos,
        "diff JSON should preserve previous and current rename paths");

    const auto restoredRoot = std::filesystem::path("dist/campaign_save_scanner_fixture_restored");
    std::filesystem::remove_all(restoredRoot);
    writeFile(restoredRoot / "Alpha Campaign Restored.sav", "alpha-anchor");
    writeFile(restoredRoot / "Gamma.sav", "gamma-loose");

    const auto restoredInventory = scanner.scan(restoredRoot);
    const auto restoredDiff = strategic_nexus::diffCampaignSaveInventories(inventory, restoredInventory);
    requireCondition(restoredDiff.addedCount == 1, "inventory restore diff should still detect unrelated newly added campaigns");
    requireCondition(restoredDiff.removedCount == 1, "inventory restore diff should still detect unrelated removed campaigns");
    requireCondition(restoredDiff.renamedCount == 0, "inventory restore diff should not classify source-kind continuity as rename");
    requireCondition(restoredDiff.restoredCount == 1, "inventory restore diff should emit restored continuity when anchor fingerprint returns in a different source kind");
    requireCondition(restoredDiff.changedCount == 0, "inventory restore diff should not misclassify restored continuity as changed");
    requireCondition(restoredDiff.unchangedCount == 0, "inventory restore diff should not mark restored continuity unchanged");

    const auto restoredDiffJson = strategic_nexus::serializeCampaignSaveInventoryDiff(restoredDiff);
    requireCondition(restoredDiffJson.find("\"restored_count\": 1") != std::string::npos, "diff JSON should include restored count");
    requireCondition(restoredDiffJson.find("\"change_kind\": \"restored\"") != std::string::npos, "diff JSON should include restored continuity changes");
    requireCondition(
        restoredDiffJson.find("\"previous_source_kind\": \"campaign_directory\"") != std::string::npos &&
        restoredDiffJson.find("\"current_source_kind\": \"loose_save\"") != std::string::npos,
        "restored continuity JSON should preserve previous/current source kinds");

    std::cout << "campaign save scanner tests passed.\n";
    return 0;
}

