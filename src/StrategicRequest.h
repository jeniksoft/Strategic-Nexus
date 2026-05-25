// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>

namespace strategic_nexus {

enum class RequestTrigger {
    Manual,
    ScheduledDecade,
    WarDeclared,
    FederationFormed,
    CrisisStageChanged,
    EmpireCollapsed,
    HegemonyDetected,
    EndgameStarted
};

struct StrategicRequest {
    std::string requestId = "manual";
    RequestTrigger trigger = RequestTrigger::Manual;
    std::filesystem::path snapshotPath;
    std::filesystem::path outputDoctrinePath = "resources/doctrine_output.json";
    bool asynchronous = true;
};

const char* toString(RequestTrigger trigger);
RequestTrigger requestTriggerFromString(const std::string& value);

} // namespace strategic_nexus

