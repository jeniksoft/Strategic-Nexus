// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "PersonalityProfileStore.h"

#include "common/FileUtil.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

strategic_nexus::PersonalityProfileRecord makeRecord()
{
    strategic_nexus::PersonalityProfileRecord record;
    record.ok = true;
    record.reason = "accepted validated personality profile";
    record.sourceSchemaVersion = 1;
    record.schemaCompatibilityState = "current";
    record.schemaCompatibilityNote.clear();
    record.campaignId = "campaign_001";
    record.empireId = "empire_002";
    record.sourceSaveDate = "2026-06-10T05:02:00+02:00";
    record.sourceSavePath = "dist/archive/session_001/save_2201.sav";
    record.validatedUpdateSummary = "steady defensive posture with cautious diplomacy";
    record.confidence = 0.79;
    record.zeroHistoryBootstrap = true;
    record.personality.boldness = 0.31;
    record.personality.paranoia = 0.72;
    record.personality.honor = 0.65;
    record.personality.opportunism = 0.28;
    return record;
}

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/personality_profile_store_fixture");
    std::filesystem::remove_all(root);

    const strategic_nexus::PersonalityProfileStore store;
    const auto record = makeRecord();
    const auto expectedPath = store.profilePathFor(root, record.campaignId, record.empireId);

    requireCondition(expectedPath == root / "campaign_001" / "empire_002.json", "profile path should be campaign and empire scoped");
    requireCondition(store.save(root, record), "store should persist a valid personality record");
    requireCondition(std::filesystem::exists(expectedPath), "store should write the expected profile file");

    const auto json = strategic_nexus::common::readTextFile(expectedPath);
    requireCondition(json.find("\"source_schema_version\": 1") != std::string::npos, "JSON should include source schema version");
    requireCondition(json.find("\"schema_compatibility_state\": \"current\"") != std::string::npos, "JSON should include current compatibility state");
    requireCondition(json.find("\"schema_compatibility_note\": \"\"") != std::string::npos, "JSON should include empty compatibility note for current records");
    requireCondition(json.find("\"profile_key\": \"campaign_001::empire_002\"") != std::string::npos, "JSON should include profile key");
    requireCondition(json.find("\"validated_update_summary\": \"steady defensive posture with cautious diplomacy\"") != std::string::npos, "JSON should include validated update summary");
    requireCondition(json.find("\"source_save_date\": \"2026-06-10T05:02:00+02:00\"") != std::string::npos, "JSON should include source save date");
    requireCondition(json.find("\"zero_history_bootstrap\": true") != std::string::npos, "JSON should include zero-history bootstrap flag");
    requireCondition(json.find("\"conversation_prompt\"") == std::string::npos, "JSON should not persist raw conversation prompts");

    const auto loaded = store.load(root, record.campaignId, record.empireId);
    requireCondition(loaded.ok, "store should load a saved personality record");
    requireCondition(loaded.reason == record.reason, "loaded record should preserve reason");
    requireCondition(loaded.campaignId == record.campaignId, "loaded record should preserve campaign id");
    requireCondition(loaded.empireId == record.empireId, "loaded record should preserve empire id");
    requireCondition(loaded.sourceSaveDate == record.sourceSaveDate, "loaded record should preserve source save date");
    requireCondition(loaded.sourceSavePath == record.sourceSavePath, "loaded record should preserve source save path");
    requireCondition(loaded.validatedUpdateSummary == record.validatedUpdateSummary, "loaded record should preserve validated update summary");
    requireCondition(loaded.confidence == record.confidence, "loaded record should preserve confidence");
    requireCondition(loaded.zeroHistoryBootstrap == record.zeroHistoryBootstrap, "loaded record should preserve zero-history flag");
    requireCondition(loaded.sourceSchemaVersion == 1, "loaded current record should preserve source schema version");
    requireCondition(loaded.schemaCompatibilityState == "current", "loaded current record should preserve current compatibility state");
    requireCondition(loaded.schemaCompatibilityNote.empty(), "loaded current record should preserve empty compatibility note");
    requireCondition(loaded.personality.boldness == record.personality.boldness, "loaded record should preserve boldness");
    requireCondition(loaded.personality.paranoia == record.personality.paranoia, "loaded record should preserve paranoia");
    requireCondition(loaded.personality.honor == record.personality.honor, "loaded record should preserve honor");
    requireCondition(loaded.personality.opportunism == record.personality.opportunism, "loaded record should preserve opportunism");

    const auto legacyPath = root / "campaign_legacy" / "empire_legacy.json";
    const std::string legacyJson =
        "{\n"
        "  \"schema_version\": 0,\n"
        "  \"ok\": true,\n"
        "  \"reason\": \"accepted validated personality profile\",\n"
        "  \"profile_key\": \"campaign_legacy::empire_legacy\",\n"
        "  \"campaign_id\": \"campaign_legacy\",\n"
        "  \"empire_id\": \"empire_legacy\",\n"
        "  \"source_save_date\": \"2026-06-10T05:02:00+02:00\",\n"
        "  \"source_save_path\": \"dist/archive/session_001/save_2201.sav\",\n"
        "  \"validated_update_summary\": \"legacy defensive posture with cautious diplomacy\",\n"
        "  \"confidence\": 0.79,\n"
        "  \"zero_history_bootstrap\": true,\n"
        "  \"personality\": {\n"
        "    \"boldness\": 0.31,\n"
        "    \"paranoia\": 0.72,\n"
        "    \"honor\": 0.65,\n"
        "    \"opportunism\": 0.28\n"
        "  }\n"
        "}\n";
    strategic_nexus::common::writeTextFileAtomically(legacyPath, legacyJson);
    const auto legacyLoaded = store.load(root, "campaign_legacy", "empire_legacy");
    requireCondition(legacyLoaded.ok, "store should load a legacy schema version 0 personality record");
    requireCondition(legacyLoaded.sourceSchemaVersion == 0, "legacy record should preserve source schema version 0");
    requireCondition(
        legacyLoaded.schemaCompatibilityState == "partial_compatibility",
        "legacy record should expose partial compatibility state");
    requireCondition(
        legacyLoaded.schemaCompatibilityNote == "migrated legacy personality profile schema_version 0 to current schema_version 1",
        "legacy record should expose a migration note");

    requireCondition(store.save(root, legacyLoaded), "store should persist a partially compatible personality record");
    const auto legacyRoundTrip = store.load(root, "campaign_legacy", "empire_legacy");
    requireCondition(legacyRoundTrip.ok, "store should reload the partially compatible personality record");
    requireCondition(legacyRoundTrip.sourceSchemaVersion == 0, "reloaded legacy record should keep source schema version 0");
    requireCondition(
        legacyRoundTrip.schemaCompatibilityState == "partial_compatibility",
        "reloaded legacy record should keep partial compatibility state");
    requireCondition(
        legacyRoundTrip.schemaCompatibilityNote == "migrated legacy personality profile schema_version 0 to current schema_version 1",
        "reloaded legacy record should keep migration note");

    const auto legacyRoundTripJson = strategic_nexus::common::readTextFile(legacyPath);
    requireCondition(legacyRoundTripJson.find("\"source_schema_version\": 0") != std::string::npos, "legacy JSON should include source schema version 0");
    requireCondition(
        legacyRoundTripJson.find("\"schema_compatibility_state\": \"partial_compatibility\"") != std::string::npos,
        "legacy JSON should include partial compatibility state");
    requireCondition(
        legacyRoundTripJson.find("\"schema_compatibility_note\": \"migrated legacy personality profile schema_version 0 to current schema_version 1\"") != std::string::npos,
        "legacy JSON should include migration note");

    const auto invalidRecord = store.save(root, [] {
        auto invalid = makeRecord();
        invalid.empireId.clear();
        return invalid;
    }());
    requireCondition(!invalidRecord, "store should reject missing empire id");
    requireCondition(!std::filesystem::exists(store.profilePathFor(root, "campaign_001", "")), "store should not write an invalid profile file");

    const auto malformedPath = root / "campaign_001" / "empire_malformed.json";
    strategic_nexus::common::writeTextFileAtomically(malformedPath, "{\n  \"schema_version\": 1,\n  \"ok\": true\n}\n");
    const auto malformed = store.load(root, "campaign_001", "empire_malformed");
    requireCondition(!malformed.ok, "store should reject malformed record files");
    requireCondition(malformed.reason == "malformed personality profile record", "store should explain malformed record files");

    const auto missing = store.load(root, "campaign_001", "missing_empire");
    requireCondition(!missing.ok, "store should reject missing record files");
    requireCondition(missing.reason == "missing personality profile record", "store should explain missing records");

    std::cout << "personality profile store tests passed.\n";
    return 0;
}
