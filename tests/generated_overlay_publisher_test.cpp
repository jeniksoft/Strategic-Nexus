// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "common/FileUtil.h"
#include "generated_overlay/DslParser.h"
#include "generated_overlay/DslValidator.h"
#include "generated_overlay/GeneratedOverlayPublisher.h"
#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/OverlayCompiler.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

using strategic_nexus::common::writeTextFileAtomically;
using strategic_nexus::generated_overlay::DslParser;
using strategic_nexus::generated_overlay::DslValidator;
using strategic_nexus::generated_overlay::GeneratedOverlayPublisher;
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
    return R"(campaign "campaign_publish" {
  empire "empire_publish" {
    rule "published_defense" {
      ministry = military_ministry
      when campaign_marker = campaign_publish
      when known.save_fingerprint = h_publishabcdef
      when known.save_date = d_2200_03_01
      when known.rule_scope = entry_scope_publish
      prefer military_posture defensive intensity 0.6
      duration = next_session
      confidence = 0.75
      rationale = "Bounded entry-point gating should survive publish."
    }
  }
}
)";
}

void writeOverlay(const std::filesystem::path& overlayRoot)
{
    const DslParser parser;
    const auto parseResult = parser.parse(validDsl());
    requireCondition(parseResult.ok, "valid DSL should parse for publish fixture");

    const DslValidator validator;
    const auto validation = validator.validate(parseResult.program);
    requireCondition(validation.ok, "valid DSL should validate for publish fixture");

    const OverlayCompiler compiler;
    const auto files = compiler.compile(parseResult.program);

    requireCondition(
        writeTextFileAtomically(overlayRoot / "events" / "strategic_nexus_generated_events.txt", files.eventsText),
        "should write staged events");
    requireCondition(
        writeTextFileAtomically(
            overlayRoot / "common" / "scripted_effects" / "strategic_nexus_generated_effects.txt",
            files.scriptedEffectsText),
        "should write staged scripted effects");
    requireCondition(
        writeTextFileAtomically(
            overlayRoot / "common" / "scripted_triggers" / "strategic_nexus_generated_triggers.txt",
            files.scriptedTriggersText),
        "should write staged scripted triggers");
    requireCondition(
        writeTextFileAtomically(overlayRoot / "strategic_nexus_generated_manifest.json", files.manifestText),
        "should write staged manifest");
}

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/generated_overlay_publisher_fixture");
    const auto stagedRoot = root / "staged";
    const auto activeRoot = root / "active";
    std::filesystem::remove_all(root);
    writeOverlay(stagedRoot);

    std::filesystem::create_directories(activeRoot / "old");
    {
        std::ofstream stale(activeRoot / "old" / "stale.txt", std::ios::binary | std::ios::trunc);
        stale << "stale";
    }

    const GeneratedOverlayPublisher publisher;
    const auto blocked = publisher.publish(stagedRoot, root / "blocked_active", true);
    requireCondition(!blocked.ok, "publisher should refuse to publish while Stellaris is running");
    requireCondition(
        blocked.reason == "Stellaris is running; generated overlay publish deferred",
        "publisher should explain Stellaris-running refusal");

    const auto same = publisher.publish(stagedRoot, stagedRoot, false);
    requireCondition(!same.ok, "publisher should reject same staged and active directories");
    requireCondition(
        same.reason == "staged and active overlay directories must be different",
        "publisher should explain same-directory rejection");

    const auto published = publisher.publish(stagedRoot, activeRoot, false);
    requireCondition(published.ok, "publisher should publish verified staged overlay");
    requireCondition(published.reason == "published", "publisher should report published reason");
    requireCondition(!published.sourceManifestHash.empty(), "publisher should expose source manifest hash");
    requireCondition(!published.publishedManifestHash.empty(), "publisher should expose published manifest hash");
    requireCondition(
        published.sourceManifestHash == published.publishedManifestHash,
        "published manifest hash should match source manifest hash");
    requireCondition(published.files.size() == 3, "publisher should report three published gameplay files");
    requireCondition(!std::filesystem::exists(activeRoot / "old" / "stale.txt"), "publisher should fully replace active overlay");

    const ManifestVerifier verifier;
    const auto activeVerification = verifier.verify(activeRoot);
    requireCondition(activeVerification.ok, "published active overlay should verify");
    requireCondition(
        activeVerification.manifestHash == published.publishedManifestHash,
        "active overlay verifier should report the published manifest hash");

    {
        std::ofstream drift(stagedRoot / "events" / "strategic_nexus_generated_events.txt", std::ios::app | std::ios::binary);
        drift << "# drift";
    }
    const auto rejected = publisher.publish(stagedRoot, root / "rejected_active", false);
    requireCondition(!rejected.ok, "publisher should reject invalid staged overlay");
    requireCondition(
        rejected.reason.find("staged generated overlay verification failed: ") == 0,
        "publisher should explain staged overlay verification failure");

    std::cout << "generated overlay publisher tests passed.\n";
    return 0;
}
