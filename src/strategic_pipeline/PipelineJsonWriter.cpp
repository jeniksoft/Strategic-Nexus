// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PipelineJsonWriter.h"

#include <sstream>

namespace strategic_nexus::strategic_pipeline {
namespace {

std::string escapeJsonString(const std::string& value)
{
    std::ostringstream escaped;
    for (const char character : value) {
        switch (character) {
        case '\\': escaped << "\\\\"; break;
        case '"': escaped << "\\\""; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default: escaped << character; break;
        }
    }
    return escaped.str();
}

void writeStringArray(std::ostringstream& json, const std::vector<std::string>& values, const int indentSpaces)
{
    const std::string indent(indentSpaces, ' ');
    const std::string itemIndent(indentSpaces + 2, ' ');
    json << "[";
    if (!values.empty()) {
        json << "\n";
        for (std::size_t index = 0; index < values.size(); ++index) {
            json << itemIndent << "\"" << escapeJsonString(values[index]) << "\"";
            if (index + 1 < values.size()) {
                json << ",";
            }
            json << "\n";
        }
        json << indent;
    }
    json << "]";
}

} // namespace

std::string serializeFinalStrategyPayload(const FinalStrategyPayload& payload)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": " << payload.schemaVersion << ",\n";
    json << "  \"sequence_id\": " << payload.sequenceId << ",\n";
    json << "  \"created_unix_ms\": " << payload.createdUnixMs << ",\n";
    json << "  \"ttl_ms\": " << payload.ttlMs << ",\n";
    json << "  \"bridge_available\": " << (payload.fallbackRequired ? "false" : "true") << ",\n";
    json << "  \"campaign_id\": \"" << escapeJsonString(payload.campaignId) << "\",\n";
    json << "  \"empire_id\": \"" << escapeJsonString(payload.empireId) << "\",\n";
    json << "  \"strategy_dimensions\": {\n";
    json << "    \"military_posture\": \"" << escapeJsonString(payload.militaryPosture) << "\",\n";
    json << "    \"research_bias\": \"" << escapeJsonString(payload.researchBias) << "\"\n";
    json << "  },\n";
    json << "  \"confidence\": " << payload.confidence << ",\n";
    json << "  \"fallback_required\": " << (payload.fallbackRequired ? "true" : "false") << "\n";
    json << "}\n";
    return json.str();
}

std::string serializeMinistryInputContext(const MinistryInputContext& input)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": " << input.schemaVersion << ",\n";
    json << "  \"source_schema_version\": " << input.sourceSchemaVersion << ",\n";
    json << "  \"schema_compatibility_state\": \"" << escapeJsonString(input.schemaCompatibilityState) << "\",\n";
    json << "  \"schema_compatibility_note\": \"" << escapeJsonString(input.schemaCompatibilityNote) << "\",\n";
    json << "  \"context_id\": \"" << escapeJsonString(input.contextId) << "\",\n";
    json << "  \"campaign_id\": \"" << escapeJsonString(input.campaignId) << "\",\n";
    json << "  \"empire_id\": \"" << escapeJsonString(input.empireId) << "\",\n";
    json << "  \"ministry\": \"" << escapeJsonString(input.ministry) << "\",\n";
    json << "  \"turn_context\": {\n";
    json << "    \"year\": " << input.turnContext.year << ",\n";
    json << "    \"is_at_war\": " << (input.turnContext.isAtWar ? "true" : "false") << ",\n";
    json << "    \"strategic_pressure\": " << input.turnContext.strategicPressure << "\n";
    json << "  },\n";
    json << "  \"known_facts\": ";
    writeStringArray(json, input.knownFacts, 2);
    json << ",\n";
    json << "  \"uncertainties\": ";
    writeStringArray(json, input.uncertainties, 2);
    json << ",\n";
    json << "  \"current_agenda\": {\n";
    json << "    \"military_posture\": \"" << escapeJsonString(input.currentMilitaryPosture) << "\",\n";
    json << "    \"research_bias\": \"" << escapeJsonString(input.currentResearchBias) << "\"\n";
    json << "  }\n";
    json << "}\n";
    return json.str();
}

std::string serializePipelineAuditRecord(const PipelineRunResult& result)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << escapeJsonString(result.reason) << "\",\n";
    json << "  \"ministry_input\": {\n";
    json << "    \"schema_version\": " << result.input.schemaVersion << ",\n";
    json << "    \"source_schema_version\": " << result.input.sourceSchemaVersion << ",\n";
    json << "    \"schema_compatibility_state\": \"" << escapeJsonString(result.input.schemaCompatibilityState) << "\",\n";
    json << "    \"schema_compatibility_note\": \"" << escapeJsonString(result.input.schemaCompatibilityNote) << "\",\n";
    json << "    \"context_id\": \"" << escapeJsonString(result.input.contextId) << "\",\n";
    json << "    \"campaign_id\": \"" << escapeJsonString(result.input.campaignId) << "\",\n";
    json << "    \"empire_id\": \"" << escapeJsonString(result.input.empireId) << "\",\n";
    json << "    \"ministry\": \"" << escapeJsonString(result.input.ministry) << "\",\n";
    json << "    \"turn_context\": {\n";
    json << "      \"year\": " << result.input.turnContext.year << ",\n";
    json << "      \"is_at_war\": " << (result.input.turnContext.isAtWar ? "true" : "false") << ",\n";
    json << "      \"strategic_pressure\": " << result.input.turnContext.strategicPressure << "\n";
    json << "    },\n";
    json << "    \"known_facts\": ";
    writeStringArray(json, result.input.knownFacts, 4);
    json << ",\n";
    json << "    \"uncertainties\": ";
    writeStringArray(json, result.input.uncertainties, 4);
    json << ",\n";
    json << "    \"current_agenda\": {\n";
    json << "      \"military_posture\": \"" << escapeJsonString(result.input.currentMilitaryPosture) << "\",\n";
    json << "      \"research_bias\": \"" << escapeJsonString(result.input.currentResearchBias) << "\"\n";
    json << "    }\n";
    json << "  },\n";
    json << "  \"processing_priority\": {\n";
    json << "    \"base_priority\": " << result.priorityScore.basePriority << ",\n";
    json << "    \"war_priority\": " << result.priorityScore.warPriority << ",\n";
    json << "    \"pressure_priority\": " << result.priorityScore.pressurePriority << ",\n";
    json << "    \"uncertainty_priority\": " << result.priorityScore.uncertaintyPriority << ",\n";
    json << "    \"total_priority\": " << result.priorityScore.totalPriority << ",\n";
    json << "    \"tier\": \"" << escapeJsonString(result.priorityScore.tier) << "\",\n";
    json << "    \"reasons\": ";
    writeStringArray(json, result.priorityScore.reasons, 4);
    json << "\n";
    json << "  },\n";
    json << "  \"ministry_output\": {\n";
    json << "    \"schema_version\": " << result.proposal.schemaVersion << ",\n";
    json << "    \"proposal_id\": \"" << escapeJsonString(result.proposal.proposalId) << "\",\n";
    json << "    \"campaign_id\": \"" << escapeJsonString(result.proposal.campaignId) << "\",\n";
    json << "    \"empire_id\": \"" << escapeJsonString(result.proposal.empireId) << "\",\n";
    json << "    \"ministry\": \"" << escapeJsonString(result.proposal.ministry) << "\",\n";
    json << "    \"requested_military_posture\": \"" << escapeJsonString(result.proposal.requestedMilitaryPosture) << "\",\n";
    json << "    \"requested_research_bias\": \"" << escapeJsonString(result.proposal.requestedResearchBias) << "\",\n";
    json << "    \"stated_reasons\": ";
    writeStringArray(json, result.proposal.statedReasons, 4);
    json << ",\n";
    json << "    \"explicit_risks\": ";
    writeStringArray(json, result.proposal.explicitRisks, 4);
    json << ",\n";
    json << "    \"explicit_uncertainties\": ";
    writeStringArray(json, result.proposal.explicitUncertainties, 4);
    json << ",\n";
    json << "    \"confidence\": " << result.proposal.confidence << "\n";
    json << "  },\n";
    json << "  \"clerk_input_brief\": {\n";
    json << "    \"schema_version\": " << result.brief.schemaVersion << ",\n";
    json << "    \"campaign_id\": \"" << escapeJsonString(result.brief.campaignId) << "\",\n";
    json << "    \"empire_id\": \"" << escapeJsonString(result.brief.empireId) << "\",\n";
    json << "    \"ministry\": \"" << escapeJsonString(result.brief.ministry) << "\",\n";
    json << "    \"source_context_id\": \"" << escapeJsonString(result.brief.sourceContextId) << "\",\n";
    json << "    \"relevant_facts\": ";
    writeStringArray(json, result.brief.relevantFacts, 4);
    json << ",\n";
    json << "    \"explicit_uncertainties\": ";
    writeStringArray(json, result.brief.explicitUncertainties, 4);
    json << ",\n";
    json << "    \"missing_information\": ";
    writeStringArray(json, result.brief.missingInformation, 4);
    json << ",\n";
    json << "    \"compression_notes\": ";
    writeStringArray(json, result.brief.compressionNotes, 4);
    json << "\n";
    json << "  },\n";
    json << "  \"final_payload\": " << serializeFinalStrategyPayload(result.payload);
    json << "}\n";
    return json.str();
}

std::string serializeProcessingQueue(
    const std::vector<EmpireProcessingQueueEntry>& entries,
    const std::vector<std::string>& invalidInputs)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"valid_entry_count\": " << entries.size() << ",\n";
    json << "  \"invalid_inputs\": ";
    writeStringArray(json, invalidInputs, 2);
    json << ",\n";
    json << "  \"entries\": [";
    if (!entries.empty()) {
        json << "\n";
        for (std::size_t index = 0; index < entries.size(); ++index) {
            const EmpireProcessingQueueEntry& entry = entries[index];
            json << "    {\n";
            json << "      \"rank\": " << (index + 1) << ",\n";
            json << "      \"original_index\": " << entry.originalIndex << ",\n";
            json << "      \"campaign_id\": \"" << escapeJsonString(entry.input.campaignId) << "\",\n";
            json << "      \"empire_id\": \"" << escapeJsonString(entry.input.empireId) << "\",\n";
            json << "      \"ministry\": \"" << escapeJsonString(entry.input.ministry) << "\",\n";
            json << "      \"tier\": \"" << escapeJsonString(entry.priorityScore.tier) << "\",\n";
            json << "      \"total_priority\": " << entry.priorityScore.totalPriority << ",\n";
            json << "      \"reasons\": ";
            writeStringArray(json, entry.priorityScore.reasons, 6);
            json << "\n";
            json << "    }";
            if (index + 1 < entries.size()) {
                json << ",";
            }
            json << "\n";
        }
        json << "  ";
    }
    json << "]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus::strategic_pipeline

