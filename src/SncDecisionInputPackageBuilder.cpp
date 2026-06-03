// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncDecisionInputPackageBuilder.h"

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

std::string countFact(const char* name, const std::size_t value)
{
    std::ostringstream text;
    text << name << ":" << value;
    return text.str();
}

std::string boolFact(const char* name, const bool value)
{
    return std::string(name) + ":" + (value ? "true" : "false");
}

std::string hashPrefix(const std::string& hash)
{
    return hash.substr(0, std::min<std::size_t>(12, hash.size()));
}

std::string blockedReasonFor(const PostPlayPackageEntry& entry)
{
    if (entry.decisionInputState == "blocked_branch_ambiguity") {
        return "branch ambiguity blocks decision input";
    }
    if (entry.decisionInputState == "needs_deferred_parse") {
        return "save date or identity requires deferred parsing";
    }
    if (entry.decisionInputState == "insufficient_history") {
        return "no compatible archived history is available";
    }
    return entry.decisionInputState.empty() ? "decision input is not allowed" : entry.decisionInputState;
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

PostPlayPackageEntry parseEntry(const std::string& json)
{
    PostPlayPackageEntry entry;
    entry.packageEntryId = stringOrEmpty(json, "package_entry_id");
    entry.entryPointId = stringOrEmpty(json, "entry_point_id");
    entry.campaignKey = stringOrEmpty(json, "campaign_key");
    entry.sourceKind = stringOrEmpty(json, "source_kind");
    entry.saveName = stringOrEmpty(json, "save_name");
    entry.saveDate = stringOrEmpty(json, "save_date");
    entry.contentHash = stringOrEmpty(json, "content_hash");
    entry.parseStatus = stringOrEmpty(json, "parse_status");
    entry.playerCountryId = stringOrEmpty(json, "player_country_id");
    entry.empireName = stringOrEmpty(json, "empire_name");
    entry.empireState.parsed = extractJsonBool(json, "empire_state_parsed").value_or(false);
    entry.empireState.parseStatus = stringOrEmpty(json, "empire_state_parse_status");
    entry.empireState.parserReason = stringOrEmpty(json, "empire_state_parser_reason");
    entry.empireState.playerCountryId = entry.playerCountryId;
    entry.empireState.empireName = entry.empireName;
    entry.empireState.government = stringOrEmpty(json, "empire_state_government");
    entry.empireState.authority = stringOrEmpty(json, "empire_state_authority");
    entry.empireState.founderSpeciesName = stringOrEmpty(json, "empire_state_founder_species_name");
    entry.empireState.capitalPlanetName = stringOrEmpty(json, "empire_state_capital_planet_name");
    entry.empireState.homeSystemName = stringOrEmpty(json, "empire_state_home_system_name");
    entry.empireState.ownedFleetCount = extractJsonSize(json, "empire_state_owned_fleet_count").value_or(0);
    entry.empireState.activeWarCount = extractJsonSize(json, "empire_state_active_war_count").value_or(0);
    entry.empireState.missingFields = common::extractJsonStringArray(json, "empire_state_missing_fields");
    entry.empireState.uncertainties = common::extractJsonStringArray(json, "empire_state_uncertainties");
    entry.analysisState = stringOrEmpty(json, "analysis_state");
    entry.decisionInputState = stringOrEmpty(json, "decision_input_state");
    entry.ruleScope = stringOrEmpty(json, "rule_scope");
    entry.evidencePolicy = stringOrEmpty(json, "evidence_policy");
    entry.decisionInputAllowed = extractJsonBool(json, "decision_input_allowed").value_or(false);
    entry.compatibleHistoryAvailable = extractJsonBool(json, "compatible_history_available").value_or(false);
    entry.futureEvidenceExcluded = extractJsonBool(json, "future_evidence_excluded").value_or(false);
    entry.compatibleArchivedEvidenceCount = extractJsonSize(json, "compatible_archived_evidence_count").value_or(0);
    entry.laterArchivedEvidenceCount = extractJsonSize(json, "later_archived_evidence_count").value_or(0);
    entry.compatibleArchivedEvidenceSamples = extractArchivedEvidenceReferences(json, "compatible_archived_evidence_samples");
    entry.laterArchivedEvidenceSamples = extractArchivedEvidenceReferences(json, "later_archived_evidence_samples");
    entry.warningCodes = common::extractJsonStringArray(json, "warning_codes");
    return entry;
}

SncDecisionInput buildDecisionInput(const PostPlayPackageEntry& entry)
{
    SncDecisionInput input;
    input.packageEntryId = entry.packageEntryId;
    input.decisionInputId = "decision-input::" + entry.ruleScope;
    input.campaignKey = entry.campaignKey;
    input.ruleScope = entry.ruleScope;
    input.sourceKind = entry.sourceKind;
    input.saveName = entry.saveName;
    input.saveDate = entry.saveDate;
    input.contentHash = entry.contentHash;
    input.parseStatus = entry.parseStatus;
    input.playerCountryId = entry.playerCountryId;
    input.empireName = entry.empireName;
    input.empireState = entry.empireState;
    input.evidencePolicy = entry.evidencePolicy;
    input.futureEvidenceExcluded = entry.futureEvidenceExcluded;
    input.compatibleArchivedEvidenceCount = entry.compatibleArchivedEvidenceCount;
    input.laterArchivedEvidenceCount = entry.laterArchivedEvidenceCount;
    input.compatibleArchivedEvidenceSamples = entry.compatibleArchivedEvidenceSamples;
    input.laterArchivedEvidenceSamples = entry.laterArchivedEvidenceSamples;

    input.knownFacts.push_back("decision_input_source:snc_post_play_package");
    input.knownFacts.push_back("rule_scope:" + entry.ruleScope);
    input.knownFacts.push_back("campaign_key:" + entry.campaignKey);
    input.knownFacts.push_back("source_kind:" + entry.sourceKind);
    input.knownFacts.push_back("save_name:" + entry.saveName);
    input.knownFacts.push_back("save_date:" + entry.saveDate);
    input.knownFacts.push_back("content_hash_prefix:" + hashPrefix(entry.contentHash));
    input.knownFacts.push_back("parse_status:" + entry.parseStatus);
    input.knownFacts.push_back("evidence_policy:" + entry.evidencePolicy);
    input.knownFacts.push_back(countFact("compatible_archived_evidence_count", entry.compatibleArchivedEvidenceCount));
    input.knownFacts.push_back(countFact("later_archived_evidence_excluded_count", entry.laterArchivedEvidenceCount));
    input.knownFacts.push_back(boolFact("future_evidence_excluded", entry.futureEvidenceExcluded));
    input.knownFacts.push_back(boolFact("empire_state_parsed", entry.empireState.parsed));
    if (!entry.playerCountryId.empty()) {
        input.knownFacts.push_back("player_country_id:" + entry.playerCountryId);
    }
    if (!entry.empireName.empty()) {
        input.knownFacts.push_back("empire_name:" + entry.empireName);
    }

    if (!entry.empireState.parsed) {
        input.uncertainties.push_back("empire_state_summary_unavailable");
    } else {
        input.uncertainties.push_back("empire_state_summary_is_headline_only");
    }
    input.uncertainties.push_back("model_output_untrusted_requires_validation");
    if (entry.futureEvidenceExcluded) {
        input.uncertainties.push_back("future_evidence_excluded_for_entry_point");
    }
    return input;
}

SncBlockedDecisionInput buildBlockedEntry(const PostPlayPackageEntry& entry)
{
    SncBlockedDecisionInput blocked;
    blocked.packageEntryId = entry.packageEntryId;
    blocked.campaignKey = entry.campaignKey;
    blocked.ruleScope = entry.ruleScope;
    blocked.saveName = entry.saveName;
    blocked.saveDate = entry.saveDate;
    blocked.decisionInputState = entry.decisionInputState;
    blocked.reason = blockedReasonFor(entry);
    blocked.warningCodes = entry.warningCodes;
    return blocked;
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

PostPlayPackageReadResult parsePostPlayPackageJson(const std::string& json)
{
    PostPlayPackageReadResult result;
    if (json.empty()) {
        result.reason = "post-play package json is empty";
        return result;
    }

    const auto schemaVersion = extractJsonSize(json, "schema_version");
    if (!schemaVersion.has_value() || *schemaVersion != 1) {
        result.reason = "unsupported post-play package schema";
        return result;
    }

    const auto ok = extractJsonBool(json, "ok");
    const auto dryRunOnly = extractJsonBool(json, "dry_run_only");
    const auto publishesOverlay = extractJsonBool(json, "publishes_overlay");
    const auto entriesBody = extractArrayBody(json, "entries");
    if (!ok.has_value() || !dryRunOnly.has_value() || !publishesOverlay.has_value() || !entriesBody.has_value()) {
        result.reason = "post-play package json is missing required fields";
        return result;
    }

    auto& package = result.package;
    package.ok = *ok;
    package.reason = stringOrEmpty(json, "reason");
    package.readiness = stringOrEmpty(json, "readiness");
    package.dryRunOnly = *dryRunOnly;
    package.publishesOverlay = *publishesOverlay;
    package.sessionArchiveDirectory = stringOrEmpty(json, "session_archive_directory");
    package.archiveVerified = extractJsonBool(json, "archive_verified").value_or(false);
    package.entryPointAnalysisReadiness = stringOrEmpty(json, "entry_point_analysis_readiness");
    package.branchAmbiguityDetected = extractJsonBool(json, "branch_ambiguity_detected").value_or(false);
    package.copiedSaveCount = extractJsonSize(json, "copied_save_count").value_or(0);
    package.totalByteCount = extractJsonSize(json, "total_byte_count").value_or(0);
    package.entryPointCount = extractJsonSize(json, "entry_point_count").value_or(0);
    package.decisionReadyEntryCount = extractJsonSize(json, "decision_ready_entry_count").value_or(0);
    package.archivedEvidenceCount = extractJsonSize(json, "archived_evidence_count").value_or(0);
    package.warningCodes = common::extractJsonStringArray(json, "warning_codes");

    for (const auto& object : extractObjectBodies(*entriesBody)) {
        auto entry = parseEntry(object);
        if (entry.packageEntryId.empty() || entry.campaignKey.empty() || entry.ruleScope.empty()) {
            result.reason = "post-play package entry is missing required identity fields";
            result.package = {};
            return result;
        }
        package.entries.push_back(entry);
    }

    result.ok = true;
    result.reason = "post-play package parsed";
    return result;
}

SncDecisionInputPackage SncDecisionInputPackageBuilder::build(
    const PostPlayPackage& postPlayPackage,
    const std::filesystem::path& sourcePostPlayPackagePath) const
{
    SncDecisionInputPackage package;
    package.sourcePostPlayPackagePath = sourcePostPlayPackagePath;
    package.sessionArchiveDirectory = postPlayPackage.sessionArchiveDirectory;
    package.sourcePostPlayReadiness = postPlayPackage.readiness;
    package.entryPointCount = postPlayPackage.entryPointCount;
    package.archivedEvidenceCount = postPlayPackage.archivedEvidenceCount;
    package.warningCodes = postPlayPackage.warningCodes;

    if (!postPlayPackage.ok) {
        package.reason = postPlayPackage.reason.empty() ? "source post-play package is not ready" : postPlayPackage.reason;
        package.readiness = "blocked";
        addUnique(package.warningCodes, "decision_input_source_post_play_not_ready");
        return package;
    }
    if (!postPlayPackage.dryRunOnly || postPlayPackage.publishesOverlay) {
        package.reason = "source post-play package violates dry-run/no-overlay contract";
        package.readiness = "blocked";
        addUnique(package.warningCodes, "decision_input_source_contract_violation");
        return package;
    }

    for (const auto& entry : postPlayPackage.entries) {
        if (entry.decisionInputAllowed) {
            package.decisionInputs.push_back(buildDecisionInput(entry));
        } else {
            package.blockedEntries.push_back(buildBlockedEntry(entry));
        }
    }

    package.decisionInputCount = package.decisionInputs.size();
    package.blockedEntryCount = package.blockedEntries.size();
    package.ok = true;

    if (package.decisionInputCount > 0 && package.blockedEntryCount > 0) {
        package.reason = "decision input package built; some entries remain blocked";
        package.readiness = "ready_partial";
    } else if (package.decisionInputCount > 0) {
        package.reason = "decision input package built";
        package.readiness = "ready";
    } else {
        package.reason = "decision input package built but no entry is decision-ready";
        package.readiness = "needs_attention";
        addUnique(package.warningCodes, "decision_input_no_ready_entries");
    }

    return package;
}

std::string serializeSncDecisionInputPackage(const SncDecisionInputPackage& package)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (package.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(package.reason) << "\",\n";
    json << "  \"readiness\": \"" << jsonEscape(package.readiness) << "\",\n";
    json << "  \"dry_run_only\": " << (package.dryRunOnly ? "true" : "false") << ",\n";
    json << "  \"publishes_overlay\": " << (package.publishesOverlay ? "true" : "false") << ",\n";
    json << "  \"model_output_trusted\": " << (package.modelOutputTrusted ? "true" : "false") << ",\n";
    json << "  \"validation_required\": " << (package.validationRequired ? "true" : "false") << ",\n";
    json << "  \"source_post_play_package_path\": \"" << jsonEscape(package.sourcePostPlayPackagePath.generic_string()) << "\",\n";
    json << "  \"session_archive_directory\": \"" << jsonEscape(package.sessionArchiveDirectory.generic_string()) << "\",\n";
    json << "  \"source_post_play_readiness\": \"" << jsonEscape(package.sourcePostPlayReadiness) << "\",\n";
    json << "  \"entry_point_count\": " << package.entryPointCount << ",\n";
    json << "  \"decision_input_count\": " << package.decisionInputCount << ",\n";
    json << "  \"blocked_entry_count\": " << package.blockedEntryCount << ",\n";
    json << "  \"archived_evidence_count\": " << package.archivedEvidenceCount << ",\n";
    json << "  \"warning_codes\": ";
    writeStringArray(json, package.warningCodes, 2);
    json << ",\n";
    json << "  \"decision_inputs\": [\n";
    for (std::size_t index = 0; index < package.decisionInputs.size(); ++index) {
        const auto& input = package.decisionInputs[index];
        json << "    {\n";
        json << "      \"decision_input_id\": \"" << jsonEscape(input.decisionInputId) << "\",\n";
        json << "      \"package_entry_id\": \"" << jsonEscape(input.packageEntryId) << "\",\n";
        json << "      \"campaign_key\": \"" << jsonEscape(input.campaignKey) << "\",\n";
        json << "      \"rule_scope\": \"" << jsonEscape(input.ruleScope) << "\",\n";
        json << "      \"source_kind\": \"" << jsonEscape(input.sourceKind) << "\",\n";
        json << "      \"save_name\": \"" << jsonEscape(input.saveName) << "\",\n";
        json << "      \"save_date\": \"" << jsonEscape(input.saveDate) << "\",\n";
        json << "      \"content_hash\": \"" << jsonEscape(input.contentHash) << "\",\n";
        json << "      \"parse_status\": \"" << jsonEscape(input.parseStatus) << "\",\n";
        json << "      \"player_country_id\": \"" << jsonEscape(input.playerCountryId) << "\",\n";
        json << "      \"empire_name\": \"" << jsonEscape(input.empireName) << "\",\n";
        json << "      \"empire_state_parsed\": " << (input.empireState.parsed ? "true" : "false") << ",\n";
        json << "      \"empire_state_parse_status\": \"" << jsonEscape(input.empireState.parseStatus) << "\",\n";
        json << "      \"empire_state_parser_reason\": \"" << jsonEscape(input.empireState.parserReason) << "\",\n";
        json << "      \"empire_state_government\": \"" << jsonEscape(input.empireState.government) << "\",\n";
        json << "      \"empire_state_authority\": \"" << jsonEscape(input.empireState.authority) << "\",\n";
        json << "      \"empire_state_founder_species_name\": \"" << jsonEscape(input.empireState.founderSpeciesName) << "\",\n";
        json << "      \"empire_state_capital_planet_name\": \"" << jsonEscape(input.empireState.capitalPlanetName) << "\",\n";
        json << "      \"empire_state_home_system_name\": \"" << jsonEscape(input.empireState.homeSystemName) << "\",\n";
        json << "      \"empire_state_owned_fleet_count\": " << input.empireState.ownedFleetCount << ",\n";
        json << "      \"empire_state_active_war_count\": " << input.empireState.activeWarCount << ",\n";
        json << "      \"empire_state_missing_fields\": ";
        writeStringArray(json, input.empireState.missingFields, 6);
        json << ",\n";
        json << "      \"empire_state_uncertainties\": ";
        writeStringArray(json, input.empireState.uncertainties, 6);
        json << ",\n";
        json << "      \"evidence_policy\": \"" << jsonEscape(input.evidencePolicy) << "\",\n";
        json << "      \"model_output_trusted\": " << (input.modelOutputTrusted ? "true" : "false") << ",\n";
        json << "      \"validation_required\": " << (input.validationRequired ? "true" : "false") << ",\n";
        json << "      \"future_evidence_excluded\": " << (input.futureEvidenceExcluded ? "true" : "false") << ",\n";
        json << "      \"compatible_archived_evidence_count\": " << input.compatibleArchivedEvidenceCount << ",\n";
        json << "      \"later_archived_evidence_count\": " << input.laterArchivedEvidenceCount << ",\n";
        json << "      \"compatible_archived_evidence_samples\": ";
        writeArchivedEvidenceReferences(json, input.compatibleArchivedEvidenceSamples, 6);
        json << ",\n";
        json << "      \"later_archived_evidence_samples\": ";
        writeArchivedEvidenceReferences(json, input.laterArchivedEvidenceSamples, 6);
        json << ",\n";
        json << "      \"known_facts\": ";
        writeStringArray(json, input.knownFacts, 6);
        json << ",\n";
        json << "      \"uncertainties\": ";
        writeStringArray(json, input.uncertainties, 6);
        json << "\n";
        json << "    }" << (index + 1 < package.decisionInputs.size() ? "," : "") << "\n";
    }
    json << "  ],\n";
    json << "  \"blocked_entries\": [\n";
    for (std::size_t index = 0; index < package.blockedEntries.size(); ++index) {
        const auto& blocked = package.blockedEntries[index];
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
        json << "    }" << (index + 1 < package.blockedEntries.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
