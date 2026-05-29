// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SeasonDeltaLedgerBuilder.h"

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

std::string countFact(const char* name, const std::size_t value)
{
    std::ostringstream text;
    text << name << ":" << value;
    return text.str();
}

std::string byteFact(const char* name, const std::uintmax_t value)
{
    std::ostringstream text;
    text << name << ":" << value;
    return text.str();
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

SeasonDeltaLedger SeasonDeltaLedgerBuilder::build(
    const AutosaveArchiveSummary& summary,
    const std::string& campaignId) const
{
    SeasonDeltaLedger ledger;
    const bool campaignIdValid = hasNonWhitespace(campaignId);
    const bool hasCopiedSaves = summary.copiedSaveCount > 0;
    ledger.ok = summary.ok && campaignIdValid && hasCopiedSaves;
    ledger.reason = summary.ok ? "accepted" : summary.reason;
    if (summary.ok && !campaignIdValid) {
        ledger.reason = "missing campaign id";
    } else if (summary.ok && !hasCopiedSaves) {
        ledger.reason = "no copied saves in verified archive";
    }
    ledger.sessionArchiveDirectory = summary.sessionArchiveDirectory;
    ledger.campaignId = campaignId;
    ledger.deltaQuality = ledger.ok ? "metadata_only" : "unavailable";
    ledger.copiedSaveCount = summary.copiedSaveCount;
    ledger.totalByteCount = summary.totalByteCount;
    ledger.firstArchivedPath = summary.firstArchivedPath;
    ledger.firstContentHash = summary.firstContentHash;
    ledger.lastArchivedPath = summary.lastArchivedPath;
    ledger.lastContentHash = summary.lastContentHash;

    if (ledger.ok) {
        ledger.facts.push_back("archive_verified:true");
        ledger.facts.push_back(countFact("archived_save_count", summary.copiedSaveCount));
        ledger.facts.push_back(byteFact("archived_total_byte_count", summary.totalByteCount));
        if (!summary.firstContentHash.empty()) {
            ledger.facts.push_back("first_archive_hash:" + summary.firstContentHash);
        }
        if (!summary.lastContentHash.empty()) {
            ledger.facts.push_back("last_archive_hash:" + summary.lastContentHash);
        }

        ledger.uncertainties.push_back("save_content_not_parsed_yet");
        ledger.uncertainties.push_back("empire_state_not_extracted_yet");
        ledger.uncertainties.push_back("campaign_identity_supplied_by_orchestrator");
    } else if (!summary.ok) {
        ledger.uncertainties.push_back("archive_not_verified");
    }
    return ledger;
}

std::string serializeSeasonDeltaLedger(const SeasonDeltaLedger& ledger)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (ledger.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(ledger.reason) << "\",\n";
    json << "  \"campaign_id\": \"" << jsonEscape(ledger.campaignId) << "\",\n";
    json << "  \"delta_quality\": \"" << jsonEscape(ledger.deltaQuality) << "\",\n";
    json << "  \"session_archive_directory\": \"" << jsonEscape(ledger.sessionArchiveDirectory.generic_string()) << "\",\n";
    json << "  \"copied_save_count\": " << ledger.copiedSaveCount << ",\n";
    json << "  \"total_byte_count\": " << ledger.totalByteCount << ",\n";
    json << "  \"first_archived_path\": \"" << jsonEscape(ledger.firstArchivedPath) << "\",\n";
    json << "  \"first_content_hash\": \"" << jsonEscape(ledger.firstContentHash) << "\",\n";
    json << "  \"last_archived_path\": \"" << jsonEscape(ledger.lastArchivedPath) << "\",\n";
    json << "  \"last_content_hash\": \"" << jsonEscape(ledger.lastContentHash) << "\",\n";
    writeStringArray(json, "facts", ledger.facts, true);
    writeStringArray(json, "uncertainties", ledger.uncertainties, false);
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus

