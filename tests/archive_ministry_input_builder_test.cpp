// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "ArchiveMinistryInputBuilder.h"

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
    strategic_nexus::AutosaveArchiveSummary summary;
    summary.ok = true;
    summary.copiedSaveCount = 2;
    summary.totalByteCount = 42;
    summary.firstContentHash = "first_hash";
    summary.lastContentHash = "last_hash";

    const strategic_nexus::ArchiveMinistryInputBuilder builder;
    const auto input = builder.build(summary, "campaign_001", "empire_001", "military");

    requireCondition(input.schemaVersion == 1, "builder should emit schema v1");
    requireCondition(input.campaignId == "campaign_001", "builder should preserve campaign id");
    requireCondition(input.empireId == "empire_001", "builder should preserve empire id");
    requireCondition(input.ministry == "military", "builder should preserve ministry");
    requireCondition(input.knownFacts.size() == 4, "builder should emit bounded archive facts");
    requireCondition(input.uncertainties.size() == 2, "builder should emit explicit archive uncertainties");
    requireCondition(input.currentMilitaryPosture == "defensive", "builder should emit safe military fallback");
    requireCondition(input.currentResearchBias == "economy", "builder should emit safe research fallback");
    requireCondition(
        input.uncertainties[0] == "save_content_not_parsed_yet",
        "metadata-only build should keep explicit parser uncertainty");

    strategic_nexus::SeasonDeltaLedger ledger;
    ledger.ok = true;
    ledger.campaignId = "campaign_ledger";
    ledger.deltaQuality = "metadata_plus_save_headline";
    ledger.facts = {"archive_verified:true", "save_headline_parsed:true", "headline_active_war_count:1"};
    ledger.uncertainties = {"empire_state_not_extracted_yet"};

    const auto ledgerInput = builder.build(ledger, "empire_ledger", "research");
    requireCondition(ledgerInput.campaignId == "campaign_ledger", "ledger build should preserve campaign id");
    requireCondition(ledgerInput.empireId == "empire_ledger", "ledger build should preserve empire id");
    requireCondition(ledgerInput.ministry == "research", "ledger build should preserve ministry");
    requireCondition(ledgerInput.knownFacts.size() == 5, "ledger build should forward parsed facts and add war hints");
    requireCondition(
        ledgerInput.knownFacts[1] == "save_headline_parsed:true",
        "ledger build should forward parsed-headline marker");
    requireCondition(
        ledgerInput.turnContext.isAtWar,
        "headline-backed ledger should set is_at_war when active war count is non-zero");
    requireCondition(
        ledgerInput.knownFacts.size() == 5,
        "headline-backed ledger should append bounded turn-context hint facts");
    requireCondition(
        ledgerInput.knownFacts[3] == "turn_context_war_hint_source:headline_active_war_count",
        "headline-backed ledger should mark war hint source");
    requireCondition(
        ledgerInput.knownFacts[4] == "turn_context_war_hint_confidence_percent:70",
        "headline-backed ledger should include explicit war hint confidence");
    requireCondition(
        ledgerInput.uncertainties.size() == 1,
        "headline-backed ledger should not force metadata-only uncertainty");

    strategic_nexus::SeasonDeltaLedger badWarLedger = ledger;
    badWarLedger.facts = {"archive_verified:true", "save_headline_parsed:true", "headline_active_war_count:abc"};
    const auto badWarInput = builder.build(badWarLedger, "empire_ledger", "research");
    requireCondition(!badWarInput.turnContext.isAtWar, "invalid war count should fail closed to not-at-war");
    requireCondition(
        badWarInput.uncertainties.size() == 2 && badWarInput.uncertainties[1] == "headline_war_count_invalid",
        "invalid war count should be surfaced as explicit uncertainty");

    std::cout << "archive ministry input builder tests passed.\n";
    return 0;
}

