// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncCandidateDecisionPackageBuilder.h"

#include "common/JsonExtract.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <sstream>

namespace strategic_nexus {
namespace {

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const char ch : value) {
        switch (ch) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                output << ' ';
            } else {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

void addUnique(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::string boolFact(const char* name, const bool value)
{
    return std::string(name) + ":" + (value ? "true" : "false");
}

std::optional<std::string> extractArrayBody(const std::string& json, const char* key)
{
    const std::regex arrayStartPattern("\"" + std::string(key) + "\"\\s*:\\s*\\[");
    std::smatch match;
    if (!std::regex_search(json, match, arrayStartPattern)) {
        return std::nullopt;
    }

    const std::size_t bodyStart = static_cast<std::size_t>(match.position()) + match.length();
    int depth = 1;
    bool inString = false;
    bool escaped = false;

    for (std::size_t index = bodyStart; index < json.size(); ++index) {
        const char ch = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return json.substr(bodyStart, index - bodyStart);
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string> extractObjectBodies(const std::string& arrayBody)
{
    std::vector<std::string> objects;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    std::size_t objectStart = std::string::npos;

    for (std::size_t index = 0; index < arrayBody.size(); ++index) {
        const char ch = arrayBody[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
            continue;
        }
        if (ch == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                objects.push_back(arrayBody.substr(objectStart, index - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }

    return objects;
}

std::optional<bool> extractJsonBool(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }
    return match[1].str() == "true";
}

std::optional<std::size_t> extractJsonSize(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(\\d+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }

    std::size_t parsed = 0;
    for (const char ch : match[1].str()) {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
            return std::nullopt;
        }
        parsed = (parsed * 10) + static_cast<std::size_t>(ch - '0');
    }
    return parsed;
}

std::string stringOrEmpty(const std::string& json, const char* key)
{
    return common::extractJsonString(json, key).value_or("");
}

SncDecisionInput parseDecisionInput(const std::string& json)
{
    SncDecisionInput input;
    input.decisionInputId = stringOrEmpty(json, "decision_input_id");
    input.packageEntryId = stringOrEmpty(json, "package_entry_id");
    input.campaignKey = stringOrEmpty(json, "campaign_key");
    input.ruleScope = stringOrEmpty(json, "rule_scope");
    input.sourceKind = stringOrEmpty(json, "source_kind");
    input.saveName = stringOrEmpty(json, "save_name");
    input.saveDate = stringOrEmpty(json, "save_date");
    input.contentHash = stringOrEmpty(json, "content_hash");
    input.evidencePolicy = stringOrEmpty(json, "evidence_policy");
    input.modelOutputTrusted = extractJsonBool(json, "model_output_trusted").value_or(false);
    input.validationRequired = extractJsonBool(json, "validation_required").value_or(true);
    input.futureEvidenceExcluded = extractJsonBool(json, "future_evidence_excluded").value_or(false);
    input.compatibleArchivedEvidenceCount = extractJsonSize(json, "compatible_archived_evidence_count").value_or(0);
    input.laterArchivedEvidenceCount = extractJsonSize(json, "later_archived_evidence_count").value_or(0);
    input.knownFacts = common::extractJsonStringArray(json, "known_facts");
    input.uncertainties = common::extractJsonStringArray(json, "uncertainties");
    return input;
}

SncBlockedDecisionInput parseBlockedSourceEntry(const std::string& json)
{
    SncBlockedDecisionInput blocked;
    blocked.packageEntryId = stringOrEmpty(json, "package_entry_id");
    blocked.campaignKey = stringOrEmpty(json, "campaign_key");
    blocked.ruleScope = stringOrEmpty(json, "rule_scope");
    blocked.saveName = stringOrEmpty(json, "save_name");
    blocked.saveDate = stringOrEmpty(json, "save_date");
    blocked.decisionInputState = stringOrEmpty(json, "decision_input_state");
    blocked.reason = stringOrEmpty(json, "reason");
    blocked.warningCodes = common::extractJsonStringArray(json, "warning_codes");
    return blocked;
}

SncCandidateDecision buildCandidateDecision(const SncDecisionInput& input)
{
    SncCandidateDecision candidate;
    candidate.candidateDecisionId = "candidate-decision::" + input.ruleScope;
    candidate.decisionInputId = input.decisionInputId;
    candidate.packageEntryId = input.packageEntryId;
    candidate.campaignKey = input.campaignKey;
    candidate.ruleScope = input.ruleScope;
    candidate.sourceKind = input.sourceKind;
    candidate.saveName = input.saveName;
    candidate.saveDate = input.saveDate;
    candidate.contentHash = input.contentHash;
    candidate.knownFacts = input.knownFacts;
    candidate.uncertainties = input.uncertainties;
    candidate.knownFacts.push_back("candidate_source:deterministic_v0_stub");
    candidate.knownFacts.push_back("candidate_action:observe_only");
    candidate.knownFacts.push_back(boolFact("candidate_publishes_overlay", candidate.publishesOverlay));
    candidate.knownFacts.push_back(boolFact("model_output_used", candidate.modelOutputUsed));
    candidate.uncertainties.push_back("real_strategy_requires_parsed_empire_state");
    candidate.uncertainties.push_back("candidate_is_not_a_mod_rule");

    if (candidate.decisionInputId.empty()) {
        candidate.validationWarnings.push_back("missing_decision_input_id");
    }
    if (candidate.campaignKey.empty()) {
        candidate.validationWarnings.push_back("missing_campaign_key");
    }
    if (candidate.ruleScope.empty()) {
        candidate.validationWarnings.push_back("missing_rule_scope");
    }
    if (candidate.saveDate.empty()) {
        candidate.validationWarnings.push_back("missing_save_date");
    }

    return candidate;
}

void validateCandidatePackage(const SncCandidateDecisionPackage& package, std::vector<std::string>& errors)
{
    if (!package.dryRunOnly) {
        errors.push_back("candidate package is not dry-run only");
    }
    if (package.publishesOverlay) {
        errors.push_back("candidate package must not publish overlay");
    }
    if (package.modelOutputUsed) {
        errors.push_back("candidate package unexpectedly used model output");
    }
    if (package.modelOutputTrusted) {
        errors.push_back("candidate package must not trust model output");
    }
    if (!package.validationRequired) {
        errors.push_back("candidate package must require validation");
    }
    if (package.candidateDecisionCount != package.candidateDecisions.size()) {
        errors.push_back("candidate decision count does not match serialized candidates");
    }

    for (const auto& candidate : package.candidateDecisions) {
        if (candidate.candidateDecisionId.empty()) {
            errors.push_back("candidate is missing candidate_decision_id");
        }
        if (candidate.decisionInputId.empty()) {
            errors.push_back("candidate is missing decision_input_id");
        }
        if (candidate.campaignKey.empty()) {
            errors.push_back("candidate is missing campaign_key");
        }
        if (candidate.ruleScope.empty()) {
            errors.push_back("candidate is missing rule_scope");
        }
        if (candidate.recommendedAction != "observe_only") {
            errors.push_back("candidate action is not observe_only");
        }
        if (candidate.militaryPosture != "preserve" || candidate.researchBias != "preserve") {
            errors.push_back("candidate must preserve posture and research bias in v0 stub");
        }
        if (candidate.publishesOverlay) {
            errors.push_back("candidate must not publish overlay");
        }
        if (candidate.modelOutputUsed || candidate.modelOutputTrusted) {
            errors.push_back("candidate crosses model trust boundary");
        }
        if (!candidate.validationRequired) {
            errors.push_back("candidate must require validation");
        }
        for (const auto& warning : candidate.validationWarnings) {
            errors.push_back("candidate validation warning: " + warning);
        }
    }
}

void writeStringArray(std::ostringstream& output, const std::vector<std::string>& values, const int indentSpaces)
{
    const std::string indent(indentSpaces, ' ');
    const std::string itemIndent(indentSpaces + 2, ' ');
    output << "[";
    if (!values.empty()) {
        output << "\n";
        for (std::size_t index = 0; index < values.size(); ++index) {
            output << itemIndent << "\"" << jsonEscape(values[index]) << "\"";
            if (index + 1 < values.size()) {
                output << ",";
            }
            output << "\n";
        }
        output << indent;
    }
    output << "]";
}

} // namespace

SncDecisionInputPackageReadResult parseSncDecisionInputPackageJson(const std::string& json)
{
    SncDecisionInputPackageReadResult result;
    if (json.empty()) {
        result.reason = "decision input package json is empty";
        return result;
    }

    const auto schemaVersion = extractJsonSize(json, "schema_version");
    if (!schemaVersion.has_value() || *schemaVersion != 1) {
        result.reason = "unsupported decision input package schema";
        return result;
    }

    const auto ok = extractJsonBool(json, "ok");
    const auto dryRunOnly = extractJsonBool(json, "dry_run_only");
    const auto publishesOverlay = extractJsonBool(json, "publishes_overlay");
    const auto modelOutputTrusted = extractJsonBool(json, "model_output_trusted");
    const auto validationRequired = extractJsonBool(json, "validation_required");
    const auto inputsBody = extractArrayBody(json, "decision_inputs");
    if (!ok.has_value() ||
        !dryRunOnly.has_value() ||
        !publishesOverlay.has_value() ||
        !modelOutputTrusted.has_value() ||
        !validationRequired.has_value() ||
        !inputsBody.has_value()) {
        result.reason = "decision input package json is missing required fields";
        return result;
    }

    auto& package = result.package;
    package.ok = *ok;
    package.reason = stringOrEmpty(json, "reason");
    package.readiness = stringOrEmpty(json, "readiness");
    package.dryRunOnly = *dryRunOnly;
    package.publishesOverlay = *publishesOverlay;
    package.modelOutputTrusted = *modelOutputTrusted;
    package.validationRequired = *validationRequired;
    package.sourcePostPlayPackagePath = stringOrEmpty(json, "source_post_play_package_path");
    package.sessionArchiveDirectory = stringOrEmpty(json, "session_archive_directory");
    package.sourcePostPlayReadiness = stringOrEmpty(json, "source_post_play_readiness");
    package.entryPointCount = extractJsonSize(json, "entry_point_count").value_or(0);
    package.decisionInputCount = extractJsonSize(json, "decision_input_count").value_or(0);
    package.blockedEntryCount = extractJsonSize(json, "blocked_entry_count").value_or(0);
    package.archivedEvidenceCount = extractJsonSize(json, "archived_evidence_count").value_or(0);
    package.warningCodes = common::extractJsonStringArray(json, "warning_codes");

    for (const auto& object : extractObjectBodies(*inputsBody)) {
        auto input = parseDecisionInput(object);
        if (input.decisionInputId.empty() || input.campaignKey.empty() || input.ruleScope.empty()) {
            result.reason = "decision input is missing required identity fields";
            result.package = {};
            return result;
        }
        package.decisionInputs.push_back(input);
    }

    if (const auto blockedBody = extractArrayBody(json, "blocked_entries"); blockedBody.has_value()) {
        for (const auto& object : extractObjectBodies(*blockedBody)) {
            package.blockedEntries.push_back(parseBlockedSourceEntry(object));
        }
    }

    result.ok = true;
    result.reason = "decision input package parsed";
    return result;
}

SncCandidateDecisionPackage SncCandidateDecisionPackageBuilder::build(
    const SncDecisionInputPackage& decisionInputPackage,
    const std::filesystem::path& sourceDecisionInputPackagePath) const
{
    SncCandidateDecisionPackage package;
    package.sourceDecisionInputPackagePath = sourceDecisionInputPackagePath;
    package.sourceDecisionInputReadiness = decisionInputPackage.readiness;
    package.entryPointCount = decisionInputPackage.entryPointCount;
    package.decisionInputCount = decisionInputPackage.decisionInputCount;
    package.blockedSourceEntryCount = decisionInputPackage.blockedEntryCount;
    package.archivedEvidenceCount = decisionInputPackage.archivedEvidenceCount;
    package.warningCodes = decisionInputPackage.warningCodes;
    package.blockedSourceEntries = decisionInputPackage.blockedEntries;

    if (!decisionInputPackage.ok) {
        package.reason = decisionInputPackage.reason.empty()
            ? "source decision input package is not ready"
            : decisionInputPackage.reason;
        package.readiness = "blocked";
        addUnique(package.warningCodes, "candidate_decision_source_not_ready");
        return package;
    }
    if (!decisionInputPackage.dryRunOnly ||
        decisionInputPackage.publishesOverlay ||
        decisionInputPackage.modelOutputTrusted ||
        !decisionInputPackage.validationRequired) {
        package.reason = "source decision input package violates dry-run/model-trust contract";
        package.readiness = "blocked";
        addUnique(package.warningCodes, "candidate_decision_source_contract_violation");
        return package;
    }

    for (const auto& input : decisionInputPackage.decisionInputs) {
        package.candidateDecisions.push_back(buildCandidateDecision(input));
    }
    package.candidateDecisionCount = package.candidateDecisions.size();

    validateCandidatePackage(package, package.validationErrors);
    package.validatorPassed = package.validationErrors.empty();
    package.rejectedCandidateCount = package.validatorPassed ? 0 : package.candidateDecisionCount;
    package.acceptedCandidateCount = package.validatorPassed ? package.candidateDecisionCount : 0;
    package.ok = package.validatorPassed;

    if (!package.validatorPassed) {
        package.reason = "candidate decision package failed validation";
        package.readiness = "blocked";
        addUnique(package.warningCodes, "candidate_decision_validation_failed");
    } else if (package.candidateDecisionCount > 0 && package.blockedSourceEntryCount > 0) {
        package.reason = "candidate decision package built; some source entries remain blocked";
        package.readiness = "ready_partial";
    } else if (package.candidateDecisionCount > 0) {
        package.reason = "candidate decision package built";
        package.readiness = "ready";
    } else {
        package.reason = "candidate decision package built but no decision inputs were available";
        package.readiness = "needs_attention";
        addUnique(package.warningCodes, "candidate_decision_no_ready_inputs");
    }

    return package;
}

std::string serializeSncCandidateDecisionPackage(const SncCandidateDecisionPackage& package)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (package.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(package.reason) << "\",\n";
    json << "  \"readiness\": \"" << jsonEscape(package.readiness) << "\",\n";
    json << "  \"dry_run_only\": " << (package.dryRunOnly ? "true" : "false") << ",\n";
    json << "  \"publishes_overlay\": " << (package.publishesOverlay ? "true" : "false") << ",\n";
    json << "  \"model_output_used\": " << (package.modelOutputUsed ? "true" : "false") << ",\n";
    json << "  \"model_output_trusted\": " << (package.modelOutputTrusted ? "true" : "false") << ",\n";
    json << "  \"validation_required\": " << (package.validationRequired ? "true" : "false") << ",\n";
    json << "  \"validator_passed\": " << (package.validatorPassed ? "true" : "false") << ",\n";
    json << "  \"candidate_source\": \"" << jsonEscape(package.candidateSource) << "\",\n";
    json << "  \"source_decision_input_package_path\": \"" << jsonEscape(package.sourceDecisionInputPackagePath.generic_string()) << "\",\n";
    json << "  \"source_decision_input_readiness\": \"" << jsonEscape(package.sourceDecisionInputReadiness) << "\",\n";
    json << "  \"entry_point_count\": " << package.entryPointCount << ",\n";
    json << "  \"decision_input_count\": " << package.decisionInputCount << ",\n";
    json << "  \"candidate_decision_count\": " << package.candidateDecisionCount << ",\n";
    json << "  \"accepted_candidate_count\": " << package.acceptedCandidateCount << ",\n";
    json << "  \"rejected_candidate_count\": " << package.rejectedCandidateCount << ",\n";
    json << "  \"blocked_source_entry_count\": " << package.blockedSourceEntryCount << ",\n";
    json << "  \"archived_evidence_count\": " << package.archivedEvidenceCount << ",\n";
    json << "  \"warning_codes\": ";
    writeStringArray(json, package.warningCodes, 2);
    json << ",\n";
    json << "  \"validation_errors\": ";
    writeStringArray(json, package.validationErrors, 2);
    json << ",\n";
    json << "  \"candidate_decisions\": [\n";
    for (std::size_t index = 0; index < package.candidateDecisions.size(); ++index) {
        const auto& candidate = package.candidateDecisions[index];
        json << "    {\n";
        json << "      \"candidate_decision_id\": \"" << jsonEscape(candidate.candidateDecisionId) << "\",\n";
        json << "      \"decision_input_id\": \"" << jsonEscape(candidate.decisionInputId) << "\",\n";
        json << "      \"package_entry_id\": \"" << jsonEscape(candidate.packageEntryId) << "\",\n";
        json << "      \"campaign_key\": \"" << jsonEscape(candidate.campaignKey) << "\",\n";
        json << "      \"rule_scope\": \"" << jsonEscape(candidate.ruleScope) << "\",\n";
        json << "      \"source_kind\": \"" << jsonEscape(candidate.sourceKind) << "\",\n";
        json << "      \"save_name\": \"" << jsonEscape(candidate.saveName) << "\",\n";
        json << "      \"save_date\": \"" << jsonEscape(candidate.saveDate) << "\",\n";
        json << "      \"content_hash\": \"" << jsonEscape(candidate.contentHash) << "\",\n";
        json << "      \"candidate_source\": \"" << jsonEscape(candidate.candidateSource) << "\",\n";
        json << "      \"recommended_action\": \"" << jsonEscape(candidate.recommendedAction) << "\",\n";
        json << "      \"military_posture\": \"" << jsonEscape(candidate.militaryPosture) << "\",\n";
        json << "      \"research_bias\": \"" << jsonEscape(candidate.researchBias) << "\",\n";
        json << "      \"decision_boundary\": \"" << jsonEscape(candidate.decisionBoundary) << "\",\n";
        json << "      \"validation_state\": \"" << jsonEscape(candidate.validationState) << "\",\n";
        json << "      \"confidence\": " << (static_cast<double>(candidate.confidencePercent) / 100.0) << ",\n";
        json << "      \"confidence_percent\": " << candidate.confidencePercent << ",\n";
        json << "      \"model_output_used\": " << (candidate.modelOutputUsed ? "true" : "false") << ",\n";
        json << "      \"model_output_trusted\": " << (candidate.modelOutputTrusted ? "true" : "false") << ",\n";
        json << "      \"validation_required\": " << (candidate.validationRequired ? "true" : "false") << ",\n";
        json << "      \"publishes_overlay\": " << (candidate.publishesOverlay ? "true" : "false") << ",\n";
        json << "      \"known_facts\": ";
        writeStringArray(json, candidate.knownFacts, 6);
        json << ",\n";
        json << "      \"uncertainties\": ";
        writeStringArray(json, candidate.uncertainties, 6);
        json << ",\n";
        json << "      \"validation_warnings\": ";
        writeStringArray(json, candidate.validationWarnings, 6);
        json << "\n";
        json << "    }" << (index + 1 < package.candidateDecisions.size() ? "," : "") << "\n";
    }
    json << "  ],\n";
    json << "  \"blocked_source_entries\": [\n";
    for (std::size_t index = 0; index < package.blockedSourceEntries.size(); ++index) {
        const auto& blocked = package.blockedSourceEntries[index];
        json << "    {\n";
        json << "      \"package_entry_id\": \"" << jsonEscape(blocked.packageEntryId) << "\",\n";
        json << "      \"campaign_key\": \"" << jsonEscape(blocked.campaignKey) << "\",\n";
        json << "      \"rule_scope\": \"" << jsonEscape(blocked.ruleScope) << "\",\n";
        json << "      \"save_name\": \"" << jsonEscape(blocked.saveName) << "\",\n";
        json << "      \"save_date\": \"" << jsonEscape(blocked.saveDate) << "\",\n";
        json << "      \"decision_input_state\": \"" << jsonEscape(blocked.decisionInputState) << "\",\n";
        json << "      \"reason\": \"" << jsonEscape(blocked.reason) << "\",\n";
        json << "      \"warning_codes\": ";
        writeStringArray(json, blocked.warningCodes, 6);
        json << "\n";
        json << "    }" << (index + 1 < package.blockedSourceEntries.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
