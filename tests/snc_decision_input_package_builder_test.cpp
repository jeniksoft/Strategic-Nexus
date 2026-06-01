// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncDecisionInputPackageBuilder.h"

#include "AutosaveArchiveSummarizer.h"
#include "AutosaveArchiver.h"
#include "PostPlayPackageBuilder.h"
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

bool containsValue(const std::vector<std::string>& values, const std::string& target)
{
    return std::find(values.begin(), values.end(), target) != values.end();
}

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/snc_decision_input_package_builder_fixture");
    const auto captureRoot = root / "capture_root";
    const auto currentRoot = root / "current_root";
    const auto archiveRoot = root / "archive";
    const std::string sessionId = "session_decision_input_fixture";
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
    const auto summary = summarizer.summarize(archiveRoot / sessionId);
    const auto analysis = analyzer.analyze(archiveRoot / sessionId, {currentRoot});
    const auto postPlayPackage = postPlayBuilder.build(summary, analysis);

    const auto postPlayJson = strategic_nexus::serializePostPlayPackage(postPlayPackage);
    const auto readResult = strategic_nexus::parsePostPlayPackageJson(postPlayJson);
    requireCondition(readResult.ok, "serialized post-play package should parse back");
    requireCondition(readResult.package.entries.size() == 3, "post-play parser should preserve entries");
    requireCondition(readResult.package.decisionReadyEntryCount == 2, "post-play parser should preserve ready count");

    const strategic_nexus::SncDecisionInputPackageBuilder builder;
    const auto decisionPackage = builder.build(readResult.package, root / "snc_post_play_package.json");
    requireCondition(decisionPackage.ok, "decision input package should build from parsed post-play package");
    requireCondition(decisionPackage.readiness == "ready_partial", "mixed entries should produce ready_partial");
    requireCondition(decisionPackage.dryRunOnly, "decision input package should be dry-run only");
    requireCondition(!decisionPackage.publishesOverlay, "decision input package must not publish an overlay");
    requireCondition(!decisionPackage.modelOutputTrusted, "decision input package must not trust model output");
    requireCondition(decisionPackage.validationRequired, "decision input package should require validation");
    requireCondition(decisionPackage.entryPointCount == 3, "decision input package should preserve entry count");
    requireCondition(decisionPackage.decisionInputCount == 2, "only ready alpha entries should become decision inputs");
    requireCondition(decisionPackage.blockedEntryCount == 1, "beta entry should remain blocked");

    bool foundFutureExclusion = false;
    for (const auto& input : decisionPackage.decisionInputs) {
        requireCondition(input.validationRequired, "each decision input should require validation");
        requireCondition(!input.modelOutputTrusted, "each decision input should distrust model output");
        requireCondition(
            containsValue(input.uncertainties, "model_output_untrusted_requires_validation"),
            "each decision input should carry model trust-boundary uncertainty");
        if (input.futureEvidenceExcluded) {
            foundFutureExclusion = true;
            requireCondition(
                containsValue(input.uncertainties, "future_evidence_excluded_for_entry_point"),
                "future-excluded decision input should carry explicit uncertainty");
        }
    }
    requireCondition(foundFutureExclusion, "older alpha entry should preserve future evidence exclusion");

    requireCondition(decisionPackage.blockedEntries[0].campaignKey == "beta_campaign", "blocked beta entry should be retained");
    requireCondition(
        decisionPackage.blockedEntries[0].reason == "no compatible archived history is available",
        "blocked beta entry should explain insufficient history");

    const auto json = strategic_nexus::serializeSncDecisionInputPackage(decisionPackage);
    requireCondition(json.find("\"decision_input_count\": 2") != std::string::npos, "JSON should expose decision input count");
    requireCondition(json.find("\"blocked_entry_count\": 1") != std::string::npos, "JSON should expose blocked count");
    requireCondition(json.find("\"model_output_trusted\": false") != std::string::npos, "JSON should expose trust boundary");
    requireCondition(json.find("\"validation_required\": true") != std::string::npos, "JSON should expose validation gate");
    requireCondition(json.find("\"empire_state_parsed\": false") != std::string::npos, "JSON should expose empire state parse gate");
    requireCondition(
        json.find("\"empire_state_parser_reason\": \"save archive extraction failed\"") != std::string::npos,
        "JSON should expose bounded parser failure reason for non-save fixtures");
    requireCondition(json.find("\"publishes_overlay\": true") == std::string::npos, "JSON must never publish overlay in this slice");
    requireCondition(json.find("\"decision_inputs\": [") != std::string::npos, "JSON should include decision inputs");
    requireCondition(json.find("\"blocked_entries\": [") != std::string::npos, "JSON should include blocked entries");

    std::cout << "SNC decision input package builder tests passed.\n";
    return 0;
}
