// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "strategic_pipeline/MinistryInputReader.h"
#include "common/FileUtil.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <algorithm>

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
    const std::filesystem::path outputPath = "dist/v0_ministry_input_reader_fixture.json";
    const std::string json =
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"context_id\": \"archive_session_summary_v0\",\n"
        "  \"campaign_id\": \"campaign_cli\",\n"
        "  \"empire_id\": \"empire_cli\",\n"
        "  \"ministry\": \"military\",\n"
        "  \"turn_context\": {\"year\": 2230, \"is_at_war\": true, \"strategic_pressure\": 0.6},\n"
        "  \"known_facts\": [\n"
        "    \"archive_verified:true\",\n"
        "    \"archived_save_count:1\",\n"
        "    \"archived_total_byte_count:449\",\n"
        "    \"first_archive_hash:a\",\n"
        "    \"last_archive_hash:b\",\n"
        "    \"save_headline_parsed:true\",\n"
        "    \"player_country_id:0\",\n"
        "    \"save_date:2230.07.01\",\n"
        "    \"headline_owned_fleet_count:0\",\n"
        "    \"headline_active_war_count:1\",\n"
        "    \"turn_context_war_hint_source:headline_active_war_count\",\n"
        "    \"turn_context_war_hint_confidence_percent:70\",\n"
        "    \"turn_context_year_hint_source:save_date\",\n"
        "    \"turn_context_year_hint_confidence_percent:80\",\n"
        "    \"turn_context_month_hint:7\",\n"
        "    \"turn_context_month_hint_source:save_date\",\n"
        "    \"turn_context_month_hint_confidence_percent:80\",\n"
        "    \"turn_context_day_hint:1\",\n"
        "    \"turn_context_day_hint_source:save_date\",\n"
        "    \"turn_context_day_hint_confidence_percent:80\",\n"
        "    \"turn_context_pressure_year_hint_source:save_date\",\n"
        "    \"turn_context_pressure_year_hint_confidence_percent:60\",\n"
        "    \"turn_context_pressure_war_hint_source:headline_active_war_count\",\n"
        "    \"turn_context_pressure_war_hint_confidence_percent:70\"\n"
        "  ],\n"
        "  \"uncertainties\": [\"empire_state_not_extracted_yet\"],\n"
        "  \"current_agenda\": {\"military_posture\": \"defensive\", \"research_bias\": \"economy\"}\n"
        "}\n";

    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(outputPath, json),
        "fixture JSON should be written");

    strategic_nexus::strategic_pipeline::MinistryInputReader reader;
    const auto result = reader.read(outputPath);

    requireCondition(result.ok, "reader should accept bounded fixture");
    requireCondition(result.error.empty(), "reader should not emit error for valid fixture");
    requireCondition(result.input.knownFacts.size() == 16, "reader should preserve v0 known-fact bound");
    requireCondition(
        result.input.knownFacts[0] == "turn_context_war_hint_source:headline_active_war_count",
        "reader should prioritize turn-context facts");
    requireCondition(
        std::find(
            result.input.knownFacts.begin(),
            result.input.knownFacts.end(),
            "turn_context_pressure_war_hint_confidence_percent:70") != result.input.knownFacts.end(),
        "reader should retain late turn-context pressure hint inside bound");

    const std::filesystem::path missingCampaignPath = "dist/v0_ministry_input_reader_missing_campaign.json";
    const std::string missingCampaignJson =
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"empire_id\": \"empire_cli\",\n"
        "  \"ministry\": \"military\"\n"
        "}\n";
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(missingCampaignPath, missingCampaignJson),
        "missing-campaign fixture JSON should be written");
    const auto missingCampaignResult = reader.read(missingCampaignPath);
    requireCondition(!missingCampaignResult.ok, "reader should reject missing campaign id");
    requireCondition(
        missingCampaignResult.error == "missing required identity fields",
        "missing campaign id should fail closed as missing required identity fields");

    const std::filesystem::path missingEmpirePath = "dist/v0_ministry_input_reader_missing_empire.json";
    const std::string missingEmpireJson =
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"campaign_id\": \"campaign_cli\",\n"
        "  \"ministry\": \"military\"\n"
        "}\n";
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(missingEmpirePath, missingEmpireJson),
        "missing-empire fixture JSON should be written");
    const auto missingEmpireResult = reader.read(missingEmpirePath);
    requireCondition(!missingEmpireResult.ok, "reader should reject missing empire id");
    requireCondition(
        missingEmpireResult.error == "missing required identity fields",
        "missing empire id should fail closed as missing required identity fields");

    std::cout << "v0 ministry input reader tests passed.\n";
    return 0;
}
