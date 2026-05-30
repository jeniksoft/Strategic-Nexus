// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "AutosaveArchiveSummarizer.h"
#include "SeasonDeltaLedgerBuilder.h"
#include "strategic_pipeline/StrategicPipelineTypes.h"

#include <string>

namespace strategic_nexus {

class ArchiveMinistryInputBuilder {
public:
    strategic_pipeline::MinistryInputContext build(
        const AutosaveArchiveSummary& summary,
        const std::string& campaignId,
        const std::string& empireId,
        const std::string& ministry) const;
    strategic_pipeline::MinistryInputContext build(
        const SeasonDeltaLedger& ledger,
        const std::string& empireId,
        const std::string& ministry) const;
};

} // namespace strategic_nexus

