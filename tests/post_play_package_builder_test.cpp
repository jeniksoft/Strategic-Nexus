// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PostPlayPackageBuilder.h"

#include "AutosaveArchiveSummarizer.h"
#include "AutosaveArchiver.h"
#include "SaveEntryPointAnalyzer.h"

#include <algorithm>
#include <cstdlib>
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

void writeFile(const std::filesystem::path& path, const std::string& content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
}

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/post_play_package_builder_fixture");
    const auto captureRoot = root / "capture_root";
    const auto currentRoot = root / "current_root";
    const auto archiveRoot = root / "archive";
    const std::string sessionId = "session_post_play_fixture";
    std::filesystem::remove_all(root);

    const strategic_nexus::AutosaveArchiver archiver;

    writeFile(captureRoot / "Alpha Campaign" / "autosave_2200.01.01.sav", "alpha month one");
    auto archiveResult = archiver.archiveLiveSaves(captureRoot, archiveRoot, sessionId, 0);
    requireCondition(archiveResult.copiedCount == 1, "first alpha autosave should be archived");

    writeFile(captureRoot / "Alpha Campaign" / "autosave_2200.02.01.sav", "alpha month two");
    archiveResult = archiver.archiveLiveSaves(captureRoot, archiveRoot, sessionId, 0);
    requireCondition(archiveResult.copiedCount == 1, "second alpha autosave should be archived");

    writeFile(captureRoot / "Alpha Campaign" / "autosave_2200.03.01.sav", "alpha month three");
    archiveResult = archiver.archiveLiveSaves(captureRoot, archiveRoot, sessionId, 0);
    requireCondition(archiveResult.copiedCount == 1, "third alpha autosave should be archived");

    writeFile(currentRoot / "Alpha Campaign" / "autosave_2200.02.01.sav", "alpha month two");
    writeFile(currentRoot / "Alpha Campaign" / "autosave_2200.03.01.sav", "alpha month three");
    writeFile(currentRoot / "Beta Campaign" / "autosave_2200.01.01.sav", "beta no matching history");

    const strategic_nexus::AutosaveArchiveSummarizer summarizer;
    const auto summary = summarizer.summarize(archiveRoot / sessionId);
    requireCondition(summary.ok, "fixture archive summary should be verified");

    const strategic_nexus::SaveEntryPointAnalyzer analyzer;
    const auto analysis = analyzer.analyze(archiveRoot / sessionId, {currentRoot});
    requireCondition(analysis.ok, "entry point analysis should accept fixture archive");
    requireCondition(analysis.entryPointCount == 3, "fixture should expose three entry points");

    const strategic_nexus::PostPlayPackageBuilder builder;
    const auto package = builder.build(summary, analysis);
    requireCondition(package.ok, "post-play package should build for verified inputs");
    requireCondition(package.dryRunOnly, "post-play package should be dry-run only");
    requireCondition(!package.publishesOverlay, "post-play package must not publish an overlay");
    requireCondition(package.readiness == "ready_partial", "mixed ready and blocked entries should be ready_partial");
    requireCondition(package.entryPointCount == 3, "post-play package should preserve entry point count");
    requireCondition(package.decisionReadyEntryCount == 2, "two alpha entries should be decision-ready");

    bool foundFutureExcludedEntry = false;
    bool foundInsufficientHistoryEntry = false;
    for (const auto& entry : package.entries) {
        if (entry.futureEvidenceExcluded &&
            entry.evidencePolicy == "compatible_history_only_future_excluded" &&
            entry.decisionInputAllowed) {
            foundFutureExcludedEntry = true;
            requireCondition(
                !entry.compatibleArchivedEvidenceSamples.empty(),
                "future-excluded entry should keep bounded compatible evidence samples");
            requireCondition(
                !entry.laterArchivedEvidenceSamples.empty(),
                "future-excluded entry should keep bounded later evidence samples");
        }
        if (entry.campaignKey == "beta_campaign" &&
            entry.decisionInputState == "insufficient_history" &&
            !entry.decisionInputAllowed) {
            foundInsufficientHistoryEntry = true;
        }
    }
    requireCondition(foundFutureExcludedEntry, "older alpha entry should exclude later evidence but remain usable");
    requireCondition(foundInsufficientHistoryEntry, "beta entry should be blocked without compatible history");

    const auto json = strategic_nexus::serializePostPlayPackage(package);
    requireCondition(
        package.campaignIdentityStateSummary == "folder_alias_fallback",
        "default fixture should fall back to folder alias campaign identity");
    requireCondition(json.find("\"dry_run_only\": true") != std::string::npos, "JSON should expose dry-run mode");
    requireCondition(json.find("\"publishes_overlay\": false") != std::string::npos, "JSON should expose no overlay publishing");
    requireCondition(json.find("\"decision_ready_entry_count\": 2") != std::string::npos, "JSON should expose ready count");
    requireCondition(
        json.find("\"entry_point_analysis_source_schema_version\": 1") != std::string::npos,
        "JSON should expose entry point analysis source schema version");
    requireCondition(
        json.find("\"entry_point_analysis_schema_compatibility_state\": \"current\"") != std::string::npos,
        "JSON should expose entry point analysis compatibility state");
    requireCondition(
        json.find("\"entry_point_analysis_schema_compatibility_note\": \"\"") != std::string::npos,
        "JSON should expose empty entry point analysis compatibility note");
    requireCondition(
        json.find("\"campaign_identity_state_summary\": \"folder_alias_fallback\"") != std::string::npos,
        "JSON should expose campaign identity state summary");
    requireCondition(
        json.find("\"campaign_identity_state\": \"folder_alias_fallback\"") != std::string::npos,
        "JSON should expose per-campaign identity fallback state");
    requireCondition(
        json.find("\"personality_profile\":") != std::string::npos,
        "JSON should expose personality profile provenance");
    requireCondition(
        json.find("\"applied\": false") != std::string::npos,
        "JSON should expose summary-only personality profile provenance");
    requireCondition(
        json.find("\"source_schema_version\": 0") != std::string::npos,
        "JSON should expose summary-only personality profile source schema version");
    requireCondition(
        json.find("\"schema_compatibility_state\": \"not_loaded\"") != std::string::npos,
        "JSON should expose summary-only personality profile compatibility state");
    requireCondition(
        json.find("\"schema_compatibility_note\": \"no validated personality profile loaded\"") !=
            std::string::npos,
        "JSON should expose summary-only personality profile compatibility note");
    requireCondition(
        json.find("\"validated_update_summary\": \"summary_only_post_play_package_contract\"") !=
            std::string::npos,
        "JSON should expose summary-only personality profile validated update summary");
    requireCondition(
        json.find(
            "\"prompt_output_note\": \"summary-only prompt-output context; no validated personality profile loaded\"") !=
            std::string::npos,
        "JSON should expose summary-only personality profile prompt output note");
    requireCondition(
        json.find("\"source_save_date\": \"\"") != std::string::npos,
        "JSON should expose empty personality profile source save date");
    requireCondition(
        json.find("\"zero_history_bootstrap\": false") != std::string::npos,
        "JSON should expose summary-only personality profile zero-history flag");
    requireCondition(json.find("\"post_play_no_compatible_history\"") != std::string::npos, "JSON should expose history warning");
    requireCondition(json.find("\"compatible_history_only_future_excluded\"") != std::string::npos, "JSON should expose future evidence policy");
    requireCondition(
        json.find("\"compatible_archived_evidence_samples\":") != std::string::npos,
        "JSON should expose bounded compatible evidence samples");
    requireCondition(
        json.find("\"later_archived_evidence_samples\":") != std::string::npos,
        "JSON should expose bounded later evidence samples");

    auto resolvedIdentityAnalysis = analysis;
    for (auto& entry : resolvedIdentityAnalysis.entryPoints) {
        if (entry.campaignKey == "alpha_campaign") {
            entry.playerCountryId = "country_alpha";
        }
    }
    const auto resolvedIdentityPackage = builder.build(summary, resolvedIdentityAnalysis);
    requireCondition(
        resolvedIdentityPackage.campaignIdentityStateSummary == "mixed_save_identity_resolution",
        "package should summarize mixed save-content and folder-alias identity resolution");
    for (const auto& entry : resolvedIdentityPackage.entries) {
        if (entry.campaignKey == "alpha_campaign") {
            requireCondition(
                entry.campaignIdentity == "country_alpha",
                "alpha campaign should resolve campaign identity from save contents");
            requireCondition(
                entry.campaignIdentityState == "resolved_from_player_country_id",
                "alpha campaign should expose save-content identity state");
        } else if (entry.campaignKey == "beta_campaign") {
            requireCondition(
                entry.campaignIdentity == "beta_campaign",
                "beta campaign should fall back to the folder alias");
            requireCondition(
                entry.campaignIdentityState == "folder_alias_fallback",
                "beta campaign should expose folder alias fallback state");
        }
    }

    auto ambiguousIdentityAnalysis = analysis;
    bool assignedFirstAlphaIdentity = false;
    for (auto& entry : ambiguousIdentityAnalysis.entryPoints) {
        if (entry.campaignKey != "alpha_campaign") {
            continue;
        }
        entry.playerCountryId = assignedFirstAlphaIdentity ? "country_beta" : "country_alpha";
        assignedFirstAlphaIdentity = true;
    }
    const auto ambiguousIdentityPackage = builder.build(summary, ambiguousIdentityAnalysis);
    requireCondition(
        ambiguousIdentityPackage.campaignIdentityStateSummary == "ambiguous_save_identity",
        "package should expose ambiguous campaign identity resolution");
    requireCondition(
        ambiguousIdentityPackage.readiness == "ambiguous",
        "ambiguous campaign identity should block package readiness");
    requireCondition(
        ambiguousIdentityPackage.decisionReadyEntryCount == 0,
        "ambiguous campaign identity should block all decision-ready entries");
    requireCondition(
        ambiguousIdentityPackage.reason.find("save-content identity ambiguity") != std::string::npos,
        "package reason should explain campaign identity ambiguity");
    bool foundBlockedIdentityCampaign = false;
    for (const auto& campaign : ambiguousIdentityPackage.campaigns) {
        if (campaign.campaignKey == "alpha_campaign") {
            foundBlockedIdentityCampaign = true;
            requireCondition(
                campaign.readiness == "blocked_identity_ambiguity",
                "alpha campaign should be blocked by identity ambiguity");
        }
    }
    requireCondition(foundBlockedIdentityCampaign, "ambiguous package should include alpha campaign summary");
    requireCondition(
        std::find(
            ambiguousIdentityPackage.warningCodes.begin(),
            ambiguousIdentityPackage.warningCodes.end(),
            "post_play_campaign_identity_ambiguity_blocks_decision_input") !=
            ambiguousIdentityPackage.warningCodes.end(),
        "package warnings should expose the identity ambiguity blocker");

    auto legacyAnalysis = analysis;
    legacyAnalysis.sourceSchemaVersion = 0;
    legacyAnalysis.schemaCompatibilityState = "partial_compatibility";
    legacyAnalysis.schemaCompatibilityNote =
        "migrated legacy save entry point analysis schema_version 0 to current schema_version 1";
    const auto legacyPackage = builder.build(summary, legacyAnalysis);
    const auto legacyJson = strategic_nexus::serializePostPlayPackage(legacyPackage);
    requireCondition(
        legacyJson.find("\"entry_point_analysis_source_schema_version\": 0") != std::string::npos,
        "legacy package JSON should expose entry point analysis source schema version 0");
    requireCondition(
        legacyJson.find("\"entry_point_analysis_schema_compatibility_state\": \"partial_compatibility\"") !=
            std::string::npos,
        "legacy package JSON should expose partial compatibility state");
    requireCondition(
        legacyJson.find(
            "\"entry_point_analysis_schema_compatibility_note\": \"migrated legacy save entry point analysis schema_version 0 to current schema_version 1\"") !=
            std::string::npos,
        "legacy package JSON should expose entry point analysis compatibility note");

    const auto ambiguousRoot = root / "ambiguous";
    const auto ambiguousCaptureRoot = ambiguousRoot / "capture_root";
    const auto ambiguousCurrentRoot = ambiguousRoot / "current_root";
    const auto ambiguousArchiveRoot = ambiguousRoot / "archive";
    const std::string ambiguousSessionId = "session_post_play_ambiguous_fixture";

    writeFile(ambiguousCaptureRoot / "Gamma Campaign" / "autosave_2200.01.01.sav", "gamma branch one");
    archiveResult = archiver.archiveLiveSaves(
        ambiguousCaptureRoot,
        ambiguousArchiveRoot,
        ambiguousSessionId,
        0);
    requireCondition(archiveResult.copiedCount == 1, "first gamma branch autosave should be archived");

    writeFile(ambiguousCaptureRoot / "Gamma Campaign" / "autosave_2200.01.01.sav", "gamma branch two");
    archiveResult = archiver.archiveLiveSaves(
        ambiguousCaptureRoot,
        ambiguousArchiveRoot,
        ambiguousSessionId,
        0);
    requireCondition(archiveResult.copiedCount == 1, "second gamma branch autosave should be archived");

    writeFile(ambiguousCurrentRoot / "Gamma Campaign" / "autosave_2200.01.01.sav", "gamma branch two");
    writeFile(ambiguousCurrentRoot / "Gamma Campaign" / "2200.02.01.sav", "gamma manual entry point");

    const auto ambiguousSummary = summarizer.summarize(ambiguousArchiveRoot / ambiguousSessionId);
    const auto ambiguousAnalysis = analyzer.analyze(ambiguousArchiveRoot / ambiguousSessionId, {ambiguousCurrentRoot});
    const auto ambiguousPackage = builder.build(ambiguousSummary, ambiguousAnalysis);

    requireCondition(ambiguousPackage.ok, "ambiguous post-play package should still serialize as evidence");
    requireCondition(ambiguousPackage.branchAmbiguityDetected, "ambiguous package should expose branch ambiguity");
    requireCondition(
        ambiguousPackage.readiness == "ambiguous",
        "campaign-level branch ambiguity should block all decision inputs");
    requireCondition(
        ambiguousPackage.decisionReadyEntryCount == 0,
        "ambiguous campaign should not produce decision-ready entries");

    for (const auto& entry : ambiguousPackage.entries) {
        requireCondition(!entry.decisionInputAllowed, "ambiguous campaign entry should not allow decision input");
        requireCondition(
            entry.decisionInputState == "blocked_branch_ambiguity",
            "ambiguous campaign entry should be blocked by branch ambiguity");
    }

    const auto ambiguousJson = strategic_nexus::serializePostPlayPackage(ambiguousPackage);
    requireCondition(
        ambiguousJson.find("\"post_play_branch_ambiguity_blocks_decision_input\"") != std::string::npos,
        "ambiguous JSON should expose decision-input blocker warning");

    std::cout << "post-play package builder tests passed.\n";
    return 0;
}
