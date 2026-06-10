// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PersonalityProfileStore.h"

#include "common/FileUtil.h"
#include "common/JsonExtract.h"
#include "common/JsonSanity.h"

#include <cmath>
#include <iomanip>
#include <regex>
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

std::string sanitizePathPart(const std::string& value)
{
    std::string sanitized;
    sanitized.reserve(value.size());
    for (const char ch : value) {
        const bool allowed = (ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')
            || ch == '_' || ch == '-';
        sanitized.push_back(allowed ? ch : '_');
    }

    if (sanitized.empty()) {
        return "unknown";
    }

    return sanitized;
}

std::string profileKey(const std::string& campaignId, const std::string& empireId)
{
    return campaignId + "::" + empireId;
}

bool extractJsonBool(const std::string& json, const char* key, bool& value)
{
    const std::regex pattern("\\\"" + std::string(key) + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return false;
    }

    value = match[1].str() == "true";
    return true;
}

bool extractJsonDouble(const std::string& json, const char* key, double& value)
{
    const std::regex pattern("\\\"" + std::string(key) + "\\\"\\s*:\\s*([-+0-9.eE]+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return false;
    }

    try {
        value = std::stod(match[1].str());
    } catch (...) {
        return false;
    }

    return std::isfinite(value);
}

bool isBoundedPersonalityValue(const double value)
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool isValidRecordForSave(const PersonalityProfileRecord& record)
{
    if (!record.ok || !hasNonWhitespace(record.campaignId) || !hasNonWhitespace(record.empireId)) {
        return false;
    }

    if (!hasNonWhitespace(record.sourceSaveDate)
        || !hasNonWhitespace(record.sourceSavePath)
        || !hasNonWhitespace(record.validatedUpdateSummary)) {
        return false;
    }

    if (!isBoundedPersonalityValue(record.confidence)) {
        return false;
    }

    return isBoundedPersonalityValue(record.personality.boldness)
        && isBoundedPersonalityValue(record.personality.paranoia)
        && isBoundedPersonalityValue(record.personality.honor)
        && isBoundedPersonalityValue(record.personality.opportunism);
}

PersonalityProfileRecord parseProfileRecord(const std::string& json)
{
    PersonalityProfileRecord record;
    if (!common::hasBalancedJsonDelimiters(json)) {
        record.reason = "malformed personality profile record";
        return record;
    }

    double schemaVersion = 0.0;
    bool ok = false;
    bool zeroHistoryBootstrap = false;
    double confidence = 0.0;
    double boldness = 0.0;
    double paranoia = 0.0;
    double honor = 0.0;
    double opportunism = 0.0;

    const auto reason = common::extractJsonString(json, "reason");
    const auto campaignId = common::extractJsonString(json, "campaign_id");
    const auto empireId = common::extractJsonString(json, "empire_id");
    const auto sourceSaveDate = common::extractJsonString(json, "source_save_date");
    const auto sourceSavePath = common::extractJsonString(json, "source_save_path");
    const auto validatedUpdateSummary = common::extractJsonString(json, "validated_update_summary");
    const auto storedProfileKey = common::extractJsonString(json, "profile_key");

    if (!extractJsonDouble(json, "schema_version", schemaVersion)
        || schemaVersion != 1.0
        || !extractJsonBool(json, "ok", ok)
        || !reason.has_value()
        || !campaignId.has_value()
        || !empireId.has_value()
        || !sourceSaveDate.has_value()
        || !sourceSavePath.has_value()
        || !validatedUpdateSummary.has_value()
        || !storedProfileKey.has_value()
        || !extractJsonDouble(json, "confidence", confidence)
        || !extractJsonBool(json, "zero_history_bootstrap", zeroHistoryBootstrap)
        || !extractJsonDouble(json, "boldness", boldness)
        || !extractJsonDouble(json, "paranoia", paranoia)
        || !extractJsonDouble(json, "honor", honor)
        || !extractJsonDouble(json, "opportunism", opportunism)) {
        record.reason = "malformed personality profile record";
        return record;
    }

    if (storedProfileKey.value() != profileKey(campaignId.value(), empireId.value())) {
        record.reason = "malformed personality profile record";
        return record;
    }

    record.ok = ok;
    record.reason = reason.value();
    record.campaignId = campaignId.value();
    record.empireId = empireId.value();
    record.sourceSaveDate = sourceSaveDate.value();
    record.sourceSavePath = sourceSavePath.value();
    record.validatedUpdateSummary = validatedUpdateSummary.value();
    record.confidence = confidence;
    record.zeroHistoryBootstrap = zeroHistoryBootstrap;
    record.personality.boldness = boldness;
    record.personality.paranoia = paranoia;
    record.personality.honor = honor;
    record.personality.opportunism = opportunism;

    if (!record.ok || !isValidRecordForSave(record)) {
        record.ok = false;
        record.reason = "malformed personality profile record";
        return record;
    }

    return record;
}

} // namespace

std::filesystem::path PersonalityProfileStore::profilePathFor(
    const std::filesystem::path& root,
    const std::string& campaignId,
    const std::string& empireId) const
{
    return root / sanitizePathPart(campaignId) / (sanitizePathPart(empireId) + ".json");
}

bool PersonalityProfileStore::save(const std::filesystem::path& root, const PersonalityProfileRecord& record) const
{
    if (!isValidRecordForSave(record)) {
        return false;
    }

    return common::writeTextFileAtomically(profilePathFor(root, record.campaignId, record.empireId), serializePersonalityProfileRecord(record));
}

PersonalityProfileRecord PersonalityProfileStore::load(
    const std::filesystem::path& root,
    const std::string& campaignId,
    const std::string& empireId) const
{
    std::string json;
    if (!common::tryReadTextFile(profilePathFor(root, campaignId, empireId), json)) {
        PersonalityProfileRecord record;
        record.reason = "missing personality profile record";
        return record;
    }

    return parseProfileRecord(json);
}

std::string serializePersonalityProfileRecord(const PersonalityProfileRecord& record)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (record.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(record.reason) << "\",\n";
    json << "  \"profile_key\": \"" << jsonEscape(profileKey(record.campaignId, record.empireId)) << "\",\n";
    json << "  \"campaign_id\": \"" << jsonEscape(record.campaignId) << "\",\n";
    json << "  \"empire_id\": \"" << jsonEscape(record.empireId) << "\",\n";
    json << "  \"source_save_date\": \"" << jsonEscape(record.sourceSaveDate) << "\",\n";
    json << "  \"source_save_path\": \"" << jsonEscape(record.sourceSavePath) << "\",\n";
    json << "  \"validated_update_summary\": \"" << jsonEscape(record.validatedUpdateSummary) << "\",\n";
    json << "  \"confidence\": " << record.confidence << ",\n";
    json << "  \"zero_history_bootstrap\": " << (record.zeroHistoryBootstrap ? "true" : "false") << ",\n";
    json << "  \"personality\": {\n";
    json << "    \"boldness\": " << record.personality.boldness << ",\n";
    json << "    \"paranoia\": " << record.personality.paranoia << ",\n";
    json << "    \"honor\": " << record.personality.honor << ",\n";
    json << "    \"opportunism\": " << record.personality.opportunism << "\n";
    json << "  }\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
