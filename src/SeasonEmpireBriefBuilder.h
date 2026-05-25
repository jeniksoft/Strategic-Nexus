// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "SeasonDeltaLedgerBuilder.h"

#include <string>
#include <vector>

namespace strategic_nexus {

struct SeasonEmpireBrief {
    bool ok = false;
    std::string reason;
    std::string campaignId;
    std::string empireId;
    std::string sourceLedgerQuality;
    std::vector<std::string> relevantFacts;
    std::vector<std::string> explicitUncertainties;
    std::vector<std::string> missingInformation;
    std::vector<std::string> compressionNotes;
};

class SeasonEmpireBriefBuilder {
public:
    SeasonEmpireBrief build(
        const SeasonDeltaLedger& ledger,
        const std::string& empireId) const;
};

std::string serializeSeasonEmpireBrief(const SeasonEmpireBrief& brief);

} // namespace strategic_nexus

