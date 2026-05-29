// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SeasonEmpireBriefBuilder.h"

#include <iomanip>
#include <sstream>

namespace strategic_nexus {
namespace {

bool hasNonWhitespace(const std::string& value)
{
    for (const char ch : value) {
        if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')) {
            return true;
        }
    }
    return false;
}

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
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(static_cast<unsigned char>(ch))
                       << std::dec << std::setfill(' ');
            } else {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

void writeStringArray(std::ostringstream& json, const char* name, const std::vector<std::string>& values, const bool trailingComma)
{
    json << "  \"" << name << "\": [\n";
    for (std::size_t index = 0; index < values.size(); ++index) {
        json << "    \"" << jsonEscape(values[index]) << "\"";
        if (index + 1 < values.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]";
    if (trailingComma) {
        json << ",";
    }
    json << "\n";
}

} // namespace

SeasonEmpireBrief SeasonEmpireBriefBuilder::build(
    const SeasonDeltaLedger& ledger,
    const std::string& empireId) const
{
    SeasonEmpireBrief brief;
    const bool campaignIdValid = hasNonWhitespace(ledger.campaignId);
    const bool empireIdValid = hasNonWhitespace(empireId);
    const bool supportedLedgerQuality = ledger.deltaQuality == "metadata_only";
    brief.ok = ledger.ok && campaignIdValid && empireIdValid && supportedLedgerQuality;
    brief.reason = ledger.ok ? "accepted" : ledger.reason;
    if (ledger.ok && !campaignIdValid) {
        brief.reason = "missing campaign id";
    } else if (ledger.ok && !empireIdValid) {
        brief.reason = "missing empire id";
    } else if (ledger.ok && !supportedLedgerQuality) {
        brief.reason = "unsupported source ledger quality";
    }
    brief.campaignId = ledger.campaignId;
    brief.empireId = empireId;
    brief.sourceLedgerQuality = ledger.deltaQuality;
    brief.relevantFacts = ledger.facts;
    brief.explicitUncertainties = ledger.uncertainties;

    if (!brief.ok) {
        return brief;
    }

    brief.missingInformation.push_back("empire_identity_resolver_not_implemented_yet");
    brief.missingInformation.push_back("empire_visible_state_not_extracted_yet");
    brief.missingInformation.push_back("diplomatic_relationships_not_extracted_yet");
    brief.missingInformation.push_back("wars_borders_economy_and_fleets_not_extracted_yet");

    brief.compressionNotes.push_back("metadata_only_archive_brief");
    brief.compressionNotes.push_back("do_not_infer_personality_or_strategy_from_this_brief_alone");
    return brief;
}

std::string serializeSeasonEmpireBrief(const SeasonEmpireBrief& brief)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (brief.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(brief.reason) << "\",\n";
    json << "  \"campaign_id\": \"" << jsonEscape(brief.campaignId) << "\",\n";
    json << "  \"empire_id\": \"" << jsonEscape(brief.empireId) << "\",\n";
    json << "  \"source_ledger_quality\": \"" << jsonEscape(brief.sourceLedgerQuality) << "\",\n";
    writeStringArray(json, "relevant_facts", brief.relevantFacts, true);
    writeStringArray(json, "explicit_uncertainties", brief.explicitUncertainties, true);
    writeStringArray(json, "missing_information", brief.missingInformation, true);
    writeStringArray(json, "compression_notes", brief.compressionNotes, false);
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus

