// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "AutosaveArchiveSummarizer.h"
#include "AutosaveArchiver.h"

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
    requireCondition(summary.copiedSaveCount == 2, "summarizer should count copied saves");
    requireCondition(summary.totalByteCount == 11, "summarizer should sum copied save bytes");
    requireCondition(!summary.firstContentHash.empty(), "summarizer should preserve first hash anchor");
    requireCondition(!summary.lastContentHash.empty(), "summarizer should preserve last hash anchor");

    const auto json = strategic_nexus::serializeAutosaveArchiveSummary(summary);
    requireCondition(json.find("\"copied_save_count\": 2") != std::string::npos, "summary JSON should include save count");
    requireCondition(json.find("\"total_byte_count\": 11") != std::string::npos, "summary JSON should include byte total");

    std::cout << "autosave archive summarizer tests passed.\n";
    return 0;
}

