// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncPostPlayArtifactBackfiller.h"

#include "SncCandidateDecisionPackageBuilder.h"
#include "common/FileUtil.h"
#include "generated_overlay/MpOverlayPackage.h"

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
        "campaign \"campaign_backfill\" {\n"
        "  empire \"country_0\" {\n"
        "    rule \"entry_backfill\" {\n"
        "      ministry = military_ministry\n"
        "      when campaign_marker = campaign_backfill\n"
        "      when known.save_fingerprint = h_backfill123456\n"
        "      when known.save_date = d_2200_03_01\n"
        "      prefer doctrine_inertia high intensity 0.8\n"
        "      duration = next_session\n"
        "      confidence = 0.25\n"
        "      rationale = \"Tray should backfill missing staged artifacts from an existing DSL draft.\"\n"
        "    }\n"
        "  }\n"
        "}\n";
}

std::string readyDslAuditJson()
{
    return
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"ok\": true,\n"
        "  \"reason\": \"dsl draft built from deterministic candidate decisions\",\n"
        "  \"readiness\": \"ready_partial\",\n"
        "  \"dry_run_only\": true,\n"
        "  \"publishes_overlay\": false,\n"
        "  \"validator_passed\": true,\n"
        "  \"dsl_rule_count\": 1,\n"
        "  \"eligible_candidate_count\": 1,\n"
        "  \"skipped_candidate_count\": 0,\n"
        "  \"warning_codes\": [],\n"
        "  \"validation_errors\": []\n"
        "}\n";
}

strategic_nexus::SncCandidateDecisionPackage buildCandidatePackage()
{
    strategic_nexus::SncCandidateDecision candidate;
    candidate.candidateDecisionId = "candidate-decision::Campaign Backfill::2200.03.01::abcd";
    candidate.decisionInputId = "decision-input::Campaign Backfill::2200.03.01::abcd";
    candidate.packageEntryId = "postplay::Campaign Backfill::2200.03.01::abcd";
    candidate.campaignKey = "Campaign Backfill";
    candidate.ruleScope = "Campaign Backfill::2200.03.01::abcd";
    candidate.sourceKind = "autosave";
    candidate.saveName = "autosave_2200.03.01.sav";
    candidate.saveDate = "2200.03.01";
    candidate.contentHash = "abcd1234";
    candidate.parseStatus = "headline_parsed";
    candidate.playerCountryId = "0";
    candidate.empireName = "Backfill Empire";
    candidate.militaryPosture = "defensive";
    candidate.researchBias = "economy";
    candidate.confidencePercent = 55;

    strategic_nexus::SncCandidateDecisionPackage package;
    package.ok = true;
    package.reason = "candidate decision package built";
    package.readiness = "ready";
    package.validatorPassed = true;
    package.entryPointCount = 1;
    package.decisionInputCount = 1;
    package.candidateDecisionCount = 1;
    package.acceptedCandidateCount = 1;
    package.rejectedCandidateCount = 0;
    package.candidateDecisions.push_back(candidate);
    return package;
}

} // namespace

int main()
{
    const std::filesystem::path root = std::filesystem::path("dist") / "snc_post_play_artifact_backfiller_test";
    const std::filesystem::path dslPath = root / "snc_validated_dsl_draft.dsl";
    const std::filesystem::path dslAuditPath = root / "snc_dsl_draft_package.json";
    const std::filesystem::path candidatePackagePath = root / "snc_candidate_decision_package.json";
    const std::filesystem::path stagingStatusPath = root / "snc_generated_overlay_staging_status.json";
    const std::filesystem::path stagedOverlayDirectory = root / "snc_generated_overlay_staged";
    const std::filesystem::path mpPackageDirectory = root / "snc_mp_overlay_package";
    const std::filesystem::path mpPackageZipPath = root / "snc_mp_overlay_package.zip";

    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(dslPath, validDsl()),
        "DSL draft fixture should write");
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(dslAuditPath, readyDslAuditJson()),
        "DSL draft audit fixture should write");
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(
            candidatePackagePath,
            strategic_nexus::serializeSncCandidateDecisionPackage(buildCandidatePackage())),
        "candidate package fixture should write");
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(mpPackageZipPath, "stale zip\n"),
        "stale MP zip fixture should write");

    strategic_nexus::SncPostPlayArtifactBackfillConfig config;
    config.dslDraftPath = dslPath;
    config.dslDraftAuditPath = dslAuditPath;
    config.candidateDecisionPackagePath = candidatePackagePath;
    config.generatedOverlayStagingStatusPath = stagingStatusPath;
    config.generatedOverlayStagingDirectory = stagedOverlayDirectory;
    config.mpOverlayPackageDirectory = mpPackageDirectory;
    config.mpOverlayPackageZipPath = mpPackageZipPath;

    const strategic_nexus::SncPostPlayArtifactBackfiller backfiller;
    const auto result = backfiller.backfill(config);
    requireCondition(result.attempted, "backfill should run when staged overlay status is missing");
    requireCondition(result.stagedOverlayStatusWritten, "backfill should write staged overlay status");
    requireCondition(result.stagedOverlayReady, "backfill should produce a ready staged overlay");
    requireCondition(result.mpOverlayPackageExported, "backfill should export MP package from staged overlay");
    requireCondition(result.mpPackageRefreshState == "ready", "backfill should surface ready refresh state");
    requireCondition(
        result.mpPackageRefreshReason == "mp_package_exported_from_existing_staged_overlay",
        "backfill should expose stable MP refresh reason");
    requireCondition(std::filesystem::exists(stagingStatusPath), "staging status file should exist");
    requireCondition(
        std::filesystem::exists(stagedOverlayDirectory / "events" / "strategic_nexus_generated_events.txt"),
        "staged events file should exist");
    requireCondition(
        std::filesystem::exists(stagedOverlayDirectory / "strategic_nexus_generated_manifest.json"),
        "staged manifest should exist");
    requireCondition(
        std::filesystem::exists(mpPackageDirectory / "strategic_nexus_mp_overlay_package_manifest.json"),
        "MP package manifest should exist");
    requireCondition(!std::filesystem::exists(mpPackageZipPath), "stale MP zip should be cleared before later regeneration");

    const auto stagingStatusJson = strategic_nexus::common::readTextFile(stagingStatusPath);
    requireCondition(
        stagingStatusJson.find("\"readiness\": \"staged_verified\"") != std::string::npos,
        "staging status should report staged_verified");

    strategic_nexus::generated_overlay::MpOverlayPackageVerifier verifier;
    const auto verification = verifier.verify(mpPackageDirectory);
    requireCondition(verification.ok, "exported MP package should verify");
    requireCondition(verification.campaignId == "campaign_backfill", "MP package should use sanitized campaign id");
    requireCondition(
        verification.overlayVersion == "overlay_v0_session",
        "MP package should use tray overlay version");

    const auto secondResult = backfiller.backfill(config);
    requireCondition(!secondResult.attempted, "backfill should become idle once artifacts are current");
    requireCondition(secondResult.stagedOverlayReady, "idempotent backfill should still observe staged overlay ready");
    requireCondition(!secondResult.mpOverlayPackageExported, "idempotent backfill should not re-export current package");
    requireCondition(
        secondResult.mpPackageRefreshState == "ready",
        "idempotent backfill should still surface ready MP refresh state for a current package");
    requireCondition(
        secondResult.mpPackageRefreshReason == "existing_mp_package_already_current",
        "idempotent backfill should explain that the existing MP package is already current");

    std::filesystem::remove(candidatePackagePath, cleanupError);
    std::filesystem::remove(stagingStatusPath, cleanupError);
    requireCondition(
        !std::filesystem::exists(candidatePackagePath),
        "candidate package should be removable for restart-like fallback coverage");
    requireCondition(
        !std::filesystem::exists(stagingStatusPath),
        "staging status should be removable for restart-like fallback coverage");

    const auto fallbackResult = backfiller.backfill(config);
    requireCondition(
        fallbackResult.attempted,
        "backfill should restage when staging status disappears after an earlier export");
    requireCondition(
        fallbackResult.stagedOverlayStatusWritten,
        "restart-like backfill should rewrite staging status");
    requireCondition(
        fallbackResult.stagedOverlayReady,
        "restart-like backfill should still produce ready staged overlay");
    requireCondition(
        !fallbackResult.mpOverlayPackageExported,
        "restart-like backfill should keep an already compatible MP package");
    requireCondition(
        fallbackResult.mpPackageRefreshState == "ready",
        "restart-like backfill should surface ready MP refresh state");
    requireCondition(
        fallbackResult.mpPackageRefreshReason == "existing_mp_package_already_current",
        "restart-like backfill should accept a compatible existing MP package without campaign metadata");

    std::cout << "SNC post-play artifact backfiller tests passed.\n";
    return 0;
}
