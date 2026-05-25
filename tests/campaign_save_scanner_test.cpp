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

void writeFile(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << "fixture";
}

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/campaign_save_scanner_fixture");
    std::filesystem::remove_all(root);
    writeFile(root / "Alpha Campaign" / "autosave_2230.sav");
    writeFile(root / "Alpha Campaign" / "ironman.sav");
    writeFile(root / "Beta.sav");
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
    requireCondition(inventory.entries[1].anchorSaveByteCount == 7, "scanner should record loose save byte count");

    const auto json = strategic_nexus::serializeCampaignSaveInventory(inventory);
    requireCondition(json.find("\"campaign_count\": 2") != std::string::npos, "inventory JSON should include campaign count");
    requireCondition(json.find("\"source_kind\": \"campaign_directory\"") != std::string::npos, "inventory JSON should include directory source kind");
    requireCondition(json.find("\"source_kind\": \"loose_save\"") != std::string::npos, "inventory JSON should include loose save source kind");
    requireCondition(json.find("\"anchor_save_hash_algorithm\": \"fnv1a64\"") != std::string::npos, "inventory JSON should include anchor save hash metadata");

    const auto nextRoot = std::filesystem::path("dist/campaign_save_scanner_fixture_next");
    std::filesystem::remove_all(nextRoot);
    writeFile(nextRoot / "Alpha Campaign" / "autosave_2230.sav");
    writeFile(nextRoot / "Alpha Campaign" / "autosave_2231.sav");
    writeFile(nextRoot / "Alpha Campaign" / "autosave_2232.sav");
    writeFile(nextRoot / "Gamma.sav");

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

    std::cout << "campaign save scanner tests passed.\n";
    return 0;
}

