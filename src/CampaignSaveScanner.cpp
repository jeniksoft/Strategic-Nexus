// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "CampaignSaveScanner.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace strategic_nexus {
namespace {

bool isSavFile(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".sav";
}

std::string stableKeyFromName(const std::string& name)
{
    std::string key;
    bool previousUnderscore = false;
    for (const char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            previousUnderscore = false;
        } else if (!previousUnderscore) {
            key.push_back('_');
            previousUnderscore = true;
        }
    }
    while (!key.empty() && key.back() == '_') {
        key.pop_back();
    }
    while (!key.empty() && key.front() == '_') {
        key.erase(key.begin());
    }
    return key.empty() ? "campaign" : key;
}

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const char ch : value) {
        switch (ch) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                output << ' ';
            } else {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

std::string genericPath(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::size_t countSavFilesInDirectory(const std::filesystem::path& directory)
{
    std::size_t count = 0;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            break;
        }
        if (entry.is_regular_file(error) && isSavFile(entry.path())) {
            ++count;
        }
    }
    return count;
}

std::optional<std::filesystem::path> firstSavFileInDirectory(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> saveFiles;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            break;
        }
        if (entry.is_regular_file(error) && isSavFile(entry.path())) {
            saveFiles.push_back(entry.path());
        }
    }

    if (saveFiles.empty()) {
        return std::nullopt;
    }

    std::sort(saveFiles.begin(), saveFiles.end(), [](const auto& left, const auto& right) {
        return genericPath(left.filename()) < genericPath(right.filename());
    });
    return saveFiles.front();
}

std::string hashFileFnv1a64Hex(const std::filesystem::path& path)
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offsetBasis;

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    char buffer[4096];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= prime;
        }
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

void populateAnchorSaveFingerprint(CampaignSaveInventoryEntry& entry, const std::filesystem::path& savePath)
{
    std::error_code error;
    entry.anchorSaveName = savePath.filename().string();
    entry.anchorSaveHashAlgorithm = "fnv1a64";
    entry.anchorSaveByteCount = std::filesystem::file_size(savePath, error);
    if (error) {
        entry.anchorSaveByteCount = 0;
    }
    entry.anchorSaveContentHash = hashFileFnv1a64Hex(savePath);
}

std::string entryIdentity(const CampaignSaveInventoryEntry& entry)
{
    return entry.campaignKey + "\n" + entry.relativePath + "\n" + entry.sourceKind;
}

bool entryMetadataChanged(const CampaignSaveInventoryEntry& previous, const CampaignSaveInventoryEntry& current)
{
    return previous.displayName != current.displayName ||
           previous.saveFileCount != current.saveFileCount ||
           previous.anchorSaveName != current.anchorSaveName ||
           previous.anchorSaveByteCount != current.anchorSaveByteCount ||
           previous.anchorSaveContentHash != current.anchorSaveContentHash;
}

} // namespace

CampaignSaveInventory CampaignSaveScanner::scan(const std::filesystem::path& saveRoot) const
{
    CampaignSaveInventory inventory;
    inventory.rootPath = saveRoot;

    std::error_code error;
    inventory.rootExists = std::filesystem::exists(saveRoot, error) && std::filesystem::is_directory(saveRoot, error);
    if (!inventory.rootExists) {
        return inventory;
    }

    for (const auto& entry : std::filesystem::directory_iterator(saveRoot, error)) {
        if (error) {
            break;
        }

        CampaignSaveInventoryEntry inventoryEntry;
        if (entry.is_directory(error)) {
            const auto saveCount = countSavFilesInDirectory(entry.path());
            if (saveCount == 0) {
                continue;
            }
            inventoryEntry.displayName = entry.path().filename().string();
            inventoryEntry.relativePath = genericPath(entry.path().filename());
            inventoryEntry.sourceKind = "campaign_directory";
            inventoryEntry.saveFileCount = saveCount;
            const auto anchorSavePath = firstSavFileInDirectory(entry.path());
            if (anchorSavePath.has_value()) {
                populateAnchorSaveFingerprint(inventoryEntry, *anchorSavePath);
            }
        } else if (entry.is_regular_file(error) && isSavFile(entry.path())) {
            inventoryEntry.displayName = entry.path().stem().string();
            inventoryEntry.relativePath = genericPath(entry.path().filename());
            inventoryEntry.sourceKind = "loose_save";
            inventoryEntry.saveFileCount = 1;
            populateAnchorSaveFingerprint(inventoryEntry, entry.path());
        } else {
            continue;
        }

        inventoryEntry.campaignKey = stableKeyFromName(inventoryEntry.displayName);
        inventory.entries.push_back(std::move(inventoryEntry));
    }

    std::sort(inventory.entries.begin(), inventory.entries.end(), [](const auto& left, const auto& right) {
        if (left.campaignKey != right.campaignKey) {
            return left.campaignKey < right.campaignKey;
        }
        return left.relativePath < right.relativePath;
    });

    return inventory;
}

CampaignSaveInventoryDiff diffCampaignSaveInventories(
    const CampaignSaveInventory& previous,
    const CampaignSaveInventory& current)
{
    CampaignSaveInventoryDiff diff;
    std::map<std::string, CampaignSaveInventoryEntry> previousEntries;
    std::map<std::string, CampaignSaveInventoryEntry> currentEntries;

    for (const auto& entry : previous.entries) {
        previousEntries[entryIdentity(entry)] = entry;
    }

    for (const auto& entry : current.entries) {
        currentEntries[entryIdentity(entry)] = entry;
    }

    for (const auto& [identity, currentEntry] : currentEntries) {
        const auto previousEntry = previousEntries.find(identity);
        if (previousEntry == previousEntries.end()) {
            ++diff.addedCount;
            diff.changes.push_back({"added", currentEntry, 0, currentEntry.saveFileCount});
        } else if (entryMetadataChanged(previousEntry->second, currentEntry)) {
            ++diff.changedCount;
            diff.changes.push_back({
                "changed",
                currentEntry,
                previousEntry->second.saveFileCount,
                currentEntry.saveFileCount
            });
        } else {
            ++diff.unchangedCount;
        }
    }

    for (const auto& [identity, previousEntry] : previousEntries) {
        if (currentEntries.find(identity) == currentEntries.end()) {
            ++diff.removedCount;
            diff.changes.push_back({"removed", previousEntry, previousEntry.saveFileCount, 0});
        }
    }

    std::sort(diff.changes.begin(), diff.changes.end(), [](const auto& left, const auto& right) {
        if (left.changeKind != right.changeKind) {
            return left.changeKind < right.changeKind;
        }
        if (left.entry.campaignKey != right.entry.campaignKey) {
            return left.entry.campaignKey < right.entry.campaignKey;
        }
        return left.entry.relativePath < right.entry.relativePath;
    });

    return diff;
}

std::string serializeCampaignSaveInventory(const CampaignSaveInventory& inventory)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"root_exists\": " << (inventory.rootExists ? "true" : "false") << ",\n";
    json << "  \"root_path\": \"" << jsonEscape(genericPath(inventory.rootPath)) << "\",\n";
    json << "  \"campaign_count\": " << inventory.entries.size() << ",\n";
    json << "  \"campaigns\": [\n";
    for (std::size_t i = 0; i < inventory.entries.size(); ++i) {
        const auto& entry = inventory.entries[i];
        json << "    {\n";
        json << "      \"campaign_key\": \"" << jsonEscape(entry.campaignKey) << "\",\n";
        json << "      \"display_name\": \"" << jsonEscape(entry.displayName) << "\",\n";
        json << "      \"relative_path\": \"" << jsonEscape(entry.relativePath) << "\",\n";
        json << "      \"source_kind\": \"" << jsonEscape(entry.sourceKind) << "\",\n";
        json << "      \"save_file_count\": " << entry.saveFileCount << ",\n";
        json << "      \"anchor_save_name\": \"" << jsonEscape(entry.anchorSaveName) << "\",\n";
        json << "      \"anchor_save_hash_algorithm\": \"" << jsonEscape(entry.anchorSaveHashAlgorithm) << "\",\n";
        json << "      \"anchor_save_content_hash\": \"" << jsonEscape(entry.anchorSaveContentHash) << "\",\n";
        json << "      \"anchor_save_byte_count\": " << entry.anchorSaveByteCount << "\n";
        json << "    }" << (i + 1 < inventory.entries.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

std::string serializeCampaignSaveInventoryDiff(const CampaignSaveInventoryDiff& diff)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"added_count\": " << diff.addedCount << ",\n";
    json << "  \"removed_count\": " << diff.removedCount << ",\n";
    json << "  \"changed_count\": " << diff.changedCount << ",\n";
    json << "  \"unchanged_count\": " << diff.unchangedCount << ",\n";
    json << "  \"changes\": [\n";
    for (std::size_t i = 0; i < diff.changes.size(); ++i) {
        const auto& change = diff.changes[i];
        json << "    {\n";
        json << "      \"change_kind\": \"" << jsonEscape(change.changeKind) << "\",\n";
        json << "      \"campaign_key\": \"" << jsonEscape(change.entry.campaignKey) << "\",\n";
        json << "      \"display_name\": \"" << jsonEscape(change.entry.displayName) << "\",\n";
        json << "      \"relative_path\": \"" << jsonEscape(change.entry.relativePath) << "\",\n";
        json << "      \"source_kind\": \"" << jsonEscape(change.entry.sourceKind) << "\",\n";
        json << "      \"anchor_save_name\": \"" << jsonEscape(change.entry.anchorSaveName) << "\",\n";
        json << "      \"anchor_save_hash_algorithm\": \"" << jsonEscape(change.entry.anchorSaveHashAlgorithm) << "\",\n";
        json << "      \"anchor_save_content_hash\": \"" << jsonEscape(change.entry.anchorSaveContentHash) << "\",\n";
        json << "      \"anchor_save_byte_count\": " << change.entry.anchorSaveByteCount << ",\n";
        json << "      \"previous_save_file_count\": " << change.previousSaveFileCount << ",\n";
        json << "      \"current_save_file_count\": " << change.currentSaveFileCount << "\n";
        json << "    }" << (i + 1 < diff.changes.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus

