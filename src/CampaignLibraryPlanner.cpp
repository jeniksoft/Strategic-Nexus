// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "CampaignLibraryPlanner.h"

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

} // namespace

CampaignLibraryPlan CampaignLibraryPlanner::build(
    const CampaignSaveInventory& inventory,
    const std::size_t maxIncludedCampaigns) const
{
    CampaignLibraryPlan plan;
    plan.saveRootAvailable = inventory.rootExists;
    plan.maxIncludedCampaigns = maxIncludedCampaigns;

    for (const auto& campaign : inventory.entries) {
        CampaignLibraryPlanEntry entry;
        entry.campaignKey = campaign.campaignKey;
        entry.displayName = campaign.displayName;
        entry.relativePath = campaign.relativePath;
        entry.anchorSaveContentHash = campaign.anchorSaveContentHash;

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
    json << "  \"campaigns\": [\n";
    for (std::size_t i = 0; i < plan.entries.size(); ++i) {
        const auto& entry = plan.entries[i];
        json << "    {\n";
        json << "      \"campaign_key\": \"" << jsonEscape(entry.campaignKey) << "\",\n";
        json << "      \"display_name\": \"" << jsonEscape(entry.displayName) << "\",\n";
        json << "      \"relative_path\": \"" << jsonEscape(entry.relativePath) << "\",\n";
        json << "      \"anchor_save_content_hash\": \"" << jsonEscape(entry.anchorSaveContentHash) << "\",\n";
        json << "      \"status\": \"" << jsonEscape(entry.status) << "\",\n";
        json << "      \"reason\": \"" << jsonEscape(entry.reason) << "\"\n";
        json << "    }" << (i + 1 < plan.entries.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus

