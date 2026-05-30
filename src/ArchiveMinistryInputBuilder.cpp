// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "ArchiveMinistryInputBuilder.h"

#include <sstream>

namespace strategic_nexus {
namespace {

std::string countFact(const char* name, const std::size_t value)
{
    std::ostringstream text;
    text << name << ":" << value;
    return text.str();
}

std::string byteFact(const char* name, const std::uintmax_t value)
{
    std::ostringstream text;
    text << name << ":" << value;
    return text.str();
}

} // namespace

strategic_pipeline::MinistryInputContext ArchiveMinistryInputBuilder::build(
    const AutosaveArchiveSummary& summary,
    const std::string& campaignId,
    const std::string& empireId,
    const std::string& ministry) const
{
    strategic_pipeline::MinistryInputContext input;
    input.schemaVersion = 1;
    input.contextId = "archive_session_summary_v0";
    input.campaignId = campaignId;
    input.empireId = empireId;
    input.ministry = ministry;
    input.turnContext.year = 0;
    input.turnContext.isAtWar = false;
    input.turnContext.strategicPressure = 0.25;
    input.knownFacts.push_back(countFact("archived_save_count", summary.copiedSaveCount));
    input.knownFacts.push_back(byteFact("archived_total_byte_count", summary.totalByteCount));
    if (!summary.firstContentHash.empty()) {
        input.knownFacts.push_back("first_archive_hash:" + summary.firstContentHash);
    }
    if (!summary.lastContentHash.empty()) {
        input.knownFacts.push_back("last_archive_hash:" + summary.lastContentHash);
    }
    input.uncertainties.push_back("save_content_not_parsed_yet");
    input.uncertainties.push_back("war_state_unknown_from_archive_metadata");
    input.currentMilitaryPosture = "defensive";
    input.currentResearchBias = "economy";
    return input;
}

strategic_pipeline::MinistryInputContext ArchiveMinistryInputBuilder::build(
    const SeasonDeltaLedger& ledger,
    const std::string& empireId,
    const std::string& ministry) const
{
    strategic_pipeline::MinistryInputContext input;
    input.schemaVersion = 1;
    input.contextId = "archive_session_summary_v0";
    input.campaignId = ledger.campaignId;
    input.empireId = empireId;
    input.ministry = ministry;
    input.turnContext.year = 0;
    input.turnContext.isAtWar = false;
    input.turnContext.strategicPressure = 0.25;
    input.knownFacts = ledger.facts;
    input.uncertainties = ledger.uncertainties;
    if (!ledger.ok) {
        input.uncertainties.push_back("archive_ledger_not_verified");
    }
    if (ledger.deltaQuality != "metadata_plus_save_headline") {
        input.uncertainties.push_back("save_content_not_parsed_yet");
    }
    input.currentMilitaryPosture = "defensive";
    input.currentResearchBias = "economy";
    return input;
}

} // namespace strategic_nexus

