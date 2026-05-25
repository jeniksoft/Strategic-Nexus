// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "DecisionFileReader.h"

#include "common/FileUtil.h"
#include "common/JsonExtract.h"
#include "common/JsonSanity.h"

#include <optional>
#include <regex>

namespace strategic_nexus::bridge_core {
namespace {

bool extractInt64(const std::string& json, const char* key, std::int64_t& value)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(-?\\d+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return false;
    }

    try {
        value = std::stoll(match[1].str());
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

bool containsForbiddenField(const std::string& json)
{
    static const std::regex forbidden(
        "\"(doctrine_id|command|command_string|script|script_text|console|console_command|raw_text|llm_text|fleet_target|build_order|save_fragment)\"\\s*:");
    return std::regex_search(json, forbidden);
}

template <typename TEnum, typename TMapper, typename TSetter>
void parseDimension(
    const std::string& json,
    const char* key,
    TMapper mapper,
    TSetter setter,
    int& validCount,
    int& invalidCount)
{
    const std::optional<std::string> rawValue = common::extractJsonString(json, key);
    if (!rawValue.has_value()) {
        return;
    }

    TEnum mappedValue;
    if (!mapper(*rawValue, mappedValue)) {
        ++invalidCount;
        return;
    }

    setter(mappedValue);
    ++validCount;
}

} // namespace

DecisionReadResult DecisionFileReader::read(const std::filesystem::path& path) const
{
    DecisionReadResult result;

    if (!std::filesystem::exists(path)) {
        result.error = "missing file";
        return result;
    }

    const std::string json = common::readTextFile(path);
    if (json.empty()) {
        result.error = "empty file";
        return result;
    }
    if (!common::hasBalancedJsonDelimiters(json)) {
        result.error = "malformed file";
        return result;
    }

    if (containsForbiddenField(json)) {
        result.error = "forbidden executable, raw text, or legacy doctrine field";
        return result;
    }

    std::int64_t schemaVersion = 0;
    if (!extractInt64(json, "schema_version", schemaVersion)) {
        result.error = "missing schema_version";
        return result;
    }
    result.payload.schemaVersion = static_cast<int>(schemaVersion);

    if (!extractInt64(json, "sequence_id", result.payload.sequenceId)) {
        result.error = "missing sequence_id";
        return result;
    }
    if (!extractInt64(json, "created_unix_ms", result.payload.createdUnixMs)) {
        result.error = "missing created_unix_ms";
        return result;
    }
    if (!extractInt64(json, "ttl_ms", result.payload.ttlMs)) {
        result.error = "missing ttl_ms";
        return result;
    }
    if (!extractBool(json, "bridge_available", result.payload.bridgeAvailable)) {
        result.error = "missing bridge_available";
        return result;
    }

    const std::optional<std::string> campaignId = common::extractJsonString(json, "campaign_id");
    if (!campaignId.has_value() || campaignId->empty()) {
        result.error = "missing campaign_id";
        return result;
    }
    result.payload.campaignId = *campaignId;

    const std::optional<std::string> empireId = common::extractJsonString(json, "empire_id");
    if (!empireId.has_value() || empireId->empty()) {
        result.error = "missing empire_id";
        return result;
    }
    result.payload.empireId = *empireId;

    if (json.find("\"strategy_dimensions\"") == std::string::npos) {
        result.error = "missing strategy_dimensions";
        return result;
    }

    parseDimension<MilitaryPosture>(
        json,
        "military_posture",
        tryParseMilitaryPosture,
        [&](const MilitaryPosture value) {
            result.payload.dimensions.hasMilitaryPosture = true;
            result.payload.dimensions.militaryPosture = value;
        },
        result.payload.validDimensionCount,
        result.payload.invalidDimensionCount);

    parseDimension<FleetPhilosophy>(
        json,
        "fleet_philosophy",
        tryParseFleetPhilosophy,
        [&](const FleetPhilosophy value) {
            result.payload.dimensions.hasFleetPhilosophy = true;
            result.payload.dimensions.fleetPhilosophy = value;
        },
        result.payload.validDimensionCount,
        result.payload.invalidDimensionCount);

    parseDimension<BorderStrategy>(
        json,
        "border_strategy",
        tryParseBorderStrategy,
        [&](const BorderStrategy value) {
            result.payload.dimensions.hasBorderStrategy = true;
            result.payload.dimensions.borderStrategy = value;
        },
        result.payload.validDimensionCount,
        result.payload.invalidDimensionCount);

    parseDimension<ExpansionPolicy>(
        json,
        "expansion_policy",
        tryParseExpansionPolicy,
        [&](const ExpansionPolicy value) {
            result.payload.dimensions.hasExpansionPolicy = true;
            result.payload.dimensions.expansionPolicy = value;
        },
        result.payload.validDimensionCount,
        result.payload.invalidDimensionCount);

    parseDimension<DiplomaticBehavior>(
        json,
        "diplomatic_behavior",
        tryParseDiplomaticBehavior,
        [&](const DiplomaticBehavior value) {
            result.payload.dimensions.hasDiplomaticBehavior = true;
            result.payload.dimensions.diplomaticBehavior = value;
        },
        result.payload.validDimensionCount,
        result.payload.invalidDimensionCount);

    parseDimension<ResearchBias>(
        json,
        "research_bias",
        tryParseResearchBias,
        [&](const ResearchBias value) {
            result.payload.dimensions.hasResearchBias = true;
            result.payload.dimensions.researchBias = value;
        },
        result.payload.validDimensionCount,
        result.payload.invalidDimensionCount);

    parseDimension<EconomicFocus>(
        json,
        "economic_focus",
        tryParseEconomicFocus,
        [&](const EconomicFocus value) {
            result.payload.dimensions.hasEconomicFocus = true;
            result.payload.dimensions.economicFocus = value;
        },
        result.payload.validDimensionCount,
        result.payload.invalidDimensionCount);

    parseDimension<CrisisPolicy>(
        json,
        "crisis_policy",
        tryParseCrisisPolicy,
        [&](const CrisisPolicy value) {
            result.payload.dimensions.hasCrisisPolicy = true;
            result.payload.dimensions.crisisPolicy = value;
        },
        result.payload.validDimensionCount,
        result.payload.invalidDimensionCount);

    parseDimension<EspionagePolicy>(
        json,
        "espionage_policy",
        tryParseEspionagePolicy,
        [&](const EspionagePolicy value) {
            result.payload.dimensions.hasEspionagePolicy = true;
            result.payload.dimensions.espionagePolicy = value;
        },
        result.payload.validDimensionCount,
        result.payload.invalidDimensionCount);

    if (!extractDouble(json, "confidence", result.payload.confidence)) {
        result.error = "missing confidence";
        return result;
    }
    if (!extractBool(json, "fallback_required", result.payload.fallbackRequired)) {
        result.error = "missing fallback_required";
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace strategic_nexus::bridge_core

