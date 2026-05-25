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
      when personality.paranoia >= medium
      when memory.repeated_border_war
      prefer military_posture defensive intensity 0.7
      prefer doctrine_inertia high intensity 0.4
      duration = next_session
      confidence = 0.72
      rationale = "Repeated border wars reinforced defensive paranoia."
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
            files.scriptedEffectsText.find("set_country_flag = strategic_nexus_pref_military_posture_defensive") != std::string::npos,
            "compiler should emit allowlisted preference flag");
        requireCondition(
            files.eventsText.find("strategic_nexus_generated_effect_campaign_001_empire_001_border_war_defense = yes") != std::string::npos,
            "compiler should emit dispatcher effect call");
        requireCondition(
            files.manifestText.find("\"snapshot_kind\": \"complete_replacement\"") != std::string::npos,
            "compiler should emit complete replacement manifest");
        requireCondition(
            files.manifestText.find("\"path\": \"events/strategic_nexus_generated_events.txt\"") != std::string::npos,
            "compiler manifest should include generated events file");
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
        const auto parseResult = parser.parse(R"(campaign "campaign_001" { empire "empire_001" { rule "bad" { ministry = research_ministry prefer research_bias economy intensity 1.5 duration = next_session confidence = 0.9 rationale = "bad" } } })");
        requireCondition(parseResult.ok, "out-of-range intensity DSL should parse");
        const auto validation = validator.validate(parseResult.program);
        requireCondition(!validation.ok, "out-of-range intensity should fail validation");
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

    std::cout << "generated overlay contract tests passed.\n";
    return 0;
}

