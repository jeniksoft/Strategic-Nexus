// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "generated_overlay/DslParser.h"
#include "generated_overlay/DslValidator.h"
#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/OverlayCompiler.h"

#include "common/FileUtil.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

using strategic_nexus::generated_overlay::DslParser;
using strategic_nexus::generated_overlay::DslValidator;
using strategic_nexus::generated_overlay::fnv1a64Hex;
using strategic_nexus::generated_overlay::ManifestVerifier;
using strategic_nexus::generated_overlay::OverlayCompiler;

void requireCondition(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

std::string validDsl()
{
    return R"(campaign "campaign_001" {
  empire "empire_001" {
    rule "border_war_defense" {
      ministry = military_ministry
      event_family = monthly_strategy_tick
      when campaign_marker = campaign_001
      when known.save_fingerprint = h_abcdef123456
      when known.save_date = d_2200_03_01
      when known.rule_scope = entry_scope_alpha
      prefer military_posture defensive intensity 0.7
      prefer doctrine_inertia high intensity 0.4
      duration = next_session
      confidence = 0.72
      rationale = "Bounded entry-point gating should survive compile."
    }
  }
}
)";
}

std::filesystem::path freshTestDirectory()
{
    const auto dir = std::filesystem::path("dist") / "generated_overlay_contract_test_tmp";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    requireCondition(!ec, "should create temp test directory");
    return dir;
}

void writeFile(const std::filesystem::path& path, const std::string& text)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    requireCondition(!ec, "should create parent directories for test file");
    requireCondition(strategic_nexus::common::writeTextFileAtomically(path, text), "should write test file");
}

} // namespace

int main()
{
    const DslParser parser;
    const DslValidator validator;
    const OverlayCompiler compiler;
    const ManifestVerifier manifestVerifier;

    {
        const auto parseResult = parser.parse(validDsl());
        requireCondition(parseResult.ok, "valid DSL should parse");
        requireCondition(parseResult.program.rules.size() == 1, "valid DSL should create one rule");

        const auto validation = validator.validate(parseResult.program);
        requireCondition(validation.ok, "valid DSL should pass validation");

        const auto files = compiler.compile(parseResult.program);
        requireCondition(
            files.scriptedTriggersText.find("strategic_nexus_generated_trigger_campaign_001_empire_001_border_war_defense") != std::string::npos,
            "compiler should emit marker-guarded trigger");
        requireCondition(
            files.scriptedTriggersText.find("    is_ai = yes") != std::string::npos,
            "compiler should skip human-controlled empires at runtime");
        requireCondition(
            files.scriptedTriggersText.find("has_global_flag = strategic_nexus_known_save_fingerprint_h_abcdef123456") != std::string::npos,
            "compiler should preserve known.save_fingerprint guard");
        requireCondition(
            files.scriptedTriggersText.find("has_global_flag = strategic_nexus_known_save_date_d_2200_03_01") != std::string::npos,
            "compiler should preserve known.save_date guard");
        requireCondition(
            files.scriptedTriggersText.find("has_global_flag = strategic_nexus_known_rule_scope_entry_scope_alpha") != std::string::npos,
            "compiler should preserve known.rule_scope guard");
        requireCondition(
            files.scriptedEffectsText.find("set_country_flag = strategic_nexus_pref_military_posture_defensive") != std::string::npos,
            "compiler should emit allowlisted preference flag");
        requireCondition(
            files.scriptedEffectsText.find("strategic_nexus_generated_monthly_strategy_tick_dispatch = {") != std::string::npos,
            "compiler should emit the monthly reactive dispatcher effect");
        requireCondition(
            files.scriptedEffectsText.find("    strategic_nexus_generated_effect_campaign_001_empire_001_border_war_defense = yes") != std::string::npos,
            "monthly reactive dispatcher should call the compiled rule effect");
        requireCondition(
            files.scriptedEffectsText.find("remove_country_flag = strategic_nexus_pref_military_posture_aggressive") != std::string::npos,
            "compiler should clear conflicting military posture flag before applying the selected one");
        requireCondition(
            files.eventsText.find("strategic_nexus_generated_monthly_strategy_tick_dispatch = yes") != std::string::npos,
            "compiler should wire generated event surface to the monthly dispatcher effect");
        requireCondition(
            files.manifestText.find("\"snapshot_kind\": \"complete_replacement\"") != std::string::npos,
            "compiler should emit complete replacement manifest");
        requireCondition(
            files.manifestText.find("\"path\": \"events/strategic_nexus_generated_events.txt\"") != std::string::npos,
            "compiler manifest should include generated events file");
        requireCondition(
            files.manifestText.find("\"empire_ids\": [\"empire_001\"]") != std::string::npos,
            "compiler manifest should include empire ids");
        requireCondition(
            files.manifestText.find("\"reactive_policy_pack_capability\": \"event_family_dispatch\"") != std::string::npos,
            "compiler manifest should expose reactive event-family capability when requested");
        requireCondition(
            files.manifestText.find("\"event_families\": [\"monthly_strategy_tick\"]") != std::string::npos,
            "compiler manifest should list covered event families");
        requireCondition(
            files.manifestText.find("\"source_qualities\": [\"history_backed\"]") != std::string::npos,
            "compiler manifest should expose default history-backed source quality");
        requireCondition(
            files.manifestText.find("\"source_quality\": \"history_backed\"") != std::string::npos,
            "compiler manifest should preserve per-campaign source quality");
        requireCondition(
            files.manifestText.find("\"checksum_relevance\": \"gameplay_affecting\"") != std::string::npos,
            "compiler manifest should classify generated files as gameplay-affecting");
        requireCondition(
            files.manifestText.find(fnv1a64Hex(files.eventsText)) != std::string::npos,
            "compiler manifest should include generated events hash");
    }

    {
        const auto parseResult = parser.parse(std::string("\xEF\xBB\xBF") + validDsl());
        requireCondition(parseResult.ok, "valid DSL with UTF-8 BOM should parse");
        requireCondition(parseResult.program.rules.size() == 1, "BOM should not create an extra token");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_bootstrap" { empire "empire_001" { rule "bootstrap_default" { ministry = military_ministry source_quality = zero_history_bootstrap when campaign_marker = campaign_bootstrap prefer military_posture defensive intensity 0.7 duration = next_session confidence = 0.8 rationale = "bootstrap" } } })");
        requireCondition(parseResult.ok, "zero-history bootstrap DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(validation.ok, "zero-history bootstrap DSL should validate");
        const auto files = compiler.compile(parseResult.program);
        requireCondition(
            files.manifestText.find("\"source_qualities\": [\"zero_history_bootstrap\"]") != std::string::npos,
            "manifest should advertise zero-history bootstrap source quality");
        requireCondition(
            files.manifestText.find("\"bootstrap_rotation_seed_id\": \"bootstrap_seed_") != std::string::npos,
            "manifest should synthesize deterministic bootstrap seed when missing");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_bootstrap" { empire "empire_001" { rule "bootstrap_epoch" { ministry = military_ministry source_quality = generic_unknown_campaign_fallback bootstrap_rotation_seed_id = pack_epoch_12 bootstrap_rotation_epoch = 12 when campaign_marker = campaign_bootstrap prefer military_posture defensive intensity 0.7 duration = next_session confidence = 0.8 rationale = "fallback" } } })");
        requireCondition(parseResult.ok, "generic fallback bootstrap DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(validation.ok, "generic fallback bootstrap DSL should validate");
        const auto files = compiler.compile(parseResult.program);
        requireCondition(
            files.manifestText.find("\"source_quality\": \"generic_unknown_campaign_fallback\"") != std::string::npos,
            "manifest should preserve generic fallback source quality");
        requireCondition(
            files.manifestText.find("\"bootstrap_rotation_seed_id\": \"pack_epoch_12\"") != std::string::npos,
            "manifest should preserve explicit bootstrap seed id");
        requireCondition(
            files.manifestText.find("\"bootstrap_rotation_epoch\": 12") != std::string::npos,
            "manifest should preserve explicit bootstrap rotation epoch");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "unsupported_runtime_condition" { ministry = military_ministry when personality.paranoia >= medium prefer military_posture defensive intensity 0.7 duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "unsupported runtime condition DSL should still parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(validation.ok, "unsupported runtime condition should remain syntactically valid");
        const auto runtimeErrors = strategic_nexus::generated_overlay::findUnsupportedRuntimeConditionErrors(parseResult.program);
        requireCondition(runtimeErrors.size() == 1, "unsupported runtime condition should be surfaced before compile");
        requireCondition(
            runtimeErrors.front().find("personality.paranoia") != std::string::npos,
            "runtime unsupported error should name the condition");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "bad" { ministry = military_ministry prefer fleet_target enemy_capital intensity 1.0 duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "syntactically valid unsafe DSL should still parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "fleet micro preference should fail validation");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "bad" { ministry = duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(!parseResult.ok, "missing ministry value should fail parse");
        requireCondition(!parseResult.error.empty(), "parse failures should include an error message");
        requireCondition(parseResult.error.find("ministry") != std::string::npos, "ministry parse error should mention ministry");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "bad;effect" { ministry = research_ministry prefer research_bias economy intensity 0.5 duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "unsafe identifier DSL should parse before validation");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "unsafe identifier should fail validation");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "bad" { ministry = military_ministry when personality. >= medium prefer military_posture defensive intensity 0.7 duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "syntactically valid condition with empty tail should still parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "condition with empty dotted tail should fail validation");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "bad" { ministry = military_ministry source_quality = history_backed bootstrap_rotation_seed_id = pack_epoch_12 prefer military_posture defensive intensity 0.7 duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "history-backed bootstrap DSL should parse before validation");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "history-backed rules should reject bootstrap rotation metadata");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_reactive" { empire "empire_001" { rule "monthly_watch" { ministry = military_ministry event_family = monthly_strategy_tick prefer military_posture defensive intensity 0.7 duration = next_session confidence = 0.72 rationale = "monthly branch" } rule "war_response" { ministry = military_ministry event_family = war_started prefer military_posture aggressive intensity 0.8 duration = next_session confidence = 0.74 rationale = "war branch" } } })");
        requireCondition(parseResult.ok, "reactive policy family DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(validation.ok, "allowlisted event families should validate");
        const auto files = compiler.compile(parseResult.program);
        requireCondition(
            files.scriptedEffectsText.find("strategic_nexus_generated_monthly_strategy_tick_dispatch = {") != std::string::npos,
            "compiler should keep the monthly dispatcher branch");
        requireCondition(
            files.scriptedEffectsText.find("strategic_nexus_generated_war_started_dispatch = {") != std::string::npos,
            "compiler should emit a war_started dispatcher branch");
        requireCondition(
            files.scriptedEffectsText.find("strategic_nexus_generated_effect_campaign_reactive_empire_001_war_response = yes") != std::string::npos,
            "war_started dispatcher should call the compiled war branch effect");
        requireCondition(
            files.manifestText.find("\"event_families\": [\"monthly_strategy_tick\", \"war_started\"]") != std::string::npos,
            "manifest should list both allowlisted event families");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_reactive_country" { empire "empire_001" { rule "attack_response" { ministry = military_ministry event_family = country_attacked prefer military_posture defensive intensity 0.75 duration = next_session confidence = 0.73 rationale = "country attacked branch" } } })");
        requireCondition(parseResult.ok, "country_attacked DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(validation.ok, "country_attacked should validate as an allowlisted event family");
        const auto files = compiler.compile(parseResult.program);
        requireCondition(
            files.scriptedEffectsText.find("strategic_nexus_generated_country_attacked_dispatch = {") != std::string::npos,
            "compiler should emit a country_attacked dispatcher branch");
        requireCondition(
            files.scriptedEffectsText.find("strategic_nexus_generated_effect_campaign_reactive_country_empire_001_attack_response = yes") != std::string::npos,
            "country_attacked dispatcher should call the compiled attack branch effect");
        requireCondition(
            files.manifestText.find("\"event_families\": [\"country_attacked\"]") != std::string::npos,
            "manifest should list country_attacked as the covered family");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_future_enum" { empire "empire_001" { rule "future_family" { ministry = military_ministry event_family = monthly_strategy_tick_plus prefer military_posture defensive intensity 0.7 duration = next_session confidence = 0.72 rationale = "future event family" } } })");
        requireCondition(parseResult.ok, "future event-family DSL should parse before validation");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "future event-family value should fail validation");
        requireCondition(
            !validation.errors.empty() && validation.errors.front().find("unsupported event family") != std::string::npos,
            "future event-family rejection should be explicit");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_future_enum" { empire "empire_001" { rule "future_source_quality" { ministry = military_ministry source_quality = history_backed_plus prefer military_posture defensive intensity 0.7 duration = next_session confidence = 0.72 rationale = "future source quality" } } })");
        requireCondition(parseResult.ok, "future source-quality DSL should parse before validation");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "future source-quality value should fail validation");
        requireCondition(
            !validation.errors.empty() && validation.errors.front().find("unsupported source quality") != std::string::npos,
            "future source-quality rejection should be explicit");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_multi_empire" {
  empire "empire_001" {
    rule "watch_one" {
      ministry = military_ministry
      prefer military_posture defensive intensity 0.6
      duration = next_session
      confidence = 0.8
      rationale = "first empire marker"
    }
  }
  empire "empire_002" {
    rule "watch_two" {
      ministry = research_ministry
      prefer research_bias economy intensity 0.7
      duration = next_session
      confidence = 0.81
      rationale = "second empire marker"
    }
  }
})");
        requireCondition(parseResult.ok, "multi-empire DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(validation.ok, "multi-empire DSL should validate");
        const auto files = compiler.compile(parseResult.program);
        requireCondition(
            files.manifestText.find("\"empire_ids\": [\"empire_001\", \"empire_002\"]") != std::string::npos,
            "compiler manifest should include all detected empire ids");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "bad_event_family" { ministry = military_ministry event_family = on_monthly_pulse_country prefer military_posture defensive intensity 0.7 duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "raw on_action-like event family should parse before validation");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "raw on_action-like event family should fail validation");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "bad" { ministry = research_ministry prefer research_bias economy intensity 1.5 duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "out-of-range intensity DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "out-of-range intensity should fail validation");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "nan_confidence" { ministry = military_ministry prefer military_posture defensive intensity 0.5 duration = next_session confidence = nan rationale = "bad" } } })");
        requireCondition(parseResult.ok, "nan confidence DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "nan confidence should fail validation");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "nan_intensity" { ministry = military_ministry prefer military_posture defensive intensity nan duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "nan intensity DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "nan intensity should fail validation");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "duplicate_preference_domain" { ministry = military_ministry prefer military_posture defensive intensity 0.4 prefer military_posture aggressive intensity 0.6 duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "duplicate domain preference DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "duplicate preference domains should fail validation");
    }

    {
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "empty_rationale" { ministry = military_ministry prefer military_posture defensive intensity 0.5 duration = next_session confidence = 0.9 rationale = "" } } })");
        requireCondition(parseResult.ok, "empty rationale DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "empty rationale should fail validation");
    }

    {
        const auto parseResult = parser.parse("campaign \"campaign_001\" { empire \"empire_001\" { rule \"control_char_rationale\" { ministry = military_ministry prefer military_posture defensive intensity 0.5 duration = next_session confidence = 0.9 rationale = \"line1\nline2\" } } }");
        requireCondition(!parseResult.ok, "quoted DSL strings with control chars should fail parse");
    }

    {
        const auto overlayDir = freshTestDirectory();
        const auto eventsPath = overlayDir / "events" / "strategic_nexus_generated_events.txt";
        const std::string eventsText = "namespace = strategic_nexus_generated\n";
        writeFile(eventsPath, eventsText);

        const std::string manifestText = std::string(R"({
  "schema_version": 1,
  "generated_at_utc": "2026-01-01T00:00:00Z",
  "files": [
    {
      "path": "events/strategic_nexus_generated_events.txt",
      "hash": ")")
            + fnv1a64Hex(eventsText) + R"(",
      "hash_algorithm": "fnv1a64",
      "byte_count": )"
            + std::to_string(eventsText.size()) + R"(,
      "checksum_relevance": "gameplay_affecting"
    }
  ]
}
)";

        writeFile(overlayDir / "strategic_nexus_generated_manifest.json", manifestText);

        const auto verification = manifestVerifier.verify(overlayDir);
        requireCondition(verification.ok, "manifest verifier should accept matching files");
        requireCondition(verification.reason == "accepted", "manifest verifier should return accepted reason");
        requireCondition(verification.files.size() == 1, "manifest verifier should return one file entry");
        requireCondition(verification.files[0].exists, "manifest verifier should report file exists");
        requireCondition(
            verification.files[0].checksumRelevance == "gameplay_affecting",
            "manifest verifier should preserve checksum classification");
        requireCondition(verification.files[0].hashMatches, "manifest verifier should report hash match");
        requireCondition(verification.files[0].byteCountMatches, "manifest verifier should report byte count match");

        writeFile(eventsPath, eventsText + "# drift\n");
        const auto mismatch = manifestVerifier.verify(overlayDir);
        requireCondition(!mismatch.ok, "manifest verifier should reject mismatched files");
        requireCondition(
            mismatch.reason == "generated overlay files do not match manifest", "manifest verifier should return mismatch reason");
        requireCondition(mismatch.files.size() == 1, "manifest verifier should still return one file entry on mismatch");
        requireCondition(mismatch.files[0].exists, "manifest verifier should report file exists on mismatch");
        requireCondition(!mismatch.files[0].hashMatches || !mismatch.files[0].byteCountMatches,
            "manifest verifier should report hash/byte count mismatch");
    }

    {
        const auto overlayDir = freshTestDirectory();
        const auto eventsPath = overlayDir / "events" / "strategic_nexus_generated_events.txt";
        const std::string eventsText = "namespace = strategic_nexus_generated\n";
        writeFile(eventsPath, eventsText);

        const std::string unsafeManifest = std::string(R"({
  "schema_version": 1,
  "files": [
    { "path": "../evil.txt", "hash": ")")
            + fnv1a64Hex(eventsText) + R"(", "byte_count": )"
            + std::to_string(eventsText.size()) + R"( }
  ]
}
)";

        writeFile(overlayDir / "strategic_nexus_generated_manifest.json", unsafeManifest);
        const auto rejected = manifestVerifier.verify(overlayDir);
        requireCondition(!rejected.ok, "manifest verifier should reject unsafe manifest paths");
        requireCondition(rejected.reason == "manifest contains invalid file entry", "manifest verifier should reject unsafe entry");
    }

    {
        const auto overlayDir = freshTestDirectory();
        const auto eventsPath = overlayDir / "events" / "strategic_nexus_generated_events.txt";
        const std::string eventsText = "namespace = strategic_nexus_generated\n";
        writeFile(eventsPath, eventsText);

        const std::string wrongClassificationManifest = std::string(R"({
  "schema_version": 1,
  "files": [
    {
      "path": "events/strategic_nexus_generated_events.txt",
      "hash": ")")
            + fnv1a64Hex(eventsText) + R"(",
      "hash_algorithm": "fnv1a64",
      "byte_count": )"
            + std::to_string(eventsText.size()) + R"(,
      "checksum_relevance": "diagnostic"
    }
  ]
}
)";

        writeFile(overlayDir / "strategic_nexus_generated_manifest.json", wrongClassificationManifest);
        const auto rejected = manifestVerifier.verify(overlayDir);
        requireCondition(!rejected.ok, "manifest verifier should reject wrong checksum classification");
        requireCondition(
            rejected.reason == "manifest contains invalid checksum relevance classification",
            "manifest verifier should return checksum classification reason");
    }

    std::cout << "generated overlay contract tests passed.\n";
    return 0;
}

