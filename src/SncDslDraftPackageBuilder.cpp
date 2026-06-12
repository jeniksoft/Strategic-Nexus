// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncDslDraftPackageBuilder.h"

#include "common/JsonExtract.h"
#include "generated_overlay/DslParser.h"
#include "generated_overlay/DslValidator.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>

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

std::optional<std::string> extractObjectBody(const std::string& json, const char* key)
{
    const std::regex objectStartPattern("\"" + std::string(key) + "\"\\s*:\\s*\\{");
    std::smatch match;
    if (!std::regex_search(json, match, objectStartPattern)) {
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
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
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

std::optional<int> extractJsonInt(const std::string& json, const char* key)
{
    const auto parsed = extractJsonSize(json, key);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(*parsed);
}

std::string stringOrEmpty(const std::string& json, const char* key)
{
    return common::extractJsonString(json, key).value_or("");
}

std::string safeIdentifier(const std::string& value, const std::string& fallback)
{
    std::string output;
    output.reserve(value.size());
    bool lastWasSeparator = false;

    for (const unsigned char raw : value) {
        if (std::isalnum(raw)) {
            output.push_back(static_cast<char>(std::tolower(raw)));
            lastWasSeparator = false;
            continue;
        }
        if (!lastWasSeparator && !output.empty()) {
            output.push_back('_');
            lastWasSeparator = true;
        }
    }

    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }
    if (output.empty()) {
        output = fallback;
    }
    if (std::isdigit(static_cast<unsigned char>(output.front())) != 0) {
        output = fallback + "_" + output;
    }
    if (output.size() > 80) {
        output.resize(80);
        while (!output.empty() && output.back() == '_') {
            output.pop_back();
        }
    }
    return output.empty() ? fallback : output;
}

std::string safeDslString(const std::string& value)
{
    std::string output;
    output.reserve(value.size());
    for (const unsigned char raw : value) {
        if (raw < 0x20 || raw == '"' || raw == '\\') {
            output.push_back(' ');
        } else {
            output.push_back(static_cast<char>(raw));
        }
    }
    while (output.find("  ") != std::string::npos) {
        output = std::regex_replace(output, std::regex("  +"), " ");
    }
    if (output.size() > 480) {
        output.resize(480);
    }
    return output;
}

std::string hashToken(const std::string& hash)
{
    std::string token;
    for (const unsigned char raw : hash) {
        if (std::isxdigit(raw) != 0) {
            token.push_back(static_cast<char>(std::tolower(raw)));
        }
        if (token.size() >= 12) {
            break;
        }
    }
    return token.empty() ? "unknown" : token;
}

std::string saveDateToken(const std::string& saveDate)
{
    std::string output;
    bool lastWasSeparator = false;
    for (const unsigned char raw : saveDate) {
        if (std::isalnum(raw)) {
            output.push_back(static_cast<char>(std::tolower(raw)));
            lastWasSeparator = false;
            continue;
        }
        if (!lastWasSeparator && !output.empty()) {
            output.push_back('_');
            lastWasSeparator = true;
        }
    }
    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }
    return "d_" + (output.empty() ? std::string("unknown_date") : output);
}

bool candidateHasRequiredIdentity(const SncCandidateDecision& candidate)
{
    return !candidate.campaignKey.empty() &&
        !candidate.ruleScope.empty() &&
        !candidate.saveDate.empty() &&
        !candidate.contentHash.empty() &&
        !candidate.playerCountryId.empty() &&
        candidate.empireState.parsed;
}

bool candidateRespectsDryRunBoundary(const SncCandidateDecision& candidate)
{
    return candidate.recommendedAction == "observe_only" &&
        !candidate.publishesOverlay &&
        !candidate.modelOutputUsed &&
        !candidate.modelOutputTrusted &&
        candidate.validationRequired &&
        candidate.validationWarnings.empty();
}

SncCandidateDecision parseCandidateDecision(const std::string& json)
{
    SncCandidateDecision candidate;
    candidate.candidateDecisionId = stringOrEmpty(json, "candidate_decision_id");
    candidate.decisionInputId = stringOrEmpty(json, "decision_input_id");
    candidate.packageEntryId = stringOrEmpty(json, "package_entry_id");
    candidate.campaignKey = stringOrEmpty(json, "campaign_key");
    candidate.ruleScope = stringOrEmpty(json, "rule_scope");
    candidate.sourceKind = stringOrEmpty(json, "source_kind");
    candidate.saveName = stringOrEmpty(json, "save_name");
    candidate.saveDate = stringOrEmpty(json, "save_date");
    candidate.contentHash = stringOrEmpty(json, "content_hash");
    candidate.parseStatus = stringOrEmpty(json, "parse_status");
    candidate.playerCountryId = stringOrEmpty(json, "player_country_id");
    candidate.empireName = stringOrEmpty(json, "empire_name");
    candidate.candidateSource = stringOrEmpty(json, "candidate_source");
    candidate.recommendedAction = stringOrEmpty(json, "recommended_action");
    candidate.militaryPosture = stringOrEmpty(json, "military_posture");
    candidate.researchBias = stringOrEmpty(json, "research_bias");
    candidate.decisionBoundary = stringOrEmpty(json, "decision_boundary");
    candidate.validationState = stringOrEmpty(json, "validation_state");
    candidate.confidencePercent = extractJsonInt(json, "confidence_percent").value_or(25);
    candidate.modelOutputUsed = extractJsonBool(json, "model_output_used").value_or(false);
    candidate.modelOutputTrusted = extractJsonBool(json, "model_output_trusted").value_or(false);
    candidate.validationRequired = extractJsonBool(json, "validation_required").value_or(true);
    candidate.publishesOverlay = extractJsonBool(json, "publishes_overlay").value_or(false);
    candidate.knownFacts = common::extractJsonStringArray(json, "known_facts");
    candidate.uncertainties = common::extractJsonStringArray(json, "uncertainties");
    candidate.validationWarnings = common::extractJsonStringArray(json, "validation_warnings");

    if (const auto summaryBody = extractObjectBody(json, "empire_state_summary"); summaryBody.has_value()) {
        candidate.empireState.parsed = extractJsonBool(*summaryBody, "parsed").value_or(false);
        candidate.empireState.parseStatus = stringOrEmpty(*summaryBody, "parse_status");
        candidate.empireState.parserReason = stringOrEmpty(*summaryBody, "parser_reason");
        candidate.empireState.playerCountryId = stringOrEmpty(*summaryBody, "player_country_id");
        candidate.empireState.empireName = stringOrEmpty(*summaryBody, "empire_name");
        candidate.empireState.government = stringOrEmpty(*summaryBody, "government");
        candidate.empireState.authority = stringOrEmpty(*summaryBody, "authority");
        candidate.empireState.founderSpeciesName = stringOrEmpty(*summaryBody, "founder_species_name");
        candidate.empireState.capitalPlanetName = stringOrEmpty(*summaryBody, "capital_planet_name");
        candidate.empireState.homeSystemName = stringOrEmpty(*summaryBody, "home_system_name");
        candidate.empireState.ownedFleetCount = extractJsonSize(*summaryBody, "owned_fleet_count").value_or(0);
        candidate.empireState.activeWarCount = extractJsonSize(*summaryBody, "active_war_count").value_or(0);
        candidate.empireState.missingFields = common::extractJsonStringArray(*summaryBody, "missing_fields");
        candidate.empireState.uncertainties = common::extractJsonStringArray(*summaryBody, "uncertainties");
    }
    if (candidate.empireState.playerCountryId.empty()) {
        candidate.empireState.playerCountryId = candidate.playerCountryId;
    }
    if (candidate.empireState.empireName.empty()) {
        candidate.empireState.empireName = candidate.empireName;
    }
    return candidate;
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

void appendRuleDsl(std::ostringstream& dsl, const SncCandidateDecision& candidate, const std::size_t index)
{
    const std::string campaignId = safeIdentifier(candidate.campaignKey, "campaign");
    const std::string empireId = safeIdentifier("country_" + candidate.playerCountryId, "empire");
    const std::string hash = hashToken(candidate.contentHash);
    const std::string ruleId = safeIdentifier("entry_" + hash + "_" + std::to_string(index + 1), "entry_rule");
    const double confidence = std::clamp(static_cast<double>(candidate.confidencePercent) / 100.0, 0.1, 0.8);
    const std::string ministry = candidate.researchBias == "military_industry" ? "research_ministry" : "military_ministry";
    const std::string rationale =
        "Conservative dry-run DSL draft from validated entry-point candidate; "
        "military_posture=" + candidate.militaryPosture + ", research_bias=" + candidate.researchBias + ".";

    dsl << "campaign \"" << campaignId << "\" {\n";
    dsl << "  empire \"" << empireId << "\" {\n";
    dsl << "    rule \"" << ruleId << "\" {\n";
    dsl << "      ministry = " << ministry << "\n";
    dsl << "      when campaign_marker\n";
    dsl << "      when known.save_fingerprint = h_" << hash << "\n";
    dsl << "      when known.save_date = " << saveDateToken(candidate.saveDate) << "\n";
    dsl << "      when known.rule_scope = " << safeIdentifier(candidate.ruleScope, "entry_scope") << "\n";
    dsl << "      prefer military_posture " << candidate.militaryPosture << " intensity 0.8\n";
    dsl << "      prefer research_bias " << candidate.researchBias << " intensity 0.7\n";
    dsl << "      prefer doctrine_inertia high intensity 0.8\n";
    dsl << "      duration = next_session\n";
    dsl << "      confidence = " << std::fixed << std::setprecision(2) << confidence << "\n";
    dsl << "      rationale = \"" << safeDslString(rationale) << "\"\n";
    dsl << "    }\n";
    dsl << "  }\n";
    dsl << "}\n";
}

} // namespace

SncCandidateDecisionPackageReadResult parseSncCandidateDecisionPackageJson(const std::string& json)
{
    SncCandidateDecisionPackageReadResult result;
    if (json.empty()) {
        result.reason = "candidate decision package json is empty";
        return result;
    }

    const auto schemaVersion = extractJsonSize(json, "schema_version");
    if (!schemaVersion.has_value() || *schemaVersion > 1) {
        result.reason = "unsupported candidate decision package schema";
        return result;
    }

    const auto sourceSchemaVersionValue = extractJsonSize(json, "source_schema_version");
    std::size_t sourceSchemaVersion = schemaVersion.value();
    if (sourceSchemaVersionValue.has_value()) {
        if (*sourceSchemaVersionValue > 1) {
            result.reason = "unsupported candidate decision package schema";
            return result;
        }
        sourceSchemaVersion = *sourceSchemaVersionValue;
        if (schemaVersion.value() == 0 && sourceSchemaVersion != 0) {
            result.reason = "candidate decision package json is malformed";
            return result;
        }
    }

    const auto ok = extractJsonBool(json, "ok");
    const auto dryRunOnly = extractJsonBool(json, "dry_run_only");
    const auto publishesOverlay = extractJsonBool(json, "publishes_overlay");
    const auto modelOutputUsed = extractJsonBool(json, "model_output_used");
    const auto modelOutputTrusted = extractJsonBool(json, "model_output_trusted");
    const auto validationRequired = extractJsonBool(json, "validation_required");
    const auto validatorPassed = extractJsonBool(json, "validator_passed");
    const auto candidatesBody = extractArrayBody(json, "candidate_decisions");
    if (!ok.has_value() ||
        !dryRunOnly.has_value() ||
        !publishesOverlay.has_value() ||
        !modelOutputUsed.has_value() ||
        !modelOutputTrusted.has_value() ||
        !validationRequired.has_value() ||
        !validatorPassed.has_value() ||
        !candidatesBody.has_value()) {
        result.reason = "candidate decision package json is missing required fields";
        return result;
    }

    auto& package = result.package;
    package.sourceSchemaVersion = sourceSchemaVersion;
    if (package.sourceSchemaVersion == 0) {
        package.schemaCompatibilityState = "partial_compatibility";
        package.schemaCompatibilityNote = stringOrEmpty(json, "schema_compatibility_note");
    } else {
        package.schemaCompatibilityState = "current";
        package.schemaCompatibilityNote.clear();
    }
    package.ok = *ok;
    package.reason = stringOrEmpty(json, "reason");
    package.readiness = stringOrEmpty(json, "readiness");
    package.dryRunOnly = *dryRunOnly;
    package.publishesOverlay = *publishesOverlay;
    package.modelOutputUsed = *modelOutputUsed;
    package.modelOutputTrusted = *modelOutputTrusted;
    package.validationRequired = *validationRequired;
    package.validatorPassed = *validatorPassed;
    package.candidateSource = stringOrEmpty(json, "candidate_source");
    package.sourceDecisionInputReadiness = stringOrEmpty(json, "source_decision_input_readiness");
    package.entryPointCount = extractJsonSize(json, "entry_point_count").value_or(0);
    package.decisionInputCount = extractJsonSize(json, "decision_input_count").value_or(0);
    package.candidateDecisionCount = extractJsonSize(json, "candidate_decision_count").value_or(0);
    package.acceptedCandidateCount = extractJsonSize(json, "accepted_candidate_count").value_or(0);
    package.rejectedCandidateCount = extractJsonSize(json, "rejected_candidate_count").value_or(0);
    package.blockedSourceEntryCount = extractJsonSize(json, "blocked_source_entry_count").value_or(0);
    package.archivedEvidenceCount = extractJsonSize(json, "archived_evidence_count").value_or(0);
    package.warningCodes = common::extractJsonStringArray(json, "warning_codes");
    package.validationErrors = common::extractJsonStringArray(json, "validation_errors");

    for (const auto& object : extractObjectBodies(*candidatesBody)) {
        auto candidate = parseCandidateDecision(object);
        if (candidate.candidateDecisionId.empty() ||
            candidate.decisionInputId.empty() ||
            candidate.campaignKey.empty() ||
            candidate.ruleScope.empty()) {
            result.reason = "candidate decision is missing required identity fields";
            result.package = {};
            return result;
        }
        package.candidateDecisions.push_back(std::move(candidate));
    }

    result.ok = true;
    result.reason = "candidate decision package parsed";
    return result;
}

SncDslDraftPackage SncDslDraftPackageBuilder::build(
    const SncCandidateDecisionPackage& candidatePackage,
    const std::filesystem::path& sourceCandidateDecisionPackagePath) const
{
    SncDslDraftPackage package;
    package.sourceCandidateDecisionPackagePath = sourceCandidateDecisionPackagePath;
    package.sourceCandidateReadiness = candidatePackage.readiness;
    package.candidateDecisionCount = candidatePackage.candidateDecisionCount;
    package.warningCodes = candidatePackage.warningCodes;

    if (!candidatePackage.ok || !candidatePackage.validatorPassed) {
        package.reason = candidatePackage.reason.empty()
            ? "source candidate decision package is not ready"
            : candidatePackage.reason;
        package.readiness = "blocked";
        addUnique(package.warningCodes, "dsl_draft_source_not_ready");
        return package;
    }
    if (!candidatePackage.dryRunOnly ||
        candidatePackage.publishesOverlay ||
        candidatePackage.modelOutputUsed ||
        candidatePackage.modelOutputTrusted ||
        !candidatePackage.validationRequired) {
        package.reason = "source candidate decision package violates dry-run/model-trust contract";
        package.readiness = "blocked";
        addUnique(package.warningCodes, "dsl_draft_source_contract_violation");
        return package;
    }

    std::ostringstream dsl;
    std::set<std::string> emittedRuleKeys;
    for (const auto& candidate : candidatePackage.candidateDecisions) {
        if (!candidateHasRequiredIdentity(candidate) || !candidateRespectsDryRunBoundary(candidate)) {
            ++package.skippedCandidateCount;
            package.skippedCandidateIds.push_back(candidate.candidateDecisionId);
            continue;
        }

        const std::string ruleKey = candidate.campaignKey + "/" + candidate.playerCountryId + "/" + candidate.contentHash;
        if (!emittedRuleKeys.insert(ruleKey).second) {
            ++package.skippedCandidateCount;
            package.skippedCandidateIds.push_back(candidate.candidateDecisionId);
            continue;
        }

        appendRuleDsl(dsl, candidate, package.dslRuleCount);
        ++package.eligibleCandidateCount;
        ++package.dslRuleCount;
    }

    package.dslText = dsl.str();
    if (package.dslRuleCount == 0) {
        package.reason = "no candidate had enough parsed entry-point identity for a DSL draft";
        package.readiness = "needs_identity";
        addUnique(package.warningCodes, "dsl_draft_no_eligible_candidates");
        return package;
    }

    const generated_overlay::DslParser parser;
    const auto parseResult = parser.parse(package.dslText);
    if (!parseResult.ok) {
        package.reason = "generated DSL draft failed parser round-trip";
        package.readiness = "blocked";
        package.validationErrors.push_back("dsl_parse_failed: " + parseResult.error);
        addUnique(package.warningCodes, "dsl_draft_parse_failed");
        return package;
    }

    const generated_overlay::DslValidator validator;
    const auto validation = validator.validate(parseResult.program);
    package.validatorPassed = validation.ok;
    package.validationErrors = validation.errors;
    if (!validation.ok) {
        package.reason = "generated DSL draft failed validation";
        package.readiness = "blocked";
        addUnique(package.warningCodes, "dsl_draft_validation_failed");
        return package;
    }

    package.ok = true;
    package.reason = "validated dry-run DSL draft built";
    package.readiness = candidatePackage.blockedSourceEntryCount > 0 ? "ready_partial" : "ready";
    package.overlayCompileAllowed = false;
    return package;
}

std::string serializeSncDslDraftPackage(const SncDslDraftPackage& package)
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
    json << "  \"overlay_compile_allowed\": " << (package.overlayCompileAllowed ? "true" : "false") << ",\n";
    json << "  \"source_candidate_decision_package_path\": \"" << jsonEscape(package.sourceCandidateDecisionPackagePath.generic_string()) << "\",\n";
    json << "  \"source_candidate_readiness\": \"" << jsonEscape(package.sourceCandidateReadiness) << "\",\n";
    json << "  \"candidate_decision_count\": " << package.candidateDecisionCount << ",\n";
    json << "  \"eligible_candidate_count\": " << package.eligibleCandidateCount << ",\n";
    json << "  \"skipped_candidate_count\": " << package.skippedCandidateCount << ",\n";
    json << "  \"dsl_rule_count\": " << package.dslRuleCount << ",\n";
    json << "  \"warning_codes\": ";
    writeStringArray(json, package.warningCodes, 2);
    json << ",\n";
    json << "  \"validation_errors\": ";
    writeStringArray(json, package.validationErrors, 2);
    json << ",\n";
    json << "  \"skipped_candidate_ids\": ";
    writeStringArray(json, package.skippedCandidateIds, 2);
    json << ",\n";
    json << "  \"dsl_text\": \"" << jsonEscape(package.dslText) << "\"\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
