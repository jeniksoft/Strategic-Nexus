// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

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
    const auto sourceRoot = std::filesystem::path("dist/autosave_archiver_source");
    const auto archiveRoot = std::filesystem::path("dist/autosave_archiver_archive");
    std::filesystem::remove_all(sourceRoot);
    std::filesystem::remove_all(archiveRoot);

    writeFile(sourceRoot / "autosave_2230.sav", "fixture one");
    writeFile(sourceRoot / "manual_save.sav", "fixture two");
    writeFile(sourceRoot / "notes.txt", "ignored");

    const strategic_nexus::AutosaveArchiver archiver;
    const auto result = archiver.archiveStableSaves(sourceRoot, archiveRoot, "session_001", 0);

    requireCondition(result.sourceExists, "archiver should mark existing source root");
    requireCondition(result.copiedCount == 2, "archiver should copy stable .sav files");
    requireCondition(result.skippedCount == 0, "archiver should ignore non-save files without recording skips");
    requireCondition(std::filesystem::exists(archiveRoot / "session_001" / "manifest.json"), "archiver should write manifest");
    requireCondition(std::filesystem::exists(archiveRoot / "session_001" / "saves" / "001_autosave_2230.sav"), "archiver should copy first save");
    requireCondition(std::filesystem::exists(archiveRoot / "session_001" / "saves" / "002_manual_save.sav"), "archiver should copy second save");
    requireCondition(!result.entries[0].contentHash.empty(), "archiver should hash archived save copy");

    const auto manifest = strategic_nexus::serializeAutosaveArchiveManifest(result);
    requireCondition(manifest.find("\"copied_count\": 2") != std::string::npos, "manifest JSON should include copied count");
    requireCondition(manifest.find("\"reason\": \"stable_copy\"") != std::string::npos, "manifest JSON should include stable copy reason");
    requireCondition(manifest.find("\"hash_algorithm\": \"fnv1a64\"") != std::string::npos, "manifest JSON should include hash algorithm");

    std::cout << "autosave archiver tests passed.\n";
    return 0;
}

