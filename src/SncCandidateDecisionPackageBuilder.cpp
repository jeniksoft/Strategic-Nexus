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

std::string textFact(const char* name, const std::string& value)
{
    return std::string(name) + ":" + value;
}

bool tryParseSaveYear(const std::string& saveDate, int& year)
{
    if (saveDate.size() != 10 || saveDate[4] != '.' || saveDate[7] != '.') {
        return false;
    }

    int parsedYear = 0;
    for (std::size_t index = 0; index < 4; ++index) {
        const unsigned char raw = static_cast<unsigned char>(saveDate[index]);
        if (std::isdigit(raw) == 0) {
            return false;
        }
        parsedYear = (parsedYear * 10) + static_cast<int>(raw - '0');
    }

    year = parsedYear;
    return true;
}

double deriveStrategicPressureHint(const SncDecisionInput& input)
{
    double pressure = 0.25;

    int saveYear = 0;
    if (tryParseSaveYear(input.saveDate, saveYear)) {
        if (saveYear >= 2300 && pressure < 0.45) {
            pressure = 0.45;
        } else if (saveYear >= 2250 && pressure < 0.35) {
            pressure = 0.35;
        } else if (pressure < 0.30) {
            pressure = 0.30;
        }
    }

    if (input.empireState.activeWarCount > 0 && pressure < 0.60) {
        pressure = 0.60;
    }

    return std::clamp(pressure, 0.0, 1.0);
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

ArchivedSaveEvidenceReference parseArchivedEvidenceReference(const std::string& json)
{
    ArchivedSaveEvidenceReference reference;
    reference.archivedPath = stringOrEmpty(json, "archived_path");
    reference.saveName = stringOrEmpty(json, "save_name");
    reference.saveDate = stringOrEmpty(json, "save_date");
    reference.contentHash = stringOrEmpty(json, "content_hash");
    reference.byteCount = extractJsonSize(json, "byte_count").value_or(0);
    return reference;
}

std::vector<ArchivedSaveEvidenceReference> extractArchivedEvidenceReferences(
    const std::string& json,
    const char* key)
{
    std::vector<ArchivedSaveEvidenceReference> references;
    const auto arrayBody = extractArrayBody(json, key);
    if (!arrayBody.has_value()) {
        return references;
    }

    for (const auto& object : extractObjectBodies(*arrayBody)) {
        auto reference = parseArchivedEvidenceReference(object);
        if (reference.archivedPath.empty() || reference.contentHash.empty()) {
            continue;
        }
        references.push_back(std::move(reference));
    }
    return references;
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
    input.parseStatus = stringOrEmpty(json, "parse_status");
    input.playerCountryId = stringOrEmpty(json, "player_country_id");
    input.empireName = stringOrEmpty(json, "empire_name");
    input.empireState.parsed = extractJsonBool(json, "empire_state_parsed").value_or(false);
    input.empireState.parseStatus = stringOrEmpty(json, "empire_state_parse_status");
    input.empireState.parserReason = stringOrEmpty(json, "empire_state_parser_reason");
    input.empireState.playerCountryId = input.playerCountryId;
    input.empireState.empireName = input.empireName;
    input.empireState.government = stringOrEmpty(json, "empire_state_government");
    input.empireState.authority = stringOrEmpty(json, "empire_state_authority");
    input.empireState.founderSpeciesName = stringOrEmpty(json, "empire_state_founder_species_name");
    input.empireState.capitalPlanetName = stringOrEmpty(json, "empire_state_capital_planet_name");
    input.empireState.homeSystemName = stringOrEmpty(json, "empire_state_home_system_name");
    input.empireState.ownedFleetCount = extractJsonSize(json, "empire_state_owned_fleet_count").value_or(0);
    input.empireState.activeWarCount = extractJsonSize(json, "empire_state_active_war_count").value_or(0);
    input.empireState.missingFields = common::extractJsonStringArray(json, "empire_state_missing_fields");
    input.empireState.uncertainties = common::extractJsonStringArray(json, "empire_state_uncertainties");
    input.evidencePolicy = stringOrEmpty(json, "evidence_policy");
    input.modelOutputTrusted = extractJsonBool(json, "model_output_trusted").value_or(false);
    input.validationRequired = extractJsonBool(json, "validation_required").value_or(true);
    input.futureEvidenceExcluded = extractJsonBool(json, "future_evidence_excluded").value_or(false);
    input.compatibleArchivedEvidenceCount = extractJsonSize(json, "compatible_archived_evidence_count").value_or(0);
    input.laterArchivedEvidenceCount = extractJsonSize(json, "later_archived_evidence_count").value_or(0);
    input.compatibleArchivedEvidenceSamples = extractArchivedEvidenceReferences(json, "compatible_archived_evidence_samples");
    input.laterArchivedEvidenceSamples = extractArchivedEvidenceReferences(json, "later_archived_evidence_samples");
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
    candidate.parseStatus = input.parseStatus;
    candidate.playerCountryId = input.playerCountryId;
    candidate.empireName = input.empireName;
    candidate.empireState = input.empireState;
    candidate.compatibleArchivedEvidenceSamples = input.compatibleArchivedEvidenceSamples;
    candidate.laterArchivedEvidenceSamples = input.laterArchivedEvidenceSamples;
    candidate.knownFacts = input.knownFacts;
    candidate.uncertainties = input.uncertainties;
    const double pressure = deriveStrategicPressureHint(input);
    candidate.militaryPosture = pressure >= 0.70 ? "aggressive" : "defensive";
    candidate.researchBias = pressure >= 0.65 ? "military_industry" : "economy";
    candidate.confidencePercent = static_cast<int>(pressure * 100.0 + 0.5);
    if (candidate.confidencePercent < 55) {
        candidate.confidencePercent = 55;
    }
    if (candidate.confidencePercent > 80) {
        candidate.confidencePercent = 80;
    }
    candidate.knownFacts.push_back("candidate_source:deterministic_v0_stub");
    candidate.knownFacts.push_back("candidate_action:observe_only");
    candidate.knownFacts.push_back(boolFact("candidate_publishes_overlay", candidate.publishesOverlay));
    candidate.knownFacts.push_back(boolFact("model_output_used", candidate.modelOutputUsed));
    candidate.knownFacts.push_back(textFact("candidate_military_posture", candidate.militaryPosture));
    candidate.knownFacts.push_back(textFact("candidate_research_bias", candidate.researchBias));
    candidate.knownFacts.push_back(textFact("candidate_pressure_hint_percent", std::to_string(candidate.confidencePercent)));
    if (!candidate.playerCountryId.empty()) {
        candidate.knownFacts.push_back("candidate_player_country_id:" + candidate.playerCountryId);
    }
    if (!candidate.empireName.empty()) {
        candidate.knownFacts.push_back("candidate_empire_name:" + candidate.empireName);
    }
    candidate.uncertainties.push_back("real_strategy_requires_more_than_headline_state");
    candidate.uncertainties.push_back("candidate_is_not_a_mod_rule");
    if (!candidate.empireState.parsed) {
        candidate.uncertainties.push_back("candidate_empire_state_summary_unavailable");
    }

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
        if (candidate.militaryPosture != "defensive" && candidate.militaryPosture != "aggressive") {
            errors.push_back("candidate has unsupported military_posture");
        }
        if (candidate.researchBias != "economy" && candidate.researchBias != "military_industry") {
            errors.push_back("candidate has unsupported research_bias");
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

void writeEmpireStateSummary(std::ostringstream& output, const SaveEmpireStateSummary& summary, const int indentSpaces)
{
    const std::string indent(indentSpaces, ' ');
    const std::string fieldIndent(indentSpaces + 2, ' ');
    output << "{\n";
    output << fieldIndent << "\"parsed\": " << (summary.parsed ? "true" : "false") << ",\n";
    output << fieldIndent << "\"parse_status\": \"" << jsonEscape(summary.parseStatus) << "\",\n";
    output << fieldIndent << "\"parser_reason\": \"" << jsonEscape(summary.parserReason) << "\",\n";
    output << fieldIndent << "\"player_country_id\": \"" << jsonEscape(summary.playerCountryId) << "\",\n";
    output << fieldIndent << "\"empire_name\": \"" << jsonEscape(summary.empireName) << "\",\n";
    output << fieldIndent << "\"government\": \"" << jsonEscape(summary.government) << "\",\n";
    output << fieldIndent << "\"authority\": \"" << jsonEscape(summary.authority) << "\",\n";
    output << fieldIndent << "\"founder_species_name\": \"" << jsonEscape(summary.founderSpeciesName) << "\",\n";
    output << fieldIndent << "\"capital_planet_name\": \"" << jsonEscape(summary.capitalPlanetName) << "\",\n";
    output << fieldIndent << "\"home_system_name\": \"" << jsonEscape(summary.homeSystemName) << "\",\n";
    output << fieldIndent << "\"owned_fleet_count\": " << summary.ownedFleetCount << ",\n";
    output << fieldIndent << "\"active_war_count\": " << summary.activeWarCount << ",\n";
    output << fieldIndent << "\"missing_fields\": ";
    writeStringArray(output, summary.missingFields, indentSpaces + 2);
    output << ",\n";
    output << fieldIndent << "\"uncertainties\": ";
    writeStringArray(output, summary.uncertainties, indentSpaces + 2);
    output << "\n";
    output << indent << "}";
}

void writeArchivedEvidenceReferences(
    std::ostringstream& output,
    const std::vector<ArchivedSaveEvidenceReference>& references,
    const int indentSpaces)
{
    const std::string indent(indentSpaces, ' ');
    const std::string itemIndent(indentSpaces + 2, ' ');
    const std::string fieldIndent(indentSpaces + 4, ' ');
    output << "[";
    if (!references.empty()) {
        output << "\n";
        for (std::size_t index = 0; index < references.size(); ++index) {
            const auto& reference = references[index];
            output << itemIndent << "{\n";
            output << fieldIndent << "\"archived_path\": \"" << jsonEscape(reference.archivedPath) << "\",\n";
            output << fieldIndent << "\"save_name\": \"" << jsonEscape(reference.saveName) << "\",\n";
            output << fieldIndent << "\"save_date\": \"" << jsonEscape(reference.saveDate) << "\",\n";
            output << fieldIndent << "\"content_hash\": \"" << jsonEscape(reference.contentHash) << "\",\n";
            output << fieldIndent << "\"byte_count\": " << reference.byteCount << "\n";
            output << itemIndent << "}" << (index + 1 < references.size() ? "," : "") << "\n";
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
    if (!schemaVersion.has_value() || *schemaVersion > 1) {
        result.reason = "unsupported decision input package schema";
        return result;
    }

    const auto sourceSchemaVersionValue = extractJsonSize(json, "source_schema_version");
    std::size_t sourceSchemaVersion = schemaVersion.value();
    if (sourceSchemaVersionValue.has_value()) {
        if (*sourceSchemaVersionValue > 1) {
            result.reason = "unsupported decision input package schema";
            return result;
        }
        sourceSchemaVersion = *sourceSchemaVersionValue;
        if (schemaVersion.value() == 0 && sourceSchemaVersion != 0) {
            result.reason = "decision input package json is malformed";
            return result;
        }
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
    package.schemaVersion = 1;
    package.sourceSchemaVersion = sourceSchemaVersion;
    if (package.sourceSchemaVersion == 0) {
        package.schemaCompatibilityState = "partial_compatibility";
        package.schemaCompatibilityNote =
            "migrated legacy decision input package schema_version 0 to current schema_version 1";
    } else {
        package.schemaCompatibilityState = "current";
        package.schemaCompatibilityNote.clear();
    }
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
    package.sourceSchemaVersion = decisionInputPackage.sourceSchemaVersion;
    package.schemaCompatibilityState = decisionInputPackage.schemaCompatibilityState;
    package.schemaCompatibilityNote = decisionInputPackage.schemaCompatibilityNote;
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
    json << "  \"source_schema_version\": " << package.sourceSchemaVersion << ",\n";
    json << "  \"schema_compatibility_state\": \"" << jsonEscape(package.schemaCompatibilityState) << "\",\n";
    json << "  \"schema_compatibility_note\": \"" << jsonEscape(package.schemaCompatibilityNote) << "\",\n";
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
        json << "      \"parse_status\": \"" << jsonEscape(candidate.parseStatus) << "\",\n";
        json << "      \"player_country_id\": \"" << jsonEscape(candidate.playerCountryId) << "\",\n";
        json << "      \"empire_name\": \"" << jsonEscape(candidate.empireName) << "\",\n";
        json << "      \"empire_state_summary\": ";
        writeEmpireStateSummary(json, candidate.empireState, 6);
        json << ",\n";
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
        json << "      \"compatible_archived_evidence_samples\": ";
        writeArchivedEvidenceReferences(json, candidate.compatibleArchivedEvidenceSamples, 6);
        json << ",\n";
        json << "      \"later_archived_evidence_samples\": ";
        writeArchivedEvidenceReferences(json, candidate.laterArchivedEvidenceSamples, 6);
        json << ",\n";
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
