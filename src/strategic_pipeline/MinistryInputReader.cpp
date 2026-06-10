// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "MinistryInputReader.h"

#include "PipelineLimits.h"
#include "common/FileUtil.h"
#include "common/JsonExtract.h"
#include "common/JsonSanity.h"

#include <algorithm>
#include <optional>
#include <regex>

namespace strategic_nexus::strategic_pipeline {
namespace {

std::string sanitizeInputString(const std::string& value)
{
    std::string sanitized;
    sanitized.reserve(std::min(value.size(), maxMinistryInputStringLength));
    for (const char character : value) {
        if (sanitized.size() >= maxMinistryInputStringLength) {
            break;
        }

        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20) {
            sanitized.push_back(' ');
            continue;
        }

        sanitized.push_back(character);
    }
    return sanitized;
}

bool extractInt(const std::string& json, const char* key, int& value)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(-?\\d+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return false;
    }

    try {
        value = std::stoi(match[1].str());
        return true;
    } catch (...) {
        return false;
    }
}

bool extractDouble(const std::string& json, const char* key, double& value)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(-?\\d+(?:\\.\\d+)?)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return false;
    }

    try {
        value = std::stod(match[1].str());
        return true;
    } catch (...) {
        return false;
    }
}

bool extractBool(const std::string& json, const char* key, bool& value)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return false;
    }

    value = match[1].str() == "true";
    return true;
}

std::string extractStringOrEmpty(const std::string& json, const char* key)
{
    const std::optional<std::string> value = common::extractJsonString(json, key);
    if (!value.has_value()) {
        return {};
    }

    return sanitizeInputString(*value);
}

std::vector<std::string> extractStringArray(const std::string& json, const char* key)
{
    const std::vector<std::string> extracted = common::extractJsonStringArray(json, key);
    std::vector<std::string> values;
    values.reserve(std::min(extracted.size(), maxMinistryInputArrayItems));

    // Keep the fixed bound but preserve turn-context hints first,
    // because they directly affect deterministic v0 reasoning.
    for (const std::string& value : extracted) {
        if (values.size() >= maxMinistryInputArrayItems) {
            break;
        }
        const std::string sanitized = sanitizeInputString(value);
        if (sanitized.rfind("turn_context_", 0) == 0) {
            values.push_back(sanitized);
        }
    }

    for (const std::string& value : extracted) {
        if (values.size() >= maxMinistryInputArrayItems) {
            break;
        }
        const std::string sanitized = sanitizeInputString(value);
        if (sanitized.rfind("turn_context_", 0) == 0) {
            continue;
        }
        values.push_back(sanitized);
    }

    return values;
}

} // namespace

MinistryInputReadResult MinistryInputReader::read(const std::filesystem::path& path) const
{
    MinistryInputReadResult result;

    if (!std::filesystem::exists(path)) {
        result.error = "missing ministry input context";
        return result;
    }

    const std::string json = common::readTextFile(path);
    if (json.empty()) {
        result.error = "empty ministry input context";
        return result;
    }
    if (!common::hasBalancedJsonDelimiters(json)) {
        result.error = "malformed ministry input context";
        return result;
    }

    if (!extractInt(json, "schema_version", result.input.schemaVersion)) {
        result.error = "missing schema_version";
        return result;
    }
    result.input.sourceSchemaVersion = result.input.schemaVersion;
    result.input.schemaVersion = 1;

    if (result.input.sourceSchemaVersion == 1) {
        result.input.schemaCompatibilityState = "current";
    } else if (result.input.sourceSchemaVersion == 0) {
        result.input.schemaCompatibilityState = "partial_compatibility";
        result.input.schemaCompatibilityNote =
            "migrated legacy ministry input schema_version 0 to current schema_version 1";
    } else {
        result.error = "unsupported schema_version";
        return result;
    }

    result.input.contextId = extractStringOrEmpty(json, "context_id");
    result.input.campaignId = extractStringOrEmpty(json, "campaign_id");
    result.input.empireId = extractStringOrEmpty(json, "empire_id");
    result.input.ministry = extractStringOrEmpty(json, "ministry");

    if (result.input.campaignId.empty() || result.input.empireId.empty() || result.input.ministry.empty()) {
        result.error = "missing required identity fields";
        return result;
    }

    extractInt(json, "year", result.input.turnContext.year);
    extractBool(json, "is_at_war", result.input.turnContext.isAtWar);
    extractDouble(json, "strategic_pressure", result.input.turnContext.strategicPressure);

    result.input.knownFacts = extractStringArray(json, "known_facts");
    result.input.uncertainties = extractStringArray(json, "uncertainties");
    result.input.currentMilitaryPosture = extractStringOrEmpty(json, "military_posture");
    result.input.currentResearchBias = extractStringOrEmpty(json, "research_bias");

    result.ok = true;
    return result;
}

} // namespace strategic_nexus::strategic_pipeline

