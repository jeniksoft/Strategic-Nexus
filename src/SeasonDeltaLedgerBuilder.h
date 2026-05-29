// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "AutosaveArchiveSummarizer.h"

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {
struct SaveParserSummary;

struct SeasonDeltaLedger {
    bool ok = false;
    std::string reason;
    std::filesystem::path sessionArchiveDirectory;
    std::string campaignId;
    std::string deltaQuality;
    std::size_t copiedSaveCount = 0;
    std::uintmax_t totalByteCount = 0;
    std::string firstArchivedPath;
    std::string firstContentHash;
    std::string lastArchivedPath;
    std::string lastContentHash;
    std::vector<std::string> facts;
    std::vector<std::string> uncertainties;
};

class SeasonDeltaLedgerBuilder {
public:
    SeasonDeltaLedger build(
        const AutosaveArchiveSummary& summary,
        const std::string& campaignId) const;
    SeasonDeltaLedger build(
        const AutosaveArchiveSummary& summary,
        const std::string& campaignId,
        const SaveParserSummary* parsedHeadline) const;
};

std::string serializeSeasonDeltaLedger(const SeasonDeltaLedger& ledger);

} // namespace strategic_nexus

