// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StellarisSavePathResolver.h"

#include <filesystem>
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
    const auto root = std::filesystem::path("dist/stellaris_save_path_resolver_fixture");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(
        root / "User" / "Documents" / "Paradox Interactive" / "Stellaris" / "save games");
    std::filesystem::create_directories(
        root / "Steam" / "userdata" / "123456" / "281990" / "remote" / "save games");

    const strategic_nexus::StellarisSavePathResolver resolver;
    const auto discovery = resolver.buildCandidates(
        root / "User",
        {root / "OneDrive"},
        {root / "Steam" / "userdata", root / "STEAM" / "userdata"});

    requireCondition(discovery.candidates.size() == 4, "resolver should emit user, OneDrive, and deduplicated Steam Cloud candidates");
    requireCondition(discovery.candidates[0].exists, "resolver should mark existing user Documents save root");
    requireCondition(!discovery.candidates[1].exists, "resolver should mark missing OneDrive Documents save root");
    requireCondition(discovery.candidates[2].source == "onedrive_dokumenty", "resolver should include localized OneDrive Dokumenty candidate");
    requireCondition(discovery.candidates[3].exists, "resolver should mark existing Steam Cloud save root");
    requireCondition(discovery.candidates[3].source == "steam_cloud_userdata", "resolver should include Steam Cloud userdata candidate");

    const auto json = strategic_nexus::serializeStellarisSaveRootDiscovery(discovery);
    requireCondition(json.find("\"candidate_count\": 4") != std::string::npos, "discovery JSON should include candidate count");
    requireCondition(json.find("\"source\": \"user_profile_documents\"") != std::string::npos, "discovery JSON should include source");
    requireCondition(json.find("\"source\": \"steam_cloud_userdata\"") != std::string::npos, "discovery JSON should include Steam source");
    requireCondition(json.find("\"exists\": true") != std::string::npos, "discovery JSON should include exists flag");

    std::cout << "stellaris save path resolver tests passed.\n";
    return 0;
}

