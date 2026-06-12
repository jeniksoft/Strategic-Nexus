// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncCandidateDecisionPackageBuilder.h"

#include "AutosaveArchiveSummarizer.h"
#include "AutosaveArchiver.h"
#include "PostPlayPackageBuilder.h"
#include "SaveEntryPointAnalyzer.h"
#include "SncDecisionInputPackageBuilder.h"

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

bool containsValue(const std::vector<std::string>& values, const std::string& target)
{
    return std::find(values.begin(), values.end(), target) != values.end();
}

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/snc_candidate_decision_package_builder_fixture");
    const auto captureRoot = root / "capture_root";
    const auto currentRoot = root / "current_root";
    const auto archiveRoot = root / "archive";
    const std::string sessionId = "session_candidate_decision_fixture";
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
    const strategic_nexus::SaveEntryPointAnalyzer analyzer;
    const strategic_nexus::PostPlayPackageBuilder postPlayBuilder;
    const strategic_nexus::SncDecisionInputPackageBuilder decisionInputBuilder;
    const strategic_nexus::SncCandidateDecisionPackageBuilder candidateBuilder;

    const auto summary = summarizer.summarize(archiveRoot / sessionId);
    const auto analysis = analyzer.analyze(archiveRoot / sessionId, {currentRoot});
    const auto postPlayPackage = postPlayBuilder.build(summary, analysis);
    const auto decisionInputPackage = decisionInputBuilder.build(postPlayPackage, root / "snc_post_play_package.json");
    const auto decisionInputJson = strategic_nexus::serializeSncDecisionInputPackage(decisionInputPackage);
    const auto decisionInputRead = strategic_nexus::parseSncDecisionInputPackageJson(decisionInputJson);

    requireCondition(decisionInputRead.ok, "serialized decision input package should parse back");
    requireCondition(
        decisionInputRead.package.sourceSchemaVersion == 1,
        "decision input parser should preserve current source schema version");
    requireCondition(
        decisionInputRead.package.schemaCompatibilityState == "current",
        "decision input parser should preserve current compatibility state");
    requireCondition(
        decisionInputRead.package.schemaCompatibilityNote.empty(),
        "decision input parser should keep current compatibility note empty");
    requireCondition(decisionInputRead.package.decisionInputCount == 2, "decision input parser should preserve ready count");
    requireCondition(decisionInputRead.package.blockedEntryCount == 1, "decision input parser should preserve blocked count");

    const auto candidatePackage = candidateBuilder.build(
        decisionInputRead.package,
        root / "snc_decision_input_package.json");
    requireCondition(candidatePackage.ok, "candidate decision package should build from parsed input package");
    requireCondition(candidatePackage.validatorPassed, "candidate decision package validator should pass");
    requireCondition(candidatePackage.readiness == "ready_partial", "mixed source entries should produce ready_partial");
    requireCondition(candidatePackage.dryRunOnly, "candidate decision package should be dry-run only");
    requireCondition(!candidatePackage.publishesOverlay, "candidate decision package must not publish an overlay");
    requireCondition(!candidatePackage.modelOutputUsed, "candidate decision package should not use model output yet");
    requireCondition(!candidatePackage.modelOutputTrusted, "candidate decision package must not trust model output");
    requireCondition(candidatePackage.validationRequired, "candidate decision package should require validation");
    requireCondition(candidatePackage.candidateSource == "deterministic_v0_stub", "candidate source should be explicit");
    requireCondition(candidatePackage.candidateDecisionCount == 2, "two alpha inputs should become candidate decisions");
    requireCondition(candidatePackage.acceptedCandidateCount == 2, "validator should accept both conservative candidates");
    requireCondition(candidatePackage.rejectedCandidateCount == 0, "validator should reject no conservative candidate");
    requireCondition(candidatePackage.blockedSourceEntryCount == 1, "blocked beta source entry should be retained");

    bool foundFutureExclusion = false;
    for (const auto& candidate : candidatePackage.candidateDecisions) {
        requireCondition(candidate.recommendedAction == "observe_only", "v0 candidate should be observe-only");
        requireCondition(candidate.militaryPosture == "defensive", "v0 candidate should emit bounded military posture");
        requireCondition(candidate.researchBias == "economy", "v0 candidate should emit bounded research bias");
        requireCondition(!candidate.publishesOverlay, "candidate must not publish overlay");
        requireCondition(!candidate.modelOutputUsed, "candidate must not use model output");
        requireCondition(!candidate.modelOutputTrusted, "candidate must not trust model output");
        requireCondition(candidate.validationRequired, "candidate should require validation");
        requireCondition(candidate.validationWarnings.empty(), "valid fixture candidates should have no validation warnings");
        requireCondition(
            containsValue(candidate.knownFacts, "candidate_source:deterministic_v0_stub"),
            "candidate should carry deterministic source fact");
        requireCondition(
            containsValue(candidate.knownFacts, "candidate_military_posture:defensive"),
            "candidate should record chosen military posture");
        requireCondition(
            containsValue(candidate.knownFacts, "candidate_research_bias:economy"),
            "candidate should record chosen research bias");
        requireCondition(
            containsValue(candidate.uncertainties, "candidate_is_not_a_mod_rule"),
            "candidate should state it is not a mod rule");
        if (containsValue(candidate.uncertainties, "future_evidence_excluded_for_entry_point")) {
            foundFutureExclusion = true;
            requireCondition(
                !candidate.compatibleArchivedEvidenceSamples.empty(),
                "future-excluded candidate should retain bounded compatible evidence samples");
            requireCondition(
                !candidate.laterArchivedEvidenceSamples.empty(),
                "future-excluded candidate should retain bounded later evidence samples");
        }
    }
    requireCondition(foundFutureExclusion, "older alpha candidate should preserve future evidence exclusion");

    const auto json = strategic_nexus::serializeSncCandidateDecisionPackage(candidatePackage);
    requireCondition(
        json.find("\"source_schema_version\": 1") != std::string::npos,
        "candidate decision package JSON should expose source schema version");
    requireCondition(
        json.find("\"schema_compatibility_state\": \"current\"") != std::string::npos,
        "candidate decision package JSON should expose current compatibility state");
    requireCondition(
        json.find("\"schema_compatibility_note\": \"\"") != std::string::npos,
        "candidate decision package JSON should keep current compatibility note empty");
    requireCondition(json.find("\"candidate_decision_count\": 2") != std::string::npos, "JSON should expose candidate count");
    requireCondition(json.find("\"accepted_candidate_count\": 2") != std::string::npos, "JSON should expose accepted count");
    requireCondition(json.find("\"blocked_source_entry_count\": 1") != std::string::npos, "JSON should expose blocked source count");
    requireCondition(json.find("\"validator_passed\": true") != std::string::npos, "JSON should expose validator gate");
    requireCondition(json.find("\"candidate_source\": \"deterministic_v0_stub\"") != std::string::npos, "JSON should expose candidate source");
    requireCondition(json.find("\"recommended_action\": \"observe_only\"") != std::string::npos, "JSON should expose observe-only action");
    requireCondition(json.find("\"military_posture\": \"defensive\"") != std::string::npos, "JSON should expose bounded military posture");
    requireCondition(json.find("\"research_bias\": \"economy\"") != std::string::npos, "JSON should expose bounded research bias");
    requireCondition(json.find("\"empire_state_summary\": {") != std::string::npos, "JSON should include bounded empire state summary");
    requireCondition(json.find("\"parsed\": false") != std::string::npos, "JSON should expose unavailable parser state for non-save fixtures");
    requireCondition(
        json.find("\"candidate_empire_state_summary_unavailable\"") != std::string::npos,
        "JSON should keep empire-state unavailability as uncertainty, not a crash");
    requireCondition(json.find("\"model_output_used\": false") != std::string::npos, "JSON should expose model output boundary");
    requireCondition(json.find("\"publishes_overlay\": true") == std::string::npos, "JSON must never publish overlay in this slice");
    requireCondition(json.find("\"candidate_decisions\": [") != std::string::npos, "JSON should include candidate decisions");
    requireCondition(json.find("\"blocked_source_entries\": [") != std::string::npos, "JSON should include blocked source entries");
    requireCondition(
        json.find("\"compatible_archived_evidence_samples\":") != std::string::npos,
        "JSON should expose bounded compatible evidence samples");
    requireCondition(
        json.find("\"later_archived_evidence_samples\":") != std::string::npos,
        "JSON should expose bounded later evidence samples");

    auto futureCandidateSchemaJson = json;
    const auto candidateSchemaMarker = futureCandidateSchemaJson.find("\"schema_version\": 1");
    requireCondition(
        candidateSchemaMarker != std::string::npos,
        "candidate decision package JSON should expose schema version marker");
    futureCandidateSchemaJson.replace(
        candidateSchemaMarker,
        std::string("\"schema_version\": 1").size(),
        "\"schema_version\": 2");
    const auto futureCandidateSchemaRead =
        strategic_nexus::parseSncCandidateDecisionPackageJson(futureCandidateSchemaJson);
    requireCondition(!futureCandidateSchemaRead.ok, "unsupported candidate decision package schema should fail closed");
    requireCondition(
        futureCandidateSchemaRead.reason == "unsupported candidate decision package schema",
        "unsupported candidate decision package schema should expose explicit compatibility reason");

    auto legacyDecisionInputPackage = decisionInputRead.package;
    legacyDecisionInputPackage.sourceSchemaVersion = 0;
    legacyDecisionInputPackage.schemaCompatibilityState = "partial_compatibility";
    legacyDecisionInputPackage.schemaCompatibilityNote =
        "migrated legacy decision input package schema_version 0 to current schema_version 1";
    const auto legacyCandidatePackage = candidateBuilder.build(
        legacyDecisionInputPackage,
        root / "legacy_snc_decision_input_package.json");
    requireCondition(
        legacyCandidatePackage.sourceSchemaVersion == 0,
        "legacy candidate decision package should preserve source schema version 0");
    requireCondition(
        legacyCandidatePackage.schemaCompatibilityState == "partial_compatibility",
        "legacy candidate decision package should expose partial compatibility");
    requireCondition(
        legacyCandidatePackage.schemaCompatibilityNote ==
            "migrated legacy decision input package schema_version 0 to current schema_version 1",
        "legacy candidate decision package should expose the migration note");

    const auto legacyCandidateJson = strategic_nexus::serializeSncCandidateDecisionPackage(legacyCandidatePackage);
    requireCondition(
        legacyCandidateJson.find("\"source_schema_version\": 0") != std::string::npos,
        "legacy candidate decision package JSON should expose source schema version 0");
    requireCondition(
        legacyCandidateJson.find("\"schema_compatibility_state\": \"partial_compatibility\"") !=
            std::string::npos,
        "legacy candidate decision package JSON should expose partial compatibility state");
    requireCondition(
        legacyCandidateJson.find(
            "\"schema_compatibility_note\": \"migrated legacy decision input package schema_version 0 to current schema_version 1\"") !=
            std::string::npos,
        "legacy candidate decision package JSON should expose the migration note");

    const auto legacyCandidateRead = strategic_nexus::parseSncCandidateDecisionPackageJson(legacyCandidateJson);
    requireCondition(legacyCandidateRead.ok, "legacy candidate decision package JSON should parse back");
    requireCondition(
        legacyCandidateRead.package.sourceSchemaVersion == 0,
        "legacy candidate decision package parse should preserve source schema version 0");
    requireCondition(
        legacyCandidateRead.package.schemaCompatibilityState == "partial_compatibility",
        "legacy candidate decision package parse should preserve partial compatibility");
    requireCondition(
        legacyCandidateRead.package.schemaCompatibilityNote ==
            "migrated legacy decision input package schema_version 0 to current schema_version 1",
        "legacy candidate decision package parse should preserve migration note");

    std::cout << "SNC candidate decision package builder tests passed.\n";
    return 0;
}
