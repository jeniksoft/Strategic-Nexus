// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "CampaignLibraryPlanner.h"

#include "common/FileUtil.h"
#include "common/JsonExtract.h"

#include <algorithm>
#include <set>
#include <optional>
#include <regex>
#include <sstream>

namespace strategic_nexus {
namespace {

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

std::vector<std::string> normalizePinnedKeys(const std::vector<std::string>& pinnedCampaignKeys)
{
    std::set<std::string> uniqueKeys;
    for (const auto& key : pinnedCampaignKeys) {
        if (!key.empty()) {
            uniqueKeys.insert(key);
        }
    }

    return {uniqueKeys.begin(), uniqueKeys.end()};
}

std::optional<std::size_t> extractJsonSizeValue(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(\\d+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }

    try {
        return static_cast<std::size_t>(std::stoull(match[1].str()));
    }
    catch (...) {
        return std::nullopt;
    }
}

CampaignLibraryPlanEntry makeSyntheticPinnedEntry(const std::string& campaignKey)
{
    CampaignLibraryPlanEntry entry;
    entry.campaignKey = campaignKey;
    entry.displayName = campaignKey;
    entry.relativePath = "";
    entry.anchorSaveContentHash = "";
    entry.pinned = true;
    entry.locallyPlayable = false;
    entry.status = "included_pinned_missing_local_save";
    entry.reason = "pinned_campaign_kept_available_without_local_save";
    return entry;
}

} // namespace

CampaignLibraryPlan CampaignLibraryPlanner::build(
    const CampaignSaveInventory& inventory,
    const std::size_t maxIncludedCampaigns) const
{
    return build(inventory, maxIncludedCampaigns, CampaignLibraryPinSet{});
}

CampaignLibraryPlan CampaignLibraryPlanner::build(
    const CampaignSaveInventory& inventory,
    const std::size_t maxIncludedCampaigns,
    const CampaignLibraryPinSet& pinSet) const
{
    CampaignLibraryPlan plan;
    plan.saveRootAvailable = inventory.rootExists;
    plan.maxIncludedCampaigns = maxIncludedCampaigns;

    const auto pinnedKeys = normalizePinnedKeys(pinSet.pinnedCampaignKeys);
    std::set<std::string> consumedCampaignKeys;

    const auto includePinnedKey = [&](const std::string& campaignKey) {
        CampaignLibraryPlanEntry entry = makeSyntheticPinnedEntry(campaignKey);

        const auto matched = std::find_if(
            inventory.entries.begin(),
            inventory.entries.end(),
            [&](const CampaignSaveInventoryEntry& candidate) {
                return candidate.campaignKey == campaignKey &&
                       consumedCampaignKeys.find(candidate.campaignKey) == consumedCampaignKeys.end();
            });

        if (matched != inventory.entries.end()) {
            consumedCampaignKeys.insert(matched->campaignKey);
            entry.displayName = matched->displayName;
            entry.relativePath = matched->relativePath;
            entry.anchorSaveContentHash = matched->anchorSaveContentHash;
            entry.locallyPlayable = true;
            entry.status = "included_pinned";
            entry.reason = "pinned_campaign_kept_available";
        } else {
            ++plan.pinnedMissingLocalSaveCount;
        }

        if (plan.includedCount >= maxIncludedCampaigns) {
            entry.status = "skipped";
            entry.reason = "active_library_limit";
            entry.pinned = true;
            entry.locallyPlayable = false;
            plan.limitReached = true;
            ++plan.skippedCount;
            ++plan.skippedDueToLimitCount;
            plan.entries.push_back(entry);
            return;
        }

        ++plan.includedCount;
        ++plan.pinnedCount;
        plan.entries.push_back(entry);
    };

    for (const auto& pinnedKey : pinnedKeys) {
        includePinnedKey(pinnedKey);
    }

    for (const auto& campaign : inventory.entries) {
        if (consumedCampaignKeys.find(campaign.campaignKey) != consumedCampaignKeys.end()) {
            continue;
        }

        CampaignLibraryPlanEntry entry;
        entry.campaignKey = campaign.campaignKey;
        entry.displayName = campaign.displayName;
        entry.relativePath = campaign.relativePath;
        entry.anchorSaveContentHash = campaign.anchorSaveContentHash;
        entry.locallyPlayable = !campaign.anchorSaveContentHash.empty();

        if (campaign.anchorSaveContentHash.empty()) {
            entry.status = "skipped";
            entry.reason = "missing_anchor_fingerprint";
            ++plan.skippedCount;
        } else if (plan.includedCount >= maxIncludedCampaigns) {
            entry.status = "skipped";
            entry.reason = "active_library_limit";
            plan.limitReached = true;
            ++plan.skippedCount;
            ++plan.skippedDueToLimitCount;
        } else {
            entry.status = "included";
            entry.reason = "local_save_available";
            ++plan.includedCount;
        }

        plan.entries.push_back(entry);
    }

    return plan;
}

CampaignLibraryPinSet loadCampaignLibraryPins(const std::filesystem::path& path)
{
    CampaignLibraryPinSet pins;
    pins.path = path;
    if (path.empty()) {
        return pins;
    }

    std::string json;
    if (!common::tryReadTextFile(path, json)) {
        return pins;
    }

    pins.present = true;
    const auto schemaVersion = extractJsonSizeValue(json, "schema_version").value_or(1);
    if (schemaVersion != 1) {
        pins.schemaSupported = false;
        pins.state = "needs_attention";
        pins.reason = "pinned campaign exception manifest schema unsupported";
        pins.nextStep =
            "Recreate the pin manifest with Strategic Nexus.exe --toggle-campaign-library-pin and a schema_version 1 manifest.";
        return pins;
    }

    pins.schemaSupported = true;
    pins.pinnedCampaignKeys = normalizePinnedKeys(common::extractJsonStringArray(json, "pinned_campaign_keys"));
    pins.pinnedCount = pins.pinnedCampaignKeys.size();
    if (pins.pinnedCampaignKeys.empty()) {
        pins.state = "ready";
        pins.reason = "pinned campaign exceptions configured but no campaigns are currently pinned";
        pins.nextStep =
            "Use Strategic Nexus.exe --toggle-campaign-library-pin <manifest> <campaign_key> pin to keep a campaign available when the local save root disappears.";
    } else {
        pins.state = "ready";
        pins.reason = "pinned campaign exceptions available for " + std::to_string(pins.pinnedCount) + " campaign(s)";
        pins.nextStep =
            "Use Strategic Nexus.exe --toggle-campaign-library-pin <manifest> <campaign_key> unpin to remove an exception or pin to keep one available.";
    }
    return pins;
}

bool writeCampaignLibraryPins(
    const std::filesystem::path& path,
    const std::vector<std::string>& pinnedCampaignKeys)
{
    if (path.empty()) {
        return false;
    }

    const auto normalizedKeys = normalizePinnedKeys(pinnedCampaignKeys);
    std::ostringstream output;
    output << "{\n";
    output << "  \"schema_version\": 1,\n";
    output << "  \"pinned_campaign_keys\": [\n";
    for (std::size_t index = 0; index < normalizedKeys.size(); ++index) {
        output << "    \"" << jsonEscape(normalizedKeys[index]) << "\""
               << (index + 1 < normalizedKeys.size() ? "," : "") << "\n";
    }
    output << "  ]\n";
    output << "}\n";
    return common::writeTextFileAtomically(path, output.str());
}

std::string serializeCampaignLibraryPlan(const CampaignLibraryPlan& plan)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"save_root_available\": " << (plan.saveRootAvailable ? "true" : "false") << ",\n";
    json << "  \"limit_reached\": " << (plan.limitReached ? "true" : "false") << ",\n";
    json << "  \"max_included_campaigns\": " << plan.maxIncludedCampaigns << ",\n";
    json << "  \"included_count\": " << plan.includedCount << ",\n";
    json << "  \"skipped_count\": " << plan.skippedCount << ",\n";
    json << "  \"skipped_due_to_limit_count\": " << plan.skippedDueToLimitCount << ",\n";
    json << "  \"pinned_count\": " << plan.pinnedCount << ",\n";
    json << "  \"pinned_missing_local_save_count\": " << plan.pinnedMissingLocalSaveCount << ",\n";
    json << "  \"campaigns\": [\n";
    for (std::size_t i = 0; i < plan.entries.size(); ++i) {
        const auto& entry = plan.entries[i];
        json << "    {\n";
        json << "      \"campaign_key\": \"" << jsonEscape(entry.campaignKey) << "\",\n";
        json << "      \"display_name\": \"" << jsonEscape(entry.displayName) << "\",\n";
        json << "      \"relative_path\": \"" << jsonEscape(entry.relativePath) << "\",\n";
        json << "      \"anchor_save_content_hash\": \"" << jsonEscape(entry.anchorSaveContentHash) << "\",\n";
        json << "      \"pinned\": " << (entry.pinned ? "true" : "false") << ",\n";
        json << "      \"locally_playable\": " << (entry.locallyPlayable ? "true" : "false") << ",\n";
        json << "      \"status\": \"" << jsonEscape(entry.status) << "\",\n";
        json << "      \"reason\": \"" << jsonEscape(entry.reason) << "\"\n";
        json << "    }" << (i + 1 < plan.entries.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus

