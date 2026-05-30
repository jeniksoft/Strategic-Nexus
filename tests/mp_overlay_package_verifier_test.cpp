// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "common/FileUtil.h"
#include "generated_overlay/DslParser.h"
#include "generated_overlay/DslValidator.h"
#include "generated_overlay/ManifestVerifier.h"
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
    const auto originalManifest = packageManifest;
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
    if (verifyResult.packageManifestHash.empty()) {
        std::cerr << "verify did not expose package manifest hash\n";
        return 1;
    }
    if (verifyResult.statusText.find("package_manifest_hash: " + verifyResult.packageManifestHash) == std::string::npos) {
        std::cerr << "copyable status text did not include package manifest hash\n";
        return 1;
    }
    if (verifyResult.readiness != "ready_for_mp" ||
        verifyResult.statusText.find("readiness: ready_for_mp") == std::string::npos ||
        verifyResult.statusText.find("host_readiness: ready_for_mp") == std::string::npos ||
        verifyResult.statusText.find("client_readiness_gate: import_and_verify_before_join") == std::string::npos ||
        verifyResult.statusText.find("verify_command: Strategic Nexus.exe --verify-mp-overlay-package \"dist/mp_overlay_package_test_package\"") ==
            std::string::npos ||
        verifyResult.statusText.find("import_command: Strategic Nexus.exe --import-mp-overlay-package \"dist/mp_overlay_package_test_package\" <target_overlay_dir>") ==
            std::string::npos ||
        verifyResult.statusText.find("strict_verify_command: Strategic Nexus.exe --verify-mp-overlay-package \"dist/mp_overlay_package_test_package\" campaign_mp_001 overlay_v1 stellaris_4.x strategic_nexus_v0 ") ==
            std::string::npos ||
        verifyResult.statusText.find("strict_import_command: Strategic Nexus.exe --import-mp-overlay-package \"dist/mp_overlay_package_test_package\" <target_overlay_dir> campaign_mp_001 overlay_v1 stellaris_4.x strategic_nexus_v0 ") ==
            std::string::npos ||
        verifyResult.statusText.find("host_next_step: share this package and package_manifest_hash with every joining player") ==
            std::string::npos ||
        verifyResult.statusText.find("client_next_step: import package, verify package_manifest_hash, then join lobby") ==
            std::string::npos ||
        verifyResult.statusText.find("host_handoff_state: previous host unavailable; manual save recovery may be needed") ==
            std::string::npos ||
        verifyResult.statusText.find("mp_join_check: every player must use this same package_manifest_hash before joining") ==
            std::string::npos) {
        std::cerr << "verify did not expose MP readiness in copyable status text\n";
        return 1;
    }

    {
        const auto importedOverlayRoot = std::filesystem::path("dist/mp_overlay_package_imported_overlay");
        std::filesystem::remove_all(importedOverlayRoot);
        const strategic_nexus::generated_overlay::MpOverlayPackageImporter importer;
        const auto importResult = importer.importPackage(packageRoot, importedOverlayRoot);
        if (!importResult.ok || importResult.reason != "accepted") {
            std::cerr << "import failed: " << importResult.reason << "\n";
            return 1;
        }
        if (importResult.importedFiles.size() != verifyResult.files.size()) {
            std::cerr << "imported file count mismatch\n";
            return 1;
        }
        if (importResult.packageManifestHash != verifyResult.packageManifestHash ||
            importResult.readiness != "ready_for_mp" ||
            importResult.statusText.find("package_manifest_hash: " + importResult.packageManifestHash) == std::string::npos) {
            std::cerr << "import did not preserve package identity/readiness metadata\n";
            return 1;
        }
        if (importResult.campaignId != verifyResult.campaignId ||
            importResult.overlayVersion != verifyResult.overlayVersion ||
            importResult.gameVersion != verifyResult.gameVersion ||
            importResult.strategicNexusModVersion != verifyResult.strategicNexusModVersion ||
            importResult.handoffStatus != verifyResult.handoffStatus) {
            std::cerr << "import did not preserve package metadata fields\n";
            return 1;
        }

        const strategic_nexus::generated_overlay::ManifestVerifier importedOverlayVerifier;
        const auto importedOverlayVerification = importedOverlayVerifier.verify(importedOverlayRoot);
        if (!importedOverlayVerification.ok || importedOverlayVerification.reason != "accepted") {
            std::cerr << "imported overlay did not verify: " << importedOverlayVerification.reason << "\n";
            return 1;
        }
    }

    {
        auto emptyGameVersionManifest = originalManifest;
        const auto needle = std::string("\"game_version\": \"stellaris_4.x\"");
        const auto replacement = std::string("\"game_version\": \"\"");
        const auto pos = emptyGameVersionManifest.find(needle);
        if (pos == std::string::npos) {
            std::cerr << "test could not locate game_version field in manifest\n";
            return 1;
        }
        emptyGameVersionManifest.replace(pos, needle.size(), replacement);
        writeTextFileAtomically(
            packageRoot / "strategic_nexus_mp_overlay_package_manifest.json",
            emptyGameVersionManifest);
        const auto emptyFieldResult = verifier.verify(packageRoot);
        if (emptyFieldResult.ok || emptyFieldResult.reason != "malformed MP overlay package manifest") {
            std::cerr << "empty-field manifest did not fail closed\n";
            return 1;
        }
        writeTextFileAtomically(
            packageRoot / "strategic_nexus_mp_overlay_package_manifest.json",
            originalManifest);
    }

    {
        auto invalidOverlayVersionManifest = originalManifest;
        const auto needle = std::string("\"overlay_version\": \"overlay_v1\"");
        const auto replacement = std::string("\"overlay_version\": \"overlay v1\"");
        const auto pos = invalidOverlayVersionManifest.find(needle);
        if (pos == std::string::npos) {
            std::cerr << "test could not locate overlay_version field in manifest\n";
            return 1;
        }
        invalidOverlayVersionManifest.replace(pos, needle.size(), replacement);
        writeTextFileAtomically(
            packageRoot / "strategic_nexus_mp_overlay_package_manifest.json",
            invalidOverlayVersionManifest);
        const auto invalidFieldResult = verifier.verify(packageRoot);
        if (invalidFieldResult.ok || invalidFieldResult.reason != "malformed MP overlay package manifest") {
            std::cerr << "invalid overlay_version did not fail closed\n";
            return 1;
        }
        writeTextFileAtomically(
            packageRoot / "strategic_nexus_mp_overlay_package_manifest.json",
            originalManifest);
    }

    writeTextFileAtomically(packageRoot / "local_debug.txt", "not part of package\n");
    const auto unexpectedResult = verifier.verify(packageRoot);
    if (unexpectedResult.ok || unexpectedResult.reason != "MP overlay package contains unexpected files") {
        std::cerr << "unexpected-file verification did not fail closed\n";
        return 1;
    }
    if (unexpectedResult.readiness != "not_ready" || !unexpectedResult.statusText.empty()) {
        std::cerr << "unexpected-file verification exposed shareable ready text\n";
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
    if (tamperedResult.readiness != "not_ready" || !tamperedResult.statusText.empty()) {
        std::cerr << "tamper verification exposed shareable ready text\n";
        return 1;
    }

    std::cout << "mp_overlay_package_test_ok=true\n";
    return 0;
}
