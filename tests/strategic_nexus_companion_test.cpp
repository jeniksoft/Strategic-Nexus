// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicNexusCompanion.h"

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

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/strategic_nexus_companion_fixture");
    const auto archiveRoot = root / "archive";
    const auto overlayRoot = root / "overlay";
    const auto overlayEmptyRoot = root / "overlay_empty";
    const auto overlayNoManifestNonEmptyRoot = root / "overlay_no_manifest_non_empty";
    const auto mpPackageRoot = root / "mp_overlay_package";
    const auto entryPointAnalysisPath = root / "snc_entry_point_analysis.json";
    const auto postPlayPackagePath = root / "snc_post_play_package.json";
    const auto decisionInputPackagePath = root / "snc_decision_input_package.json";
    const auto candidateDecisionPackagePath = root / "snc_candidate_decision_package.json";
    const auto dslDraftPath = root / "snc_validated_dsl_draft.dsl";
    const auto dslDraftAuditPath = root / "snc_dsl_draft_package.json";
    const auto generatedOverlayStagingStatusPath = root / "snc_generated_overlay_staging_status.json";
    const auto missingGameplayAcceptanceReport = root / "missing_gameplay_acceptance_v0.json";
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
    }

    writeTextFileAtomically(
        entryPointAnalysisPath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"readiness\": \"ready\",\n"
        "  \"entry_point_count\": 3,\n"
        "  \"branch_ambiguity_detected\": false\n"
        "}\n");
    writeTextFileAtomically(
        postPlayPackagePath,
        "{\n"
        "  \"schema_version\": 1,\n"
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
        "  ]\n"
        "}\n");
    writeTextFileAtomically(
        decisionInputPackagePath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"readiness\": \"ready_partial\",\n"
        "  \"decision_input_count\": 2\n"
        "}\n");
    writeTextFileAtomically(
        candidateDecisionPackagePath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"readiness\": \"ready\",\n"
        "  \"candidate_decision_count\": 2,\n"
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
        "  \"readiness\": \"ready\",\n"
        "  \"dsl_rule_count\": 1,\n"
        "  \"validator_passed\": true\n"
        "}\n");
    writeTextFileAtomically(
        generatedOverlayStagingStatusPath,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"readiness\": \"staged_verified\",\n"
        "  \"dsl_rule_count\": 1,\n"
        "  \"manifest_verified\": true,\n"
        "  \"publish_allowed\": false\n"
        "}\n");

    const strategic_nexus::StrategicNexusCompanion companion;
    const auto ready = companion.buildStatusSnapshot({
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
        generatedOverlayStagingStatusPath
    });

    requireCondition(ready.appName == "Strategic Nexus Companion", "SNC app name should be stable");
    requireCondition(ready.abbreviation == "SNC", "SNC abbreviation should be stable");
    requireCondition(ready.lifecycle.startWithWindowsEnabled, "lifecycle should carry start-with-Windows state");
    requireCondition(ready.lifecycle.windowCloseBehavior == "minimize_to_tray", "window close should minimize to tray");
    requireCondition(ready.lifecycle.explicitExitBehavior == "stop_without_restart", "explicit exit should stop without restart");
    requireCondition(ready.lifecycle.crashRestartPolicy == "bounded_backoff_with_crash_loop_guard", "crash policy should be bounded");
    requireCondition(ready.saveDiscovery.state == "ready", "save discovery should find at least one save root in fixture");
    requireCondition(ready.archive.state == "starting", "archive should start when archive root exists but has no sessions");
    requireCondition(ready.generatedOverlay.state == "ready", "overlay should be ready when manifest verifies");
    requireCondition(!ready.generatedOverlay.manifestHash.empty(), "overlay status should expose generated overlay manifest hash");
    requireCondition(ready.generatedOverlayPublishGate.state == "ready", "publish gate should be ready when Stellaris is not running");
    requireCondition(
        ready.generatedOverlayPublishGate.reason == "Stellaris is not running; generated overlay publish allowed",
        "publish gate should explain publish readiness");
    requireCondition(ready.mpOverlayPackage.state == "ready", "mp overlay package should be ready when verified");
    requireCondition(ready.postPlayPipeline.state == "ready", "post-play pipeline should be ready when generated overlay staging verifies");
    requireCondition(
        ready.postPlayPipeline.reason == "generated overlay staging verified",
        "post-play pipeline should summarize generated overlay staging readiness");
    requireCondition(ready.postPlayPipeline.entryPointCount == 3, "post-play pipeline should expose entry point count");
    requireCondition(
        ready.postPlayPipeline.postPlayDecisionReadyEntryCount == 2,
        "post-play pipeline should expose decision-ready entry count");
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
    requireCondition(ready.postPlayPipeline.candidateDecisionCount == 2, "post-play pipeline should expose candidate decision count");
    requireCondition(
        ready.postPlayPipeline.candidateDecisionValidatorPassed,
        "post-play pipeline should expose candidate validator result");
    requireCondition(
        ready.postPlayPipeline.dslDraftReadiness == "ready",
        "post-play pipeline should expose DSL draft readiness");
    requireCondition(
        ready.postPlayPipeline.dslDraftRuleCount == 1,
        "post-play pipeline should expose DSL draft rule count");
    requireCondition(
        ready.postPlayPipeline.dslDraftValidatorPassed,
        "post-play pipeline should expose DSL draft validator result");
    requireCondition(
        ready.postPlayPipeline.generatedOverlayStagingReadiness == "staged_verified",
        "post-play pipeline should expose generated overlay staging readiness");
    requireCondition(
        ready.postPlayPipeline.generatedOverlayStagingRuleCount == 1,
        "post-play pipeline should expose generated overlay staging rule count");
    requireCondition(
        ready.postPlayPipeline.generatedOverlayManifestVerified,
        "post-play pipeline should expose generated overlay manifest verification");
    requireCondition(
        !ready.postPlayPipeline.generatedOverlayPublishAllowed,
        "post-play pipeline should expose publish gate still disabled for staged overlay");
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
    requireCondition(ready.statusCenter.state == "starting", "status center should start when any subsystem is starting");
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
        ready.statusCenterSummaryText.find("post_play_pipeline: ready - generated overlay staging verified") != std::string::npos,
        "status center summary should include post-play pipeline state");
    requireCondition(
        ready.statusCenterSummaryText.find("entry_point_count: 3") != std::string::npos,
        "status center summary should include entry point count");
    requireCondition(
        ready.statusCenterSummaryText.find("post_play_campaign_count: 2") != std::string::npos,
        "status center summary should include post-play campaign count");
    requireCondition(
        ready.statusCenterSummaryText.find("post_play_campaign_summary: alpha_campaign: ready_partial (1/2 ready)") != std::string::npos,
        "status center summary should include campaign readiness summary");
    requireCondition(
        ready.statusCenterSummaryText.find("decision_input_count: 2") != std::string::npos,
        "status center summary should include decision input count");
    requireCondition(
        ready.statusCenterSummaryText.find("candidate_decision_validator_passed: true") != std::string::npos,
        "status center summary should include candidate validator result");
    requireCondition(
        ready.statusCenterSummaryText.find("dsl_draft_readiness: ready") != std::string::npos,
        "status center summary should include DSL draft readiness");
    requireCondition(
        ready.statusCenterSummaryText.find("generated_overlay_staging_readiness: staged_verified") != std::string::npos,
        "status center summary should include generated overlay staging readiness");
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
        ready.statusCenterSummaryText.find("campaign_id: campaign_mp_001") != std::string::npos,
        "status center summary should include MP campaign id");
    requireCondition(
        ready.statusCenterSummaryText.find("package_manifest_hash: " + ready.mpOverlayPackage.packageManifestHash) != std::string::npos,
        "status center summary should include MP package manifest hash");
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
    requireCondition(directArchiveSession.statusCenter.state == "ready", "status center should be ready when archive session and overlay are ready");

    const auto stagedOverlayRoot = root / "staged_generated_overlay";
    const auto activeOverlayRoot = root / "active_generated_overlay";
    const auto stagedOverlayStatusPath = root / "snc_generated_overlay_staging_status.json";
    const auto publishStatusPath = root / "snc_generated_overlay_publish_status.json";
    const auto publishBackupRoot = root / "snc_generated_overlay_backups";
    std::filesystem::copy(overlayRoot, stagedOverlayRoot, std::filesystem::copy_options::recursive);
    std::filesystem::copy(overlayRoot, activeOverlayRoot, std::filesystem::copy_options::recursive);
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
        publishBackupRoot
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

    const auto json = strategic_nexus::serializeCompanionStatusSnapshot(ready);
    requireCondition(json.find("\"app_name\": \"Strategic Nexus Companion\"") != std::string::npos, "JSON should include app name");
    requireCondition(json.find("\"abbreviation\": \"SNC\"") != std::string::npos, "JSON should include abbreviation");
    requireCondition(json.find("\"generated_at_local\": \"") != std::string::npos, "JSON should include generated_at_local timestamp");
    requireCondition(json.find("\"archive_status\"") != std::string::npos, "JSON should include archive status");
    requireCondition(json.find("\"save_discovery_status\"") != std::string::npos, "JSON should include save discovery status");
    requireCondition(json.find("\"generated_overlay_status\"") != std::string::npos, "JSON should include overlay status");
    requireCondition(json.find("\"manifest_hash\": \"") != std::string::npos, "JSON should include generated overlay manifest hash");
    requireCondition(json.find("\"generated_overlay_publish_gate_status\"") != std::string::npos, "JSON should include generated overlay publish gate status");
    requireCondition(json.find("\"mp_overlay_package_status\"") != std::string::npos, "JSON should include mp overlay package status");
    requireCondition(json.find("\"post_play_pipeline_status\"") != std::string::npos, "JSON should include post-play pipeline status");
    requireCondition(json.find("\"gameplay_acceptance_status\"") != std::string::npos, "JSON should include gameplay acceptance status");
    requireCondition(
        json.find("\"post_play_campaign_count\": 2") != std::string::npos,
        "JSON should include post-play campaign count");
    requireCondition(
        json.find("\"post_play_campaign_summaries\": [\"alpha_campaign: ready_partial (1/2 ready)\", \"beta_campaign: ready (1/1 ready)\"]") != std::string::npos,
        "JSON should include post-play campaign summaries");
    requireCondition(
        json.find("\"candidate_decision_package_readiness\": \"ready\"") != std::string::npos,
        "JSON should include candidate decision package readiness");
    requireCondition(
        json.find("\"candidate_decision_validator_passed\": true") != std::string::npos,
        "JSON should include candidate validator state");
    requireCondition(
        json.find("\"dsl_draft_readiness\": \"ready\"") != std::string::npos,
        "JSON should include DSL draft readiness");
    requireCondition(
        json.find("\"generated_overlay_staging_readiness\": \"staged_verified\"") != std::string::npos,
        "JSON should include generated overlay staging readiness");
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
    requireCondition(json.find("\"status_center_summary_text\"") != std::string::npos, "JSON should include status center summary text");
    requireCondition(json.find("Strategic Nexus Status Center") != std::string::npos, "JSON should include copyable status center summary");
    requireCondition(json.find("generated_at_local: ") != std::string::npos, "status center summary should include generated_at_local timestamp");
    requireCondition(json.find("mp_warning_count: 0") != std::string::npos, "ready status center summary should include zero MP warning count");
    requireCondition(
        json.find("mp_identity_mismatch_warning: false") != std::string::npos,
        "ready status center summary should include false identity mismatch warning");

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
