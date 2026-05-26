// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicNexusCompanion.h"

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
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(archiveRoot);
    std::filesystem::create_directories(overlayRoot);

    {
        std::ofstream manifest(overlayRoot / "strategic_nexus_generated_manifest.json", std::ios::trunc);
        manifest << "{ \"schema_version\": 1 }\n";
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
    requireCondition(ready.generatedOverlay.state == "ready", "overlay should be ready when manifest exists");
    requireCondition(ready.statusCenter.state == "starting", "status center should start when any subsystem is starting");

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

    std::cout << "strategic nexus companion tests passed.\n";
    return 0;
}
