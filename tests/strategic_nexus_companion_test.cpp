// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicNexusCompanion.h"

#include "generated_overlay/ManifestVerifier.h"

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
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(archiveRoot);
    std::filesystem::create_directories(overlayRoot);
    std::filesystem::create_directories(overlayEmptyRoot);

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

    const strategic_nexus::StrategicNexusCompanion companion;
    const auto ready = companion.buildStatusSnapshot({
        archiveRoot,
        overlayRoot,
        true
    });

    requireCondition(ready.appName == "Strategic Nexus Companion", "SNC app name should be stable");
    requireCondition(ready.abbreviation == "SNC", "SNC abbreviation should be stable");
    requireCondition(ready.lifecycle.startWithWindowsEnabled, "lifecycle should carry start-with-Windows state");
    requireCondition(ready.lifecycle.windowCloseBehavior == "minimize_to_tray", "window close should minimize to tray");
    requireCondition(ready.lifecycle.explicitExitBehavior == "stop_without_restart", "explicit exit should stop without restart");
    requireCondition(ready.lifecycle.crashRestartPolicy == "bounded_backoff_with_crash_loop_guard", "crash policy should be bounded");
    requireCondition(ready.archive.state == "starting", "archive should start when archive root exists but has no sessions");
    requireCondition(ready.generatedOverlay.state == "ready", "overlay should be ready when manifest verifies");
    requireCondition(ready.statusCenter.state == "starting", "status center should start when any subsystem is starting");

    const auto emptyOverlay = companion.buildStatusSnapshot({
        archiveRoot,
        overlayEmptyRoot,
        false
    });
    requireCondition(emptyOverlay.generatedOverlay.state == "starting", "empty overlay should be starting");
    requireCondition(emptyOverlay.statusCenter.state == "starting", "status center should start when overlay is starting");

    const auto missingOverlay = companion.buildStatusSnapshot({
        archiveRoot,
        root / "missing_overlay",
        false
    });
    requireCondition(missingOverlay.generatedOverlay.state == "needs_attention", "missing overlay should need attention");
    requireCondition(missingOverlay.statusCenter.state == "attention_required", "status center should surface attention");

    const auto json = strategic_nexus::serializeCompanionStatusSnapshot(ready);
    requireCondition(json.find("\"app_name\": \"Strategic Nexus Companion\"") != std::string::npos, "JSON should include app name");
    requireCondition(json.find("\"abbreviation\": \"SNC\"") != std::string::npos, "JSON should include abbreviation");
    requireCondition(json.find("\"archive_status\"") != std::string::npos, "JSON should include archive status");
    requireCondition(json.find("\"generated_overlay_status\"") != std::string::npos, "JSON should include overlay status");
    requireCondition(json.find("\"status_center\"") != std::string::npos, "JSON should include status center");

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
            { archiveRoot, overlayRoot, true },
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
    }

    std::cout << "strategic nexus companion tests passed.\n";
    return 0;
}
