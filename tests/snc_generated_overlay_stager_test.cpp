// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncGeneratedOverlayStager.h"

#include "common/FileUtil.h"

#include <cstdlib>
#include <filesystem>
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

std::string validDsl()
{
    return
        "campaign \"campaign_001\" {\n"
        "  empire \"country_0\" {\n"
        "    rule \"entry_001\" {\n"
        "      ministry = military_ministry\n"
        "      when campaign_marker = campaign_001\n"
        "      when known.save_fingerprint = h_abcdef123456\n"
        "      when known.save_date = d_2200_03_01\n"
        "      prefer doctrine_inertia high intensity 0.8\n"
        "      duration = next_session\n"
        "      confidence = 0.25\n"
        "      rationale = \"Conservative staging test rule.\"\n"
        "    }\n"
        "  }\n"
        "}\n";
}

std::string invalidDsl()
{
    return
        "campaign \"campaign_001\" {\n"
        "  empire \"country_0\" {\n"
        "    rule \"entry_001\" {\n"
        "      ministry = unknown_ministry\n"
        "      prefer doctrine_inertia high intensity 0.8\n"
        "      duration = next_session\n"
        "      confidence = 0.25\n"
        "    }\n"
        "  }\n"
        "}\n";
}

std::string unsupportedRuntimeConditionDsl()
{
    return
        "campaign \"campaign_001\" {\n"
        "  empire \"country_0\" {\n"
        "    rule \"entry_unsupported\" {\n"
        "      ministry = military_ministry\n"
        "      when personality.paranoia >= medium\n"
        "      prefer doctrine_inertia high intensity 0.8\n"
        "      duration = next_session\n"
        "      confidence = 0.25\n"
        "      rationale = \"Unsupported runtime condition test.\"\n"
        "    }\n"
        "  }\n"
        "}\n";
}

} // namespace

int main()
{
    const std::filesystem::path root = std::filesystem::path("dist") / "snc_generated_overlay_stager_test";
    const std::filesystem::path dslPath = root / "input.dsl";
    const std::filesystem::path invalidDslPath = root / "invalid.dsl";
    const std::filesystem::path unsupportedDslPath = root / "unsupported.dsl";
    const std::filesystem::path stagedRoot = root / "staged";

    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    requireCondition(strategic_nexus::common::writeTextFileAtomically(dslPath, validDsl()), "valid DSL fixture should write");
    requireCondition(strategic_nexus::common::writeTextFileAtomically(invalidDslPath, invalidDsl()), "invalid DSL fixture should write");
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(unsupportedDslPath, unsupportedRuntimeConditionDsl()),
        "unsupported DSL fixture should write");
    requireCondition(strategic_nexus::common::writeTextFileAtomically(stagedRoot / "stale.txt", "stale"), "stale staging file should write");

    const strategic_nexus::SncGeneratedOverlayStager stager;
    const auto status = stager.stage(dslPath, stagedRoot);
    requireCondition(status.ok, "valid DSL should stage overlay");
    requireCondition(status.reason == "validated generated overlay staged", "valid staging should explain success");
    requireCondition(status.readiness == "staged_verified", "valid staging should be staged_verified");
    requireCondition(status.dslRuleCount == 1, "valid staging should expose DSL rule count");
    requireCondition(status.validatorPassed, "valid staging should pass validator");
    requireCondition(status.manifestVerified, "valid staging should verify manifest");
    requireCondition(!status.publishAllowed, "staging must not allow publish");
    requireCondition(!status.publishesOverlay, "staging must not publish overlay");
    requireCondition(status.eventsWritten, "events should be written");
    requireCondition(status.effectsWritten, "effects should be written");
    requireCondition(status.triggersWritten, "triggers should be written");
    requireCondition(status.manifestWritten, "manifest should be written");
    requireCondition(!std::filesystem::exists(stagedRoot / "stale.txt"), "staging should replace stale snapshot");
    requireCondition(std::filesystem::exists(stagedRoot / "events" / "strategic_nexus_generated_events.txt"), "events file should exist");
    requireCondition(std::filesystem::exists(stagedRoot / "common" / "scripted_effects" / "strategic_nexus_generated_effects.txt"), "effects file should exist");
    requireCondition(std::filesystem::exists(stagedRoot / "common" / "scripted_triggers" / "strategic_nexus_generated_triggers.txt"), "triggers file should exist");
    requireCondition(std::filesystem::exists(stagedRoot / "strategic_nexus_generated_manifest.json"), "manifest file should exist");

    const auto json = strategic_nexus::serializeSncGeneratedOverlayStagingStatus(status);
    requireCondition(json.find("\"publish_allowed\": false") != std::string::npos, "status JSON should keep publish disabled");
    requireCondition(json.find("\"publishes_overlay\": false") != std::string::npos, "status JSON should say it does not publish");
    requireCondition(json.find("\"manifest_verified\": true") != std::string::npos, "status JSON should expose manifest verification");
    requireCondition(json.find("\"dsl_rule_count\": 1") != std::string::npos, "status JSON should expose rule count");

    const auto invalidStatus = stager.stage(invalidDslPath, root / "invalid_staged");
    requireCondition(!invalidStatus.ok, "invalid DSL should not stage overlay");
    requireCondition(!invalidStatus.publishAllowed, "invalid staging must not allow publish");
    requireCondition(!invalidStatus.publishesOverlay, "invalid staging must not publish overlay");
    requireCondition(!invalidStatus.validatorPassed, "invalid DSL should fail validator");
    requireCondition(!invalidStatus.validationErrors.empty(), "invalid DSL should expose validation errors");

    const auto unsupportedStatus = stager.stage(unsupportedDslPath, root / "unsupported_staged");
    requireCondition(!unsupportedStatus.ok, "unsupported runtime conditions should fail closed");
    requireCondition(unsupportedStatus.validatorPassed, "unsupported runtime conditions should still pass DSL validation");
    requireCondition(!unsupportedStatus.validationErrors.empty(), "unsupported runtime conditions should expose gating errors");
    requireCondition(
        unsupportedStatus.reason == "generated overlay runtime conditions are not safely enforceable yet",
        "unsupported runtime conditions should expose fail-safe reason");

    std::cout << "SNC generated overlay stager tests passed.\n";
    return 0;
}
