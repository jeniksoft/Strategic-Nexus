// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "AutosaveArchiveSummarizer.h"
#include "AutosaveArchiver.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
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
    std::ofstream output(path, std::ios::trunc | std::ios::binary);
    output << content;
}

} // namespace

int main()
{
    const auto sourceRoot = std::filesystem::path("dist/autosave_archive_summarizer_source");
    const auto archiveRoot = std::filesystem::path("dist/autosave_archive_summarizer_archive");
    std::filesystem::remove_all(sourceRoot);
    std::filesystem::remove_all(archiveRoot);
    writeFile(sourceRoot / "autosave_2230.sav", "first");
    writeFile(sourceRoot / "autosave_2231.sav", "second");

    const strategic_nexus::AutosaveArchiver archiver;
    const auto archiveResult = archiver.archiveStableSaves(sourceRoot, archiveRoot, "session_001", 0);
    requireCondition(archiveResult.copiedCount == 2, "fixture archive should copy two saves");

    const strategic_nexus::AutosaveArchiveSummarizer summarizer;
    const auto summary = summarizer.summarize(archiveRoot / "session_001");
    requireCondition(summary.ok, "summarizer should accept verified archive");
    requireCondition(summary.schemaVersion == 1, "summarizer should expose current schema version");
    requireCondition(summary.schemaCompatibilityState == "current", "summarizer should expose current compatibility state");
    requireCondition(summary.copiedSaveCount == 2, "summarizer should count copied saves");
    requireCondition(summary.totalByteCount == 11, "summarizer should sum copied save bytes");
    requireCondition(!summary.firstContentHash.empty(), "summarizer should preserve first hash anchor");
    requireCondition(!summary.lastContentHash.empty(), "summarizer should preserve last hash anchor");

    const auto json = strategic_nexus::serializeAutosaveArchiveSummary(summary);
    requireCondition(json.find("\"copied_save_count\": 2") != std::string::npos, "summary JSON should include save count");
    requireCondition(json.find("\"total_byte_count\": 11") != std::string::npos, "summary JSON should include byte total");
    requireCondition(
        json.find("\"source_schema_version\": 1") != std::string::npos,
        "summary JSON should include source schema version");
    requireCondition(
        json.find("\"schema_compatibility_state\": \"current\"") != std::string::npos,
        "summary JSON should include compatibility state");

    const auto manifestPath = archiveRoot / "session_001" / "manifest.json";
    const auto originalManifest = [&]() {
        std::ifstream input(manifestPath);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }();
    const auto legacyManifest = std::regex_replace(
        originalManifest,
        std::regex("\"schema_version\"\\s*:\\s*1"),
        "\"schema_version\": 0");
    writeFile(manifestPath, legacyManifest);

    const auto legacySummary = summarizer.summarize(archiveRoot / "session_001");
    requireCondition(legacySummary.ok, "summarizer should accept legacy schema archive");
    requireCondition(
        legacySummary.schemaVersion == 0,
        "legacy summary should expose schema_version 0");
    requireCondition(
        legacySummary.schemaCompatibilityState == "partial_compatibility",
        "legacy summary should expose partial compatibility state");
    requireCondition(
        legacySummary.schemaCompatibilityNote ==
            "migrated legacy autosave archive schema_version 0 to current schema_version 1",
        "legacy summary should expose migration note");
    const auto legacyJson = strategic_nexus::serializeAutosaveArchiveSummary(legacySummary);
    requireCondition(
        legacyJson.find("\"source_schema_version\": 0") != std::string::npos,
        "legacy summary JSON should include schema version 0");
    requireCondition(
        legacyJson.find("\"schema_compatibility_state\": \"partial_compatibility\"") != std::string::npos,
        "legacy summary JSON should include partial compatibility state");
    requireCondition(
        legacyJson.find("\"schema_compatibility_note\": \"migrated legacy autosave archive schema_version 0 to current schema_version 1\"") !=
            std::string::npos,
        "legacy summary JSON should include migration note");

    std::cout << "autosave archive summarizer tests passed.\n";
    return 0;
}

