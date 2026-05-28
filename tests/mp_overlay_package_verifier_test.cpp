// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "common/FileUtil.h"
#include "generated_overlay/DslParser.h"
#include "generated_overlay/DslValidator.h"
#include "generated_overlay/MpOverlayPackage.h"
#include "generated_overlay/OverlayCompiler.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

using strategic_nexus::common::readTextFile;
using strategic_nexus::common::writeTextFileAtomically;
using strategic_nexus::generated_overlay::DslParser;
using strategic_nexus::generated_overlay::DslValidator;
using strategic_nexus::generated_overlay::MpOverlayPackageExporter;
using strategic_nexus::generated_overlay::MpOverlayPackageVerifier;
using strategic_nexus::generated_overlay::OverlayCompiler;

namespace {

std::string validDsl()
{
    return R"(campaign "campaign_mp_001" {
  empire "empire_alpha" {
    rule "border_defense" {
      ministry = military_ministry
      when personality.paranoia >= medium
      when memory.repeated_border_war
      prefer military_posture defensive intensity 0.7
      duration = next_session
      confidence = 0.72
      rationale = "Border threat memory."
    }
  }
}
)";
}

void writeOverlay(const std::filesystem::path& overlayRoot)
{
    const DslParser parser;
    const auto parseResult = parser.parse(validDsl());
    if (!parseResult.ok) {
        throw std::runtime_error("parse failed");
    }

    const DslValidator validator;
    const auto validation = validator.validate(parseResult.program);
    if (!validation.ok) {
        throw std::runtime_error("validation failed");
    }

    const OverlayCompiler compiler;
    const auto files = compiler.compile(parseResult.program);

    writeTextFileAtomically(overlayRoot / "events/strategic_nexus_generated_events.txt", files.eventsText);
    writeTextFileAtomically(
        overlayRoot / "common/scripted_effects/strategic_nexus_generated_effects.txt",
        files.scriptedEffectsText);
    writeTextFileAtomically(
        overlayRoot / "common/scripted_triggers/strategic_nexus_generated_triggers.txt",
        files.scriptedTriggersText);
    writeTextFileAtomically(overlayRoot / "strategic_nexus_generated_manifest.json", files.manifestText);
}

} // namespace

int main()
{
    const auto overlayRoot = std::filesystem::path("dist/mp_overlay_package_test_overlay");
    const auto packageRoot = std::filesystem::path("dist/mp_overlay_package_test_package");
    std::filesystem::remove_all(overlayRoot);
    std::filesystem::remove_all(packageRoot);

    writeOverlay(overlayRoot);

    const MpOverlayPackageExporter exporter;
    const auto exportResult = exporter.exportPackage(
        overlayRoot,
        "campaign_mp_001",
        "overlay_v1",
        "stellaris_4.x",
        "strategic_nexus_v0",
        packageRoot,
        false);
    if (!exportResult.ok) {
        std::cerr << "export failed: " << exportResult.reason << "\n";
        return 1;
    }
    if (exportResult.reason != "accepted_degraded_previous_host_unavailable") {
        std::cerr << "missing degraded previous-host status\n";
        return 1;
    }
    if (exportResult.files.size() != 4) {
        std::cerr << "unexpected exported file count\n";
        return 1;
    }

    const auto packageManifest = readTextFile(packageRoot / "strategic_nexus_mp_overlay_package_manifest.json");
    if (packageManifest.find("\"handoff_status\": \"degraded_previous_host_unavailable\"") == std::string::npos) {
        std::cerr << "package manifest missing degraded handoff status\n";
        return 1;
    }
    if (packageManifest.find("\"checksum_relevance\": \"gameplay_affecting\"") == std::string::npos) {
        std::cerr << "package manifest missing gameplay-affecting file classification\n";
        return 1;
    }
    if (packageManifest.find("\"checksum_relevance\": \"local_only_diagnostic\"") == std::string::npos) {
        std::cerr << "package manifest missing local-only diagnostic classification\n";
        return 1;
    }

    const MpOverlayPackageVerifier verifier;
    const auto verifyResult = verifier.verify(packageRoot);
    if (!verifyResult.ok || verifyResult.reason != "accepted") {
        std::cerr << "verify failed: " << verifyResult.reason << "\n";
        return 1;
    }
    if (verifyResult.campaignId != "campaign_mp_001" || verifyResult.overlayVersion != "overlay_v1") {
        std::cerr << "verify did not parse package metadata\n";
        return 1;
    }
    if (verifyResult.statusText.find("campaign_id: campaign_mp_001") == std::string::npos) {
        std::cerr << "verify did not produce copyable status text\n";
        return 1;
    }

    writeTextFileAtomically(packageRoot / "local_debug.txt", "not part of package\n");
    const auto unexpectedResult = verifier.verify(packageRoot);
    if (unexpectedResult.ok || unexpectedResult.reason != "MP overlay package contains unexpected files") {
        std::cerr << "unexpected-file verification did not fail closed\n";
        return 1;
    }

    std::filesystem::remove(packageRoot / "local_debug.txt");
    writeTextFileAtomically(
        packageRoot / "events/strategic_nexus_generated_events.txt",
        readTextFile(packageRoot / "events/strategic_nexus_generated_events.txt") + "# tamper\n");
    const auto tamperedResult = verifier.verify(packageRoot);
    if (tamperedResult.ok || tamperedResult.reason != "MP overlay package files do not match manifest") {
        std::cerr << "tamper verification did not fail closed\n";
        return 1;
    }

    std::cout << "mp_overlay_package_test_ok=true\n";
    return 0;
}
