// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <string>
#include <vector>

namespace strategic_nexus::generated_overlay {

struct DslCondition {
    std::string source;
    std::string op;
    std::string value;
};

struct DslPreference {
    std::string domain;
    std::string value;
    double intensity = 1.0;
};

struct DslRule {
    std::string campaignId;
    std::string empireId;
    std::string ruleId;
    std::string ministry;
    std::vector<DslCondition> conditions;
    std::vector<DslPreference> preferences;
    std::string duration = "next_session";
    double confidence = 0.0;
    std::string rationale;
};

struct DslProgram {
    std::vector<DslRule> rules;
};

struct DslParseResult {
    bool ok = false;
    std::string error;
    DslProgram program;
};

struct DslValidationResult {
    bool ok = false;
    std::vector<std::string> errors;
};

struct GeneratedOverlayFiles {
    std::string eventsText;
    std::string scriptedEffectsText;
    std::string scriptedTriggersText;
    std::string manifestText;
};

} // namespace strategic_nexus::generated_overlay

