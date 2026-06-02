// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncGeneratedOverlayPublishGate.h"
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
        "campaign \"campaign_gate\" {\n"
        "  empire \"country_0\" {\n"
        "    rule \"gate_rule_001\" {\n"
        "      ministry = military_ministry\n"
        "      when campaign_marker = campaign_gate\n"
        "      when known.save_fingerprint = h_abcdef123456\n"
        "      when known.save_date = d_2200_03_01\n"
        "      prefer military_posture defensive intensity 0.7\n"
        "      duration = next_session\n"
        "      confidence = 0.55\n"
        "      rationale = \"Owner approved publish gate fixture.\"\n"
        "    }\n"
        "  }\n"
        "}\n";
}

strategic_nexus::SncGeneratedOverlayPublishGateRequest gateRequest(
    const std::filesystem::path& statusPath,
    const std::filesystem::path& activeRoot,
    const std::filesystem::path& backupRoot,
    const std::string& token,
    const bool stellarisRunning)
{
    strategic_nexus::SncGeneratedOverlayPublishGateRequest request;
    request.stagingStatusPath = statusPath;
    request.activeOverlayDirectory = activeRoot;
    request.backupRootDirectory = backupRoot;
    request.ownerApprovalToken = token;
    request.stellarisRunning = stellarisRunning;
    return request;
}

} // namespace

int main()
{
    const std::filesystem::path root = std::filesystem::path("dist") / "snc_generated_overlay_publish_gate_test";
    const std::filesystem::path dslPath = root / "input.dsl";
    const std::filesystem::path stagedRoot = root / "staged";
    const std::filesystem::path statusPath = root / "staging_status.json";
    const std::filesystem::path activeRoot = root / "active";
    const std::filesystem::path backupRoot = root / "backups";

    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    requireCondition(strategic_nexus::common::writeTextFileAtomically(dslPath, validDsl()), "valid DSL fixture should write");

    const strategic_nexus::SncGeneratedOverlayStager stager;
    const auto stagingStatus = stager.stage(dslPath, stagedRoot);
    requireCondition(stagingStatus.ok, "valid DSL should stage overlay");
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(
            statusPath,
            strategic_nexus::serializeSncGeneratedOverlayStagingStatus(stagingStatus)),
        "staging status should write");

    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(activeRoot / "old" / "stale.txt", "stale"),
        "stale active file should write");

    const strategic_nexus::SncGeneratedOverlayPublishGate gate;
    const auto unapproved = gate.publish(gateRequest(statusPath, activeRoot, backupRoot, "", false));
    requireCondition(!unapproved.ok, "gate should refuse publish without owner approval");
    requireCondition(!unapproved.published, "unapproved gate should not publish");
    requireCondition(!unapproved.backupCreated, "unapproved gate should not create backup");
    requireCondition(
        std::filesystem::exists(activeRoot / "old" / "stale.txt"),
        "unapproved gate should leave active overlay untouched");
    requireCondition(
        unapproved.reason == "owner approval token is required",
        "unapproved gate should explain missing owner approval");

    const auto blockedByStellaris = gate.publish(gateRequest(statusPath, activeRoot, backupRoot, "owner-approved", true));
    requireCondition(!blockedByStellaris.ok, "gate should refuse publish while Stellaris is running");
    requireCondition(!blockedByStellaris.published, "Stellaris-running gate should not publish");
    requireCondition(!blockedByStellaris.backupCreated, "Stellaris-running gate should not create backup");
    requireCondition(
        std::filesystem::exists(activeRoot / "old" / "stale.txt"),
        "Stellaris-running gate should leave active overlay untouched");

    const auto published = gate.publish(gateRequest(statusPath, activeRoot, backupRoot, "owner-approved", false));
    requireCondition(published.ok, "approved gate should publish");
    requireCondition(published.published, "approved gate should mark publish");
    requireCondition(published.ownerApproved, "approved gate should record owner approval");
    requireCondition(published.backupCreated, "approved gate should backup existing active overlay");
    requireCondition(published.readiness == "published", "approved gate readiness should be published");
    requireCondition(!published.sourceManifestHash.empty(), "approved gate should expose source manifest hash");
    requireCondition(!published.publishedManifestHash.empty(), "approved gate should expose published manifest hash");
    requireCondition(
        published.sourceManifestHash == published.publishedManifestHash,
        "approved gate published manifest hash should match source");
    requireCondition(published.publishedFileCount == 3, "approved gate should report three published files");
    requireCondition(
        !std::filesystem::exists(activeRoot / "old" / "stale.txt"),
        "approved gate should replace stale active overlay");
    requireCondition(
        std::filesystem::exists(published.backupDirectory / "old" / "stale.txt"),
        "approved gate should preserve previous active overlay in backup");

    const auto audit = strategic_nexus::serializeSncGeneratedOverlayPublishGateResult(published);
    requireCondition(audit.find("\"owner_approved\": true") != std::string::npos, "audit should record owner approval");
    requireCondition(audit.find("\"backup_created\": true") != std::string::npos, "audit should record backup");
    requireCondition(audit.find("\"published\": true") != std::string::npos, "audit should record publish");

    const std::filesystem::path badStatusPath = root / "bad_status.json";
    const std::string badStatus = "{\n"
        "  \"ok\": true,\n"
        "  \"readiness\": \"staged_verified\",\n"
        "  \"publish_allowed\": false,\n"
        "  \"publishes_overlay\": false,\n"
        "  \"manifest_verified\": false,\n"
        "  \"staged_overlay_directory\": \"" + stagedRoot.generic_string() + "\",\n"
        "  \"manifest_hash\": \"h_bad\"\n"
        "}\n";
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(badStatusPath, badStatus),
        "bad staging status should write");
    const auto rejected = gate.publish(gateRequest(badStatusPath, root / "bad_active", backupRoot, "owner-approved", false));
    requireCondition(!rejected.ok, "gate should reject unverified staging status");
    requireCondition(!rejected.published, "gate should not publish unverified staging status");
    requireCondition(
        rejected.reason == "staging manifest is not verified",
        "gate should explain unverified staging status");

    std::cout << "SNC generated overlay publish gate tests passed.\n";
    return 0;
}
