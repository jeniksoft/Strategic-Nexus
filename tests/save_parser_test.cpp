// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SaveParser.h"
#include "common/FileUtil.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "save_parser_test failed: " << message << "\n";
        std::exit(1);
    }
}

std::filesystem::path fixtureRoot()
{
    return std::filesystem::temp_directory_path() / "strategic_nexus_save_parser_test";
}

const strategic_nexus::SaveParserFieldAvailability* findFieldAvailability(
    const std::vector<strategic_nexus::SaveParserFieldAvailability>& fields,
    const std::string& fieldGroup)
{
    for (const auto& field : fields) {
        if (field.fieldGroup == fieldGroup) {
            return &field;
        }
    }
    return nullptr;
}

} // namespace

int main()
{
    const auto root = fixtureRoot();
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    requireCondition(!ec, "fixture directory should be created");

    const std::string meta = R"(version="v4.0.22"
revision="abcdef"
name="Synthetic Aeel Corp"
)";

    const std::string gamestate = R"(date="2200.01.13"
player={
    {
        name="Jeniksoft"
        country=0
    }
}
country={
    0={
        name="Aeel Corp"
        government="gov_megacorporation"
        authority="auth_corporate"
        founder_species=7
        capital=42
        starting_system=9
    }
}
species={
    7={
        name="Aeel"
    }
}
planet={
    42={
        name="Aeel Prime"
    }
}
galactic_object={
    9={
        name="Aeel System"
    }
}
fleet={
    100={
        name="First Survey Fleet"
        owner=0
        military_power=17
    }
    101={
        name="Foreign Fleet"
        owner=1
        military_power=99
    }
}
war={
    3={
        name="Synthetic War"
    }
}
)";

    requireCondition(strategic_nexus::common::writeTextFileAtomically(root / "meta", meta), "meta should be written");
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(root / "gamestate", gamestate),
        "gamestate should be written");

    const strategic_nexus::SaveParser parser;
    const auto summary = parser.parseSummary(root);
    requireCondition(summary.ok, "summary should parse");
    requireCondition(summary.reason == "parsed headline summary", "summary reason should be stable");
    requireCondition(summary.saveVersion == "v4.0.22", "save version should parse");
    requireCondition(summary.saveDate == "2200.01.13", "save date should parse");
    requireCondition(summary.playerName == "Jeniksoft", "player name should parse");
    requireCondition(summary.playerCountryId == "0", "player country should parse");
    requireCondition(summary.empireName == "Aeel Corp", "empire name should parse");
    requireCondition(summary.founderSpeciesName == "Aeel", "founder species should parse");
    requireCondition(summary.capitalPlanetName == "Aeel Prime", "capital planet should parse");
    requireCondition(summary.homeSystemName == "Aeel System", "home system should parse");
    requireCondition(summary.ownedFleetCount == 1, "owned fleet count should filter by owner");
    requireCondition(summary.activeWarCount == 1, "active war count should parse");
    requireCondition(summary.fieldAvailability.size() >= 6, "field availability map should be populated");

    const auto* identityAvailability = findFieldAvailability(summary.fieldAvailability, "identity");
    requireCondition(identityAvailability != nullptr, "identity availability should exist");
    requireCondition(identityAvailability->available, "identity should be available");
    requireCondition(
        identityAvailability->sourceQuality == "headline_identity",
        "identity should have headline identity source quality");

    const auto* fleetsAvailability = findFieldAvailability(summary.fieldAvailability, "owned_fleets");
    requireCondition(fleetsAvailability != nullptr, "owned fleets availability should exist");
    requireCondition(fleetsAvailability->available, "owned fleets should be available");
    requireCondition(
        fleetsAvailability->sourceQuality == "headline_owned_fleets",
        "owned fleets should have headline owned fleets source quality");

    const auto* countryCoreAvailability = findFieldAvailability(summary.fieldAvailability, "country_core");
    requireCondition(countryCoreAvailability != nullptr, "country core availability should exist");
    requireCondition(countryCoreAvailability->available, "country core should be available");
    requireCondition(
        countryCoreAvailability->sourceQuality == "headline_country_core",
        "country core should have headline country core source quality");

    const auto* diplomacyAvailability = findFieldAvailability(summary.fieldAvailability, "diplomacy");
    requireCondition(diplomacyAvailability != nullptr, "diplomacy availability should exist");
    requireCondition(!diplomacyAvailability->available, "diplomacy should remain unavailable");
    requireCondition(
        diplomacyAvailability->sourceQuality == "not_extracted_yet",
        "diplomacy should report not extracted yet source quality");
    requireCondition(
        !diplomacyAvailability->missingReasons.empty() &&
            diplomacyAvailability->missingReasons.front() == "diplomatic_relationships_not_extracted_yet",
        "diplomacy should explain the missing relationship evidence");

    const std::string json = parser.parseSummaryJson(root);
    requireCondition(json.find("\"empire_name\": \"Aeel Corp\"") != std::string::npos, "json should include empire name");
    requireCondition(
        json.find("\"field_group\":\"diplomacy\"") != std::string::npos,
        "json should include diplomacy field availability");
    requireCondition(
        json.find("\"field_group\":\"country_core\"") != std::string::npos,
        "json should include country core field availability");
    requireCondition(
        json.find("\"source_quality\":\"headline_owned_fleets\"") != std::string::npos,
        "json should include owned fleets source quality");
    requireCondition(
        json.find("llm_input_must_use_this_bounded_summary_not_raw_gamestate") != std::string::npos,
        "json should include LLM raw-input safety uncertainty");

    const auto galaxy = parser.parseSnapshot(root);
    requireCondition(!galaxy.empires.empty(), "legacy snapshot should still return a player empire");
    requireCondition(galaxy.empires.front().displayName == "Aeel Corp", "legacy snapshot should use parsed empire name");

    std::filesystem::remove_all(root, ec);
    std::cout << "save parser tests passed.\n";
    return 0;
}
