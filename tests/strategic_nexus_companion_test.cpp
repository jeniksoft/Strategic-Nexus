// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicNexusCompanion.h"

#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/MpOverlayPackage.h"

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

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/strategic_nexus_companion_fixture");
    const auto archiveRoot = root / "archive";
    const auto overlayRoot = root / "overlay";
    const auto overlayEmptyRoot = root / "overlay_empty";
    const auto overlayNoManifestNonEmptyRoot = root / "overlay_no_manifest_non_empty";
    const auto mpPackageRoot = root / "mp_overlay_package";
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

    const strategic_nexus::StrategicNexusCompanion companion;
    const auto ready = companion.buildStatusSnapshot({
        archiveRoot,
        overlayRoot,
        mpPackageRoot,
        true,
        false,
        false,
        missingGameplayAcceptanceReport
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
    requireCondition(ready.mpOverlayPackage.campaignId == "campaign_mp_001", "mp overlay package should expose campaign id");
    requireCondition(ready.mpOverlayPackage.overlayVersion == "overlay_v1", "mp overlay package should expose overlay version");
    requireCondition(ready.mpOverlayPackage.gameVersion == "stellaris_4.x", "mp overlay package should expose game version");
    requireCondition(ready.mpOverlayPackage.strategicNexusModVersion == "strategic_nexus_v0", "mp overlay package should expose mod version");
    requireCondition(ready.mpOverlayPackage.handoffStatus == "degraded_previous_host_unavailable", "mp overlay package should expose handoff status");
    requireCondition(ready.mpOverlayPackage.readiness == "ready_for_mp", "mp overlay package should expose MP readiness");
    requireCondition(!ready.mpOverlayPackage.packageManifestHash.empty(), "mp overlay package should expose package manifest hash");
    requireCondition(
        ready.mpOverlayPackage.verifyCommand.find("Strategic Nexus.exe --verify-mp-overlay-package ") == 0,
        "mp overlay package should expose verify command");
    requireCondition(
        ready.mpOverlayPackage.importCommand.find("Strategic Nexus.exe --import-mp-overlay-package ") == 0,
        "mp overlay package should expose import command");
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
        ready.statusCenterSummaryText.find("mp_readiness: ready_for_mp") != std::string::npos,
        "status center summary should include MP readiness");
    requireCondition(
        ready.statusCenterSummaryText.find("host_readiness: ready_for_mp") != std::string::npos,
        "status center summary should include host readiness text");
    requireCondition(
        ready.statusCenterSummaryText.find("client_readiness_gate: import_and_verify_before_join") != std::string::npos,
        "status center summary should include client readiness gate text");
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
            missingMpPackage.mpOverlayPackage.warningCodes.front() == "mp overlay package directory missing",
        "missing mp overlay package should expose structured warning code");
    requireCondition(
        missingMpPackage.mpOverlayPackage.statusText.find("readiness: not_ready") != std::string::npos,
        "missing mp overlay package should expose not_ready status text");
    requireCondition(
        missingMpPackage.mpOverlayPackage.statusText.find("warning_code: mp overlay package directory missing") != std::string::npos,
        "missing mp overlay package should expose warning code in status text");
    requireCondition(missingMpPackage.statusCenter.state == "attention_required", "status center should surface mp overlay package attention");
    requireCondition(missingMpPackage.statusCenter.reason == "mp overlay package needs attention", "status center reason should name mp overlay package attention");
    requireCondition(missingMpPackage.statusCenter.path == (root / "missing_mp_overlay_package"), "status center path should point to the mp overlay package needing attention");
    requireCondition(
        missingMpPackage.statusCenterSummaryText.find("warning_code: mp overlay package directory missing") != std::string::npos,
        "status center summary should include MP package warning code");
    requireCondition(
        missingMpPackage.statusCenterSummaryText.find("mp_warning_code: mp overlay package directory missing") != std::string::npos,
        "status center summary should include structured MP warning code");

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
    requireCondition(json.find("\"gameplay_acceptance_status\"") != std::string::npos, "JSON should include gameplay acceptance status");
    requireCondition(json.find("\"campaign_id\": \"campaign_mp_001\"") != std::string::npos, "JSON should include mp overlay package campaign id");
    requireCondition(json.find("\"overlay_version\": \"overlay_v1\"") != std::string::npos, "JSON should include mp overlay package overlay version");
    requireCondition(json.find("\"game_version\": \"stellaris_4.x\"") != std::string::npos, "JSON should include mp overlay package game version");
    requireCondition(json.find("\"strategic_nexus_mod_version\": \"strategic_nexus_v0\"") != std::string::npos, "JSON should include mp overlay package mod version");
    requireCondition(
        json.find("\"handoff_status\": \"degraded_previous_host_unavailable\"") != std::string::npos,
        "JSON should include mp overlay package handoff status");
    requireCondition(json.find("\"readiness\": \"ready_for_mp\"") != std::string::npos, "JSON should include mp overlay package readiness");
    requireCondition(json.find("\"package_manifest_hash\": \"") != std::string::npos, "JSON should include mp overlay package manifest hash");
    requireCondition(
        json.find("\"verify_command\": \"Strategic Nexus.exe --verify-mp-overlay-package ") != std::string::npos,
        "JSON should include mp overlay package verify command");
    requireCondition(
        json.find("\"import_command\": \"Strategic Nexus.exe --import-mp-overlay-package ") != std::string::npos,
        "JSON should include mp overlay package import command");
    requireCondition(json.find("\"warning_codes\": []") != std::string::npos, "ready MP package JSON should include empty warning codes array");
    requireCondition(json.find("\"status_center\"") != std::string::npos, "JSON should include status center");
    requireCondition(json.find("\"status_center_summary_text\"") != std::string::npos, "JSON should include status center summary text");
    requireCondition(json.find("Strategic Nexus Status Center") != std::string::npos, "JSON should include copyable status center summary");
    requireCondition(json.find("generated_at_local: ") != std::string::npos, "status center summary should include generated_at_local timestamp");

    const auto gameplayAcceptanceReport = root / "gameplay_acceptance_v0.json";
    {
        std::ofstream out(gameplayAcceptanceReport, std::ios::binary | std::ios::trunc);
        out << "{\n"
            << "  \"schema_version\": 1,\n"
            << "  \"acceptance_state\": \"verified_for_v0_domains\"\n"
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
        acceptanceReady.gameplayAcceptance.reason == "gameplay acceptance verified for v0 domains",
        "gameplay acceptance should expose verified reason");
    requireCondition(
        acceptanceReady.statusCenterSummaryText.find("gameplay_acceptance: ready - gameplay acceptance verified for v0 domains") != std::string::npos,
        "status center summary should include verified gameplay acceptance state");

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

    std::cout << "strategic nexus companion tests passed.\n";
    return 0;
}
