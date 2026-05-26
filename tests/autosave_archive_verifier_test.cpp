// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "AutosaveArchiveVerifier.h"
#include "AutosaveArchiver.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
    const auto sourceRoot = std::filesystem::path("dist/autosave_archive_verifier_source");
    const auto archiveRoot = std::filesystem::path("dist/autosave_archive_verifier_archive");
    std::filesystem::remove_all(sourceRoot);
    std::filesystem::remove_all(archiveRoot);
    writeFile(sourceRoot / "autosave_2230.sav", "fixture");

    const strategic_nexus::AutosaveArchiver archiver;
    const auto archiveResult = archiver.archiveStableSaves(sourceRoot, archiveRoot, "session_001", 0);
    requireCondition(archiveResult.copiedCount == 1, "fixture archive should copy one save");

    const strategic_nexus::AutosaveArchiveVerifier verifier;
    const auto accepted = verifier.verify(archiveRoot / "session_001");
    requireCondition(accepted.ok, "verifier should accept intact archive");
    requireCondition(accepted.files.size() == 1, "verifier should inspect copied save");

    const auto manifestPath = archiveRoot / "session_001" / "manifest.json";
    const auto originalManifest = [&]() {
        std::ifstream input(manifestPath);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }();
    const auto portableManifest = std::regex_replace(
        originalManifest,
        std::regex("\"archived_path\"\\s*:\\s*\"[^\"]*/001_autosave_2230\\.sav\""),
        "\"archived_path\": \"001_autosave_2230.sav\"");
    writeFile(manifestPath, portableManifest);
    const auto portableAccepted = verifier.verify(archiveRoot / "session_001");
    requireCondition(portableAccepted.ok, "verifier should resolve archive-relative copied save filenames");

    writeFile(archiveRoot / "session_001" / "saves" / "unexpected.sav", "fixture");
    const auto extraFileRejected = verifier.verify(archiveRoot / "session_001");
    requireCondition(!extraFileRejected.ok, "verifier should reject unexpected archive files");
    requireCondition(
        extraFileRejected.reason == "autosave archive contains unexpected files",
        "verifier should explain unexpected file rejection");
    std::filesystem::remove(archiveRoot / "session_001" / "saves" / "unexpected.sav");

    const auto outsidePath = std::filesystem::absolute(std::filesystem::path("dist/autosave_archive_verifier_outside/evil.sav"));
    writeFile(outsidePath, "fixture");
    const auto unsafeManifest = std::regex_replace(
        portableManifest,
        std::regex("\"archived_path\"\\s*:\\s*\"[^\"]+\""),
        "\"archived_path\": \"" + outsidePath.generic_string() + "\"");
    writeFile(manifestPath, unsafeManifest);
    const auto unsafeRejected = verifier.verify(archiveRoot / "session_001");
    requireCondition(!unsafeRejected.ok, "verifier should reject unsafe archived_path outside the session archive");
    requireCondition(
        unsafeRejected.reason == "autosave archive manifest contains unsafe archived_path",
        "verifier should explain unsafe archived_path rejection");

    writeFile(manifestPath, portableManifest);
    writeFile(archiveRoot / "session_001" / "saves" / "001_autosave_2230.sav", "tampered");
    const auto rejected = verifier.verify(archiveRoot / "session_001");
    requireCondition(!rejected.ok, "verifier should reject tampered archive");
    requireCondition(rejected.reason == "autosave archive files do not match manifest", "verifier should explain mismatch");

    std::cout << "autosave archive verifier tests passed.\n";
    return 0;
}

