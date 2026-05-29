// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "DslValidator.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <set>
#include <string>

namespace strategic_nexus::generated_overlay {
namespace {

constexpr std::size_t maxRulesPerProgram = 32;
constexpr std::size_t maxPreferencesPerRule = 4;
constexpr std::size_t maxConditionsPerRule = 8;
constexpr std::size_t maxRationaleLength = 512;

bool isSafeIdentifier(const std::string& value)
{
    if (value.empty() || value.size() > 96) {
        return false;
    }

    return std::all_of(value.begin(), value.end(), [](const char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-';
    });
}

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.rfind(prefix, 0) == 0;
}

bool isSafeDottedIdentifierTail(const std::string& tail)
{
    if (tail.empty() || tail.size() > 128) {
        return false;
    }

    std::size_t segmentStart = 0;
    while (segmentStart < tail.size()) {
        const std::size_t dot = tail.find('.', segmentStart);
        const std::size_t segmentEnd = dot == std::string::npos ? tail.size() : dot;
        const std::size_t segmentLen = segmentEnd - segmentStart;
        if (segmentLen == 0 || segmentLen > 64) {
            return false;
        }

        const std::string segment = tail.substr(segmentStart, segmentLen);
        if (!isSafeIdentifier(segment)) {
            return false;
        }

        if (dot == std::string::npos) {
            break;
        }
        segmentStart = dot + 1;
    }

    return true;
}

bool isNumericLiteral(const std::string& value)
{
    if (value.empty() || value.size() > 32) {
        return false;
    }

    char* end = nullptr;
    (void)std::strtod(value.c_str(), &end);
    return end != value.c_str() && *end == '\0';
}

bool isAllowedMinistry(const std::string& ministry)
{
    return ministry == "military_ministry" || ministry == "research_ministry";
}

bool isAllowedPreference(const DslPreference& preference)
{
    if (preference.domain == "military_posture") {
        return preference.value == "defensive" || preference.value == "aggressive";
    }
    if (preference.domain == "research_bias") {
        return preference.value == "economy" || preference.value == "military_industry";
    }
    if (preference.domain == "doctrine_inertia") {
        return preference.value == "low" || preference.value == "medium" || preference.value == "high";
    }
    return false;
}

bool isAllowedCondition(const DslCondition& condition)
{
    if (!(startsWith(condition.source, "personality.") ||
          startsWith(condition.source, "memory.") ||
          startsWith(condition.source, "current.") ||
          startsWith(condition.source, "known.") ||
          startsWith(condition.source, "uncertainty.") ||
          condition.source == "campaign_marker")) {
        return false;
    }

    if (condition.source != "campaign_marker") {
        const auto dot = condition.source.find('.');
        if (dot == std::string::npos || dot + 1 >= condition.source.size()) {
            return false;
        }
        if (!isSafeDottedIdentifierTail(condition.source.substr(dot + 1))) {
            return false;
        }
    }

    if (!condition.op.empty()) {
        if (!isSafeIdentifier(condition.value) && !isNumericLiteral(condition.value)) {
            return false;
        }
    }

    return condition.op.empty() ||
        condition.op == "=" ||
        condition.op == ">=" ||
        condition.op == "<=";
}

void addError(std::vector<std::string>& errors, const std::string& ruleId, const std::string& message)
{
    errors.push_back(ruleId.empty() ? message : ruleId + ": " + message);
}

} // namespace

DslValidationResult DslValidator::validate(const DslProgram& program) const
{
    std::vector<std::string> errors;

    if (program.rules.empty()) {
        errors.push_back("program has no rules");
    }

    if (program.rules.size() > maxRulesPerProgram) {
        errors.push_back("program exceeds max rule count");
    }

    std::set<std::string> seenRuleKeys;
    for (const auto& rule : program.rules) {
        const std::string ruleKey = rule.campaignId + "/" + rule.empireId + "/" + rule.ruleId;
        if (!seenRuleKeys.insert(ruleKey).second) {
            addError(errors, rule.ruleId, "duplicate rule identity");
        }

        if (!isSafeIdentifier(rule.campaignId)) {
            addError(errors, rule.ruleId, "unsafe campaign id");
        }
        if (!isSafeIdentifier(rule.empireId)) {
            addError(errors, rule.ruleId, "unsafe empire id");
        }
        if (!isSafeIdentifier(rule.ruleId)) {
            addError(errors, rule.ruleId, "unsafe rule id");
        }
        if (!isAllowedMinistry(rule.ministry)) {
            addError(errors, rule.ruleId, "unknown ministry");
        }
        if (rule.rationale.empty()) {
            addError(errors, rule.ruleId, "rationale must not be empty");
        }
        if (rule.rationale.size() > maxRationaleLength) {
            addError(errors, rule.ruleId, "rationale exceeds max length");
        }
        if (rule.preferences.empty()) {
            addError(errors, rule.ruleId, "rule has no preferences");
        }
        if (rule.preferences.size() > maxPreferencesPerRule) {
            addError(errors, rule.ruleId, "rule exceeds preference budget");
        }
        if (rule.conditions.size() > maxConditionsPerRule) {
            addError(errors, rule.ruleId, "rule exceeds condition budget");
        }
        if (rule.duration != "next_session") {
            addError(errors, rule.ruleId, "unsupported duration");
        }
        if (!std::isfinite(rule.confidence)) {
            addError(errors, rule.ruleId, "confidence must be finite");
        }
        if (rule.confidence < 0.0 || rule.confidence > 1.0) {
            addError(errors, rule.ruleId, "confidence outside allowed range");
        }

        for (const auto& condition : rule.conditions) {
            if (!isAllowedCondition(condition)) {
                addError(errors, rule.ruleId, "unsupported condition");
            }
        }

        std::set<std::string> seenPreferenceDomains;
        for (const auto& preference : rule.preferences) {
            if (!isAllowedPreference(preference)) {
                addError(errors, rule.ruleId, "unsupported preference");
            }
            if (!seenPreferenceDomains.insert(preference.domain).second) {
                addError(errors, rule.ruleId, "duplicate preference domain");
            }
            if (!std::isfinite(preference.intensity)) {
                addError(errors, rule.ruleId, "intensity must be finite");
            }
            if (preference.intensity < 0.0 || preference.intensity > 1.0) {
                addError(errors, rule.ruleId, "intensity outside allowed range");
            }
        }
    }

    return {errors.empty(), std::move(errors)};
}

} // namespace strategic_nexus::generated_overlay

