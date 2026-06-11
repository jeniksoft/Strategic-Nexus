// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>

namespace strategic_nexus {

struct AutosaveArchiveSummary {
    bool ok = false;
    std::string reason;
    std::size_t schemaVersion = 1;
    std::string schemaCompatibilityState = "current";
    std::string schemaCompatibilityNote;
    std::filesystem::path sessionArchiveDirectory;
    std::size_t copiedSaveCount = 0;
    std::uintmax_t totalByteCount = 0;
    std::string firstArchivedPath;
    std::string firstContentHash;
    std::string lastArchivedPath;
    std::string lastContentHash;
};

class AutosaveArchiveSummarizer {
public:
    AutosaveArchiveSummary summarize(const std::filesystem::path& sessionArchiveDirectory) const;
};

std::string serializeAutosaveArchiveSummary(const AutosaveArchiveSummary& summary);

} // namespace strategic_nexus

