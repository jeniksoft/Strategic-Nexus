// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "common/FileUtil.h"
#include "generated_overlay/DslParser.h"
#include "generated_overlay/DslValidator.h"
#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/OverlayCompiler.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

using strategic_nexus::common::writeTextFileAtomically;
using strategic_nexus::generated_overlay::DslParser;
using strategic_nexus::generated_overlay::DslValidator;
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

void writeBinaryFile(const std::filesystem::path& path, const std::string& content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string readBinaryFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void rewriteFileWithCrLfNewlines(const std::filesystem::path& path)
{
    const std::string original = readBinaryFile(path);
    std::string rewritten;
    rewritten.reserve(original.size() * 2);
    for (std::size_t i = 0; i < original.size(); ++i) {
        const char ch = original[i];
        if (ch == '\n') {
            rewritten.push_back('\r');
            rewritten.push_back('\n');
        } else {
            rewritten.push_back(ch);
        }
    }
    writeBinaryFile(path, rewritten);
}

} // namespace

int main()
{
    const auto overlayRoot = std::filesystem::path("dist/generated_overlay_verifier_overlay");
    std::filesystem::remove_all(overlayRoot);
    std::filesystem::create_directories(overlayRoot);

    const DslParser parser;
    const auto parseResult = parser.parse(validDsl());
    requireCondition(parseResult.ok, "valid DSL should parse");

    const DslValidator validator;
    const auto validation = validator.validate(parseResult.program);
    requireCondition(validation.ok, "valid DSL should validate");

    const OverlayCompiler compiler;
    const auto files = compiler.compile(parseResult.program);

    requireCondition(
        writeTextFileAtomically(
            overlayRoot / "events" / "strategic_nexus_generated_events.txt",
            files.eventsText),
        "should write generated events");
    requireCondition(
        writeTextFileAtomically(
            overlayRoot / "common" / "scripted_effects" / "strategic_nexus_generated_effects.txt",
            files.scriptedEffectsText),
        "should write generated scripted effects");
    requireCondition(
        writeTextFileAtomically(
            overlayRoot / "common" / "scripted_triggers" / "strategic_nexus_generated_triggers.txt",
            files.scriptedTriggersText),
        "should write generated scripted triggers");
    requireCondition(
        writeTextFileAtomically(
            overlayRoot / "strategic_nexus_generated_manifest.json",
            files.manifestText),
        "should write generated manifest");

    const ManifestVerifier verifier;
    const auto accepted = verifier.verify(overlayRoot);
    requireCondition(accepted.ok, "verifier should accept intact generated overlay");

    rewriteFileWithCrLfNewlines(overlayRoot / "events" / "strategic_nexus_generated_events.txt");
    const auto rejected = verifier.verify(overlayRoot);
    requireCondition(!rejected.ok, "verifier should reject newline-tampered overlay");
    requireCondition(
        rejected.reason == "generated overlay files do not match manifest",
        "verifier should explain mismatch");

    std::cout << "generated overlay verifier tests passed.\n";
    return 0;
}


