// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SeasonEmpireBriefBuilder.h"

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

} // namespace

int main()
{
    strategic_nexus::SeasonDeltaLedger ledger;
    ledger.ok = true;
    ledger.reason = "accepted";
    ledger.campaignId = "campaign_001";
    ledger.deltaQuality = "metadata_only";
    ledger.facts.push_back("archive_verified:true");
    ledger.facts.push_back(std::string("archived_save_count:2") + '\x02');
    ledger.uncertainties.push_back("save_content_not_parsed_yet");

    const strategic_nexus::SeasonEmpireBriefBuilder builder;
    const auto brief = builder.build(ledger, "empire_001");

    requireCondition(brief.ok, "brief should accept valid ledger and empire id");
    requireCondition(brief.campaignId == "campaign_001", "brief should preserve campaign id");
    requireCondition(brief.empireId == "empire_001", "brief should preserve empire id");
    requireCondition(brief.sourceLedgerQuality == "metadata_only", "brief should preserve ledger quality");
    requireCondition(brief.relevantFacts.size() == 2, "brief should carry ledger facts");
    requireCondition(brief.explicitUncertainties.size() == 1, "brief should carry ledger uncertainties");
    requireCondition(brief.missingInformation.size() == 4, "brief should declare missing parser information");
    requireCondition(
        brief.compressionNotes.size() == 2 && brief.compressionNotes[0] == "metadata_only_archive_brief",
        "metadata-only brief should advertise metadata-only compression");

    const auto json = strategic_nexus::serializeSeasonEmpireBrief(brief);
    requireCondition(json.find("\"empire_id\": \"empire_001\"") != std::string::npos, "brief JSON should include empire id");
    requireCondition(json.find("\"source_ledger_quality\": \"metadata_only\"") != std::string::npos, "brief JSON should include quality");
    requireCondition(json.find("do_not_infer_personality_or_strategy_from_this_brief_alone") != std::string::npos, "brief JSON should include caution note");
    requireCondition(json.find("\\u0002") != std::string::npos, "brief JSON should escape control characters");

    const auto rejected = builder.build(ledger, "");
    requireCondition(!rejected.ok, "brief should reject missing empire id");
    requireCondition(rejected.reason == "missing empire id", "brief should explain missing empire id");
    requireCondition(rejected.missingInformation.empty(), "rejected brief should not emit missing-information payload");
    requireCondition(rejected.compressionNotes.empty(), "rejected brief should not emit compression notes");

    const auto whitespaceRejected = builder.build(ledger, " \t\r\n");
    requireCondition(!whitespaceRejected.ok, "brief should reject whitespace-only empire id");
    requireCondition(whitespaceRejected.reason == "missing empire id", "brief should treat whitespace empire id as missing");

    strategic_nexus::SeasonDeltaLedger missingCampaignLedger = ledger;
    missingCampaignLedger.campaignId = " \t\r\n";
    const auto missingCampaign = builder.build(missingCampaignLedger, "empire_001");
    requireCondition(!missingCampaign.ok, "brief should reject missing campaign id");
    requireCondition(missingCampaign.reason == "missing campaign id", "brief should explain missing campaign id");

    strategic_nexus::SeasonDeltaLedger unsupportedQualityLedger = ledger;
    unsupportedQualityLedger.deltaQuality = "future_quality";
    const auto unsupportedQuality = builder.build(unsupportedQualityLedger, "empire_001");
    requireCondition(!unsupportedQuality.ok, "brief should reject unsupported source ledger quality");
    requireCondition(
        unsupportedQuality.reason == "unsupported source ledger quality",
        "brief should explain unsupported source ledger quality");

    strategic_nexus::SeasonDeltaLedger parsedHeadlineLedger = ledger;
    parsedHeadlineLedger.deltaQuality = "metadata_plus_save_headline";
    const auto parsedHeadlineBrief = builder.build(parsedHeadlineLedger, "empire_001");
    requireCondition(
        parsedHeadlineBrief.ok,
        "brief should accept metadata_plus_save_headline ledger quality");
    requireCondition(
        parsedHeadlineBrief.missingInformation.size() == 5,
        "headline-enhanced brief should still declare full save extraction gap");
    requireCondition(
        parsedHeadlineBrief.missingInformation[4] == "full_save_state_still_not_extracted",
        "headline-enhanced brief should name remaining full-save extraction gap");
    requireCondition(
        parsedHeadlineBrief.compressionNotes.size() == 2 &&
            parsedHeadlineBrief.compressionNotes[0] == "metadata_plus_headline_archive_brief",
        "headline-enhanced brief should advertise metadata-plus-headline compression");

    std::cout << "season empire brief builder tests passed.\n";
    return 0;
}

