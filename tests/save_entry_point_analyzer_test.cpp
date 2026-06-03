// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SaveEntryPointAnalyzer.h"
#include "AutosaveArchiver.h"

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
    const auto root = std::filesystem::path("dist/save_entry_point_analyzer_fixture");
    const auto captureRoot = root / "capture_root";
    const auto currentRoot = root / "current_root";
    const auto archiveRoot = root / "archive";
    const std::string sessionId = "session_branch_fixture";
    std::filesystem::remove_all(root);

    const strategic_nexus::AutosaveArchiver archiver;

    writeFile(captureRoot / "Alpha Campaign" / "autosave_2200.01.01.sav", "alpha branch one");
    auto archiveResult = archiver.archiveLiveSaves(captureRoot, archiveRoot, sessionId, 0);
    requireCondition(archiveResult.copiedCount == 1, "first branch autosave should be archived");

    writeFile(captureRoot / "Alpha Campaign" / "autosave_2200.01.01.sav", "alpha branch two");
    archiveResult = archiver.archiveLiveSaves(captureRoot, archiveRoot, sessionId, 0);
    requireCondition(archiveResult.copiedCount == 1, "same-date changed autosave should create a second archive revision");

    writeFile(captureRoot / "Alpha Campaign" / "autosave_2200.04.01.sav", "alpha later abandoned evidence");
    archiveResult = archiver.archiveLiveSaves(captureRoot, archiveRoot, sessionId, 0);
    requireCondition(archiveResult.copiedCount == 1, "later autosave should be archived as historical evidence");

    writeFile(currentRoot / "Alpha Campaign" / "autosave_2200.01.01.sav", "alpha branch two");
    writeFile(currentRoot / "Alpha Campaign" / "2200.02.01.sav", "manual entry point");
    writeFile(currentRoot / "Beta Campaign" / "ironman.sav", "beta ironman entry point");

    const strategic_nexus::SaveEntryPointAnalyzer analyzer;
    const auto analysis = analyzer.analyze(archiveRoot / sessionId, {currentRoot});

    requireCondition(analysis.ok, "entry point analysis should accept verified archive and current save root");
    requireCondition(analysis.archiveVerified, "entry point analysis should verify the archive first");
    requireCondition(analysis.entryPointCount == 3, "entry point analysis should list every loadable .sav");
    requireCondition(analysis.archivedEvidenceCount == 3, "entry point analysis should list archived evidence");
    requireCondition(analysis.branchAmbiguityDetected, "same campaign/date with different hashes should flag branch ambiguity");
    requireCondition(analysis.readiness == "ambiguous", "branch ambiguity should set ambiguous readiness");

    bool foundConservativeAlphaEntry = false;
    bool foundAmbiguousAlphaEntry = false;
    bool foundEvidenceSamples = false;
    for (const auto& entry : analysis.entryPoints) {
        if (entry.campaignKey != "alpha_campaign") {
            continue;
        }
        if (entry.laterArchivedEvidenceCount > 0) {
            foundConservativeAlphaEntry = true;
            requireCondition(
                !entry.compatibleArchivedEvidenceSamples.empty(),
                "older alpha entry should retain bounded compatible evidence samples");
            requireCondition(
                !entry.laterArchivedEvidenceSamples.empty(),
                "older alpha entry should retain bounded later evidence samples");
            foundEvidenceSamples = true;
        }
        if (entry.analysisState == "ambiguous") {
            foundAmbiguousAlphaEntry = true;
        }
    }
    requireCondition(foundConservativeAlphaEntry, "older alpha entry point should exclude later archived evidence");
    requireCondition(foundAmbiguousAlphaEntry, "alpha entry point should be marked ambiguous");
    requireCondition(foundEvidenceSamples, "analysis should keep bounded evidence samples for conservative entry points");

    const auto json = strategic_nexus::serializeSaveEntryPointAnalysis(analysis);
    requireCondition(json.find("\"branch_ambiguity_detected\": true") != std::string::npos, "JSON should expose branch ambiguity");
    requireCondition(json.find("\"entry_point_count\": 3") != std::string::npos, "JSON should expose entry point count");
    requireCondition(json.find("\"later_archived_evidence_count\":") != std::string::npos, "JSON should expose later evidence counts");
    requireCondition(
        json.find("\"compatible_archived_evidence_samples\":") != std::string::npos,
        "JSON should expose bounded compatible evidence samples");
    requireCondition(
        json.find("\"later_archived_evidence_samples\":") != std::string::npos,
        "JSON should expose bounded later evidence samples");
    requireCondition(json.find("\"campaign_branch_ambiguity_detected\"") != std::string::npos, "JSON should expose warning code");
    requireCondition(json.find("\"source_kind\": \"ironman\"") != std::string::npos, "JSON should classify ironman entry points");

    std::cout << "save entry point analyzer tests passed.\n";
    return 0;
}
