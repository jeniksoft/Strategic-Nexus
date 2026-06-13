// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PersonalityProfileStore.h"
#include "RequestFileReader.h"
#include "StrategicWorker.h"
#include "common/FileUtil.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "strategic_worker_profile_reuse_test failed: " << message << "\n";
        std::exit(1);
    }
}

std::filesystem::path fixtureRoot()
{
    return std::filesystem::temp_directory_path() / "strategic_nexus_strategic_worker_profile_reuse_test";
}

} // namespace

int main()
{
    const auto root = fixtureRoot();
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    requireCondition(!ec, "fixture root should be created");

    const auto saveRoot = root / "save_root";
    const auto profileRoot = root / "personality_profiles";
    const auto outputPath = root / "doctrine_output.json";
    const auto requestPath = root / "request.json";
    std::filesystem::create_directories(saveRoot, ec);
    requireCondition(!ec, "save root should be created");

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
}
)";

    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(saveRoot / "meta", meta),
        "meta should be written");
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(saveRoot / "gamestate", gamestate),
        "gamestate should be written");

    strategic_nexus::PersonalityProfileRecord profileRecord;
    profileRecord.ok = true;
    profileRecord.campaignId = "campaign_profile_reuse";
    profileRecord.empireId = "country_0";
    profileRecord.sourceSaveDate = "2026-06-13";
    profileRecord.sourceSavePath = (saveRoot / "gamestate").string();
    profileRecord.validatedUpdateSummary = "validated profile reuse";
    profileRecord.confidence = 0.88;
    profileRecord.zeroHistoryBootstrap = false;
    profileRecord.personality.boldness = 0.12;
    profileRecord.personality.paranoia = 0.91;
    profileRecord.personality.honor = 0.42;
    profileRecord.personality.opportunism = 0.27;

    const strategic_nexus::PersonalityProfileStore profileStore;
    requireCondition(
        profileStore.save(profileRoot, profileRecord),
        "personality profile should be saved");

    const std::string requestJson = R"({
  "request_id": "profile_reuse_request",
  "trigger": "manual",
  "campaign_id": "campaign_profile_reuse",
  "snapshot_path": ")" + saveRoot.string() + R"(",
  "output_doctrine_path": ")" + outputPath.string() + R"(",
  "personality_profile_root": ")" + profileRoot.string() + R"(",
  "async": false
})";
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(requestPath, requestJson),
        "request should be written");

    const strategic_nexus::RequestFileReader requestReader;
    const auto request = requestReader.read(requestPath);
    requireCondition(request.campaignId == "campaign_profile_reuse", "request should parse campaign id");
    requireCondition(request.personalityProfileRoot == profileRoot, "request should parse personality profile root");

    const strategic_nexus::StrategicWorker worker;
    std::ostringstream capturedOut;
    auto* const originalCout = std::cout.rdbuf(capturedOut.rdbuf());
    const int exitCode = worker.processRequestSafely(request);
    std::cout.rdbuf(originalCout);

    requireCondition(exitCode == 0, "strategic worker should succeed");
    requireCondition(std::filesystem::exists(outputPath), "doctrine output should be written");

    const std::string output = capturedOut.str();
    requireCondition(
        output.find("Personality bias: risk-sensitive balancing") != std::string::npos,
        "strategic worker should reuse the validated profile in its prompt bias");
    requireCondition(
        output.find("Validated profile reuse") == std::string::npos,
        "test should verify runtime bias rather than leaked profile summary text");

    std::filesystem::remove_all(root, ec);
    return 0;
}
