// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PostPlayPackageBuilder.h"

#include "AutosaveArchiveSummarizer.h"
#include "AutosaveArchiver.h"
#include "SaveEntryPointAnalyzer.h"

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

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/post_play_package_builder_fixture");
    const auto captureRoot = root / "capture_root";
    const auto currentRoot = root / "current_root";
    const auto archiveRoot = root / "archive";
    const std::string sessionId = "session_post_play_fixture";
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
    const auto summary = summarizer.summarize(archiveRoot / sessionId);
    requireCondition(summary.ok, "fixture archive summary should be verified");

    const strategic_nexus::SaveEntryPointAnalyzer analyzer;
    const auto analysis = analyzer.analyze(archiveRoot / sessionId, {currentRoot});
    requireCondition(analysis.ok, "entry point analysis should accept fixture archive");
    requireCondition(analysis.entryPointCount == 3, "fixture should expose three entry points");

    const strategic_nexus::PostPlayPackageBuilder builder;
    const auto package = builder.build(summary, analysis);
    requireCondition(package.ok, "post-play package should build for verified inputs");
    requireCondition(package.dryRunOnly, "post-play package should be dry-run only");
    requireCondition(!package.publishesOverlay, "post-play package must not publish an overlay");
    requireCondition(package.readiness == "ready_partial", "mixed ready and blocked entries should be ready_partial");
    requireCondition(package.entryPointCount == 3, "post-play package should preserve entry point count");
    requireCondition(package.decisionReadyEntryCount == 2, "two alpha entries should be decision-ready");

    bool foundFutureExcludedEntry = false;
    bool foundInsufficientHistoryEntry = false;
    for (const auto& entry : package.entries) {
        if (entry.futureEvidenceExcluded &&
            entry.evidencePolicy == "compatible_history_only_future_excluded" &&
            entry.decisionInputAllowed) {
            foundFutureExcludedEntry = true;
            requireCondition(
                !entry.compatibleArchivedEvidenceSamples.empty(),
                "future-excluded entry should keep bounded compatible evidence samples");
            requireCondition(
                !entry.laterArchivedEvidenceSamples.empty(),
                "future-excluded entry should keep bounded later evidence samples");
        }
        if (entry.campaignKey == "beta_campaign" &&
            entry.decisionInputState == "insufficient_history" &&
            !entry.decisionInputAllowed) {
            foundInsufficientHistoryEntry = true;
        }
    }
    requireCondition(foundFutureExcludedEntry, "older alpha entry should exclude later evidence but remain usable");
    requireCondition(foundInsufficientHistoryEntry, "beta entry should be blocked without compatible history");

    const auto json = strategic_nexus::serializePostPlayPackage(package);
    requireCondition(json.find("\"dry_run_only\": true") != std::string::npos, "JSON should expose dry-run mode");
    requireCondition(json.find("\"publishes_overlay\": false") != std::string::npos, "JSON should expose no overlay publishing");
    requireCondition(json.find("\"decision_ready_entry_count\": 2") != std::string::npos, "JSON should expose ready count");
    requireCondition(json.find("\"post_play_no_compatible_history\"") != std::string::npos, "JSON should expose history warning");
    requireCondition(json.find("\"compatible_history_only_future_excluded\"") != std::string::npos, "JSON should expose future evidence policy");
    requireCondition(
        json.find("\"compatible_archived_evidence_samples\":") != std::string::npos,
        "JSON should expose bounded compatible evidence samples");
    requireCondition(
        json.find("\"later_archived_evidence_samples\":") != std::string::npos,
        "JSON should expose bounded later evidence samples");

    std::cout << "post-play package builder tests passed.\n";
    return 0;
}
