// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SeasonDeltaLedgerBuilder.h"

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
    summary.reason = "accepted";
    summary.sessionArchiveDirectory = "dist/session_delta_fixture";
    summary.copiedSaveCount = 3;
    summary.totalByteCount = 64;
    summary.firstArchivedPath = "saves/001_autosave.sav";
    summary.firstContentHash = "first_hash";
    summary.lastArchivedPath = "saves/003_autosave.sav";
    summary.lastContentHash = "last_hash";

    const strategic_nexus::SeasonDeltaLedgerBuilder builder;
    const auto ledger = builder.build(summary, "campaign_001");

    requireCondition(ledger.ok, "ledger should accept verified archive summary");
    requireCondition(ledger.campaignId == "campaign_001", "ledger should preserve campaign id");
    requireCondition(ledger.deltaQuality == "metadata_only", "ledger should expose metadata-only quality");
    requireCondition(ledger.copiedSaveCount == 3, "ledger should preserve copied save count");
    requireCondition(ledger.facts.size() == 5, "ledger should emit bounded metadata facts");
    requireCondition(ledger.uncertainties.size() == 3, "ledger should emit parser uncertainties");

    const auto json = strategic_nexus::serializeSeasonDeltaLedger(ledger);
    requireCondition(json.find("\"campaign_id\": \"campaign_001\"") != std::string::npos, "ledger JSON should include campaign id");
    requireCondition(json.find("\"delta_quality\": \"metadata_only\"") != std::string::npos, "ledger JSON should include quality");
    requireCondition(json.find("\"archive_verified:true\"") != std::string::npos, "ledger JSON should include verification fact");
    requireCondition(json.find("\"save_content_not_parsed_yet\"") != std::string::npos, "ledger JSON should include uncertainty");

    const auto rejected = builder.build(summary, "");
    requireCondition(!rejected.ok, "ledger should reject missing campaign id");
    requireCondition(rejected.reason == "missing campaign id", "ledger should explain missing campaign id");
    requireCondition(rejected.deltaQuality == "unavailable", "rejected ledger should mark delta quality as unavailable");

    strategic_nexus::AutosaveArchiveSummary badSummary;
    badSummary.ok = false;
    badSummary.reason = "archive failure";
    badSummary.sessionArchiveDirectory = "dist/session_delta_fixture";

    const auto badLedger = builder.build(badSummary, "campaign_001");
    requireCondition(!badLedger.ok, "ledger should reject unverified archive summary");
    requireCondition(badLedger.reason == "archive failure", "ledger should preserve archive failure reason");
    requireCondition(badLedger.deltaQuality == "unavailable", "failed ledger should mark delta quality as unavailable");
    requireCondition(badLedger.facts.empty(), "failed ledger should not emit verification facts");
    requireCondition(badLedger.uncertainties.size() == 1, "failed ledger should emit bounded uncertainties");
    requireCondition(badLedger.uncertainties[0] == "archive_not_verified", "failed ledger should explain uncertainty");

    std::cout << "season delta ledger builder tests passed.\n";
    return 0;
}

