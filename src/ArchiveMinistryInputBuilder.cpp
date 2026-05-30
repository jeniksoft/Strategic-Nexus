// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "ArchiveMinistryInputBuilder.h"

#include <cctype>
#include <limits>
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

bool parseNonNegativeInteger(const std::string& value, std::size_t& outValue)
{
    if (value.empty()) {
        return false;
    }

    std::size_t parsed = 0;
    for (const char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
        const std::size_t digit = static_cast<std::size_t>(ch - '0');
        if (parsed > ((std::numeric_limits<std::size_t>::max() - digit) / 10)) {
            return false;
        }
        parsed = (parsed * 10) + digit;
    }

    outValue = parsed;
    return true;
}

bool parseFactValue(const std::string& fact, const char* key, std::string& outValue)
{
    const std::string prefix = std::string(key) + ":";
    if (fact.rfind(prefix, 0) != 0) {
        return false;
    }

    outValue = fact.substr(prefix.size());
    return true;
}

bool parseSaveDate(const std::string& value, std::size_t& outYear, std::size_t& outMonth, std::size_t& outDay)
{
    if (value.size() != 10) {
        return false;
    }
    if (value[4] != '.' || value[7] != '.') {
        return false;
    }

    if (!parseNonNegativeInteger(value.substr(0, 4), outYear) ||
        !parseNonNegativeInteger(value.substr(5, 2), outMonth) ||
        !parseNonNegativeInteger(value.substr(8, 2), outDay)) {
        return false;
    }

    if (outMonth < 1 || outMonth > 12) {
        return false;
    }
    if (outDay < 1 || outDay > 31) {
        return false;
    }
    return true;
}

void appendUncertaintyIfMissing(strategic_pipeline::MinistryInputContext& input, const std::string& value)
{
    for (const auto& existing : input.uncertainties) {
        if (existing == value) {
            return;
        }
    }
    input.uncertainties.push_back(value);
}

void applyParsedHeadlineWarHint(const SeasonDeltaLedger& ledger, strategic_pipeline::MinistryInputContext& input)
{
    if (ledger.deltaQuality != "metadata_plus_save_headline") {
        return;
    }

    std::string rawWarCount;
    for (const auto& fact : ledger.facts) {
        if (parseFactValue(fact, "headline_active_war_count", rawWarCount)) {
            break;
        }
    }

    if (rawWarCount.empty()) {
        appendUncertaintyIfMissing(input, "headline_war_count_missing");
        return;
    }

    std::size_t warCount = 0;
    if (!parseNonNegativeInteger(rawWarCount, warCount)) {
        appendUncertaintyIfMissing(input, "headline_war_count_invalid");
        return;
    }

    input.turnContext.isAtWar = warCount > 0;
    input.knownFacts.push_back("turn_context_war_hint_source:headline_active_war_count");
    input.knownFacts.push_back(countFact("turn_context_war_hint_confidence_percent", 70));
}

void applyParsedHeadlineYearHint(const SeasonDeltaLedger& ledger, strategic_pipeline::MinistryInputContext& input)
{
    if (ledger.deltaQuality != "metadata_plus_save_headline") {
        return;
    }

    std::string rawSaveDate;
    for (const auto& fact : ledger.facts) {
        if (parseFactValue(fact, "save_date", rawSaveDate)) {
            break;
        }
    }

    if (rawSaveDate.empty()) {
        appendUncertaintyIfMissing(input, "headline_save_date_missing");
        return;
    }

    std::size_t year = 0;
    std::size_t month = 0;
    std::size_t day = 0;
    if (!parseSaveDate(rawSaveDate, year, month, day)) {
        appendUncertaintyIfMissing(input, "headline_save_date_invalid");
        return;
    }

    input.turnContext.year = static_cast<int>(year);
    input.knownFacts.push_back("turn_context_year_hint_source:save_date");
    input.knownFacts.push_back(countFact("turn_context_year_hint_confidence_percent", 80));
}

void applyParsedHeadlineMonthHint(const SeasonDeltaLedger& ledger, strategic_pipeline::MinistryInputContext& input)
{
    if (ledger.deltaQuality != "metadata_plus_save_headline") {
        return;
    }

    std::string rawSaveDate;
    for (const auto& fact : ledger.facts) {
        if (parseFactValue(fact, "save_date", rawSaveDate)) {
            break;
        }
    }

    if (rawSaveDate.empty()) {
        appendUncertaintyIfMissing(input, "headline_save_date_missing");
        return;
    }

    std::size_t year = 0;
    std::size_t month = 0;
    std::size_t day = 0;
    if (!parseSaveDate(rawSaveDate, year, month, day)) {
        appendUncertaintyIfMissing(input, "headline_save_date_invalid");
        return;
    }

    input.knownFacts.push_back(countFact("turn_context_month_hint", month));
    input.knownFacts.push_back("turn_context_month_hint_source:save_date");
    input.knownFacts.push_back(countFact("turn_context_month_hint_confidence_percent", 80));
}

void applyParsedHeadlineDayHint(const SeasonDeltaLedger& ledger, strategic_pipeline::MinistryInputContext& input)
{
    if (ledger.deltaQuality != "metadata_plus_save_headline") {
        return;
    }

    std::string rawSaveDate;
    for (const auto& fact : ledger.facts) {
        if (parseFactValue(fact, "save_date", rawSaveDate)) {
            break;
        }
    }

    if (rawSaveDate.empty()) {
        appendUncertaintyIfMissing(input, "headline_save_date_missing");
        return;
    }

    std::size_t year = 0;
    std::size_t month = 0;
    std::size_t day = 0;
    if (!parseSaveDate(rawSaveDate, year, month, day)) {
        appendUncertaintyIfMissing(input, "headline_save_date_invalid");
        return;
    }

    input.knownFacts.push_back(countFact("turn_context_day_hint", day));
    input.knownFacts.push_back("turn_context_day_hint_source:save_date");
    input.knownFacts.push_back(countFact("turn_context_day_hint_confidence_percent", 80));
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
        appendUncertaintyIfMissing(input, "archive_ledger_not_verified");
    }
    applyParsedHeadlineWarHint(ledger, input);
    applyParsedHeadlineYearHint(ledger, input);
    applyParsedHeadlineMonthHint(ledger, input);
    applyParsedHeadlineDayHint(ledger, input);
    if (ledger.deltaQuality != "metadata_plus_save_headline") {
        appendUncertaintyIfMissing(input, "save_content_not_parsed_yet");
    }
    input.currentMilitaryPosture = "defensive";
    input.currentResearchBias = "economy";
    return input;
}

} // namespace strategic_nexus

