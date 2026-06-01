// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct AutosaveArchiveEntry {
    std::string sourcePath;
    std::string archivedPath;
    std::string status;
    std::string reason;
    std::uintmax_t byteCount = 0;
    std::string hashAlgorithm;
    std::string contentHash;
};

struct AutosaveArchiveResult {
    bool sourceExists = false;
    std::filesystem::path sourceRoot;
    std::filesystem::path archiveDirectory;
    std::string sessionId;
    std::size_t copiedCount = 0;
    std::size_t skippedCount = 0;
    std::vector<AutosaveArchiveEntry> entries;
};

class AutosaveArchiver {
public:
    AutosaveArchiveResult archiveStableSaves(
        const std::filesystem::path& saveRoot,
        const std::filesystem::path& archiveRoot,
        const std::string& sessionId,
        std::uint32_t stabilityDelayMs = 250) const;

    AutosaveArchiveResult archiveLiveSaves(
        const std::filesystem::path& saveGamesRoot,
        const std::filesystem::path& archiveRoot,
        const std::string& sessionId,
        std::uint32_t stabilityDelayMs = 250) const;

    AutosaveArchiveResult writeLiveArchiveManifest(
        const std::filesystem::path& archiveRoot,
        const std::string& sessionId,
        const std::filesystem::path& sourceRootLabel,
        bool sourceExists) const;
};

std::string serializeAutosaveArchiveManifest(const AutosaveArchiveResult& result);

} // namespace strategic_nexus

