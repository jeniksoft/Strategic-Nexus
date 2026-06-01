// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "AutosaveArchiver.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <system_error>
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

std::size_t countSavFiles(const std::filesystem::path& path)
{
    std::size_t count = 0;
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return 0;
    }
    for (const auto& entry : std::filesystem::directory_iterator(path, error)) {
        if (error) {
            break;
        }
        if (entry.is_regular_file(error) && entry.path().extension() == ".sav") {
            ++count;
        }
    }
    return count;
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

    std::error_code error;
    const auto baseTime = std::filesystem::file_time_type::clock::now();
    std::filesystem::last_write_time(sourceRoot / "manual_save.sav", baseTime - std::chrono::seconds(5), error);
    std::filesystem::last_write_time(sourceRoot / "autosave_2230.sav", baseTime + std::chrono::seconds(5), error);

    const strategic_nexus::AutosaveArchiver archiver;
    const auto result = archiver.archiveStableSaves(sourceRoot, archiveRoot, "session_001", 0);

    requireCondition(result.sourceExists, "archiver should mark existing source root");
    requireCondition(result.copiedCount == 2, "archiver should copy stable .sav files");
    requireCondition(result.skippedCount == 0, "archiver should ignore non-save files without recording skips");
    requireCondition(std::filesystem::exists(archiveRoot / "session_001" / "manifest.json"), "archiver should write manifest");
    requireCondition(std::filesystem::exists(archiveRoot / "session_001" / "saves" / "001_manual_save.sav"), "archiver should copy oldest save first");
    requireCondition(std::filesystem::exists(archiveRoot / "session_001" / "saves" / "002_autosave_2230.sav"), "archiver should copy newest save second");
    requireCondition(!result.entries[0].contentHash.empty(), "archiver should hash archived save copy");

    const auto manifest = strategic_nexus::serializeAutosaveArchiveManifest(result);
    requireCondition(manifest.find("\"copied_count\": 2") != std::string::npos, "manifest JSON should include copied count");
    requireCondition(manifest.find("\"reason\": \"stable_copy\"") != std::string::npos, "manifest JSON should include stable copy reason");
    requireCondition(manifest.find("\"hash_algorithm\": \"fnv1a64\"") != std::string::npos, "manifest JSON should include hash algorithm");

    const auto liveSourceRoot = std::filesystem::path("dist/autosave_archiver_live_source");
    const auto liveArchiveRoot = std::filesystem::path("dist/autosave_archiver_live_archive");
    std::filesystem::remove_all(liveSourceRoot);
    std::filesystem::remove_all(liveArchiveRoot);

    writeFile(liveSourceRoot / "campaign_a" / "autosave_2200.01.01.sav", "live autosave one");
    writeFile(liveSourceRoot / "campaign_b" / "ironman.sav", "live ironman one");
    writeFile(liveSourceRoot / "campaign_b" / "2200.01.01.sav", "date save ignored by live capture");

    const auto liveResult = archiver.archiveLiveSaves(liveSourceRoot, liveArchiveRoot, "live_session", 0);
    requireCondition(liveResult.sourceExists, "live archiver should mark existing save games root");
    requireCondition(liveResult.copiedCount == 2, "live archiver should copy autosave and ironman candidates");
    requireCondition(countSavFiles(liveArchiveRoot / "live_session" / "saves") == 2, "live archiver should store two unique revisions");
    requireCondition(std::filesystem::exists(liveArchiveRoot / "live_session" / "manifest.json"), "live archiver should write aggregate manifest");

    const auto repeatedLiveResult = archiver.archiveLiveSaves(liveSourceRoot, liveArchiveRoot, "live_session", 0);
    requireCondition(repeatedLiveResult.copiedCount == 0, "live archiver should not duplicate unchanged revisions");
    requireCondition(repeatedLiveResult.skippedCount == 2, "live archiver should report already archived live saves");
    requireCondition(countSavFiles(liveArchiveRoot / "live_session" / "saves") == 2, "live archiver should keep stable archive count for unchanged files");

    writeFile(liveSourceRoot / "campaign_a" / "autosave_2200.01.01.sav", "live autosave two");
    const auto changedLiveResult = archiver.archiveLiveSaves(liveSourceRoot, liveArchiveRoot, "live_session", 0);
    requireCondition(changedLiveResult.copiedCount == 1, "live archiver should capture a changed autosave revision");
    requireCondition(countSavFiles(liveArchiveRoot / "live_session" / "saves") == 3, "live archiver should retain old and new autosave revisions");

    writeFile(liveSourceRoot / "campaign_c" / "autosave_2200.02.01.sav", "live autosave from new campaign");
    const auto newCampaignLiveResult = archiver.archiveLiveSaves(liveSourceRoot, liveArchiveRoot, "live_session", 0);
    requireCondition(newCampaignLiveResult.copiedCount == 1, "live archiver should detect newly added campaign folders");
    requireCondition(countSavFiles(liveArchiveRoot / "live_session" / "saves") == 4, "live archiver should archive autosaves from new campaign folders");

    const auto aggregateManifestResult = archiver.writeLiveArchiveManifest(
        liveArchiveRoot,
        "live_session",
        std::filesystem::path("multiple_stellaris_save_roots"),
        true);
    requireCondition(aggregateManifestResult.copiedCount == 4, "aggregate live manifest should describe all archived revisions");

    const auto liveManifestText = [&]() {
        std::ifstream input(liveArchiveRoot / "live_session" / "manifest.json");
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }();
    requireCondition(liveManifestText.find("\"copied_count\": 4") != std::string::npos, "live manifest should describe all archived revisions");
    requireCondition(
        liveManifestText.find("\"source_root\": \"multiple_stellaris_save_roots\"") != std::string::npos,
        "aggregate live manifest should avoid claiming a single source root");
    requireCondition(liveManifestText.find("\"reason\": \"live_archive_copy\"") != std::string::npos, "live manifest should mark aggregate live copies");

    std::cout << "autosave archiver tests passed.\n";
    return 0;
}

