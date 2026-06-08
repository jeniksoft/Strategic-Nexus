// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicNexusCompanion.h"
#include "LocalLlmModelManager.h"
#include "LocalLlmRuntimeAdapter.h"
#include "SncFriendPackage.h"

#include "common/FileUtil.h"
#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/MpOverlayPackage.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using strategic_nexus::common::readTextFile;
using strategic_nexus::common::writeTextFileAtomically;

namespace {

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

std::size_t countSavFiles(const std::filesystem::path& path)
{
    std::size_t count = 0;
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return 0;
    }
    for (const auto& entry : std::filesystem::directory_iterator(path, error)) {
        if (error) {
            break;
        }
        if (entry.is_regular_file(error) && entry.path().extension() == ".sav") {
            ++count;
        }
    }
    return count;
}

strategic_nexus::SncFriendPackageIdentity makeFriendIdentity(
    const std::string& nodeId,
    const std::string& displayName,
    const std::string& fingerprint)
{
    strategic_nexus::SncFriendPackageIdentity identity;
    identity.nodeId = nodeId;
    identity.displayName = displayName;
    identity.signingPublicKey = "ed25519:" + nodeId + "-signing-public-key";
    identity.encryptionPublicKey = "x25519:" + nodeId + "-encryption-public-key";
    identity.fingerprint = fingerprint;
    identity.capabilities = { "mp_package_sync", "handoff_sync" };
    return identity;
}

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/strategic_nexus_companion_fixture");
    const auto archiveRoot = root / "archive";
    const auto overlayRoot = root / "overlay";
    const auto overlayEmptyRoot = root / "overlay_empty";
    const auto overlayNoManifestNonEmptyRoot = root / "overlay_no_manifest_non_empty";
    const auto mpPackageRoot = root / "mp_overlay_package";
    const auto mpPackageCompleteRoot = root / "mp_overlay_package_complete";
    const auto entryPointAnalysisPath = root / "snc_entry_point_analysis.json";
    const auto memoryRecoveryEntryPointAnalysisPath = root / "snc_entry_point_analysis_memory_recovery.json";
    const auto postPlayPackagePath = root / "snc_post_play_package.json";
    const auto decisionInputPackagePath = root / "snc_decision_input_package.json";
    const auto candidateDecisionPackagePath = root / "snc_candidate_decision_package.json";
    const auto dslDraftPath = root / "snc_validated_dsl_draft.dsl";
    const auto dslDraftAuditPath = root / "snc_dsl_draft_package.json";
    const auto generatedOverlayStagingStatusPath = root / "snc_generated_overlay_staging_status.json";
    const auto mpPackageZipPath = root / "mp_overlay_package.zip";
    const auto missingGameplayAcceptanceReport = root / "missing_gameplay_acceptance_v0.json";
    const auto readyGameplayAcceptanceReport = root / "generated_overlay_gameplay_acceptance_v0.json";
    const auto crashRecoveryStatePath = root / "snc_crash_recovery_state.json";
    const auto friendTrustStorePath = root / "snc_friend_trust_store.json";
    const auto brokenFriendTrustStorePath = root / "snc_friend_trust_store_broken.json";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(archiveRoot);
    std::filesystem::create_directories(overlayRoot);
    std::filesystem::create_directories(overlayEmptyRoot);
    std::filesystem::create_directories(overlayNoManifestNonEmptyRoot);

    const auto userProfileRoot = root / "user_profile";
    std::filesystem::create_directories(
        userProfileRoot / "Documents" / "Paradox Interactive" / "Stellaris" / "save games");
    _putenv_s("USERPROFILE", userProfileRoot.string().c_str());
    _putenv_s("OneDrive", (root / "one_drive").string().c_str());
    {
        std::ofstream out(overlayNoManifestNonEmptyRoot / "unexpected.txt", std::ios::binary);
        out << "unexpected\n";
    }

    {
        const std::string eventsText = "event test\n";
        const std::string effectsText = "effects test\n";
        const std::string triggersText = "triggers test\n";
        std::filesystem::create_directories(overlayRoot / "events");
        std::filesystem::create_directories(overlayRoot / "common" / "scripted_effects");
        std::filesystem::create_directories(overlayRoot / "common" / "scripted_triggers");

        {
            std::ofstream events(
                overlayRoot / "events" / "strategic_nexus_generated_events.txt",
                std::ios::binary | std::ios::trunc);
            events << eventsText;
        }
        {
            std::ofstream effects(
                overlayRoot / "common" / "scripted_effects" / "strategic_nexus_generated_effects.txt",
                std::ios::binary | std::ios::trunc);
            effects << effectsText;
        }
        {
            std::ofstream triggers(
                overlayRoot / "common" / "scripted_triggers" / "strategic_nexus_generated_triggers.txt",
                std::ios::binary | std::ios::trunc);
            triggers << triggersText;
        }

        const std::string eventsHash = strategic_nexus::generated_overlay::fnv1a64Hex(eventsText);
        const std::string effectsHash = strategic_nexus::generated_overlay::fnv1a64Hex(effectsText);
        const std::string triggersHash = strategic_nexus::generated_overlay::fnv1a64Hex(triggersText);

        std::ofstream manifest(overlayRoot / "strategic_nexus_generated_manifest.json", std::ios::trunc);
        manifest
            << "{\n"
            << "  \"schema_version\": 1,\n"
            << "  \"reactive_policy_pack_capability\": \"event_family_dispatch\",\n"
            << "  \"event_families\": [\"monthly_strategy_tick\"],\n"
            << "  \"source_qualities\": [\"history_backed\", \"zero_history_bootstrap\"],\n"
            << "  \"campaign_rule_sources\": [\n"
            << "    {\n"
            << "      \"campaign_id\": \"campaign_mp_001\",\n"
            << "      \"source_quality\": \"history_backed\",\n"
            << "      \"rule_count\": 1\n"
            << "    },\n"
            << "    {\n"
            << "      \"campaign_id\": \"campaign_bootstrap\",\n"
            << "      \"source_quality\": \"zero_history_bootstrap\",\n"
            << "      \"rule_count\": 1,\n"
            << "      \"bootstrap_rotation_seed_id\": \"bootstrap_seed_fixture\",\n"
            << "      \"bootstrap_rotation_epoch\": 4\n"
            << "    }\n"
            << "  ],\n"
            << "  \"files\": [\n"
            << "    {\n"
            << "      \"path\": \"events/strategic_nexus_generated_events.txt\",\n"
            << "      \"checksum_relevance\": \"gameplay_affecting\",\n"
            << "      \"hash\": \"" << eventsHash << "\",\n"
            << "      \"byte_count\": " << eventsText.size() << "\n"
            << "    },\n"
            << "    {\n"
            << "      \"path\": \"common/scripted_effects/strategic_nexus_generated_effects.txt\",\n"
            << "      \"checksum_relevance\": \"gameplay_affecting\",\n"
            << "      \"hash\": \"" << effectsHash << "\",\n"
            << "      \"byte_count\": " << effectsText.size() << "\n"
            << "    },\n"
            << "    {\n"
            << "      \"path\": \"common/scripted_triggers/strategic_nexus_generated_triggers.txt\",\n"
            << "      \"checksum_relevance\": \"gameplay_affecting\",\n"
            << "      \"hash\": \"" << triggersHash << "\",\n"
            << "      \"byte_count\": " << triggersText.size() << "\n"
            << "    }\n"
            << "  ]\n"
            << "}\n";
    }

    {
        strategic_nexus::generated_overlay::MpOverlayPackageExporter exporter;
        const auto exportResult = exporter.exportPackage(
            overlayRoot,
            "campaign_mp_001",
            "overlay_v1",
            "stellaris_4.x",
            "strategic_nexus_v0",
            mpPackageRoot,
            false);
        requireCondition(exportResult.ok, "MP overlay package export should succeed for companion test fixture");
        const auto completeExportResult = exporter.exportPackage(
            overlayRoot,
            "campaign_mp_001",
            "overlay_v1",
            "stellaris_4.x",
            "strategic_nexus_v0",
            mpPackageCompleteRoot,
            true);
        requireCondition(
            completeExportResult.ok,
            "complete MP overlay package export should succeed for memory recovery fixture");
    }
    {
        std::ofstream zipOut(mpPackageZipPath, std::ios::binary | std::ios::trunc);
        zipOut << "zip handoff payload";
    }
    writeTextFileAtomically(
        root / "strategic_nexus_campaign_library_plan.json",
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"save_root_available\": true,\n"
        "  \"limit_reached\": false,\n"
        "  \"max_included_campaigns\": 4,\n"
        "  \"included_count\": 2,\n"
        "  \"skipped_count\": 0,\n"
        "  \"skipped_due_to_limit_count\": 0,\n"
        "  \"campaigns\": []\n"
        "}\n");

    writeTextFileAtomically(
        entryPointAnalysisPath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"reason\": \"entry points scanned\",\n"
        "  \"readiness\": \"ready\",\n"
        "  \"entry_point_count\": 3,\n"
        "  \"archived_evidence_count\": 5,\n"
        "  \"branch_ambiguity_detected\": false,\n"
        "  \"entry_points\": [\n"
        "    {\n"
        "      \"id\": \"alpha_autosave_2200_01_01\",\n"
        "      \"campaign_key\": \"alpha_campaign\",\n"
        "      \"source_root\": \"current_save_root\",\n"
        "      \"relative_path\": \"Alpha/autosave_2200.01.01.sav\",\n"
        "      \"source_kind\": \"autosave\",\n"
        "      \"save_name\": \"autosave_2200.01.01.sav\",\n"
        "      \"save_date\": \"2200.01.01\",\n"
        "      \"analysis_state\": \"ready_conservative\",\n"
        "      \"compatible_archived_evidence_count\": 1,\n"
        "      \"later_archived_evidence_count\": 1\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"alpha_manual_2200_03_01\",\n"
        "      \"campaign_key\": \"alpha_campaign\",\n"
        "      \"source_root\": \"current_save_root\",\n"
        "      \"relative_path\": \"Alpha/2200.03.01.sav\",\n"
        "      \"source_kind\": \"date_named_save\",\n"
        "      \"save_name\": \"2200.03.01.sav\",\n"
        "      \"save_date\": \"2200.03.01\",\n"
        "      \"analysis_state\": \"ready\",\n"
        "      \"compatible_archived_evidence_count\": 2,\n"
        "      \"later_archived_evidence_count\": 0\n"
        "    },\n"
        "    {\n"
        "      \"id\": \"beta_ironman_2200_05_01\",\n"
        "      \"campaign_key\": \"beta_campaign\",\n"
        "      \"source_root\": \"current_save_root\",\n"
        "      \"relative_path\": \"Beta/ironman.sav\",\n"
        "      \"source_kind\": \"ironman\",\n"
        "      \"save_name\": \"ironman.sav\",\n"
        "      \"save_date\": \"2200.05.01\",\n"
        "      \"analysis_state\": \"ready\",\n"
        "      \"compatible_archived_evidence_count\": 2,\n"
        "      \"later_archived_evidence_count\": 0\n"
        "    }\n"
        "  ]\n"
        "}\n");
    writeTextFileAtomically(
        memoryRecoveryEntryPointAnalysisPath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"reason\": \"entry points scanned with branch ambiguity\",\n"
        "  \"readiness\": \"ready_partial\",\n"
        "  \"entry_point_count\": 1,\n"
        "  \"archived_evidence_count\": 2,\n"
        "  \"branch_ambiguity_detected\": true,\n"
        "  \"entry_points\": [\n"
        "    {\n"
        "      \"id\": \"gamma_autosave_2200_04_01\",\n"
        "      \"campaign_key\": \"gamma_campaign\",\n"
        "      \"source_root\": \"current_save_root\",\n"
        "      \"relative_path\": \"Gamma/autosave_2200.04.01.sav\",\n"
        "      \"source_kind\": \"autosave\",\n"
        "      \"save_name\": \"autosave_2200.04.01.sav\",\n"
        "      \"save_date\": \"2200.04.01\",\n"
        "      \"analysis_state\": \"ready_conservative\",\n"
        "      \"compatible_archived_evidence_count\": 1,\n"
        "      \"later_archived_evidence_count\": 1\n"
        "    }\n"
        "  ]\n"
        "}\n");
    writeTextFileAtomically(
        postPlayPackagePath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"reason\": \"post-play package built; some entry points need more history or parsing\",\n"
        "  \"readiness\": \"ready_partial\",\n"
        "  \"decision_ready_entry_count\": 2,\n"
        "  \"campaigns\": [\n"
        "    {\n"
        "      \"campaign_key\": \"alpha_campaign\",\n"
        "      \"readiness\": \"ready_partial\",\n"
        "      \"branch_ambiguity_detected\": false,\n"
        "      \"entry_point_count\": 2,\n"
        "      \"decision_ready_entry_count\": 1\n"
        "    },\n"
        "    {\n"
        "      \"campaign_key\": \"beta_campaign\",\n"
        "      \"readiness\": \"ready\",\n"
        "      \"branch_ambiguity_detected\": false,\n"
        "      \"entry_point_count\": 1,\n"
        "      \"decision_ready_entry_count\": 1\n"
        "    }\n"
        "  ],\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"entry_point_id\": \"alpha_entry_point\",\n"
        "      \"campaign_key\": \"alpha_campaign\",\n"
        "      \"player_country_id\": \"0\"\n"
        "    }\n"
        "  ]\n"
        "}\n");
    writeTextFileAtomically(
        decisionInputPackagePath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"reason\": \"decision input package built; some entries remain blocked\",\n"
        "  \"readiness\": \"ready_partial\",\n"
        "  \"decision_input_count\": 2,\n"
        "  \"blocked_entry_count\": 1\n"
        "}\n");
    writeTextFileAtomically(
        candidateDecisionPackagePath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"reason\": \"candidate decision package built; some source entries remain blocked\",\n"
        "  \"readiness\": \"ready\",\n"
        "  \"candidate_decision_count\": 2,\n"
        "  \"blocked_source_entry_count\": 1,\n"
        "  \"validator_passed\": true\n"
        "}\n");
    writeTextFileAtomically(
        dslDraftPath,
        "campaign test_campaign\n"
        "entry_point alpha_entry\n"
        "rule doctrine_inertia high\n");
    writeTextFileAtomically(
        dslDraftAuditPath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"reason\": \"validated dry-run DSL draft built\",\n"
        "  \"readiness\": \"ready\",\n"
        "  \"dsl_rule_count\": 1,\n"
        "  \"eligible_candidate_count\": 1,\n"
        "  \"skipped_candidate_count\": 1,\n"
        "  \"validator_passed\": true\n"
        "}\n");
    writeTextFileAtomically(
        generatedOverlayStagingStatusPath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"reason\": \"validated generated overlay staged\",\n"
        "  \"readiness\": \"staged_verified\",\n"
        "  \"dsl_rule_count\": 1,\n"
        "  \"manifest_verified\": true,\n"
        "  \"publish_allowed\": false\n"
        "}\n");
    writeTextFileAtomically(
        readyGameplayAcceptanceReport,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"acceptance_state\": \"verified_for_v0_domains\",\n"
        "  \"summary\": \"gameplay acceptance verified for monthly reactive v0 markers\",\n"
        "  \"cases\": [\n"
        "    { \"case_id\": \"case_a_defensive_military_posture\", \"result\": \"pass\" },\n"
        "    { \"case_id\": \"case_b_aggressive_military_posture\", \"result\": \"pass\" },\n"
        "    { \"case_id\": \"case_c_economy_research_bias\", \"result\": \"pass\" },\n"
        "    { \"case_id\": \"case_d_military_industry_research_bias\", \"result\": \"pass\" },\n"
        "    { \"case_id\": \"case_e_invalid_tactical_domain\", \"result\": \"pass\" },\n"
        "    { \"case_id\": \"case_f_manifest_drift_before_publish\", \"result\": \"pass\" }\n"
        "  ]\n"
        "}\n");
    {
        strategic_nexus::SncFriendTrustStore store;
        store.ok = true;

        strategic_nexus::SncTrustedFriend trustedFriend;
        trustedFriend.identity = makeFriendIdentity("snc-node-client-001", "Client SNC", "fp-client-001");
        trustedFriend.localAlias = "Client";
        trustedFriend.trustState = "trusted";
        trustedFriend.autoSyncEnabled = true;
        trustedFriend.acceptedAt = "2026-06-08T17:36:00Z";
        trustedFriend.updatedAt = "2026-06-08T17:36:00Z";
        store.friends.push_back(trustedFriend);

        strategic_nexus::SncTrustedFriend revokedFriend;
        revokedFriend.identity = makeFriendIdentity("snc-node-revoked-001", "Revoked SNC", "fp-revoked-001");
        revokedFriend.localAlias = "Revoked";
        revokedFriend.trustState = "revoked";
        revokedFriend.autoSyncEnabled = false;
        revokedFriend.acceptedAt = "2026-06-07T17:36:00Z";
        revokedFriend.updatedAt = "2026-06-08T17:36:00Z";
        store.friends.push_back(revokedFriend);

        strategic_nexus::SncTrustedFriend blockedFriend;
        blockedFriend.identity = makeFriendIdentity("snc-node-blocked-001", "Blocked SNC", "fp-blocked-001");
        blockedFriend.localAlias = "Blocked";
        blockedFriend.trustState = "blocked";
        blockedFriend.autoSyncEnabled = false;
        blockedFriend.acceptedAt = "2026-06-07T17:36:00Z";
        blockedFriend.updatedAt = "2026-06-08T17:36:00Z";
        store.friends.push_back(blockedFriend);

        writeTextFileAtomically(friendTrustStorePath, strategic_nexus::serializeSncFriendTrustStore(store));
    }
    writeTextFileAtomically(
        brokenFriendTrustStorePath,
        "{\n"
        "  \"schema_version\": 2,\n"
        "  \"friends\": []\n"
        "}\n");

    const strategic_nexus::StrategicNexusCompanion companion;
    strategic_nexus::CompanionStatusConfig readyConfig;
    readyConfig.archiveRoot = archiveRoot;
    readyConfig.generatedOverlayDirectory = overlayRoot;
    readyConfig.mpOverlayPackageDirectory = mpPackageRoot;
    readyConfig.startWithWindowsEnabled = true;
    readyConfig.useConfiguredStartWithWindowsState = true;
    readyConfig.useDetectedStellarisState = false;
    readyConfig.stellarisRunningOverride = false;
    readyConfig.gameplayAcceptanceReportPath = missingGameplayAcceptanceReport;
    readyConfig.entryPointAnalysisPath = entryPointAnalysisPath;
    readyConfig.postPlayPackagePath = postPlayPackagePath;
    readyConfig.decisionInputPackagePath = decisionInputPackagePath;
    readyConfig.candidateDecisionPackagePath = candidateDecisionPackagePath;
    readyConfig.dslDraftPath = dslDraftPath;
    readyConfig.dslDraftAuditPath = dslDraftAuditPath;
    readyConfig.postPlayGeneratedOverlayStagingStatusPath = generatedOverlayStagingStatusPath;
    readyConfig.mpOverlayPackageZipPath = mpPackageZipPath;
    readyConfig.startWithWindowsShortcutPath = root / "Strategic Nexus Companion.lnk";
    readyConfig.supportReportPreviewPath = root / "snc_support_report_preview.txt";
    readyConfig.crashRecoveryStatePath = crashRecoveryStatePath;
    readyConfig.friendTrustStorePath = friendTrustStorePath;
    const auto ready = companion.buildStatusSnapshot(readyConfig);

    requireCondition(ready.appName == "Strategic Nexus Companion", "SNC app name should be stable");
    requireCondition(ready.abbreviation == "SNC", "SNC abbreviation should be stable");
    requireCondition(ready.lifecycle.startWithWindowsEnabled, "lifecycle should carry start-with-Windows state");
    requireCondition(
        ready.lifecycle.startWithWindowsSource == "config_override",
        "lifecycle should expose configured start-with-Windows source");
    requireCondition(
        ready.lifecycle.startWithWindowsShortcutState == "override_enabled",
        "lifecycle should expose configured shortcut state");
    requireCondition(
        ready.lifecycle.startWithWindowsShortcutPath == readyConfig.startWithWindowsShortcutPath,
        "lifecycle should expose configured startup shortcut path");
    requireCondition(
        ready.lifecycle.startWithWindowsCommandHint == "cmd /c tools\\remove_snc_tray_startup_shortcut.cmd",
        "enabled lifecycle should expose startup removal command hint");
    requireCondition(
        ready.lifecycle.startWithWindowsCommandHintSource == "startup_shortcut_remove_command",
        "enabled lifecycle should expose startup removal command source");
    requireCondition(
        ready.lifecycle.startWithWindowsEnableCommandHint == "cmd /c tools\\install_snc_tray_startup_shortcut.cmd",
        "lifecycle should expose startup install command hint");
    requireCondition(
        ready.lifecycle.startWithWindowsDisableCommandHint == "cmd /c tools\\remove_snc_tray_startup_shortcut.cmd",
        "lifecycle should expose startup remove command hint");
    requireCondition(
        ready.supportReport.state == "not_prepared",
        "support report should start as not prepared when preview file is missing");
    requireCondition(
        ready.supportReport.reason == "prepare local support report preview before manual review or send",
        "support report should explain missing preview state");
    requireCondition(
        ready.supportReport.previewPath == readyConfig.supportReportPreviewPath,
        "support report should expose configured preview path");
    requireCondition(
        ready.supportReport.supportContactEmail == "support@jeniksoft.cz",
        "support report should expose support contact");
    requireCondition(
        ready.supportReport.sendRequiresApproval,
        "support report should require explicit approval before send");
    requireCondition(
        !ready.supportReport.rawSavesIncluded,
        "support report should avoid raw saves by default");
    requireCondition(
        ready.supportReport.prepareCommandHint ==
            "powershell -NoProfile -ExecutionPolicy Bypass -File tools\\prepare_snc_support_report.ps1",
        "support report should expose the preview prepare command");
    requireCondition(
        ready.supportReport.openCommandHint.empty(),
        "support report should not expose an open command before the preview exists");
    requireCondition(
        ready.crashRecovery.state == "no_recent_unexpected_crash",
        "crash recovery should default to a clean baseline when no state file exists");
    requireCondition(
        ready.crashRecovery.reason == "no crash recovery record present",
        "crash recovery baseline should explain the missing state file");
    requireCondition(
        ready.crashRecovery.statePath == crashRecoveryStatePath,
        "crash recovery should expose the configured state path");
    requireCondition(
        !ready.crashRecovery.restartBudgetExhausted,
        "crash recovery baseline should not exhaust the restart budget");
    requireCondition(
        !ready.crashRecovery.warningVisible,
        "crash recovery baseline should not show a warning");
    requireCondition(
        !ready.crashRecovery.supportReportRecommended,
        "crash recovery baseline should not recommend a support report");
    requireCondition(
        ready.friendTrustStore.state == "ready",
        "valid friend trust store with auto-sync should be ready");
    requireCondition(
        ready.friendTrustStore.trustedFriendCount == 1,
        "friend trust store should expose trusted friend count");
    requireCondition(
        ready.friendTrustStore.revokedFriendCount == 1,
        "friend trust store should expose revoked friend count");
    requireCondition(
        ready.friendTrustStore.blockedFriendCount == 1,
        "friend trust store should expose blocked friend count");
    requireCondition(
        ready.friendTrustStore.autoSyncEnabledCount == 1 && ready.friendTrustStore.autoSyncAvailable,
        "friend trust store should expose auto-sync availability");
    requireCondition(
        ready.statusCenterSummaryText.find("friend_trust_store_auto_sync_enabled_count: 1") != std::string::npos,
        "status center summary should expose friend trust store auto-sync count");
    auto brokenFriendConfig = readyConfig;
    brokenFriendConfig.friendTrustStorePath = brokenFriendTrustStorePath;
    const auto brokenFriendTrustStore = companion.buildStatusSnapshot(brokenFriendConfig);
    requireCondition(
        brokenFriendTrustStore.friendTrustStore.state == "needs_attention",
        "unsupported friend trust store schema should fail closed");
    requireCondition(
        brokenFriendTrustStore.friendTrustStore.reason == "unsupported friend trust store schema",
        "unsupported friend trust store schema should expose parser reason");
    requireCondition(
        brokenFriendTrustStore.statusCenter.state == "attention_required",
        "status center should surface invalid friend trust store");
    requireCondition(
        brokenFriendTrustStore.statusCenter.reason == "friend trust store needs attention",
        "status center should attribute invalid friend trust store");
    requireCondition(
        brokenFriendTrustStore.statusCenter.path == brokenFriendTrustStorePath,
        "status center should point to invalid friend trust store");
    requireCondition(ready.lifecycle.windowCloseBehavior == "minimize_to_tray", "window close should minimize to tray");
    requireCondition(ready.lifecycle.explicitExitBehavior == "stop_without_restart", "explicit exit should stop without restart");
    requireCondition(ready.lifecycle.crashRestartPolicy == "bounded_backoff_with_crash_loop_guard", "crash policy should be bounded");
    requireCondition(ready.saveDiscovery.state == "ready", "save discovery should find at least one save root in fixture");
    requireCondition(ready.archive.state == "starting", "archive should start when archive root exists but has no sessions");
    requireCondition(ready.generatedOverlay.state == "ready", "overlay should be ready when manifest verifies");
    requireCondition(!ready.generatedOverlay.manifestHash.empty(), "overlay status should expose generated overlay manifest hash");
    requireCondition(
        ready.generatedOverlay.reactivePolicyPackCapability == "event_family_dispatch",
        "overlay status should expose reactive capability");
    requireCondition(
        ready.generatedOverlay.eventFamilies.size() == 1 && ready.generatedOverlay.eventFamilies.front() == "monthly_strategy_tick",
        "overlay status should expose event-family coverage");
    requireCondition(
        ready.generatedOverlay.sourceQualities.size() == 2,
        "overlay status should expose source qualities");
    requireCondition(
        ready.generatedOverlay.bootstrapCampaignCount == 1,
        "overlay status should expose bootstrap provenance count");
    requireCondition(ready.generatedOverlayPublishGate.state == "ready", "publish gate should be ready when Stellaris is not running");
    requireCondition(
        ready.generatedOverlayPublishGate.reason == "Stellaris is not running; generated overlay publish allowed",
        "publish gate should explain publish readiness");
    requireCondition(ready.mpOverlayPackage.state == "ready", "mp overlay package should be ready when verified");
    requireCondition(ready.mpOverlayPackage.packageZipState == "ready", "mp overlay package zip should be ready when configured");
    requireCondition(
        ready.mpOverlayPackage.packageZipReason == "mp overlay package zip ready for handoff",
        "mp overlay package zip should expose handoff reason");
    requireCondition(ready.mpOverlayPackage.packageZipPath == mpPackageZipPath, "mp overlay package zip should expose path");
    requireCondition(!ready.mpOverlayPackage.packageZipHash.empty(), "mp overlay package zip should expose hash");
    requireCondition(ready.mpOverlayPackage.packageZipBytes > 0, "mp overlay package zip should expose byte count");
    requireCondition(std::filesystem::exists(mpPackageZipPath), "mp overlay package zip should be created on disk");
    requireCondition(
        ready.nextAction == "review_mp_handoff_continuity",
        "degraded previous-host continuity should become the top-level next action");
    requireCondition(
        ready.nextActionReason == "mp_handoff_degraded_previous_host_unavailable",
        "degraded previous-host continuity should expose a stable next-action reason");
    requireCondition(
        ready.nextActionCommandHint.find("Strategic Nexus.exe --verify-mp-overlay-package ") == 0,
        "degraded previous-host continuity should expose a verify-oriented command hint");
    requireCondition(
        ready.nextActionCommandHintSource == "mp_overlay_package_strict_verify_command",
        "degraded previous-host continuity should prefer strict verify command hints");
    requireCondition(
        ready.nextActionPath == mpPackageRoot,
        "degraded previous-host continuity should point next-action path at the MP package");
    requireCondition(
        ready.localLlm.state == "no_model_installed",
        "local LLM status should default to reduced mode when no model state is configured");
    requireCondition(
        ready.localLlm.reducedMode,
        "local LLM should explicitly report reduced mode when no supported model is installed");
    requireCondition(
        !ready.localLlm.canRunInference,
        "local LLM should not allow inference without a ready selected model");
    requireCondition(
        ready.statusCenterSummaryText.find("local_llm_reduced_mode: true") != std::string::npos,
        "status center summary should expose local LLM reduced mode");

    const auto readyLocalModelStatePath = root / "snc_local_model_state_ready.json";
    writeTextFileAtomically(
        readyLocalModelStatePath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"selected_model_id\": \"ollama:llama3.2:3b\",\n"
        "  \"model_version\": \"3b\",\n"
        "  \"runtime\": \"ollama\",\n"
        "  \"status\": \"model_ready\",\n"
        "  \"user_license_accepted\": true,\n"
        "  \"runtime_available\": true,\n"
        "  \"runtime_model_present\": true\n"
        "}\n");
    auto readyModelConfig = readyConfig;
    readyModelConfig.localLlmModelStatePath = readyLocalModelStatePath;
    const auto readyWithModel = companion.buildStatusSnapshot(readyModelConfig);
    requireCondition(
        readyWithModel.localLlm.state == "model_ready",
        "accepted supported local LLM state should be ready");
    requireCondition(
        readyWithModel.localLlm.canRunInference,
        "accepted supported local LLM state should allow inference wiring");
    requireCondition(
        !readyWithModel.localLlm.reducedMode,
        "accepted supported local LLM state should leave reduced mode");
    requireCondition(
        readyWithModel.statusCenterSummaryText.find("local_llm_can_run_inference: true") != std::string::npos,
        "status center summary should expose local LLM inference readiness");
    requireCondition(
        readyWithModel.localLlm.modelStatePath == readyLocalModelStatePath,
        "local LLM status should expose the configured model state path");
    requireCondition(
        readyWithModel.localLlm.prepareCommandHint.find(
            "Strategic Nexus.exe --prepare-local-llm-model ollama:llama3.2:3b") != std::string::npos,
        "local LLM status should expose a prepare command hint");
    requireCondition(
        readyWithModel.localLlm.prepareCommandHint.find("accept-license download") != std::string::npos,
        "local LLM prepare command hint should include explicit acceptance and download");
    requireCondition(
        readyWithModel.localLlm.prepareCommandHint.find(readyLocalModelStatePath.string()) != std::string::npos,
        "local LLM prepare command hint should include the configured state path");

    auto memoryRecoveryWarningConfig = readyConfig;
    memoryRecoveryWarningConfig.entryPointAnalysisPath = memoryRecoveryEntryPointAnalysisPath;
    memoryRecoveryWarningConfig.mpOverlayPackageDirectory = mpPackageCompleteRoot;
    memoryRecoveryWarningConfig.stellarisRunningOverride = true;
    const auto memoryRecoveryWarning = companion.buildStatusSnapshot(memoryRecoveryWarningConfig);
    requireCondition(
        memoryRecoveryWarning.mpOverlayPackage.handoffStatus == "complete",
        "memory recovery fixture should not rely on degraded MP handoff priority");
    requireCondition(
        memoryRecoveryWarning.postPlayPipeline.memoryRecovery.state == "degraded",
        "branch ambiguity should degrade memory recovery confidence");
    requireCondition(
        memoryRecoveryWarning.postPlayPipeline.memoryRecovery.warningVisible,
        "degraded memory recovery should be visible as a warning");
    requireCondition(
        memoryRecoveryWarning.nextAction == "review_memory_recovery_status",
        "degraded memory recovery should become the top-level next action when higher-priority gates do not apply");
    requireCondition(
        memoryRecoveryWarning.nextActionReason == "branch ambiguity requires conservative recovery anchor",
        "degraded memory recovery should expose its reason as the next-action reason");
    requireCondition(
        memoryRecoveryWarning.nextActionCommandHint.empty(),
        "memory recovery review should not invent a command hint");
    requireCondition(
        memoryRecoveryWarning.nextActionCommandHintSource == "none",
        "memory recovery review should keep the command hint source fail-closed");
    requireCondition(
        memoryRecoveryWarning.nextActionPath == memoryRecoveryEntryPointAnalysisPath,
        "degraded memory recovery should point next-action path at entry-point evidence");

    const auto unsupportedLocalModelStatePath = root / "snc_local_model_state_unsupported.json";
    writeTextFileAtomically(
        unsupportedLocalModelStatePath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"selected_model_id\": \"unknown:unsafe-model\",\n"
        "  \"runtime\": \"unknown\",\n"
        "  \"status\": \"model_ready\",\n"
        "  \"runtime_available\": true,\n"
        "  \"runtime_model_present\": true\n"
        "}\n");
    auto unsupportedModelConfig = readyConfig;
    unsupportedModelConfig.localLlmModelStatePath = unsupportedLocalModelStatePath;
    const auto unsupportedModel = companion.buildStatusSnapshot(unsupportedModelConfig);
    requireCondition(
        unsupportedModel.localLlm.state == "model_license_not_supported",
        "unknown local LLM model should fail closed as unsupported");
    requireCondition(
        unsupportedModel.statusCenter.state == "attention_required",
        "unsupported selected local LLM model should surface in Status Center");
    requireCondition(
        unsupportedModel.statusCenterSummaryText.find("local_llm_model: model_license_not_supported") !=
            std::string::npos,
        "status center summary should expose unsupported local LLM model state");

    const auto crashRecoveryWarningStatePath = root / "snc_crash_recovery_warning.json";
    writeTextFileAtomically(
        crashRecoveryWarningStatePath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"state\": \"restart_budget_exhausted\",\n"
        "  \"reason\": \"unexpected crashes exhausted restart budget; manual start required\",\n"
        "  \"last_crash_at_local\": \"8.6.2026  0:41:22\",\n"
        "  \"last_recovery_action\": \"restart_blocked_until_manual_start\",\n"
        "  \"last_operation\": \"publish_generated_overlay\",\n"
        "  \"app_version\": \"v0-fixture\",\n"
        "  \"recent_unexpected_restart_count\": 3,\n"
        "  \"restart_window_minutes\": 10,\n"
        "  \"next_backoff_seconds\": 300,\n"
        "  \"restart_budget_exhausted\": true,\n"
        "  \"warning_visible\": true,\n"
        "  \"support_report_recommended\": true\n"
        "}\n");
    auto crashRecoveryWarningConfig = readyConfig;
    crashRecoveryWarningConfig.crashRecoveryStatePath = crashRecoveryWarningStatePath;
    const auto crashRecoveryWarning = companion.buildStatusSnapshot(crashRecoveryWarningConfig);
    requireCondition(
        crashRecoveryWarning.crashRecovery.state == "restart_budget_exhausted",
        "crash recovery warning fixture should expose exhausted restart budget state");
    requireCondition(
        crashRecoveryWarning.crashRecovery.restartBudgetExhausted,
        "crash recovery warning fixture should mark restart budget exhausted");
    requireCondition(
        crashRecoveryWarning.crashRecovery.warningVisible,
        "crash recovery warning fixture should expose a visible warning");
    requireCondition(
        crashRecoveryWarning.crashRecovery.supportReportRecommended,
        "crash recovery warning fixture should recommend a support report");
    requireCondition(
        crashRecoveryWarning.statusCenter.state == "attention_required",
        "crash recovery warning should lift Status Center into attention_required");
    requireCondition(
        crashRecoveryWarning.statusCenter.reason == "crash recovery needs attention",
        "crash recovery warning should expose a stable Status Center reason");
    requireCondition(
        crashRecoveryWarning.nextAction == "review_crash_recovery_status",
        "crash recovery warning should become the top-level next action");
    requireCondition(
        crashRecoveryWarning.nextActionReason == "unexpected crashes exhausted restart budget; manual start required",
        "crash recovery warning should expose the state-file reason as next-action reason");
    requireCondition(
        crashRecoveryWarning.nextActionCommandHint ==
            "powershell -NoProfile -ExecutionPolicy Bypass -File tools\\prepare_snc_support_report.ps1",
        "crash recovery warning should recommend preparing the support report");
    requireCondition(
        crashRecoveryWarning.nextActionCommandHintSource == "support_report_prepare_command",
        "crash recovery warning should expose the support report prepare command source");
    requireCondition(
        crashRecoveryWarning.nextActionPath == readyConfig.supportReportPreviewPath,
        "crash recovery warning should point follow-up at the support report preview path");
    requireCondition(
        crashRecoveryWarning.statusCenterSummaryText.find("crash_recovery_state: restart_budget_exhausted") !=
            std::string::npos,
        "crash recovery warning should be copied into the status center summary");
    requireCondition(
        crashRecoveryWarning.statusCenterSummaryText.find(
            "crash_recovery_last_recovery_action: restart_blocked_until_manual_start") != std::string::npos,
        "crash recovery warning summary should expose the last recovery action");

    requireCondition(
        strategic_nexus::ollamaModelNameFromCatalogId("ollama:llama3.2:3b") == "llama3.2:3b",
        "Ollama catalog id should map to runtime model name");
    requireCondition(
        strategic_nexus::ollamaModelNameFromCatalogId("llama3.2:3b").empty(),
        "non-Ollama catalog id should not map to runtime model name");
    requireCondition(
        strategic_nexus::ollamaTagsContainModel(
            "{\"models\":[{\"name\":\"llama3.2:3b\"},{\"name\":\"qwen2.5:7b\"}]}",
            "llama3.2:3b"),
        "Ollama tags parser should recognize installed catalog model");
    requireCondition(
        !strategic_nexus::ollamaTagsContainModel(
            "{\"models\":[{\"name\":\"llama3.2:latest\"}]}",
            "llama3.2:3b"),
        "Ollama tags parser should not confuse a different tag with the selected model");

    strategic_nexus::LocalLlmModelState serializedState;
    serializedState.selectedModelId = "ollama:llama3.2:3b";
    serializedState.runtime = "ollama";
    serializedState.status = "model_ready";
    serializedState.userLicenseAccepted = true;
    serializedState.runtimeAvailable = true;
    serializedState.runtimeModelPresent = true;
    const auto localLlmSerializedStatePath = root / "local_llm_model_state_serialized.json";
    requireCondition(
        strategic_nexus::writeLocalLlmModelState(localLlmSerializedStatePath, serializedState),
        "local LLM model state should serialize to disk");
    const auto loadedSerializedState =
        strategic_nexus::loadLocalLlmModelState(localLlmSerializedStatePath);
    requireCondition(
        loadedSerializedState.selectedModelId == serializedState.selectedModelId,
        "serialized local LLM state should preserve selected model id");
    requireCondition(
        loadedSerializedState.runtime == "ollama",
        "serialized local LLM state should preserve runtime");
    requireCondition(
        loadedSerializedState.userLicenseAccepted &&
            loadedSerializedState.runtimeAvailable &&
            loadedSerializedState.runtimeModelPresent,
        "serialized local LLM state should preserve readiness booleans");

    const auto missingMpPackageZipPath = root / "missing_mp_overlay_package.zip";
    const auto readyWithoutZip = companion.buildStatusSnapshot({
        archiveRoot,
        overlayRoot,
        mpPackageRoot,
        true,
        false,
        false,
        missingGameplayAcceptanceReport,
        std::filesystem::path(),
        std::filesystem::path(),
        std::filesystem::path(),
        std::filesystem::path(),
        entryPointAnalysisPath,
        postPlayPackagePath,
        decisionInputPackagePath,
        candidateDecisionPackagePath,
        dslDraftPath,
        dslDraftAuditPath,
        generatedOverlayStagingStatusPath,
        missingMpPackageZipPath
    });
    requireCondition(
        readyWithoutZip.mpOverlayPackage.state == "ready",
        "auto-exported tray MP package zip should keep verified MP package readiness");
    requireCondition(
        readyWithoutZip.mpOverlayPackage.packageZipState == "ready",
        "missing tray MP package zip should be exported by current status source");
    requireCondition(
        readyWithoutZip.mpOverlayPackage.packageZipReason == "mp overlay package zip ready for handoff",
        "exported tray MP package zip should expose handoff-ready reason");
    requireCondition(
        readyWithoutZip.mpOverlayPackage.packageZipPath == missingMpPackageZipPath,
        "current status source should export missing tray MP package zip to requested path");
    requireCondition(
        !readyWithoutZip.mpOverlayPackage.packageZipHash.empty(),
        "current status source should expose exported tray MP package zip hash");
    requireCondition(
        std::filesystem::exists(missingMpPackageZipPath),
        "current status source should create the missing tray MP package zip");
    requireCondition(
        readyWithoutZip.nextAction == "review_mp_handoff_continuity",
        "degraded previous-host continuity should still win when the optional tray zip is missing");
    requireCondition(ready.postPlayPipeline.state == "ready", "post-play pipeline should be ready when generated overlay staging verifies");
    requireCondition(
        ready.postPlayPipeline.reason == "generated overlay staging verified",
        "post-play pipeline should summarize generated overlay staging readiness");
    requireCondition(ready.postPlayPipeline.entryPointCount == 3, "post-play pipeline should expose entry point count");
    requireCondition(
        ready.postPlayPipeline.entryPointReason == "entry points scanned",
        "post-play pipeline should expose entry point analysis reason");
    requireCondition(
        ready.postPlayPipeline.postPlayDecisionReadyEntryCount == 2,
        "post-play pipeline should expose decision-ready entry count");
    requireCondition(
        ready.postPlayPipeline.postPlayPackageReason == "post-play package built; some entry points need more history or parsing",
        "post-play pipeline should expose post-play package reason");
    requireCondition(ready.postPlayPipeline.postPlayCampaignCount == 2, "post-play pipeline should expose campaign count");
    requireCondition(
        ready.postPlayPipeline.postPlayReadyCampaignCount == 1,
        "post-play pipeline should expose ready campaign count");
    requireCondition(
        ready.postPlayPipeline.postPlayPartialCampaignCount == 1,
        "post-play pipeline should expose partial campaign count");
    requireCondition(
        ready.postPlayPipeline.postPlayBlockedCampaignCount == 0,
        "post-play pipeline should expose blocked campaign count");
    requireCondition(
        ready.postPlayPipeline.postPlayCampaignReadinessSummaries.size() == 2,
        "post-play pipeline should expose campaign readiness summaries");
    requireCondition(
        ready.postPlayPipeline.postPlayCampaignReadinessSummaries.front() == "alpha_campaign: ready_partial (1/2 ready)",
        "post-play pipeline should preserve first campaign summary");
    requireCondition(ready.postPlayPipeline.decisionInputCount == 2, "post-play pipeline should expose decision input count");
    requireCondition(
        ready.postPlayPipeline.decisionInputPackageReason == "decision input package built; some entries remain blocked",
        "post-play pipeline should expose decision input package reason");
    requireCondition(
        ready.postPlayPipeline.decisionInputBlockedEntryCount == 1,
        "post-play pipeline should expose blocked decision input count");
    requireCondition(ready.postPlayPipeline.candidateDecisionCount == 2, "post-play pipeline should expose candidate decision count");
    requireCondition(
        ready.postPlayPipeline.candidateDecisionPackageReason ==
            "candidate decision package built; some source entries remain blocked",
        "post-play pipeline should expose candidate decision package reason");
    requireCondition(
        ready.postPlayPipeline.candidateDecisionBlockedSourceEntryCount == 1,
        "post-play pipeline should expose blocked candidate source count");
    requireCondition(
        ready.postPlayPipeline.candidateDecisionValidatorPassed,
        "post-play pipeline should expose candidate validator result");
    requireCondition(
        ready.postPlayPipeline.dslDraftReadiness == "ready",
        "post-play pipeline should expose DSL draft readiness");
    requireCondition(
        ready.postPlayPipeline.dslDraftReason == "validated dry-run DSL draft built",
        "post-play pipeline should expose DSL draft reason");
    requireCondition(
        ready.postPlayPipeline.dslDraftRuleCount == 1,
        "post-play pipeline should expose DSL draft rule count");
    requireCondition(
        ready.postPlayPipeline.dslDraftEligibleCandidateCount == 1,
        "post-play pipeline should expose DSL draft eligible candidate count");
    requireCondition(
        ready.postPlayPipeline.dslDraftSkippedCandidateCount == 1,
        "post-play pipeline should expose DSL draft skipped candidate count");
    requireCondition(
        ready.postPlayPipeline.dslDraftValidatorPassed,
        "post-play pipeline should expose DSL draft validator result");
    requireCondition(
        ready.postPlayPipeline.generatedOverlayStagingReadiness == "staged_verified",
        "post-play pipeline should expose generated overlay staging readiness");
    requireCondition(
        ready.postPlayPipeline.generatedOverlayStagingReason == "validated generated overlay staged",
        "post-play pipeline should expose generated overlay staging reason");
    requireCondition(
        ready.postPlayPipeline.generatedOverlayStagingRuleCount == 1,
        "post-play pipeline should expose generated overlay staging rule count");
    requireCondition(
        ready.postPlayPipeline.generatedOverlayManifestVerified,
        "post-play pipeline should expose generated overlay manifest verification");
    requireCondition(
        !ready.postPlayPipeline.generatedOverlayPublishAllowed,
        "post-play pipeline should expose publish gate still disabled for staged overlay");
    requireCondition(
        writeTextFileAtomically(
            candidateDecisionPackagePath,
            "{\n"
            "  \"schema_version\": 1,\n"
            "  \"reason\": \"candidate decision package blocked by validator\",\n"
            "  \"readiness\": \"blocked\",\n"
            "  \"candidate_decision_count\": 2,\n"
            "  \"blocked_source_entry_count\": 1,\n"
            "  \"validator_passed\": false\n"
            "}\n"),
        "stale-stage regression fixture should rewrite candidate decision package");
    const auto staleStageBlocked = companion.buildStatusSnapshot({
        archiveRoot,
        overlayRoot,
        mpPackageRoot,
        true,
        false,
        false,
        missingGameplayAcceptanceReport,
        std::filesystem::path(),
        std::filesystem::path(),
        std::filesystem::path(),
        std::filesystem::path(),
        entryPointAnalysisPath,
        postPlayPackagePath,
        decisionInputPackagePath,
        candidateDecisionPackagePath,
        dslDraftPath,
        dslDraftAuditPath,
        generatedOverlayStagingStatusPath,
        mpPackageZipPath
    });
    requireCondition(
        staleStageBlocked.postPlayPipeline.state == "needs_attention",
        "stale generated overlay staging must not hide blocked candidate decisions");
    requireCondition(
        staleStageBlocked.postPlayPipeline.reason == "candidate decision package blocked",
        "blocked candidate decisions should win over stale downstream staging");
    requireCondition(
        staleStageBlocked.statusCenter.state == "attention_required",
        "status center should surface blocked candidate decisions even if stale staging exists");
    requireCondition(
        staleStageBlocked.statusCenter.reason == "post-play pipeline needs attention",
        "status center should attribute stale-stage regression to post-play pipeline");
    requireCondition(
        staleStageBlocked.statusCenter.path == candidateDecisionPackagePath,
        "status center should focus the blocked candidate decision package path");
    requireCondition(
        staleStageBlocked.statusCenterSummaryText.find("candidate_decision_package_readiness: blocked") !=
            std::string::npos,
        "status center summary should expose blocked candidate decision readiness");
    requireCondition(
        staleStageBlocked.statusCenterSummaryText.find("generated_overlay_staging_readiness: staged_verified") !=
            std::string::npos,
        "status center summary should still expose stale staged overlay evidence for debugging");
    requireCondition(
        writeTextFileAtomically(
            candidateDecisionPackagePath,
            "{\n"
            "  \"schema_version\": 1,\n"
            "  \"reason\": \"candidate decision package built; some source entries remain blocked\",\n"
            "  \"readiness\": \"ready\",\n"
            "  \"candidate_decision_count\": 2,\n"
            "  \"blocked_source_entry_count\": 1,\n"
            "  \"validator_passed\": true\n"
            "}\n"),
        "stale-stage regression fixture should restore ready candidate decision package");
    requireCondition(ready.mpOverlayPackage.campaignId == "campaign_mp_001", "mp overlay package should expose campaign id");
    requireCondition(ready.mpOverlayPackage.overlayVersion == "overlay_v1", "mp overlay package should expose overlay version");
    requireCondition(ready.mpOverlayPackage.gameVersion == "stellaris_4.x", "mp overlay package should expose game version");
    requireCondition(ready.mpOverlayPackage.strategicNexusModVersion == "strategic_nexus_v0", "mp overlay package should expose mod version");
    requireCondition(ready.mpOverlayPackage.handoffStatus == "degraded_previous_host_unavailable", "mp overlay package should expose handoff status");
    requireCondition(ready.mpOverlayPackage.readiness == "ready_for_mp", "mp overlay package should expose MP readiness");
    requireCondition(ready.mpOverlayPackage.hostReadiness == "ready_for_mp", "mp overlay package should expose host readiness");
    requireCondition(
        ready.mpOverlayPackage.clientReadinessGate == "import_and_verify_before_join",
        "mp overlay package should expose client readiness gate");
    requireCondition(
        ready.mpOverlayPackage.hostNextStep == "share this package and package_manifest_hash with every joining player",
        "mp overlay package should expose host next step");
    requireCondition(
        ready.mpOverlayPackage.clientNextStep == "import package, verify package_manifest_hash, then join lobby",
        "mp overlay package should expose client next step");
    requireCondition(!ready.mpOverlayPackage.packageManifestHash.empty(), "mp overlay package should expose package manifest hash");
    requireCondition(ready.mpOverlayPackage.provenanceState == "present", "mp overlay package should expose provenance state");
    requireCondition(
        ready.mpOverlayPackage.sourceQualities.size() == 2 &&
            ready.mpOverlayPackage.sourceQualities.front() == "history_backed" &&
            ready.mpOverlayPackage.sourceQualities.back() == "zero_history_bootstrap",
        "mp overlay package should expose source provenance qualities");
    requireCondition(
        ready.mpOverlayPackage.bootstrapCampaignCount == 1,
        "mp overlay package should expose bootstrap provenance count");
    requireCondition(
        ready.mpOverlayPackage.verifyCommand.find("Strategic Nexus.exe --verify-mp-overlay-package ") == 0,
        "mp overlay package should expose verify command");
    requireCondition(
        ready.mpOverlayPackage.importCommand.find("Strategic Nexus.exe --import-mp-overlay-package ") == 0,
        "mp overlay package should expose import command");
    requireCondition(
        ready.mpOverlayPackage.strictVerifyCommand.find("Strategic Nexus.exe --verify-mp-overlay-package ") == 0,
        "mp overlay package should expose strict verify command");
    requireCondition(
        ready.mpOverlayPackage.strictImportCommand.find("Strategic Nexus.exe --import-mp-overlay-package ") == 0,
        "mp overlay package should expose strict import command");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("campaign_id: campaign_mp_001") != std::string::npos,
        "mp overlay package should include copyable status text");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("package_manifest_hash: " + ready.mpOverlayPackage.packageManifestHash) != std::string::npos,
        "mp overlay package status text should include package manifest hash");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("provenance_state: present") != std::string::npos,
        "mp overlay package status text should include provenance state");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("source_quality: zero_history_bootstrap") != std::string::npos,
        "mp overlay package status text should include zero-history bootstrap provenance");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("bootstrap_campaign_count: 1") != std::string::npos,
        "mp overlay package status text should include bootstrap provenance count");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("readiness: ready_for_mp") != std::string::npos,
        "mp overlay package status text should include readiness");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("host_readiness: ready_for_mp") != std::string::npos,
        "mp overlay package status text should include host readiness");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("client_readiness_gate: import_and_verify_before_join") != std::string::npos,
        "mp overlay package status text should include client readiness gate");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("strict_verify_command: Strategic Nexus.exe --verify-mp-overlay-package ") != std::string::npos,
        "mp overlay package status text should include strict verify command");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("strict_import_command: Strategic Nexus.exe --import-mp-overlay-package ") != std::string::npos,
        "mp overlay package status text should include strict import command");
    requireCondition(
        ready.mpOverlayPackage.statusText.find("human_control_guard: runtime_is_ai_yes") != std::string::npos,
        "mp overlay package status text should expose the runtime human-control guard");
    requireCondition(
        ready.mpOverlayPackage.humanControlGuardState == "runtime_is_ai_yes",
        "mp overlay package should expose the runtime human-control guard state");
    requireCondition(
        ready.mpOverlayPackage.previousHostAvailableKnown,
        "mp overlay package should expose whether previous-host continuity is known");
    requireCondition(
        !ready.mpOverlayPackage.previousHostAvailable,
        "mp overlay package should expose degraded previous-host continuity explicitly");
    requireCondition(
        ready.nextAction == "review_mp_handoff_continuity",
        "ready snapshot should prioritize degraded MP handoff continuity as next action");
    requireCondition(
        ready.nextActionReason == "mp_handoff_degraded_previous_host_unavailable",
        "ready snapshot should expose degraded MP handoff next-action reason");
    requireCondition(
        ready.nextActionCommandHint.find("Strategic Nexus.exe --verify-mp-overlay-package ") == 0,
        "ready snapshot should expose a verify-oriented command hint for degraded MP handoff");
    requireCondition(
        ready.nextActionCommandHintSource == "mp_overlay_package_strict_verify_command",
        "ready snapshot should expose strict verify as the degraded-handoff command source");
    requireCondition(
        ready.nextActionPath == mpPackageRoot,
        "ready snapshot should focus the MP package path for degraded handoff follow-up");
    requireCondition(ready.statusCenter.state == "starting", "status center should start when any subsystem is starting");
    requireCondition(
        ready.statusCenterSummaryText.find("startup_lifecycle_state: owner_enabled_start_with_windows") !=
            std::string::npos,
        "status center summary should expose start-with-Windows lifecycle state");
    requireCondition(
        ready.statusCenterSummaryText.find("startup_start_with_windows_enabled: true") !=
            std::string::npos,
        "status center summary should expose start-with-Windows enabled flag");
    requireCondition(
        ready.statusCenterSummaryText.find("startup_start_with_windows_source: config_override") !=
            std::string::npos,
        "status center summary should expose start-with-Windows source");
    requireCondition(
        ready.statusCenterSummaryText.find("startup_start_with_windows_shortcut_state: override_enabled") !=
            std::string::npos,
        "status center summary should expose startup shortcut state");
    requireCondition(
        ready.statusCenterSummaryText.find(
            "startup_start_with_windows_shortcut_path: " +
            readyConfig.startWithWindowsShortcutPath.generic_string()) != std::string::npos,
        "status center summary should expose startup shortcut path");
    requireCondition(
        ready.statusCenterSummaryText.find(
            "startup_start_with_windows_command_hint: cmd /c tools\\remove_snc_tray_startup_shortcut.cmd") !=
            std::string::npos,
        "status center summary should expose startup command hint");
    requireCondition(
        ready.statusCenterSummaryText.find(
            "startup_start_with_windows_command_hint_source: startup_shortcut_remove_command") !=
            std::string::npos,
        "status center summary should expose startup command hint source");
    requireCondition(
        ready.statusCenterSummaryText.find("support_report_state: not_prepared") != std::string::npos,
        "status center summary should expose support report state");
    requireCondition(
        ready.statusCenterSummaryText.find(
            "support_report_preview_path: " + readyConfig.supportReportPreviewPath.generic_string()) !=
            std::string::npos,
        "status center summary should expose support report preview path");
    requireCondition(
        ready.statusCenterSummaryText.find("support_report_contact_email: support@jeniksoft.cz") !=
            std::string::npos,
        "status center summary should expose support contact email");
    requireCondition(
        ready.statusCenterSummaryText.find("support_report_send_requires_approval: true") !=
            std::string::npos,
        "status center summary should expose explicit approval requirement");
    requireCondition(
        ready.statusCenterSummaryText.find("support_report_raw_saves_included: false") !=
            std::string::npos,
        "status center summary should expose raw-save exclusion");
    requireCondition(
        ready.statusCenterSummaryText.find(
            "support_report_prepare_command_hint: powershell -NoProfile -ExecutionPolicy Bypass -File tools\\prepare_snc_support_report.ps1") !=
            std::string::npos,
        "status center summary should expose support report prepare command");
    requireCondition(
        ready.statusCenterSummaryText.find("crash_recovery_state: no_recent_unexpected_crash") !=
            std::string::npos,
        "status center summary should expose crash recovery baseline state");
    requireCondition(
        ready.statusCenterSummaryText.find("crash_recovery_reason: no crash recovery record present") !=
            std::string::npos,
        "status center summary should expose crash recovery baseline reason");
    requireCondition(
        ready.statusCenterSummaryText.find(
            "crash_recovery_state_path: " + crashRecoveryStatePath.generic_string()) != std::string::npos,
        "status center summary should expose crash recovery state path");
    requireCondition(
        ready.statusCenterSummaryText.find("crash_recovery_restart_budget_exhausted: false") !=
            std::string::npos,
        "status center summary should expose crash recovery budget state");
    requireCondition(
        ready.statusCenterSummaryText.find("crash_recovery_warning_visible: false") !=
            std::string::npos,
        "status center summary should expose crash recovery warning visibility");
    requireCondition(
        ready.statusCenterSummaryText.find("crash_recovery_support_report_recommended: false") !=
            std::string::npos,
        "status center summary should expose crash recovery support-report recommendation state");
    requireCondition(
        ready.statusCenterSummaryText.find("stav: starting - waiting for archive to become ready") != std::string::npos,
        "status center summary should include overall state");
    requireCondition(
        ready.statusCenterSummaryText.find("archiv: starting - no archive sessions yet") != std::string::npos,
        "status center summary should include archive state");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_overlay_balicek: ready - mp overlay package verified") != std::string::npos,
        "status center summary should include mp overlay package state");
    requireCondition(
        ready.statusCenterSummaryText.find("human_control_guard: runtime_is_ai_yes") != std::string::npos,
        "status center summary should include the runtime human-control guard");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_previous_host_available: false") != std::string::npos,
        "status center summary should include explicit previous-host continuity state");
    const auto readyJson = strategic_nexus::serializeCompanionStatusSnapshot(ready);
    requireCondition(
        readyJson.find("\"human_control_guard_state\": \"runtime_is_ai_yes\"") != std::string::npos,
        "snapshot JSON should expose the runtime human-control guard state");
    requireCondition(
        readyJson.find("\"post_play_player_country_id\": \"0\"") != std::string::npos,
        "snapshot JSON should expose the post-play player-country marker");
    requireCondition(
        readyJson.find("\"memory_recovery\": {") != std::string::npos,
        "snapshot JSON should expose the memory recovery object");
    requireCondition(
        readyJson.find("\"state\": \"ready\"") != std::string::npos &&
            readyJson.find("\"anchor_entry_point_id\": \"beta_ironman_2200_05_01\"") != std::string::npos,
        "snapshot JSON should expose the selected memory recovery anchor");
    requireCondition(
        readyJson.find("\"confidence\": \"high\"") != std::string::npos &&
            readyJson.find("\"warning_visible\": false") != std::string::npos,
        "snapshot JSON should expose high-confidence memory recovery");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_package_zip_state: ready") != std::string::npos,
        "status center summary should include MP package zip state");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_package_zip_reason: mp overlay package zip ready for handoff") != std::string::npos,
        "status center summary should include MP package zip reason");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_package_zip_path: " + mpPackageZipPath.generic_string()) != std::string::npos,
        "status center summary should include MP package zip path");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_package_zip_hash: " + ready.mpOverlayPackage.packageZipHash) != std::string::npos,
        "status center summary should include MP package zip hash");
    requireCondition(
        ready.statusCenterSummaryText.find("post_play_pipeline: ready - generated overlay staging verified") != std::string::npos,
        "status center summary should include post-play pipeline state");
    requireCondition(
        ready.statusCenterSummaryText.find("post_play_player_country_id: 0") != std::string::npos,
        "status center summary should expose the post-play player-country marker");
    requireCondition(
        ready.statusCenterSummaryText.find("entry_point_count: 3") != std::string::npos,
        "status center summary should include entry point count");
    requireCondition(
        ready.statusCenterSummaryText.find("entry_point_reason: entry points scanned") != std::string::npos,
        "status center summary should include entry point reason");
    requireCondition(
        ready.statusCenterSummaryText.find("memory_recovery: ready - latest loadable save selected as recovery anchor") !=
            std::string::npos,
        "status center summary should include memory recovery state");
    requireCondition(
        ready.statusCenterSummaryText.find("memory_recovery_confidence: high") != std::string::npos,
        "status center summary should include memory recovery confidence");
    requireCondition(
        ready.statusCenterSummaryText.find("memory_recovery_warning_visible: false") != std::string::npos,
        "status center summary should include memory recovery warning visibility");
    requireCondition(
        ready.statusCenterSummaryText.find("memory_recovery_anchor_entry_point_id: beta_ironman_2200_05_01") !=
            std::string::npos,
        "status center summary should include memory recovery anchor entry point id");
    requireCondition(
        ready.statusCenterSummaryText.find("memory_recovery_anchor_save_name: ironman.sav") != std::string::npos,
        "status center summary should include memory recovery anchor save name");
    requireCondition(
        ready.statusCenterSummaryText.find("memory_recovery_anchor_source_kind: ironman") != std::string::npos,
        "status center summary should include memory recovery anchor source kind");
    requireCondition(
        ready.statusCenterSummaryText.find("memory_recovery_anchor_path: current_save_root/Beta/ironman.sav") !=
            std::string::npos,
        "status center summary should include memory recovery anchor path");
    requireCondition(
        ready.statusCenterSummaryText.find("post_play_campaign_count: 2") != std::string::npos,
        "status center summary should include post-play campaign count");
    requireCondition(
        ready.statusCenterSummaryText.find("post_play_campaign_summary: alpha_campaign: ready_partial (1/2 ready)") != std::string::npos,
        "status center summary should include campaign readiness summary");
    requireCondition(
        ready.statusCenterSummaryText.find(
            "campaign_library_owner_note: active generated campaign library fits within the configured limit") !=
            std::string::npos,
        "status center summary should mention healthy campaign library limit state");
    requireCondition(
        ready.statusCenterSummaryText.find("campaign_library_limit_reached: false") != std::string::npos,
        "status center summary should expose non-saturated campaign library state");
    requireCondition(
        ready.statusCenterSummaryText.find("decision_input_count: 2") != std::string::npos,
        "status center summary should include decision input count");
    requireCondition(
        ready.statusCenterSummaryText.find(
            "decision_input_package_reason: decision input package built; some entries remain blocked") !=
            std::string::npos,
        "status center summary should include decision input package reason");
    requireCondition(
        ready.statusCenterSummaryText.find("decision_input_blocked_entry_count: 1") != std::string::npos,
        "status center summary should include blocked decision input count");
    requireCondition(
        ready.statusCenterSummaryText.find("candidate_decision_validator_passed: true") != std::string::npos,
        "status center summary should include candidate validator result");
    requireCondition(
        ready.statusCenterSummaryText.find(
            "candidate_decision_package_reason: candidate decision package built; some source entries remain blocked") !=
            std::string::npos,
        "status center summary should include candidate decision package reason");
    requireCondition(
        ready.statusCenterSummaryText.find("dsl_draft_readiness: ready") != std::string::npos,
        "status center summary should include DSL draft readiness");
    requireCondition(
        ready.statusCenterSummaryText.find("dsl_draft_reason: validated dry-run DSL draft built") !=
            std::string::npos,
        "status center summary should include DSL draft reason");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_staging_readiness: staged_verified") != std::string::npos,
        "status center summary should include generated overlay staging readiness");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_staging_reason: validated generated overlay staged") !=
            std::string::npos,
        "status center summary should include generated overlay staging reason");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_manifest_verified: true") != std::string::npos,
        "status center summary should include generated overlay staging manifest verification");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_publish_allowed: false") != std::string::npos,
        "status center summary should include generated overlay staging publish gate state");
    requireCondition(
        ready.statusCenterSummaryText.find("publish_gate: ready - Stellaris is not running; generated overlay publish allowed") != std::string::npos,
        "status center summary should include publish gate state");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_manifest_hash: " + ready.generatedOverlay.manifestHash) != std::string::npos,
        "status center summary should include generated overlay manifest hash");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_reactive_capability: event_family_dispatch") != std::string::npos,
        "status center summary should include reactive capability");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_event_families: monthly_strategy_tick") != std::string::npos,
        "status center summary should include event-family coverage");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_source_qualities: history_backed,zero_history_bootstrap") != std::string::npos,
        "status center summary should include source qualities");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_bootstrap_campaign_count: 1") != std::string::npos,
        "status center summary should include bootstrap provenance count");
    requireCondition(
        ready.statusCenterSummaryText.find("campaign_id: campaign_mp_001") != std::string::npos,
        "status center summary should include MP campaign id");
    requireCondition(
        ready.statusCenterSummaryText.find("package_manifest_hash: " + ready.mpOverlayPackage.packageManifestHash) != std::string::npos,
        "status center summary should include MP package manifest hash");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_provenance_state: present") != std::string::npos,
        "status center summary should include MP provenance state");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_source_quality: zero_history_bootstrap") != std::string::npos,
        "status center summary should include zero-history MP provenance");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_bootstrap_campaign_count: 1") != std::string::npos,
        "status center summary should include MP bootstrap provenance count");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_verify_command: " + ready.mpOverlayPackage.verifyCommand) != std::string::npos,
        "status center summary should include MP verify command");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_import_command: " + ready.mpOverlayPackage.importCommand) != std::string::npos,
        "status center summary should include MP import command");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_strict_verify_command: " + ready.mpOverlayPackage.strictVerifyCommand) != std::string::npos,
        "status center summary should include MP strict verify command");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_strict_import_command: " + ready.mpOverlayPackage.strictImportCommand) != std::string::npos,
        "status center summary should include MP strict import command");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_readiness: ready_for_mp") != std::string::npos,
        "status center summary should include MP readiness");
    requireCondition(
        ready.statusCenterSummaryText.find("host_readiness: ready_for_mp") != std::string::npos,
        "status center summary should include host readiness text");
    requireCondition(
        ready.statusCenterSummaryText.find("client_readiness_gate: import_and_verify_before_join") != std::string::npos,
        "status center summary should include client readiness gate text");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_host_next_step: share this package and package_manifest_hash with every joining player") != std::string::npos,
        "status center summary should include host next step");
    requireCondition(
        ready.statusCenterSummaryText.find("mp_client_next_step: import package, verify package_manifest_hash, then join lobby") != std::string::npos,
        "status center summary should include client next step");
    requireCondition(
        ready.statusCenterSummaryText.find("strict_verify_command: Strategic Nexus.exe --verify-mp-overlay-package ") != std::string::npos,
        "status center summary should include strict verify command text");
    requireCondition(
        ready.statusCenterSummaryText.find("strict_import_command: Strategic Nexus.exe --import-mp-overlay-package ") != std::string::npos,
        "status center summary should include strict import command text");
    requireCondition(
        ready.statusCenterSummaryText.find("gameplay_acceptance: starting - gameplay acceptance pending") != std::string::npos,
        "status center summary should include gameplay acceptance pending state");
    requireCondition(
        ready.statusCenterSummaryText.find("next_action: review_mp_handoff_continuity") != std::string::npos,
        "status center summary should include next action");
    requireCondition(
        ready.statusCenterSummaryText.find("next_action_command_hint_source: mp_overlay_package_strict_verify_command") != std::string::npos,
        "status center summary should include next action command hint source");

    const auto emptyOverlay = companion.buildStatusSnapshot({
        archiveRoot,
        overlayEmptyRoot,
        std::filesystem::path(),
        false,
        false,
        false,
        missingGameplayAcceptanceReport
    });
    requireCondition(emptyOverlay.generatedOverlay.state == "starting", "empty overlay should be starting");
    requireCondition(emptyOverlay.generatedOverlayPublishGate.state == "starting", "publish gate should wait for generated overlay");
    requireCondition(emptyOverlay.statusCenter.state == "starting", "status center should start when overlay is starting");
    requireCondition(emptyOverlay.statusCenter.reason == "waiting for archive, generated overlay, and publish gate to become ready", "status center reason should name the waiting subsystems");
    requireCondition(emptyOverlay.statusCenter.path.empty(), "status center path should remain empty when multiple subsystems block readiness");

    const auto missingOverlay = companion.buildStatusSnapshot({
        archiveRoot,
        root / "missing_overlay",
        std::filesystem::path(),
        false,
        false,
        false,
        missingGameplayAcceptanceReport
    });
    requireCondition(missingOverlay.generatedOverlay.state == "needs_attention", "missing overlay should need attention");
    requireCondition(missingOverlay.statusCenter.state == "attention_required", "status center should surface attention");
    requireCondition(missingOverlay.statusCenter.reason == "generated overlay needs attention", "status center reason should name the subsystem needing attention");
    requireCondition(missingOverlay.statusCenter.path == (root / "missing_overlay"), "status center path should point to the subsystem needing attention");

    const auto noManifestNonEmptyOverlay = companion.buildStatusSnapshot({
        archiveRoot,
        overlayNoManifestNonEmptyRoot,
        std::filesystem::path(),
        false,
        false,
        false,
        missingGameplayAcceptanceReport
    });
    requireCondition(noManifestNonEmptyOverlay.generatedOverlay.state == "needs_attention", "non-empty overlay without manifest should need attention");
    requireCondition(noManifestNonEmptyOverlay.statusCenter.state == "attention_required", "status center should surface overlay attention");
    requireCondition(noManifestNonEmptyOverlay.statusCenter.reason == "generated overlay needs attention", "status center reason should name the subsystem needing attention");
    requireCondition(noManifestNonEmptyOverlay.statusCenter.path == overlayNoManifestNonEmptyRoot, "status center path should point to the overlay needing attention");

    const auto archiveSessionRoot = root / "archive_session";
    std::filesystem::create_directories(archiveSessionRoot);
    {
        std::ofstream manifest(archiveSessionRoot / "manifest.json", std::ios::binary);
        manifest << "{\n"
                 << "  \"schema_version\": 1,\n"
                 << "  \"entries\": []\n"
                 << "}\n";
    }

    const auto directArchiveSession = companion.buildStatusSnapshot({
        archiveSessionRoot,
        overlayRoot,
        std::filesystem::path(),
        true,
        false,
        false,
        missingGameplayAcceptanceReport
    });
    requireCondition(directArchiveSession.archive.state == "ready", "archive session root with manifest should be ready");
    requireCondition(
        directArchiveSession.statusCenter.state == "starting",
        "status center should wait when gameplay acceptance evidence is still missing");
    requireCondition(
        directArchiveSession.statusCenter.reason == "waiting for gameplay acceptance",
        "status center should explicitly call out missing gameplay acceptance evidence");

    const auto stagedOverlayRoot = root / "staged_generated_overlay";
    const auto activeOverlayRoot = root / "active_generated_overlay";
    const auto stagedOverlayStatusPath = root / "snc_generated_overlay_staging_status.json";
    const auto publishStatusPath = root / "snc_generated_overlay_publish_status.json";
    const auto publishBackupRoot = root / "snc_generated_overlay_backups";
    std::filesystem::copy(overlayRoot, stagedOverlayRoot, std::filesystem::copy_options::recursive);
    std::filesystem::copy(overlayRoot, activeOverlayRoot, std::filesystem::copy_options::recursive);
    writeTextFileAtomically(
        root / "strategic_nexus_campaign_library_plan.json",
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"save_root_available\": true,\n"
        "  \"limit_reached\": true,\n"
        "  \"max_included_campaigns\": 1,\n"
        "  \"included_count\": 1,\n"
        "  \"skipped_count\": 2,\n"
        "  \"skipped_due_to_limit_count\": 2,\n"
        "  \"campaigns\": []\n"
        "}\n");
    writeTextFileAtomically(
        stagedOverlayStatusPath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"ok\": true,\n"
        "  \"reason\": \"validated generated overlay staged\",\n"
        "  \"readiness\": \"staged_verified\",\n"
        "  \"dry_run_only\": true,\n"
        "  \"publish_allowed\": false,\n"
        "  \"publishes_overlay\": false,\n"
        "  \"staged_overlay_directory\": \"" + stagedOverlayRoot.generic_string() + "\",\n"
        "  \"dsl_rule_count\": 10,\n"
        "  \"manifest_verified\": true,\n"
        "  \"manifest_hash\": \"" + directArchiveSession.generatedOverlay.manifestHash + "\"\n"
        "}\n");
    const auto stagedPublishReady = companion.buildStatusSnapshot({
        archiveSessionRoot,
        activeOverlayRoot,
        std::filesystem::path(),
        true,
        false,
        false,
        missingGameplayAcceptanceReport,
        stagedOverlayStatusPath,
        activeOverlayRoot,
        publishStatusPath,
        publishBackupRoot,
        entryPointAnalysisPath,
        postPlayPackagePath,
        decisionInputPackagePath,
        candidateDecisionPackagePath,
        dslDraftPath,
        dslDraftAuditPath,
        stagedOverlayStatusPath
    });
    requireCondition(
        stagedPublishReady.generatedOverlayPublishGate.state == "ready",
        "staged publish gate should be ready when Stellaris is not running");
    requireCondition(
        stagedPublishReady.generatedOverlayPublishGate.reason ==
            "staged generated overlay ready; owner approval required before publish",
        "staged publish gate should explain owner approval requirement");
    requireCondition(
        stagedPublishReady.generatedOverlayPublishGate.stagingStatusPath == stagedOverlayStatusPath,
        "staged publish gate should expose staging status path");
    requireCondition(
        stagedPublishReady.generatedOverlayPublishGate.stagedOverlayDirectory == stagedOverlayRoot,
        "staged publish gate should expose staged overlay directory");
    requireCondition(
        stagedPublishReady.generatedOverlayPublishGate.activeOverlayDirectory == activeOverlayRoot,
        "staged publish gate should expose active overlay directory");
    requireCondition(
        stagedPublishReady.generatedOverlayPublishGate.publishStatusPath == publishStatusPath,
        "staged publish gate should expose publish status output path");
    requireCondition(
        stagedPublishReady.generatedOverlayPublishGate.backupRootDirectory == publishBackupRoot,
        "staged publish gate should expose backup root");
    requireCondition(
        stagedPublishReady.generatedOverlayPublishGate.manifestHash == directArchiveSession.generatedOverlay.manifestHash,
        "staged publish gate should expose manifest hash");
    requireCondition(stagedPublishReady.generatedOverlayPublishGate.dslRuleCount == 10, "staged publish gate should expose rule count");
    requireCondition(stagedPublishReady.generatedOverlayPublishGate.ownerApprovalRequired, "staged publish gate should require owner approval");
    requireCondition(stagedPublishReady.generatedOverlayPublishGate.canPublish, "staged publish gate should expose publish readiness");
    requireCondition(stagedPublishReady.generatedOverlayPublishGate.activeOverlayExists, "staged publish gate should detect existing active overlay");
    requireCondition(stagedPublishReady.generatedOverlayPublishGate.backupBeforeReplace, "staged publish gate should plan backup before replace");
    requireCondition(
        stagedPublishReady.nextAction == "review_staged_overlay_and_publish_if_desired",
        "staged publish-ready snapshot should prioritize publish review as next action");
    requireCondition(
        stagedPublishReady.nextActionReason == "staged_overlay_ready_owner_gate_available",
        "staged publish-ready snapshot should expose publish next action reason");
    requireCondition(
        stagedPublishReady.nextActionCommandHint == stagedPublishReady.generatedOverlayPublishGate.publishCommand,
        "staged publish-ready snapshot should expose publish command hint");
    requireCondition(
        stagedPublishReady.nextActionCommandHintSource == "generated_overlay_publish_gate_publish_command",
        "staged publish-ready snapshot should expose publish command hint source");
    requireCondition(
        stagedPublishReady.nextActionPath == stagedOverlayStatusPath,
        "staged publish-ready snapshot should focus the staging status path");
    requireCondition(
        stagedPublishReady.generatedOverlayPublishGate.publishCommand.find(
            "Strategic Nexus.exe --publish-snc-generated-overlay ") == 0,
        "staged publish gate should expose owner-approved publish command");
    requireCondition(
        stagedPublishReady.statusCenterSummaryText.find("publish_gate_owner_approval_required: true") != std::string::npos,
        "status center summary should expose owner approval requirement");
    requireCondition(
        stagedPublishReady.statusCenterSummaryText.find("publish_gate_can_publish: true") != std::string::npos,
        "status center summary should expose can_publish");
    requireCondition(
        stagedPublishReady.statusCenterSummaryText.find("publish_gate_backup_before_replace: true") != std::string::npos,
        "status center summary should expose backup-before-replace state");
    requireCondition(
        stagedPublishReady.statusCenterSummaryText.find("publish_gate_command: Strategic Nexus.exe --publish-snc-generated-overlay ") != std::string::npos,
        "status center summary should expose publish command");
    requireCondition(
        stagedPublishReady.statusCenterSummaryText.find("next_action: review_staged_overlay_and_publish_if_desired") != std::string::npos,
        "status center summary should expose publish next action");
    requireCondition(
        stagedPublishReady.statusCenterSummaryText.find("campaign_library_plan_source: post_play_status_directory") !=
            std::string::npos,
        "status center summary should expose campaign library plan source");
    requireCondition(
        stagedPublishReady.statusCenterSummaryText.find("campaign_library_limit_reached: true") != std::string::npos,
        "status center summary should expose saturated campaign library state");
    requireCondition(
        stagedPublishReady.statusCenterSummaryText.find(
            "campaign_library_owner_note: active generated campaign library is truncated by the configured limit; raise the cap or clean local campaigns before broader coverage tests") !=
            std::string::npos,
        "status center summary should explain truncated campaign library follow-up");

    const auto stagedPublishBlocked = companion.buildStatusSnapshot({
        archiveSessionRoot,
        activeOverlayRoot,
        std::filesystem::path(),
        true,
        false,
        true,
        missingGameplayAcceptanceReport,
        stagedOverlayStatusPath,
        activeOverlayRoot,
        publishStatusPath,
        publishBackupRoot
    });
    requireCondition(stagedPublishBlocked.generatedOverlayPublishGate.state == "blocked", "staged publish gate should block while Stellaris is running");
    requireCondition(!stagedPublishBlocked.generatedOverlayPublishGate.canPublish, "blocked staged publish gate should not expose can_publish");
    requireCondition(stagedPublishBlocked.generatedOverlayPublishGate.publishCommand.empty(), "blocked staged publish gate should not expose publish command");

    const auto blockedPublishGate = companion.buildStatusSnapshot({
        archiveSessionRoot,
        overlayRoot,
        std::filesystem::path(),
        true,
        false,
        true,
        missingGameplayAcceptanceReport
    });
    requireCondition(blockedPublishGate.generatedOverlayPublishGate.state == "blocked", "publish gate should block while Stellaris is running");
    requireCondition(
        blockedPublishGate.generatedOverlayPublishGate.reason == "Stellaris is running; generated overlay publish deferred",
        "publish gate should explain Stellaris-running block");
    requireCondition(blockedPublishGate.statusCenter.state == "waiting", "status center should wait while publish is blocked by running Stellaris");
    requireCondition(
        blockedPublishGate.statusCenter.reason == "Stellaris is running; generated overlay publish deferred",
        "status center should surface publish gate block reason");

    const auto publishedBackupDirectory = publishBackupRoot / "snc_generated_overlay_active_20260603T153400000";
    requireCondition(
        writeTextFileAtomically(
            publishStatusPath,
            "{\n"
            "  \"schema_version\": 1,\n"
            "  \"ok\": true,\n"
            "  \"reason\": \"owner approved generated overlay published\",\n"
            "  \"readiness\": \"published\",\n"
            "  \"owner_approved\": true,\n"
            "  \"published\": true,\n"
            "  \"backup_created\": true,\n"
            "  \"stellaris_running\": false,\n"
            "  \"staging_status_path\": \"" + stagedOverlayStatusPath.generic_string() + "\",\n"
            "  \"staged_overlay_directory\": \"" + stagedOverlayRoot.generic_string() + "\",\n"
            "  \"active_overlay_directory\": \"" + activeOverlayRoot.generic_string() + "\",\n"
            "  \"backup_directory\": \"" + publishedBackupDirectory.generic_string() + "\",\n"
            "  \"source_manifest_hash\": \"" + directArchiveSession.generatedOverlay.manifestHash + "\",\n"
            "  \"published_manifest_hash\": \"" + directArchiveSession.generatedOverlay.manifestHash + "\",\n"
            "  \"published_file_count\": 3,\n"
            "  \"warning_codes\": [],\n"
            "  \"validation_errors\": []\n"
            "}\n"),
        "publish gate fixture should write publish status");

    const auto publishedSnapshot = companion.buildStatusSnapshot({
        archiveSessionRoot,
        activeOverlayRoot,
        std::filesystem::path(),
        true,
        false,
        false,
        missingGameplayAcceptanceReport,
        stagedOverlayStatusPath,
        activeOverlayRoot,
        publishStatusPath,
        publishBackupRoot
    });
    requireCondition(
        publishedSnapshot.generatedOverlayPublishGate.state == "published",
        "published snapshot should surface published state");
    requireCondition(
        publishedSnapshot.generatedOverlayPublishGate.reason == "current staged generated overlay already published",
        "published snapshot should explain current staged publish state");
    requireCondition(
        publishedSnapshot.generatedOverlayPublishGate.published,
        "published snapshot should expose published flag");
    requireCondition(
        !publishedSnapshot.generatedOverlayPublishGate.canPublish,
        "published snapshot should not expose publish action for the current staged overlay");
    requireCondition(
        !publishedSnapshot.generatedOverlayPublishGate.ownerApprovalRequired,
        "published snapshot should not keep owner approval pending after current staged publish");
    requireCondition(
        publishedSnapshot.generatedOverlayPublishGate.backupCreated,
        "published snapshot should expose backup creation");
    requireCondition(
        !publishedSnapshot.generatedOverlayPublishGate.backupDirectory.empty(),
        "published snapshot should expose backup directory");
    requireCondition(
        publishedSnapshot.generatedOverlayPublishGate.publishedManifestHash == directArchiveSession.generatedOverlay.manifestHash,
        "published snapshot should expose published manifest hash");
    requireCondition(
        publishedSnapshot.generatedOverlayPublishGate.publishedFileCount == 3,
        "published snapshot should expose published file count");
    requireCondition(
        publishedSnapshot.statusCenterSummaryText.find("publish_gate: published - current staged generated overlay already published") != std::string::npos,
        "status center summary should expose published publish-gate state");
    requireCondition(
        publishedSnapshot.statusCenterSummaryText.find("publish_gate_published: true") != std::string::npos,
        "status center summary should expose published flag");
    requireCondition(
        publishedSnapshot.statusCenterSummaryText.find("publish_gate_backup_created: true") != std::string::npos,
        "status center summary should expose backup-created flag");
    requireCondition(
        publishedSnapshot.statusCenterSummaryText.find("publish_gate_published_file_count: 3") != std::string::npos,
        "status center summary should expose published file count");
    requireCondition(
        publishedSnapshot.statusCenterSummaryText.find("publish_gate_published_manifest_hash: " + directArchiveSession.generatedOverlay.manifestHash) != std::string::npos,
        "status center summary should expose published manifest hash");
    requireCondition(
        publishedSnapshot.statusCenterSummaryText.find("publish_gate_backup_directory: ") != std::string::npos,
        "status center summary should expose backup directory");
    requireCondition(
        publishedSnapshot.nextAction == "review_published_overlay_status",
        "published snapshot without ready acceptance should stay fail-closed on owner test guidance");
    requireCondition(
        publishedSnapshot.statusCenterSummaryText.find("owner_test_contract_state: ready_for_monthly_reactive_session_test") ==
            std::string::npos,
        "published snapshot without ready acceptance should not advertise owner test contract");
    requireCondition(
        publishedSnapshot.statusCenterSummaryText.find("owner_test_playbook_path: docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md") ==
            std::string::npos,
        "published snapshot without ready acceptance should keep the owner-test playbook path hidden");
    requireCondition(
        publishedSnapshot.ownerTestPlaybookPath.empty(),
        "published snapshot without ready acceptance should keep owner test playbook field empty");

    const auto publishedOwnerTestReady = companion.buildStatusSnapshot({
        archiveSessionRoot,
        activeOverlayRoot,
        std::filesystem::path(),
        true,
        false,
        false,
        readyGameplayAcceptanceReport,
        stagedOverlayStatusPath,
        activeOverlayRoot,
        publishStatusPath,
        publishBackupRoot
    });
    requireCondition(
        publishedOwnerTestReady.nextAction == "run_monthly_reactive_owner_test",
        "published reactive overlay with verified acceptance should advertise owner test");
    requireCondition(
        publishedOwnerTestReady.nextActionReason == "published_monthly_reactive_overlay_ready_for_owner_test",
        "published reactive owner test should expose stable next-action reason");
    requireCondition(
        publishedOwnerTestReady.nextActionPath == readyGameplayAcceptanceReport,
        "published reactive owner test should focus the acceptance contract artifact");
    requireCondition(
        publishedOwnerTestReady.ownerTestPlaybookPath == std::filesystem::path("docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md"),
        "published reactive owner test should expose stable owner test playbook field");
    requireCondition(
        publishedOwnerTestReady.statusCenterSummaryText.find(
            "owner_test_contract_state: ready_for_monthly_reactive_session_test") != std::string::npos,
        "status center summary should expose owner test readiness");
    requireCondition(
        publishedOwnerTestReady.statusCenterSummaryText.find(
            "owner_test_scope: load_or_resume_a_real_non_ironman_session_with_the_current_published_overlay_and_wait_for_the_next_monthly_pulse") !=
            std::string::npos,
        "status center summary should describe the monthly reactive test scope");
    requireCondition(
        publishedOwnerTestReady.statusCenterSummaryText.find(
            "owner_test_playbook_path: docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md") != std::string::npos,
        "status center summary should point owner-ready state at the monthly reactive playbook");
    requireCondition(
        publishedOwnerTestReady.statusCenterSummaryText.find(
            "owner_test_visible_markers: Strategic Nexus v0: Defensive military posture|Strategic Nexus v0: Aggressive military posture|Strategic Nexus v0: Economy research bias|Strategic Nexus v0: Military industry research bias") !=
            std::string::npos,
        "status center summary should enumerate visible harmless owner-test markers");
    requireCondition(
        publishedOwnerTestReady.statusCenterSummaryText.find(
            "owner_test_codex_artifacts: generated_overlay_publish_status.json|generated_overlay_gameplay_acceptance_v0.json|Stellaris logs/error.log") !=
            std::string::npos,
        "status center summary should name concrete post-test artifacts for Codex review");
    const auto publishedOwnerTestReadyJson = strategic_nexus::serializeCompanionStatusSnapshot(publishedOwnerTestReady);
    requireCondition(
        publishedOwnerTestReadyJson.find("\"owner_test_playbook_path\": \"docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md\"") !=
            std::string::npos,
        "published reactive owner test JSON should expose stable owner test playbook field");

    const auto ambiguousEntryPointAnalysisPath = root / "snc_entry_point_analysis_ambiguous.json";
    writeTextFileAtomically(
        ambiguousEntryPointAnalysisPath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"reason\": \"entry points scanned; branch ambiguity requires conservative evidence selection\",\n"
        "  \"readiness\": \"ambiguous\",\n"
        "  \"archive_verified\": true,\n"
        "  \"branch_ambiguity_detected\": true,\n"
        "  \"entry_point_count\": 1,\n"
        "  \"archived_evidence_count\": 1,\n"
        "  \"entry_points\": [\n"
        "    {\n"
        "      \"id\": \"beta_ironman_2200_05_01\",\n"
        "      \"campaign_key\": \"beta_campaign\",\n"
        "      \"source_root\": \"current_save_root\",\n"
        "      \"relative_path\": \"Beta/ironman.sav\",\n"
        "      \"source_kind\": \"ironman\",\n"
        "      \"save_name\": \"ironman.sav\",\n"
        "      \"save_date\": \"2200.05.01\",\n"
        "      \"analysis_state\": \"ready_conservative\",\n"
        "      \"compatible_archived_evidence_count\": 1,\n"
        "      \"later_archived_evidence_count\": 1\n"
        "    }\n"
        "  ]\n"
        "}\n");

    strategic_nexus::CompanionStatusConfig ambiguousConfig = readyConfig;
    ambiguousConfig.entryPointAnalysisPath = ambiguousEntryPointAnalysisPath;

    const auto ambiguousRecovery = companion.buildStatusSnapshot(ambiguousConfig);
    requireCondition(
        ambiguousRecovery.postPlayPipeline.memoryRecovery.state == "degraded",
        "branch ambiguity should downgrade memory recovery state");
    requireCondition(
        ambiguousRecovery.postPlayPipeline.memoryRecovery.confidence == "reduced",
        "branch ambiguity should reduce memory recovery confidence");
    requireCondition(
        ambiguousRecovery.postPlayPipeline.memoryRecovery.warningVisible,
        "branch ambiguity should keep memory recovery warning visible");
    requireCondition(
        ambiguousRecovery.postPlayPipeline.memoryRecovery.reason == "branch ambiguity requires conservative recovery anchor",
        "branch ambiguity should expose a conservative recovery reason");
    requireCondition(
        ambiguousRecovery.statusCenterSummaryText.find("memory_recovery: degraded - branch ambiguity requires conservative recovery anchor") !=
            std::string::npos,
        "status center summary should surface the degraded recovery state");
    requireCondition(
        ambiguousRecovery.statusCenterSummaryText.find("memory_recovery_confidence: reduced") != std::string::npos,
        "status center summary should surface reduced memory recovery confidence");
    requireCondition(
        ambiguousRecovery.statusCenterSummaryText.find("memory_recovery_warning_visible: true") != std::string::npos,
        "status center summary should surface the recovery warning");
    requireCondition(
        ambiguousRecovery.statusCenterSummaryText.find("memory_recovery_owner_note: latest loadable save anchor is degraded or needs attention") !=
            std::string::npos,
        "status center summary should keep the degraded recovery note explicit");
    const auto ambiguousRecoveryJson = strategic_nexus::serializeCompanionStatusSnapshot(ambiguousRecovery);
    requireCondition(
        ambiguousRecoveryJson.find("\"memory_recovery\": {") != std::string::npos,
        "snapshot JSON should include the memory recovery object for branch ambiguity");
    requireCondition(
        ambiguousRecoveryJson.find("\"state\": \"degraded\"") != std::string::npos &&
            ambiguousRecoveryJson.find("\"confidence\": \"reduced\"") != std::string::npos &&
            ambiguousRecoveryJson.find("\"warning_visible\": true") != std::string::npos,
        "snapshot JSON should expose degraded branch-aware recovery state");

    const auto missingEntryPointAnalysisPath = root / "snc_entry_point_analysis_missing.json";
    strategic_nexus::CompanionStatusConfig missingAnalysisConfig = readyConfig;
    missingAnalysisConfig.entryPointAnalysisPath = missingEntryPointAnalysisPath;

    const auto missingAnalysisRecovery = companion.buildStatusSnapshot(missingAnalysisConfig);
    requireCondition(
        missingAnalysisRecovery.postPlayPipeline.memoryRecovery.state == "needs_attention",
        "missing entry point analysis should require attention");
    requireCondition(
        missingAnalysisRecovery.postPlayPipeline.memoryRecovery.confidence == "low",
        "missing entry point analysis should keep low recovery confidence");
    requireCondition(
        missingAnalysisRecovery.postPlayPipeline.memoryRecovery.warningVisible,
        "missing entry point analysis should keep the warning visible");
    requireCondition(
        missingAnalysisRecovery.postPlayPipeline.memoryRecovery.reason == "entry point analysis unavailable",
        "missing entry point analysis should expose an unavailable reason");
    requireCondition(
        missingAnalysisRecovery.statusCenterSummaryText.find("memory_recovery: needs_attention - entry point analysis unavailable") !=
            std::string::npos,
        "status center summary should surface missing-analysis attention");
    requireCondition(
        missingAnalysisRecovery.statusCenterSummaryText.find("memory_recovery_confidence: low") != std::string::npos,
        "status center summary should surface low recovery confidence for missing analysis");
    requireCondition(
        missingAnalysisRecovery.statusCenterSummaryText.find("memory_recovery_warning_visible: true") != std::string::npos,
        "status center summary should surface the missing-analysis warning");
    const auto missingAnalysisRecoveryJson = strategic_nexus::serializeCompanionStatusSnapshot(missingAnalysisRecovery);
    requireCondition(
        missingAnalysisRecoveryJson.find("\"state\": \"needs_attention\"") != std::string::npos &&
            missingAnalysisRecoveryJson.find("\"confidence\": \"low\"") != std::string::npos &&
            missingAnalysisRecoveryJson.find("\"warning_visible\": true") != std::string::npos,
        "snapshot JSON should expose fail-closed missing-analysis recovery state");

    const auto missingMpPackage = companion.buildStatusSnapshot({
        archiveSessionRoot,
        overlayRoot,
        root / "missing_mp_overlay_package",
        true,
        false,
        false,
        missingGameplayAcceptanceReport
    });
    requireCondition(missingMpPackage.archive.state == "ready", "archive should remain ready when mp package missing");
    requireCondition(missingMpPackage.generatedOverlay.state == "ready", "overlay should remain ready when mp package missing");
    requireCondition(missingMpPackage.mpOverlayPackage.state == "needs_attention", "missing mp overlay package should need attention");
    requireCondition(
        missingMpPackage.mpOverlayPackage.warningCodes.size() == 1 &&
            missingMpPackage.mpOverlayPackage.warningCodes.front() == "mp_overlay_package_directory_missing",
        "missing mp overlay package should expose structured warning code");
    requireCondition(
        missingMpPackage.mpOverlayPackage.statusText.find("readiness: not_ready") != std::string::npos,
        "missing mp overlay package should expose not_ready status text");
    requireCondition(
        missingMpPackage.mpOverlayPackage.statusText.find("warning_code: mp_overlay_package_directory_missing") != std::string::npos,
        "missing mp overlay package should expose warning code in status text");
    requireCondition(
        missingMpPackage.mpOverlayPackage.statusText.find("verify_command: Strategic Nexus.exe --verify-mp-overlay-package ") != std::string::npos,
        "missing mp overlay package should expose verify command in status text");
    requireCondition(
        missingMpPackage.mpOverlayPackage.statusText.find("import_command: Strategic Nexus.exe --import-mp-overlay-package ") != std::string::npos,
        "missing mp overlay package should expose import command in status text");
    requireCondition(missingMpPackage.statusCenter.state == "attention_required", "status center should surface mp overlay package attention");
    requireCondition(missingMpPackage.statusCenter.reason == "mp overlay package needs attention", "status center reason should name mp overlay package attention");
    requireCondition(missingMpPackage.statusCenter.path == (root / "missing_mp_overlay_package"), "status center path should point to the mp overlay package needing attention");
    requireCondition(
        missingMpPackage.statusCenterSummaryText.find("warning_code: mp_overlay_package_directory_missing") != std::string::npos,
        "status center summary should include MP package warning code");
    requireCondition(
        missingMpPackage.statusCenterSummaryText.find("mp_warning_code: mp_overlay_package_directory_missing") != std::string::npos,
        "status center summary should include structured MP warning code");
    requireCondition(
        missingMpPackage.statusCenterSummaryText.find("mp_warning_count: 1") != std::string::npos,
        "status center summary should include MP warning count");

    writeTextFileAtomically(
        mpPackageRoot / "events/strategic_nexus_generated_events.txt",
        readTextFile(mpPackageRoot / "events/strategic_nexus_generated_events.txt") + "# tamper\n");
    const auto tamperedMpPackage = companion.buildStatusSnapshot({
        archiveSessionRoot,
        overlayRoot,
        mpPackageRoot,
        true,
        false,
        false,
        missingGameplayAcceptanceReport
    });
    requireCondition(tamperedMpPackage.mpOverlayPackage.state == "needs_attention", "tampered mp package should need attention");
    requireCondition(
        std::find(
            tamperedMpPackage.mpOverlayPackage.warningCodes.begin(),
            tamperedMpPackage.mpOverlayPackage.warningCodes.end(),
            "mp_overlay_package_files_mismatch_manifest") != tamperedMpPackage.mpOverlayPackage.warningCodes.end(),
        "tampered mp package should expose manifest mismatch warning code");
    requireCondition(
        tamperedMpPackage.statusCenterSummaryText.find("mp_warning_code: mp_overlay_package_files_mismatch_manifest") != std::string::npos,
        "status center summary should include structured mismatch warning code");
    requireCondition(
        tamperedMpPackage.statusCenterSummaryText.find("mp_warning_count: 1") != std::string::npos,
        "status center summary should include mismatch warning count");
    requireCondition(
        tamperedMpPackage.mpOverlayPackage.identityMismatchWarning,
        "tampered mp package should expose identity mismatch warning");
    requireCondition(
        std::find(
            tamperedMpPackage.mpOverlayPackage.identityMismatchWarningCodes.begin(),
            tamperedMpPackage.mpOverlayPackage.identityMismatchWarningCodes.end(),
            "mp_overlay_package_files_mismatch_manifest") != tamperedMpPackage.mpOverlayPackage.identityMismatchWarningCodes.end(),
        "tampered mp package should expose file-hash mismatch identity code");
    requireCondition(
        tamperedMpPackage.statusCenterSummaryText.find("mp_identity_mismatch_warning: true") != std::string::npos,
        "status center summary should include true identity mismatch warning");
    requireCondition(
        tamperedMpPackage.statusCenterSummaryText.find("mp_identity_mismatch_warning_code: mp_overlay_package_files_mismatch_manifest") != std::string::npos,
        "status center summary should include identity mismatch warning code");
    requireCondition(
        tamperedMpPackage.statusCenterSummaryText.find(
            "mp_identity_mismatch_alert: package identity mismatch detected (campaign/version/mod/hash); run strict verify/import before MP join") !=
            std::string::npos,
        "status center summary should include explicit owner-facing identity mismatch alert");
    requireCondition(
        tamperedMpPackage.mpOverlayPackage.warningCount == 1,
        "tampered mp package should expose structured warning count");
    requireCondition(
        tamperedMpPackage.mpOverlayPackage.mismatchWarningState == "warning",
        "tampered mp package should expose mismatch warning state");
    requireCondition(
        tamperedMpPackage.mpOverlayPackage.mismatchWarningReason == "package_identity_mismatch_detected",
        "tampered mp package should expose mismatch warning reason");
    requireCondition(
        tamperedMpPackage.mpOverlayPackage.identityMismatchAlert ==
            "package identity mismatch detected (campaign/version/mod/hash); run strict verify/import before MP join",
        "tampered mp package should expose explicit identity mismatch alert text");
    requireCondition(
        tamperedMpPackage.mpOverlayPackage.mismatchWarningCodes.size() == 1 &&
            tamperedMpPackage.mpOverlayPackage.mismatchWarningCodes.front() == "mp_overlay_package_files_mismatch_manifest",
        "tampered mp package should expose structured mismatch warning codes");
    requireCondition(
        tamperedMpPackage.nextAction == "review_mp_package_mismatch_warning",
        "tampered mp package should prioritize mismatch review as next action");
    requireCondition(
        tamperedMpPackage.nextActionReason == "package_identity_mismatch_detected",
        "tampered mp package should expose mismatch next action reason");
    requireCondition(
        tamperedMpPackage.nextActionCommandHint == tamperedMpPackage.mpOverlayPackage.strictVerifyCommand,
        "tampered mp package should surface strict verify command as next action hint");
    requireCondition(
        tamperedMpPackage.nextActionCommandHintSource == "mp_overlay_package_strict_verify_command",
        "tampered mp package should expose strict verify command hint source");
    requireCondition(
        tamperedMpPackage.nextActionPath == mpPackageRoot,
        "tampered mp package should focus the mp package directory");
    requireCondition(
        tamperedMpPackage.statusCenterSummaryText.find("next_action: review_mp_package_mismatch_warning") != std::string::npos,
        "status center summary should include mismatch next action");
    requireCondition(
        tamperedMpPackage.statusCenterSummaryText.find("next_action_command_hint_source: mp_overlay_package_strict_verify_command") != std::string::npos,
        "status center summary should include mismatch command hint source");

    const auto json = strategic_nexus::serializeCompanionStatusSnapshot(ready);
    requireCondition(json.find("\"app_name\": \"Strategic Nexus Companion\"") != std::string::npos, "JSON should include app name");
    requireCondition(json.find("\"abbreviation\": \"SNC\"") != std::string::npos, "JSON should include abbreviation");
    requireCondition(json.find("\"generated_at_local\": \"") != std::string::npos, "JSON should include generated_at_local timestamp");
    requireCondition(json.find("\"archive_status\"") != std::string::npos, "JSON should include archive status");
    requireCondition(json.find("\"save_discovery_status\"") != std::string::npos, "JSON should include save discovery status");
    requireCondition(json.find("\"generated_overlay_status\"") != std::string::npos, "JSON should include overlay status");
    requireCondition(json.find("\"manifest_hash\": \"") != std::string::npos, "JSON should include generated overlay manifest hash");
    requireCondition(
        json.find("\"reactive_policy_pack_capability\": \"event_family_dispatch\"") != std::string::npos,
        "JSON should include generated overlay reactive capability");
    requireCondition(
        json.find("\"event_families\": [\"monthly_strategy_tick\"]") != std::string::npos,
        "JSON should include generated overlay event families");
    requireCondition(
        json.find("\"source_qualities\": [\"history_backed\", \"zero_history_bootstrap\"]") != std::string::npos,
        "JSON should include generated overlay source qualities");
    requireCondition(
        json.find("\"bootstrap_campaign_count\": 1") != std::string::npos,
        "JSON should include generated overlay bootstrap provenance count");
    requireCondition(json.find("\"generated_overlay_publish_gate_status\"") != std::string::npos, "JSON should include generated overlay publish gate status");
    requireCondition(json.find("\"mp_overlay_package_status\"") != std::string::npos, "JSON should include mp overlay package status");
    requireCondition(json.find("\"post_play_pipeline_status\"") != std::string::npos, "JSON should include post-play pipeline status");
    requireCondition(json.find("\"gameplay_acceptance_status\"") != std::string::npos, "JSON should include gameplay acceptance status");
    requireCondition(
        json.find("\"post_play_campaign_count\": 2") != std::string::npos,
        "JSON should include post-play campaign count");
    requireCondition(
        json.find("\"entry_point_reason\": \"entry points scanned\"") != std::string::npos,
        "JSON should include entry point reason");
    requireCondition(
        json.find("\"post_play_package_reason\": \"post-play package built; some entry points need more history or parsing\"") !=
            std::string::npos,
        "JSON should include post-play package reason");
    requireCondition(
        json.find("\"post_play_campaign_summaries\": [\"alpha_campaign: ready_partial (1/2 ready)\", \"beta_campaign: ready (1/1 ready)\"]") != std::string::npos,
        "JSON should include post-play campaign summaries");
    requireCondition(
        json.find("\"decision_input_package_reason\": \"decision input package built; some entries remain blocked\"") !=
            std::string::npos,
        "JSON should include decision input package reason");
    requireCondition(
        json.find("\"decision_input_blocked_entry_count\": 1") != std::string::npos,
        "JSON should include blocked decision input count");
    requireCondition(
        json.find("\"candidate_decision_package_readiness\": \"ready\"") != std::string::npos,
        "JSON should include candidate decision package readiness");
    requireCondition(
        json.find("\"candidate_decision_package_reason\": \"candidate decision package built; some source entries remain blocked\"") !=
            std::string::npos,
        "JSON should include candidate decision package reason");
    requireCondition(
        json.find("\"candidate_decision_blocked_source_entry_count\": 1") != std::string::npos,
        "JSON should include blocked candidate source count");
    requireCondition(
        json.find("\"candidate_decision_validator_passed\": true") != std::string::npos,
        "JSON should include candidate validator state");
    requireCondition(
        json.find("\"dsl_draft_readiness\": \"ready\"") != std::string::npos,
        "JSON should include DSL draft readiness");
    requireCondition(
        json.find("\"dsl_draft_reason\": \"validated dry-run DSL draft built\"") != std::string::npos,
        "JSON should include DSL draft reason");
    requireCondition(
        json.find("\"dsl_draft_eligible_candidate_count\": 1") != std::string::npos,
        "JSON should include DSL draft eligible candidate count");
    requireCondition(
        json.find("\"dsl_draft_skipped_candidate_count\": 1") != std::string::npos,
        "JSON should include DSL draft skipped candidate count");
    requireCondition(
        json.find("\"generated_overlay_staging_readiness\": \"staged_verified\"") != std::string::npos,
        "JSON should include generated overlay staging readiness");
    requireCondition(
        json.find("\"generated_overlay_staging_reason\": \"validated generated overlay staged\"") !=
            std::string::npos,
        "JSON should include generated overlay staging reason");
    requireCondition(
        json.find("\"generated_overlay_manifest_verified\": true") != std::string::npos,
        "JSON should include generated overlay staging manifest verification");
    requireCondition(
        json.find("\"generated_overlay_publish_allowed\": false") != std::string::npos,
        "JSON should include generated overlay staging publish flag");
    requireCondition(json.find("\"campaign_id\": \"campaign_mp_001\"") != std::string::npos, "JSON should include mp overlay package campaign id");
    requireCondition(json.find("\"overlay_version\": \"overlay_v1\"") != std::string::npos, "JSON should include mp overlay package overlay version");
    requireCondition(json.find("\"game_version\": \"stellaris_4.x\"") != std::string::npos, "JSON should include mp overlay package game version");
    requireCondition(json.find("\"strategic_nexus_mod_version\": \"strategic_nexus_v0\"") != std::string::npos, "JSON should include mp overlay package mod version");
    requireCondition(
        json.find("\"handoff_status\": \"degraded_previous_host_unavailable\"") != std::string::npos,
        "JSON should include mp overlay package handoff status");
    requireCondition(
        json.find("\"previous_host_available\": false") != std::string::npos,
        "JSON should include explicit previous-host continuity field");
    requireCondition(
        json.find("\"previous_host_available_known\": true") != std::string::npos,
        "JSON should include previous-host continuity known state");
    requireCondition(json.find("\"readiness\": \"ready_for_mp\"") != std::string::npos, "JSON should include mp overlay package readiness");
    requireCondition(json.find("\"host_readiness\": \"ready_for_mp\"") != std::string::npos, "JSON should include mp overlay package host readiness");
    requireCondition(
        json.find("\"client_readiness_gate\": \"import_and_verify_before_join\"") != std::string::npos,
        "JSON should include mp overlay package client readiness gate");
    requireCondition(
        json.find("\"host_next_step\": \"share this package and package_manifest_hash with every joining player\"") != std::string::npos,
        "JSON should include mp overlay package host next step");
    requireCondition(
        json.find("\"client_next_step\": \"import package, verify package_manifest_hash, then join lobby\"") != std::string::npos,
        "JSON should include mp overlay package client next step");
    requireCondition(json.find("\"package_manifest_hash\": \"") != std::string::npos, "JSON should include mp overlay package manifest hash");
    requireCondition(
        json.find("\"provenance_state\": \"present\"") != std::string::npos,
        "JSON should include MP provenance state");
    requireCondition(
        json.find("\"source_qualities\": [\"history_backed\", \"zero_history_bootstrap\"]") != std::string::npos,
        "JSON should include MP source provenance qualities");
    requireCondition(
        json.find("\"bootstrap_campaign_count\": 1") != std::string::npos,
        "JSON should include MP bootstrap provenance count");
    requireCondition(
        json.find("\"verify_command\": \"Strategic Nexus.exe --verify-mp-overlay-package ") != std::string::npos,
        "JSON should include mp overlay package verify command");
    requireCondition(
        json.find("\"import_command\": \"Strategic Nexus.exe --import-mp-overlay-package ") != std::string::npos,
        "JSON should include mp overlay package import command");
    requireCondition(
        json.find("\"strict_verify_command\": \"Strategic Nexus.exe --verify-mp-overlay-package ") != std::string::npos,
        "JSON should include mp overlay package strict verify command");
    requireCondition(
        json.find("\"strict_import_command\": \"Strategic Nexus.exe --import-mp-overlay-package ") != std::string::npos,
        "JSON should include mp overlay package strict import command");
    requireCondition(json.find("\"warning_codes\": []") != std::string::npos, "ready MP package JSON should include empty warning codes array");
    requireCondition(json.find("\"warning_count\": 0") != std::string::npos, "ready MP package JSON should include warning_count");
    requireCondition(
        json.find("\"identity_mismatch_warning\": false") != std::string::npos,
        "ready MP package JSON should include false identity mismatch warning");
    requireCondition(
        json.find("\"mismatch_warning_state\": \"no_mismatch\"") != std::string::npos,
        "ready MP package JSON should include mismatch warning state");
    requireCondition(
        json.find("\"mismatch_warning_reason\": \"no_identity_mismatch_detected\"") != std::string::npos,
        "ready MP package JSON should include mismatch warning reason");
    requireCondition(
        json.find("\"identity_mismatch_alert\": \"\"") != std::string::npos,
        "ready MP package JSON should include empty mismatch alert");
    requireCondition(json.find("\"status_center\"") != std::string::npos, "JSON should include status center");
    requireCondition(
        json.find("\"next_action\": \"review_mp_handoff_continuity\"") != std::string::npos,
        "JSON should include next action");
    requireCondition(
        json.find("\"next_action_reason\": \"mp_handoff_degraded_previous_host_unavailable\"") != std::string::npos,
        "JSON should include next action reason");
    requireCondition(
        json.find("\"next_action_command_hint\": \"Strategic Nexus.exe --verify-mp-overlay-package ") != std::string::npos,
        "JSON should include degraded-handoff verify command hint");
    requireCondition(
        json.find("\"next_action_command_hint_source\": \"mp_overlay_package_strict_verify_command\"") != std::string::npos,
        "JSON should include next action command hint source");
    requireCondition(
        json.find("\"next_action_path\": \"" + mpPackageRoot.generic_string() + "\"") != std::string::npos,
        "JSON should include next action path");
    requireCondition(
        json.find("\"owner_test_playbook_path\": \"\"") != std::string::npos,
        "JSON should include empty owner test playbook path when not ready");
    requireCondition(
        json.find("\"start_with_windows_enabled\": true") != std::string::npos,
        "JSON should include start-with-Windows enabled flag");
    requireCondition(
        json.find("\"startup_lifecycle_state\": \"owner_enabled_start_with_windows\"") != std::string::npos,
        "JSON should include startup lifecycle state");
    requireCondition(
        json.find("\"support_report_state\": \"not_prepared\"") != std::string::npos,
        "JSON should include support report state");
    requireCondition(
        json.find("\"support_report_reason\": \"prepare local support report preview before manual review or send\"") !=
            std::string::npos,
        "JSON should include support report reason");
    requireCondition(
        json.find("\"support_report_preview_path\": \"" +
                  readyConfig.supportReportPreviewPath.generic_string() + "\"") != std::string::npos,
        "JSON should include support report preview path");
    requireCondition(
        json.find("\"support_report_contact_email\": \"support@jeniksoft.cz\"") != std::string::npos,
        "JSON should include support report contact");
    requireCondition(
        json.find("\"support_report_send_requires_approval\": true") != std::string::npos,
        "JSON should include support report approval requirement");
    requireCondition(
        json.find("\"support_report_raw_saves_included\": false") != std::string::npos,
        "JSON should include support report raw-save exclusion");
    requireCondition(
        json.find(
            "\"support_report_prepare_command_hint\": \"powershell -NoProfile -ExecutionPolicy Bypass -File tools\\\\prepare_snc_support_report.ps1\"") !=
            std::string::npos,
        "JSON should include support report prepare command");
    requireCondition(
        json.find("\"crash_recovery_state\": \"no_recent_unexpected_crash\"") != std::string::npos,
        "JSON should include crash recovery baseline state");
    requireCondition(
        json.find("\"crash_recovery_reason\": \"no crash recovery record present\"") != std::string::npos,
        "JSON should include crash recovery baseline reason");
    requireCondition(
        json.find("\"crash_recovery_state_path\": \"" + crashRecoveryStatePath.generic_string() + "\"") !=
            std::string::npos,
        "JSON should include crash recovery state path");
    requireCondition(
        json.find("\"crash_recovery_restart_budget_exhausted\": false") != std::string::npos,
        "JSON should include crash recovery restart budget state");
    requireCondition(
        json.find("\"crash_recovery_warning_visible\": false") != std::string::npos,
        "JSON should include crash recovery warning visibility");
    requireCondition(
        json.find("\"crash_recovery_support_report_recommended\": false") != std::string::npos,
        "JSON should include crash recovery support-report recommendation state");
    requireCondition(
        json.find("\"crash_recovery\": ") != std::string::npos,
        "JSON should include nested crash recovery object");
    requireCondition(
        json.find("\"start_with_windows_source\": \"config_override\"") != std::string::npos,
        "JSON should include start-with-Windows source");
    requireCondition(
        json.find("\"start_with_windows_shortcut_state\": \"override_enabled\"") != std::string::npos,
        "JSON should include startup shortcut state");
    requireCondition(
        json.find("\"start_with_windows_shortcut_path\": \"" +
                  readyConfig.startWithWindowsShortcutPath.generic_string() + "\"") != std::string::npos,
        "JSON should include startup shortcut path");
    requireCondition(
        json.find("\"start_with_windows_command_hint\": \"cmd /c tools\\\\remove_snc_tray_startup_shortcut.cmd\"") !=
            std::string::npos,
        "JSON should include startup command hint");
    requireCondition(
        json.find("\"start_with_windows_command_hint_source\": \"startup_shortcut_remove_command\"") !=
            std::string::npos,
        "JSON should include startup command hint source");
    requireCondition(
        json.find("\"start_with_windows_enable_command_hint\": \"cmd /c tools\\\\install_snc_tray_startup_shortcut.cmd\"") !=
            std::string::npos,
        "JSON should include startup install command hint");
    requireCondition(
        json.find("\"start_with_windows_disable_command_hint\": \"cmd /c tools\\\\remove_snc_tray_startup_shortcut.cmd\"") !=
            std::string::npos,
        "JSON should include startup remove command hint");
    requireCondition(json.find("\"status_center_summary_text\"") != std::string::npos, "JSON should include status center summary text");
    requireCondition(json.find("Strategic Nexus Status Center") != std::string::npos, "JSON should include copyable status center summary");
    requireCondition(
        json.find("\"campaign_library_limit_reached\": false") != std::string::npos,
        "JSON should include non-saturated campaign library state");
    requireCondition(
        json.find("\"campaign_library_plan_source\": \"post_play_status_directory\"") != std::string::npos,
        "JSON should include campaign library plan source");
    requireCondition(json.find("\"package_zip_state\": \"ready\"") != std::string::npos, "JSON should include MP package zip state");
    requireCondition(
        json.find("\"package_zip_reason\": \"mp overlay package zip ready for handoff\"") != std::string::npos,
        "JSON should include MP package zip reason");
    requireCondition(
        json.find("\"package_zip_path\": \"" + mpPackageZipPath.generic_string() + "\"") != std::string::npos,
        "JSON should include MP package zip path");
    requireCondition(
        json.find("\"package_zip_hash\": \"" + ready.mpOverlayPackage.packageZipHash + "\"") != std::string::npos,
        "JSON should include MP package zip hash");
    requireCondition(json.find("generated_at_local: ") != std::string::npos, "status center summary should include generated_at_local timestamp");
    requireCondition(json.find("mp_warning_count: 0") != std::string::npos, "ready status center summary should include zero MP warning count");
    requireCondition(
        json.find("mp_identity_mismatch_warning: false") != std::string::npos,
        "ready status center summary should include false identity mismatch warning");

    strategic_nexus::CompanionStatusConfig detectedStartupConfig = readyConfig;
    detectedStartupConfig.startWithWindowsEnabled = false;
    detectedStartupConfig.useConfiguredStartWithWindowsState = false;
    detectedStartupConfig.startWithWindowsShortcutPath = root / "startup-shortcut-probe.lnk";
    std::filesystem::remove(detectedStartupConfig.startWithWindowsShortcutPath);
    const auto detectedStartupDisabled = companion.buildStatusSnapshot(detectedStartupConfig);
    requireCondition(
        !detectedStartupDisabled.lifecycle.startWithWindowsEnabled,
        "missing startup shortcut should keep start-with-Windows disabled");
    requireCondition(
        detectedStartupDisabled.lifecycle.startWithWindowsSource == "startup_shortcut_probe",
        "missing startup shortcut should report probe source");
    requireCondition(
        detectedStartupDisabled.lifecycle.startWithWindowsShortcutState == "shortcut_not_installed",
        "missing startup shortcut should report not installed");
    requireCondition(
        detectedStartupDisabled.lifecycle.startWithWindowsCommandHint ==
            "cmd /c tools\\install_snc_tray_startup_shortcut.cmd",
        "missing startup shortcut should recommend install command");
    requireCondition(
        detectedStartupDisabled.lifecycle.startWithWindowsCommandHintSource == "startup_shortcut_install_command",
        "missing startup shortcut should report install command source");

    writeTextFileAtomically(
        detectedStartupConfig.startWithWindowsShortcutPath,
        "synthetic startup shortcut for status detection test");
    const auto detectedStartupEnabled = companion.buildStatusSnapshot(detectedStartupConfig);
    requireCondition(
        detectedStartupEnabled.lifecycle.startWithWindowsEnabled,
        "present startup shortcut should enable detected start-with-Windows state");
    requireCondition(
        detectedStartupEnabled.lifecycle.startWithWindowsShortcutState == "shortcut_installed",
        "present startup shortcut should report installed");
    requireCondition(
        detectedStartupEnabled.lifecycle.startWithWindowsCommandHint ==
            "cmd /c tools\\remove_snc_tray_startup_shortcut.cmd",
        "present startup shortcut should recommend remove command");

    writeTextFileAtomically(
        readyConfig.supportReportPreviewPath,
        "synthetic local support report preview");
    const auto readyWithSupportReport = companion.buildStatusSnapshot(readyConfig);
    requireCondition(
        readyWithSupportReport.supportReport.state == "ready_for_review",
        "support report should become ready when local preview exists");
    requireCondition(
        readyWithSupportReport.supportReport.openCommandHint.find("cmd /c start") == 0,
        "ready support report should expose an open command hint");
    requireCondition(
        readyWithSupportReport.statusCenterSummaryText.find("support_report_state: ready_for_review") !=
            std::string::npos,
        "ready status center summary should expose prepared support report state");

    const auto stagedPublishJson = strategic_nexus::serializeCompanionStatusSnapshot(stagedPublishReady);
    requireCondition(
        stagedPublishJson.find("\"staging_status_path\": \"") != std::string::npos,
        "staged publish JSON should include staging status path");
    requireCondition(
        stagedPublishJson.find("\"active_overlay_directory\": \"") != std::string::npos,
        "staged publish JSON should include active overlay directory");
    requireCondition(
        stagedPublishJson.find("\"publish_status_path\": \"") != std::string::npos,
        "staged publish JSON should include publish status path");
    requireCondition(
        stagedPublishJson.find("\"published\": false") != std::string::npos,
        "staged publish JSON should include false published state");
    requireCondition(
        stagedPublishJson.find("\"backup_created\": false") != std::string::npos,
        "staged publish JSON should include false backup-created state");
    requireCondition(
        stagedPublishJson.find("\"owner_approval_required\": true") != std::string::npos,
        "staged publish JSON should include owner approval requirement");
    requireCondition(
        stagedPublishJson.find("\"can_publish\": true") != std::string::npos,
        "staged publish JSON should include can_publish");
    requireCondition(
        stagedPublishJson.find("\"backup_before_replace\": true") != std::string::npos,
        "staged publish JSON should include backup-before-replace state");
    requireCondition(
        stagedPublishJson.find("\"publish_command\": \"Strategic Nexus.exe --publish-snc-generated-overlay ") != std::string::npos,
        "staged publish JSON should include publish command");
    requireCondition(
        stagedPublishJson.find("\"campaign_library_limit_reached\": true") != std::string::npos,
        "staged publish JSON should include saturated campaign library state");
    requireCondition(
        stagedPublishJson.find("\"campaign_library_skipped_due_to_limit_count\": 2") != std::string::npos,
        "staged publish JSON should include skipped campaign count");

    writeTextFileAtomically(
        root / "strategic_nexus_campaign_library_plan.json",
        "{\n"
        "  \"schema_version\": 2,\n"
        "  \"limit_reached\": true,\n"
        "  \"skipped_due_to_limit_count\": 3,\n"
        "  \"campaigns\": []\n"
        "}\n");
    const auto invalidCampaignLibraryPlan = companion.buildStatusSnapshot({
        archiveSessionRoot,
        activeOverlayRoot,
        std::filesystem::path(),
        true,
        false,
        false,
        missingGameplayAcceptanceReport,
        stagedOverlayStatusPath,
        activeOverlayRoot,
        publishStatusPath,
        publishBackupRoot,
        entryPointAnalysisPath,
        postPlayPackagePath,
        decisionInputPackagePath,
        candidateDecisionPackagePath,
        dslDraftPath,
        dslDraftAuditPath,
        stagedOverlayStatusPath
    });
    requireCondition(
        invalidCampaignLibraryPlan.postPlayPipeline.state == "needs_attention",
        "invalid campaign library plan should fail closed");
    requireCondition(
        invalidCampaignLibraryPlan.postPlayPipeline.reason == "campaign library plan schema unsupported",
        "invalid campaign library plan should expose schema guard reason");
    requireCondition(
        invalidCampaignLibraryPlan.statusCenter.state == "attention_required",
        "status center should surface invalid campaign library plan");
    requireCondition(
        invalidCampaignLibraryPlan.statusCenter.reason == "post-play pipeline needs attention",
        "status center should attribute invalid campaign library plan to post-play pipeline");
    requireCondition(
        invalidCampaignLibraryPlan.statusCenterSummaryText.find(
            "campaign_library_plan_reason: campaign library plan schema unsupported") != std::string::npos,
        "status center summary should expose invalid campaign library plan reason");
    writeTextFileAtomically(
        root / "strategic_nexus_campaign_library_plan.json",
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"save_root_available\": true,\n"
        "  \"limit_reached\": true,\n"
        "  \"max_included_campaigns\": 1,\n"
        "  \"included_count\": 1,\n"
        "  \"skipped_count\": 2,\n"
        "  \"skipped_due_to_limit_count\": 2,\n"
        "  \"campaigns\": []\n"
        "}\n");

    const auto publishedJson = strategic_nexus::serializeCompanionStatusSnapshot(publishedSnapshot);
    requireCondition(
        publishedJson.find("\"published\": true") != std::string::npos,
        "published JSON should include true published state");
    requireCondition(
        publishedJson.find("\"backup_created\": true") != std::string::npos,
        "published JSON should include true backup-created state");
    requireCondition(
        publishedJson.find("\"published_file_count\": 3") != std::string::npos,
        "published JSON should include published file count");
    requireCondition(
        publishedJson.find("\"published_manifest_hash\": \"" + directArchiveSession.generatedOverlay.manifestHash + "\"") != std::string::npos,
        "published JSON should include published manifest hash");

    const auto gameplayAcceptanceReport = root / "gameplay_acceptance_v0.json";
    {
        std::ofstream out(gameplayAcceptanceReport, std::ios::binary | std::ios::trunc);
        out << "{\n"
            << "  \"schema_version\": 1,\n"
            << "  \"acceptance_state\": \"verified_for_v0_domains\",\n"
            << "  \"summary\": \"custom verified gameplay acceptance summary\",\n"
            << "  \"cases\": [\n"
            << "    { \"case_id\": \"case_a_defensive_military_posture\", \"result\": \"pass\" },\n"
            << "    { \"case_id\": \"case_b_aggressive_military_posture\", \"result\": \"pass\" },\n"
            << "    { \"case_id\": \"case_c_economy_research_bias\", \"result\": \"pass\" },\n"
            << "    { \"case_id\": \"case_d_military_industry_research_bias\", \"result\": \"pass\" },\n"
            << "    { \"case_id\": \"case_e_invalid_tactical_domain\", \"result\": \"pass\" },\n"
            << "    { \"case_id\": \"case_f_manifest_drift_before_publish\", \"result\": \"pass\" }\n"
            << "  ]\n"
            << "}\n";
    }

    const auto acceptanceReady = companion.buildStatusSnapshot({
        archiveRoot,
        overlayRoot,
        mpPackageRoot,
        true,
        false,
        false,
        gameplayAcceptanceReport
    });
    requireCondition(acceptanceReady.gameplayAcceptance.state == "ready", "gameplay acceptance should be ready when report is verified");
    requireCondition(
        acceptanceReady.gameplayAcceptance.reason == "custom verified gameplay acceptance summary",
        "gameplay acceptance should expose summary reason");
    requireCondition(
        acceptanceReady.statusCenterSummaryText.find("gameplay_acceptance: ready - custom verified gameplay acceptance summary") != std::string::npos,
        "status center summary should include verified gameplay acceptance state");

    const auto incompleteGameplayAcceptanceReport = root / "gameplay_acceptance_incomplete_v0.json";
    {
        std::ofstream out(incompleteGameplayAcceptanceReport, std::ios::binary | std::ios::trunc);
        out << "{\n"
            << "  \"schema_version\": 1,\n"
            << "  \"acceptance_state\": \"verified_for_v0_domains\",\n"
            << "  \"summary\": \"should not be trusted\",\n"
            << "  \"cases\": [\n"
            << "    { \"case_id\": \"case_a_defensive_military_posture\", \"result\": \"pass\" },\n"
            << "    { \"case_id\": \"case_b_aggressive_military_posture\", \"result\": \"pass\" },\n"
            << "    { \"case_id\": \"case_c_economy_research_bias\", \"result\": \"pass\" }\n"
            << "  ]\n"
            << "}\n";
    }

    const auto incompleteAcceptance = companion.buildStatusSnapshot({
        archiveRoot,
        overlayRoot,
        mpPackageRoot,
        true,
        false,
        false,
        incompleteGameplayAcceptanceReport
    });
    requireCondition(
        incompleteAcceptance.gameplayAcceptance.state == "needs_attention",
        "verified gameplay acceptance report should require full case coverage");
    requireCondition(
        incompleteAcceptance.gameplayAcceptance.reason == "gameplay acceptance report incomplete for verified state",
        "incomplete verified gameplay acceptance should fail closed");
    requireCondition(
        incompleteAcceptance.statusCenter.state == "attention_required",
        "status center should surface gameplay acceptance regressions");
    requireCondition(
        incompleteAcceptance.statusCenter.reason == "gameplay acceptance needs attention",
        "status center should name gameplay acceptance as the attention source");
    requireCondition(
        incompleteAcceptance.statusCenter.path == incompleteGameplayAcceptanceReport,
        "status center should point to the incomplete gameplay acceptance report");

    const auto brokenDslDraftAuditPath = root / "broken_dsl_draft_audit";
    std::filesystem::create_directories(brokenDslDraftAuditPath);
    const auto postPlayAttention = companion.buildStatusSnapshot({
        archiveSessionRoot,
        overlayRoot,
        mpPackageRoot,
        true,
        false,
        false,
        gameplayAcceptanceReport,
        std::filesystem::path(),
        std::filesystem::path(),
        std::filesystem::path(),
        std::filesystem::path(),
        entryPointAnalysisPath,
        postPlayPackagePath,
        decisionInputPackagePath,
        candidateDecisionPackagePath,
        dslDraftPath,
        brokenDslDraftAuditPath
    });
    requireCondition(
        postPlayAttention.postPlayPipeline.state == "needs_attention",
        "post-play pipeline should fail closed when DSL draft audit path is not a file");
    requireCondition(
        postPlayAttention.postPlayPipeline.reason == "dsl draft audit path is not a file",
        "post-play pipeline should expose DSL draft audit file errors");
    requireCondition(
        postPlayAttention.statusCenter.state == "attention_required",
        "status center should surface post-play pipeline regressions");
    requireCondition(
        postPlayAttention.statusCenter.reason == "post-play pipeline needs attention" ||
            postPlayAttention.statusCenter.reason == "post-play pipeline and memory recovery need attention" ||
            postPlayAttention.statusCenter.reason == "memory recovery needs attention",
        "status center should name the post-play or memory-recovery attention source");
    requireCondition(
        postPlayAttention.statusCenterSummaryText.find(
            "dsl_draft_audit_path: " + brokenDslDraftAuditPath.generic_string()) != std::string::npos,
        "status center summary should point to the failing post-play pipeline artifact");

    {
        strategic_nexus::CompanionStatusSnapshot snapshot;
        snapshot.appName = std::string("A\"B\\C\nD\tE\rF\bG\fH") + std::string(1, '\x01');
        snapshot.abbreviation = "T";
        snapshot.archive.state = "ready";
        snapshot.archive.reason = "x";
        snapshot.generatedOverlay.state = "ready";
        snapshot.generatedOverlay.reason = "y";
        snapshot.statusCenter.state = "ready";
        snapshot.statusCenter.reason = snapshot.appName;
        snapshot.statusCenterSummaryText = snapshot.appName;

        const auto escaped = strategic_nexus::serializeCompanionStatusSnapshot(snapshot);
        const std::string expected = "A\\\"B\\\\C\\nD\\tE\\rF\\bG\\fH\\u0001";
        if (escaped.find(expected) == std::string::npos) {
            std::cerr << "[FAIL] JSON serializer should escape control characters and backslashes\n";
            std::cerr << "expected_substring=" << expected << "\n";
            std::cerr << "actual_json=" << escaped << "\n";
            return 1;
        }
    }

    {
        const auto outputPath = root / "snc_status_snapshot.json";
        const strategic_nexus::CompanionStatusLoopConfig loopConfig{
            { archiveRoot, overlayRoot, std::filesystem::path(), true, false, false },
            outputPath,
            std::chrono::milliseconds(0),
            2
        };
        const auto loopResult = strategic_nexus::runCompanionStatusLoop(companion, loopConfig);
        requireCondition(loopResult.ok, "runCompanionStatusLoop should succeed");
        requireCondition(loopResult.iterationsRun == 2, "runCompanionStatusLoop should run the requested number of iterations");

        std::ifstream in(outputPath, std::ios::binary);
        const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        requireCondition(content.find("\"schema_version\": 1") != std::string::npos, "status snapshot should include schema version");
        requireCondition(content.find("\"app_name\": \"Strategic Nexus Companion\"") != std::string::npos, "status snapshot should include app name");
        requireCondition(content.find("\"status_center\"") != std::string::npos, "status snapshot should include status center");
        requireCondition(content.find("\"status_center_summary_text\"") != std::string::npos, "status snapshot should include status center summary text");
    }

    {
        const strategic_nexus::CompanionStatusLoopConfig loopConfig{
            { archiveRoot, overlayRoot, std::filesystem::path(), true, false, false },
            root,
            std::chrono::milliseconds(0),
            1
        };
        const auto loopResult = strategic_nexus::runCompanionStatusLoop(companion, loopConfig);
        requireCondition(!loopResult.ok, "runCompanionStatusLoop should fail when output path is a directory");
        requireCondition(loopResult.reason.find("status output path is a directory") != std::string::npos, "loop failure should name directory output path");
    }

    {
        const auto liveSaveRoot = root / "live_save_root";
        const auto liveArchiveRoot = root / "live_archive";
        const auto liveStatusPath = root / "live_monitor_status.json";
        std::filesystem::create_directories(liveSaveRoot / "campaign_a");
        {
            std::ofstream autosave(liveSaveRoot / "campaign_a" / "autosave_2200.01.01.sav", std::ios::binary | std::ios::trunc);
            autosave << "live autosave one";
        }
        {
            std::ofstream ironman(liveSaveRoot / "campaign_a" / "ironman.sav", std::ios::binary | std::ios::trunc);
            ironman << "ironman autosave one";
        }

        const auto monitorResult = strategic_nexus::runCompanionLiveAutosaveMonitor({
            { liveSaveRoot },
            liveArchiveRoot,
            liveStatusPath,
            "snc_live_session",
            std::chrono::milliseconds(0),
            0,
            1,
            false,
            false,
            false
        });

        requireCondition(monitorResult.ok, "SNC live autosave monitor should succeed for an existing save root");
        requireCondition(monitorResult.iterationsRun == 1, "SNC live autosave monitor should run the requested iteration count");
        requireCondition(monitorResult.candidateRootCount == 1, "SNC live autosave monitor should expose configured root count");
        requireCondition(monitorResult.existingRootCount == 1, "SNC live autosave monitor should expose existing root count");
        requireCondition(monitorResult.copiedCount == 2, "SNC live autosave monitor should capture autosave and ironman files");
        requireCondition(
            std::filesystem::exists(liveArchiveRoot / "snc_live_session" / "manifest.json"),
            "SNC live autosave monitor should write a manifest");
        requireCondition(monitorResult.statusOutputWritten, "SNC live autosave monitor should write a heartbeat status file");
        const auto liveStatusText = readTextFile(liveStatusPath);
        requireCondition(
            liveStatusText.find("\"state\": \"completed\"") != std::string::npos,
            "SNC live autosave status should mark bounded runs completed");
        requireCondition(
            liveStatusText.find("\"copied_count\": 2") != std::string::npos,
            "SNC live autosave status should expose copied count");

        std::filesystem::create_directories(liveSaveRoot / "campaign_b");
        {
            std::ofstream autosave(liveSaveRoot / "campaign_b" / "autosave_2200.02.01.sav", std::ios::binary | std::ios::trunc);
            autosave << "live autosave from new campaign";
        }

        const auto newCampaignMonitorResult = strategic_nexus::runCompanionLiveAutosaveMonitor({
            { liveSaveRoot },
            liveArchiveRoot,
            liveStatusPath,
            "snc_live_session",
            std::chrono::milliseconds(0),
            0,
            1,
            false,
            false,
            false
        });
        requireCondition(newCampaignMonitorResult.ok, "SNC live autosave monitor should keep scanning the whole save games root");
        requireCondition(newCampaignMonitorResult.copiedCount == 1, "SNC live autosave monitor should capture new campaign folders");
        requireCondition(
            countSavFiles(liveArchiveRoot / "snc_live_session" / "saves") == 3,
            "SNC live autosave monitor should preserve saves from multiple campaign folders");

        const auto invalidContinuousResult = strategic_nexus::runCompanionLiveAutosaveMonitor({
            { liveSaveRoot },
            liveArchiveRoot,
            std::filesystem::path(),
            "snc_live_session",
            std::chrono::milliseconds(0),
            0,
            0,
            false,
            false,
            false
        });
        requireCondition(
            !invalidContinuousResult.ok &&
                invalidContinuousResult.reason == "continuous monitor requires a positive poll interval",
            "SNC live autosave monitor should reject a continuous zero-delay loop");
    }

    std::cout << "strategic nexus companion tests passed.\n";
    return 0;
}
