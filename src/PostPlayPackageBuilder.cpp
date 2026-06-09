// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PostPlayPackageBuilder.h"

#include <cctype>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <utility>

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

void addUnique(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::string hashPrefix(const std::string& hash)
{
    return hash.substr(0, std::min<std::size_t>(12, hash.size()));
}

std::string normalizedIdentityFromEmpireName(const std::string& empireName)
{
    std::string identity;
    bool previousUnderscore = false;
    for (const char ch : empireName) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            identity.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            previousUnderscore = false;
        } else if (!previousUnderscore) {
            identity.push_back('_');
            previousUnderscore = true;
        }
    }
    while (!identity.empty() && identity.front() == '_') {
        identity.erase(identity.begin());
    }
    while (!identity.empty() && identity.back() == '_') {
        identity.pop_back();
    }
    return identity;
}

struct ResolvedCampaignIdentity {
    std::string identity;
    std::string state;
    bool ambiguous = false;
};

ResolvedCampaignIdentity resolveCampaignIdentity(
    const SaveEntryPointCampaignAnalysis& campaign,
    const std::vector<SaveEntryPoint>& entries)
{
    std::set<std::string> playerCountryIds;
    std::set<std::string> empireNames;
    for (const auto& entry : entries) {
        if (entry.campaignKey != campaign.campaignKey) {
            continue;
        }
        if (!entry.playerCountryId.empty()) {
            playerCountryIds.insert(entry.playerCountryId);
        }
        const auto normalizedEmpireName = normalizedIdentityFromEmpireName(entry.empireName);
        if (!normalizedEmpireName.empty()) {
            empireNames.insert(normalizedEmpireName);
        }
    }

    if (playerCountryIds.size() > 1) {
        return {campaign.campaignKey, "ambiguous_save_identity", true};
    }
    if (playerCountryIds.size() == 1) {
        return {*playerCountryIds.begin(), "resolved_from_player_country_id", false};
    }
    if (empireNames.size() > 1) {
        return {campaign.campaignKey, "ambiguous_save_identity", true};
    }
    if (empireNames.size() == 1) {
        return {*empireNames.begin(), "resolved_from_empire_name", false};
    }
    return {campaign.campaignKey, "folder_alias_fallback", false};
}

std::string ruleScopeFor(const SaveEntryPoint& entry)
{
    std::ostringstream scope;
    scope << entry.campaignKey << "::";
    if (!entry.saveDate.empty()) {
        scope << entry.saveDate;
    } else {
        scope << "unknown_date";
    }
    scope << "::";
    if (!entry.contentHash.empty()) {
        scope << hashPrefix(entry.contentHash);
    } else {
        scope << "missing_hash";
    }
    return scope.str();
}

PostPlayPackageEntry buildEntry(
    const SaveEntryPoint& entry,
    const bool campaignBranchAmbiguous,
    const bool campaignIdentityAmbiguous,
    const std::string& campaignIdentity,
    const std::string& campaignIdentityState)
{
    PostPlayPackageEntry packageEntry;
    packageEntry.entryPointId = entry.id;
    packageEntry.campaignKey = entry.campaignKey;
    packageEntry.campaignIdentity = campaignIdentity;
    packageEntry.campaignIdentityState = campaignIdentityState;
    packageEntry.sourceKind = entry.sourceKind;
    packageEntry.saveName = entry.saveName;
    packageEntry.saveDate = entry.saveDate;
    packageEntry.contentHash = entry.contentHash;
    packageEntry.parseStatus = entry.parseStatus;
    packageEntry.playerCountryId = entry.playerCountryId;
    packageEntry.empireName = entry.empireName;
    packageEntry.empireState = entry.empireState;
    packageEntry.analysisState = entry.analysisState;
    packageEntry.ruleScope = ruleScopeFor(entry);
    packageEntry.packageEntryId = "postplay::" + packageEntry.ruleScope;
    packageEntry.compatibleArchivedEvidenceCount = entry.compatibleArchivedEvidenceCount;
    packageEntry.laterArchivedEvidenceCount = entry.laterArchivedEvidenceCount;
    packageEntry.compatibleHistoryAvailable = entry.compatibleArchivedEvidenceCount > 0;
    packageEntry.futureEvidenceExcluded = entry.laterArchivedEvidenceCount > 0;
    packageEntry.warningCodes = entry.warningCodes;

    if (campaignIdentityAmbiguous) {
        packageEntry.decisionInputState = "blocked_campaign_identity_ambiguity";
        packageEntry.evidencePolicy = "blocked";
        packageEntry.decisionInputAllowed = false;
        addUnique(packageEntry.warningCodes, "post_play_campaign_identity_ambiguity_blocks_decision_input");
    } else if (campaignBranchAmbiguous) {
        packageEntry.decisionInputState = "blocked_branch_ambiguity";
        packageEntry.evidencePolicy = "blocked";
        packageEntry.decisionInputAllowed = false;
        addUnique(packageEntry.warningCodes, "post_play_branch_ambiguity_blocks_decision_input");
    } else if (entry.analysisState == "needs_parse" || entry.saveDate.empty()) {
        packageEntry.decisionInputState = "needs_deferred_parse";
        packageEntry.evidencePolicy = "defer_until_save_date_known";
        packageEntry.decisionInputAllowed = false;
        addUnique(packageEntry.warningCodes, "post_play_entry_point_needs_deferred_parse");
    } else if (entry.compatibleArchivedEvidenceCount == 0) {
        packageEntry.decisionInputState = "insufficient_history";
        packageEntry.evidencePolicy = "no_compatible_history_available";
        packageEntry.decisionInputAllowed = false;
        addUnique(packageEntry.warningCodes, "post_play_no_compatible_history");
    } else {
        packageEntry.decisionInputState = "ready_for_decision_input";
        packageEntry.evidencePolicy = entry.laterArchivedEvidenceCount > 0
            ? "compatible_history_only_future_excluded"
            : "compatible_history_only";
        packageEntry.decisionInputAllowed = true;
    }

    packageEntry.compatibleArchivedEvidenceSamples = entry.compatibleArchivedEvidenceSamples;
    packageEntry.laterArchivedEvidenceSamples = entry.laterArchivedEvidenceSamples;

    return packageEntry;
}

void writeStringArray(std::ostringstream& output, const std::vector<std::string>& values)
{
    output << "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << "\"" << jsonEscape(values[index]) << "\"";
    }
    output << "]";
}

void writeArchivedEvidenceReferences(
    std::ostringstream& output,
    const std::vector<ArchivedSaveEvidenceReference>& references,
    const int indentSpaces)
{
    const std::string indent(indentSpaces, ' ');
    const std::string itemIndent(indentSpaces + 2, ' ');
    const std::string fieldIndent(indentSpaces + 4, ' ');
    output << "[";
    if (!references.empty()) {
        output << "\n";
        for (std::size_t index = 0; index < references.size(); ++index) {
            const auto& reference = references[index];
            output << itemIndent << "{\n";
            output << fieldIndent << "\"archived_path\": \"" << jsonEscape(reference.archivedPath) << "\",\n";
            output << fieldIndent << "\"save_name\": \"" << jsonEscape(reference.saveName) << "\",\n";
            output << fieldIndent << "\"save_date\": \"" << jsonEscape(reference.saveDate) << "\",\n";
            output << fieldIndent << "\"content_hash\": \"" << jsonEscape(reference.contentHash) << "\",\n";
            output << fieldIndent << "\"byte_count\": " << reference.byteCount << "\n";
            output << itemIndent << "}" << (index + 1 < references.size() ? "," : "") << "\n";
        }
        output << indent;
    }
    output << "]";
}

} // namespace

PostPlayPackage PostPlayPackageBuilder::build(
    const AutosaveArchiveSummary& archiveSummary,
    const SaveEntryPointAnalysis& entryPointAnalysis) const
{
    PostPlayPackage package;
    package.sessionArchiveDirectory = archiveSummary.sessionArchiveDirectory;
    package.archiveVerified = archiveSummary.ok && entryPointAnalysis.archiveVerified;
    package.entryPointAnalysisReadiness = entryPointAnalysis.readiness;
    package.branchAmbiguityDetected = entryPointAnalysis.branchAmbiguityDetected;
    package.copiedSaveCount = archiveSummary.copiedSaveCount;
    package.totalByteCount = archiveSummary.totalByteCount;
    package.entryPointCount = entryPointAnalysis.entryPointCount;
    package.archivedEvidenceCount = entryPointAnalysis.archivedEvidenceCount;

    if (!archiveSummary.ok) {
        package.reason = archiveSummary.reason.empty() ? "archive summary not ready" : archiveSummary.reason;
        package.readiness = "blocked";
        addUnique(package.warningCodes, "post_play_archive_not_verified");
        return package;
    }
    if (!entryPointAnalysis.ok) {
        package.reason = entryPointAnalysis.reason.empty() ? "entry point analysis not ready" : entryPointAnalysis.reason;
        package.readiness = "blocked";
        addUnique(package.warningCodes, "post_play_entry_point_analysis_not_ready");
        return package;
    }
    if (entryPointAnalysis.entryPointCount == 0) {
        package.ok = true;
        package.reason = "archive verified but no loadable entry points found";
        package.readiness = "needs_attention";
        addUnique(package.warningCodes, "post_play_no_loadable_entry_points");
        return package;
    }

    std::map<std::string, PostPlayPackageCampaign> campaignPackages;
    std::map<std::string, bool> ambiguousCampaigns;
    for (const auto& campaign : entryPointAnalysis.campaigns) {
        auto& item = campaignPackages[campaign.campaignKey];
        item.campaignKey = campaign.campaignKey;
        item.entryPointCount = campaign.entryPointCount;
        item.archivedEvidenceCount = campaign.archivedEvidenceCount;
        item.branchAmbiguityDetected = campaign.branchAmbiguityDetected;
        item.warningCodes = campaign.warningCodes;
        ambiguousCampaigns[campaign.campaignKey] = campaign.branchAmbiguityDetected;
    }

    std::map<std::string, ResolvedCampaignIdentity> campaignIdentities;
    for (const auto& campaign : entryPointAnalysis.campaigns) {
        const auto resolvedIdentity = resolveCampaignIdentity(campaign, entryPointAnalysis.entryPoints);
        auto& packageCampaign = campaignPackages[campaign.campaignKey];
        packageCampaign.campaignKey = campaign.campaignKey;
        packageCampaign.campaignIdentity = resolvedIdentity.identity;
        packageCampaign.campaignIdentityState = resolvedIdentity.state;
        campaignIdentities[campaign.campaignKey] = resolvedIdentity;
    }

    bool anyReady = false;
    bool anyBlocked = false;
    bool anyFutureExcluded = false;
    bool anyCampaignIdentityAmbiguous = false;
    std::size_t resolvedCampaignIdentityCount = 0;
    std::size_t fallbackCampaignIdentityCount = 0;
    for (const auto& entry : entryPointAnalysis.entryPoints) {
        const bool campaignBranchAmbiguous = ambiguousCampaigns[entry.campaignKey];
        const auto campaignIdentityIt = campaignIdentities.find(entry.campaignKey);
        const bool campaignIdentityAmbiguous =
            campaignIdentityIt != campaignIdentities.end() && campaignIdentityIt->second.ambiguous;
        const std::string campaignIdentity =
            campaignIdentityIt != campaignIdentities.end() ? campaignIdentityIt->second.identity : entry.campaignKey;
        const std::string campaignIdentityState =
            campaignIdentityIt != campaignIdentities.end()
                ? campaignIdentityIt->second.state
                : "folder_alias_fallback";
        auto packageEntry = buildEntry(
            entry,
            campaignBranchAmbiguous,
            campaignIdentityAmbiguous,
            campaignIdentity,
            campaignIdentityState);
        anyReady = anyReady || packageEntry.decisionInputAllowed;
        anyBlocked = anyBlocked || !packageEntry.decisionInputAllowed;
        anyFutureExcluded = anyFutureExcluded || packageEntry.futureEvidenceExcluded;
        anyCampaignIdentityAmbiguous = anyCampaignIdentityAmbiguous || campaignIdentityAmbiguous;
        if (packageEntry.decisionInputAllowed) {
            ++package.decisionReadyEntryCount;
        }
        for (const auto& warning : packageEntry.warningCodes) {
            addUnique(package.warningCodes, warning);
        }

        auto& campaign = campaignPackages[packageEntry.campaignKey];
        campaign.campaignKey = packageEntry.campaignKey;
        campaign.campaignIdentity = packageEntry.campaignIdentity;
        campaign.campaignIdentityState = packageEntry.campaignIdentityState;
        if (packageEntry.campaignIdentityState == "folder_alias_fallback") {
            ++fallbackCampaignIdentityCount;
        } else if (packageEntry.campaignIdentityState == "resolved_from_player_country_id" ||
                   packageEntry.campaignIdentityState == "resolved_from_empire_name") {
            ++resolvedCampaignIdentityCount;
        }
        if (packageEntry.decisionInputAllowed) {
            ++campaign.decisionReadyEntryCount;
        }
        package.entries.push_back(std::move(packageEntry));
    }

    for (auto& [campaignKey, campaign] : campaignPackages) {
        if (campaign.campaignIdentityState == "ambiguous_save_identity") {
            campaign.readiness = "blocked_identity_ambiguity";
        } else if (campaign.branchAmbiguityDetected) {
            campaign.readiness = "blocked_ambiguous";
        } else if (campaign.entryPointCount == 0) {
            campaign.readiness = "needs_attention";
        } else if (campaign.decisionReadyEntryCount == campaign.entryPointCount) {
            campaign.readiness = "ready";
        } else if (campaign.decisionReadyEntryCount > 0) {
            campaign.readiness = "ready_partial";
        } else {
            campaign.readiness = "needs_attention";
        }
        package.campaigns.push_back(campaign);
    }

    package.ok = true;
    if (anyCampaignIdentityAmbiguous) {
        package.campaignIdentityStateSummary = "ambiguous_save_identity";
    } else if (resolvedCampaignIdentityCount > 0 && fallbackCampaignIdentityCount > 0) {
        package.campaignIdentityStateSummary = "mixed_save_identity_resolution";
    } else if (fallbackCampaignIdentityCount > 0) {
        package.campaignIdentityStateSummary = "folder_alias_fallback";
    } else {
        package.campaignIdentityStateSummary = "resolved_from_save_contents";
    }
    if (anyCampaignIdentityAmbiguous) {
        package.reason = "entry point package built but save-content identity ambiguity blocks some decision input";
        package.readiness = anyReady ? "ready_partial_ambiguous" : "ambiguous";
    } else if (entryPointAnalysis.branchAmbiguityDetected) {
        package.reason = "entry point package built but branch ambiguity blocks some decision input";
        package.readiness = anyReady ? "ready_partial_ambiguous" : "ambiguous";
    } else if (anyReady && anyBlocked) {
        package.reason = "post-play package built; some entry points need more history or parsing";
        package.readiness = "ready_partial";
    } else if (anyReady && anyFutureExcluded) {
        package.reason = "post-play package built; future evidence excluded where needed";
        package.readiness = "ready_conservative";
    } else if (anyReady) {
        package.reason = "post-play package built";
        package.readiness = "ready";
    } else {
        package.reason = "post-play package built but no entry point is ready for decision input";
        package.readiness = "needs_attention";
    }
    return package;
}

std::string serializePostPlayPackage(const PostPlayPackage& package)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (package.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(package.reason) << "\",\n";
    json << "  \"readiness\": \"" << jsonEscape(package.readiness) << "\",\n";
    json << "  \"campaign_identity_state_summary\": \"" << jsonEscape(package.campaignIdentityStateSummary) << "\",\n";
    json << "  \"dry_run_only\": " << (package.dryRunOnly ? "true" : "false") << ",\n";
    json << "  \"publishes_overlay\": " << (package.publishesOverlay ? "true" : "false") << ",\n";
    json << "  \"session_archive_directory\": \"" << jsonEscape(package.sessionArchiveDirectory.generic_string()) << "\",\n";
    json << "  \"archive_verified\": " << (package.archiveVerified ? "true" : "false") << ",\n";
    json << "  \"entry_point_analysis_readiness\": \"" << jsonEscape(package.entryPointAnalysisReadiness) << "\",\n";
    json << "  \"branch_ambiguity_detected\": " << (package.branchAmbiguityDetected ? "true" : "false") << ",\n";
    json << "  \"copied_save_count\": " << package.copiedSaveCount << ",\n";
    json << "  \"total_byte_count\": " << package.totalByteCount << ",\n";
    json << "  \"entry_point_count\": " << package.entryPointCount << ",\n";
    json << "  \"decision_ready_entry_count\": " << package.decisionReadyEntryCount << ",\n";
    json << "  \"archived_evidence_count\": " << package.archivedEvidenceCount << ",\n";
    json << "  \"warning_codes\": ";
    writeStringArray(json, package.warningCodes);
    json << ",\n";
    json << "  \"campaigns\": [\n";
    for (std::size_t index = 0; index < package.campaigns.size(); ++index) {
        const auto& campaign = package.campaigns[index];
        json << "    {\n";
        json << "      \"campaign_key\": \"" << jsonEscape(campaign.campaignKey) << "\",\n";
        json << "      \"campaign_identity\": \"" << jsonEscape(campaign.campaignIdentity) << "\",\n";
        json << "      \"campaign_identity_state\": \"" << jsonEscape(campaign.campaignIdentityState) << "\",\n";
        json << "      \"readiness\": \"" << jsonEscape(campaign.readiness) << "\",\n";
        json << "      \"branch_ambiguity_detected\": " << (campaign.branchAmbiguityDetected ? "true" : "false") << ",\n";
        json << "      \"entry_point_count\": " << campaign.entryPointCount << ",\n";
        json << "      \"decision_ready_entry_count\": " << campaign.decisionReadyEntryCount << ",\n";
        json << "      \"archived_evidence_count\": " << campaign.archivedEvidenceCount << ",\n";
        json << "      \"warning_codes\": ";
        writeStringArray(json, campaign.warningCodes);
        json << "\n";
        json << "    }" << (index + 1 < package.campaigns.size() ? "," : "") << "\n";
    }
    json << "  ],\n";
    json << "  \"entries\": [\n";
    for (std::size_t index = 0; index < package.entries.size(); ++index) {
        const auto& entry = package.entries[index];
        json << "    {\n";
        json << "      \"package_entry_id\": \"" << jsonEscape(entry.packageEntryId) << "\",\n";
        json << "      \"entry_point_id\": \"" << jsonEscape(entry.entryPointId) << "\",\n";
        json << "      \"campaign_key\": \"" << jsonEscape(entry.campaignKey) << "\",\n";
        json << "      \"campaign_identity\": \"" << jsonEscape(entry.campaignIdentity) << "\",\n";
        json << "      \"campaign_identity_state\": \"" << jsonEscape(entry.campaignIdentityState) << "\",\n";
        json << "      \"source_kind\": \"" << jsonEscape(entry.sourceKind) << "\",\n";
        json << "      \"save_name\": \"" << jsonEscape(entry.saveName) << "\",\n";
        json << "      \"save_date\": \"" << jsonEscape(entry.saveDate) << "\",\n";
        json << "      \"content_hash\": \"" << jsonEscape(entry.contentHash) << "\",\n";
        json << "      \"parse_status\": \"" << jsonEscape(entry.parseStatus) << "\",\n";
        json << "      \"player_country_id\": \"" << jsonEscape(entry.playerCountryId) << "\",\n";
        json << "      \"empire_name\": \"" << jsonEscape(entry.empireName) << "\",\n";
        json << "      \"empire_state_parsed\": " << (entry.empireState.parsed ? "true" : "false") << ",\n";
        json << "      \"empire_state_parse_status\": \"" << jsonEscape(entry.empireState.parseStatus) << "\",\n";
        json << "      \"empire_state_parser_reason\": \"" << jsonEscape(entry.empireState.parserReason) << "\",\n";
        json << "      \"empire_state_government\": \"" << jsonEscape(entry.empireState.government) << "\",\n";
        json << "      \"empire_state_authority\": \"" << jsonEscape(entry.empireState.authority) << "\",\n";
        json << "      \"empire_state_founder_species_name\": \"" << jsonEscape(entry.empireState.founderSpeciesName) << "\",\n";
        json << "      \"empire_state_capital_planet_name\": \"" << jsonEscape(entry.empireState.capitalPlanetName) << "\",\n";
        json << "      \"empire_state_home_system_name\": \"" << jsonEscape(entry.empireState.homeSystemName) << "\",\n";
        json << "      \"empire_state_owned_fleet_count\": " << entry.empireState.ownedFleetCount << ",\n";
        json << "      \"empire_state_active_war_count\": " << entry.empireState.activeWarCount << ",\n";
        json << "      \"empire_state_missing_fields\": ";
        writeStringArray(json, entry.empireState.missingFields);
        json << ",\n";
        json << "      \"empire_state_uncertainties\": ";
        writeStringArray(json, entry.empireState.uncertainties);
        json << ",\n";
        json << "      \"analysis_state\": \"" << jsonEscape(entry.analysisState) << "\",\n";
        json << "      \"decision_input_state\": \"" << jsonEscape(entry.decisionInputState) << "\",\n";
        json << "      \"rule_scope\": \"" << jsonEscape(entry.ruleScope) << "\",\n";
        json << "      \"evidence_policy\": \"" << jsonEscape(entry.evidencePolicy) << "\",\n";
        json << "      \"decision_input_allowed\": " << (entry.decisionInputAllowed ? "true" : "false") << ",\n";
        json << "      \"compatible_history_available\": " << (entry.compatibleHistoryAvailable ? "true" : "false") << ",\n";
        json << "      \"future_evidence_excluded\": " << (entry.futureEvidenceExcluded ? "true" : "false") << ",\n";
        json << "      \"compatible_archived_evidence_count\": " << entry.compatibleArchivedEvidenceCount << ",\n";
        json << "      \"later_archived_evidence_count\": " << entry.laterArchivedEvidenceCount << ",\n";
        json << "      \"compatible_archived_evidence_samples\": ";
        writeArchivedEvidenceReferences(json, entry.compatibleArchivedEvidenceSamples, 6);
        json << ",\n";
        json << "      \"later_archived_evidence_samples\": ";
        writeArchivedEvidenceReferences(json, entry.laterArchivedEvidenceSamples, 6);
        json << ",\n";
        json << "      \"warning_codes\": ";
        writeStringArray(json, entry.warningCodes);
        json << "\n";
        json << "    }" << (index + 1 < package.entries.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
