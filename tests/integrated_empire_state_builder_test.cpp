// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "IntegratedEmpireStateBuilder.h"
#include "PersonalityProfileStore.h"
#include "SeasonEmpireBriefBuilder.h"

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

strategic_nexus::SeasonEmpireBrief makeBrief(const std::string& empireId)
{
    strategic_nexus::SeasonDeltaLedger ledger;
    ledger.ok = true;
    ledger.reason = "accepted";
    ledger.campaignId = "campaign_001";
    ledger.deltaQuality = "metadata_plus_save_headline";
    ledger.facts.push_back("archive_verified:true");
    ledger.facts.push_back("shared_event:trade_breakdown");
    ledger.uncertainties.push_back("ethics_distribution_not_parsed_yet");

    strategic_nexus::SeasonEmpireBriefBuilder briefBuilder;
    return briefBuilder.build(ledger, empireId);
}

strategic_nexus::EmpireState makeEmpireAlpha()
{
    strategic_nexus::EmpireState empire;
    empire.id = "empire_alpha";
    empire.displayName = "Alpha Concordat";
    empire.power.fleetPower = 180;
    empire.power.economicRank = 4;
    empire.power.technologyRank = 4;
    empire.power.diplomaticInfluence = 88;
    empire.personality.boldness = 0.74;
    empire.personality.paranoia = 0.22;
    empire.personality.honor = 0.63;
    empire.personality.opportunism = 0.58;
    empire.adaptiveState.fearOfPlayer = 0.18;
    empire.adaptiveState.warTrauma = 0.12;
    empire.adaptiveState.trustInFederations = 0.72;
    return empire;
}

strategic_nexus::EmpireState makeEmpireBeta()
{
    strategic_nexus::EmpireState empire;
    empire.id = "empire_beta";
    empire.displayName = "Beta Synod";
    empire.power.fleetPower = 24;
    empire.power.economicRank = 1;
    empire.power.technologyRank = 1;
    empire.power.diplomaticInfluence = 12;
    empire.personality.boldness = 0.28;
    empire.personality.paranoia = 0.84;
    empire.personality.honor = 0.34;
    empire.personality.opportunism = 0.77;
    empire.adaptiveState.fearOfPlayer = 0.79;
    empire.adaptiveState.warTrauma = 0.66;
    empire.adaptiveState.trustInFederations = 0.18;
    return empire;
}

strategic_nexus::PersonalityProfileRecord makeStoredProfile()
{
    strategic_nexus::PersonalityProfileRecord record;
    record.ok = true;
    record.reason = "accepted validated personality profile";
    record.sourceSchemaVersion = 1;
    record.schemaCompatibilityState = "current";
    record.schemaCompatibilityNote.clear();
    record.campaignId = "campaign_001";
    record.empireId = "empire_alpha";
    record.sourceSaveDate = "2026-06-10";
    record.sourceSavePath = "dist/private_reports/personality_profile_source.sav";
    record.validatedUpdateSummary = "profile-backed defensive caution";
    record.confidence = 0.91;
    record.zeroHistoryBootstrap = false;
    record.personality.boldness = 0.18;
    record.personality.paranoia = 0.94;
    record.personality.honor = 0.12;
    record.personality.opportunism = 0.21;
    return record;
}

} // namespace

int main()
{
    const auto root = std::filesystem::path("dist/integrated_empire_state_builder_fixture");
    const auto emptyProfileRoot = root / "empty_profile_root";
    const auto storedProfileRoot = root / "stored_profile_root";
    std::filesystem::remove_all(root);

    const strategic_nexus::PersonalityProfileStore store;
    const auto storedProfile = makeStoredProfile();
    requireCondition(store.save(storedProfileRoot, storedProfile), "stored profile fixture should be writable");

    const strategic_nexus::IntegratedEmpireStateBuilder builder(emptyProfileRoot);
    const strategic_nexus::IntegratedEmpireStateBuilder profileBackedBuilder(storedProfileRoot);
    const auto alphaBrief = makeBrief("empire_alpha");
    const auto betaBrief = makeBrief("empire_beta");
    const auto alpha = makeEmpireAlpha();
    const auto beta = makeEmpireBeta();

    {
        const auto state = builder.build(alphaBrief, alpha, 0.58);
        requireCondition(state.ok, "integrated empire state should accept valid brief and empire");
        requireCondition(state.summaryOnly, "integrated empire state should stay summary-only");
        requireCondition(!state.personalityProfileApplied, "summary-only builder should not mark a loaded personality profile");
        requireCondition(state.personalityProfileSchemaCompatibilityState == "not_loaded", "summary-only builder should expose not-loaded profile state");
        requireCondition(state.campaignId == "campaign_001", "campaign id should be preserved");
        requireCondition(state.empireId == "empire_alpha", "empire id should be preserved");
        requireCondition(state.sourceBriefQuality == "metadata_plus_save_headline", "brief quality should be preserved");
        requireCondition(state.sourceEmpireStateQuality == "summary_only_empire_state_contract", "empire-state quality should be explicit");
        requireCondition(state.evidenceReferences.size() == 3, "evidence references should combine facts and uncertainties");
        requireCondition(state.fieldAvailability.size() == 13, "field availability map should cover the integrated state");
        requireCondition(state.missingInformation.size() == 8, "missing information should stay bounded");
        requireCondition(state.compressionNotes.size() == 4, "compression notes should stay bounded");
        requireCondition(state.internalPressure.securityAnxiety > 0.0, "security anxiety should be derived");
        requireCondition(state.internalPressure.economicPressure > 0.0, "economic pressure should be derived");
        requireCondition(state.strategicReputation.privateSelfImage > 0.0, "private self-image should be derived");
        requireCondition(state.strategicReputation.observerAudiencePerception > 0.0, "observer perception should be derived");
        requireCondition(state.doctrineInertia.inertia > 0.0, "doctrine inertia should be derived");
        requireCondition(state.doctrineInertia.reformCost > 0.0, "reform cost should be derived");
        requireCondition(state.internalPressure.securityAnxiety <= 1.0, "security anxiety should clamp to 1");
        requireCondition(state.strategicReputation.observerAudiencePerception <= 1.0, "observer perception should clamp to 1");
        requireCondition(state.doctrineInertia.reformCost <= 1.0, "reform cost should clamp to 1");

        const auto json = strategic_nexus::serializeIntegratedEmpireState(state);
        requireCondition(json.find("\"internal_pressure\": {") != std::string::npos, "JSON should include internal pressure");
        requireCondition(json.find("\"strategic_reputation\": {") != std::string::npos, "JSON should include strategic reputation");
        requireCondition(json.find("\"doctrine_inertia\": {") != std::string::npos, "JSON should include doctrine inertia");
        requireCondition(json.find("\"personality_profile\": {") != std::string::npos, "JSON should include personality profile provenance");
        requireCondition(json.find("\"applied\": false") != std::string::npos, "JSON should expose unloaded profile state");
        requireCondition(json.find("\"schema_compatibility_state\": \"not_loaded\"") != std::string::npos, "JSON should expose not-loaded compatibility state");
        requireCondition(json.find("\"prompt_output_note\": \"summary-only prompt-output context; no validated personality profile loaded\"") != std::string::npos, "JSON should expose summary-only prompt-output context");
        requireCondition(json.find("\"field_group\": \"ethics\"") != std::string::npos, "JSON should expose ethics field availability");
        requireCondition(json.find("\"field_group\": \"relationships\"") != std::string::npos, "JSON should expose relationships field availability");
        requireCondition(json.find("integrated_empire_state_summary_only_contract") != std::string::npos, "JSON should record the summary-only contract note");
    }

    {
        const auto state = builder.build(betaBrief, beta, 0.58);
        requireCondition(state.ok, "beta empire should also build successfully");
        requireCondition(
            state.internalPressure.securityAnxiety > alpha.personality.paranoia,
            "beta empire should have stronger security anxiety than the alpha personality baseline");
        requireCondition(
            state.strategicReputation.observerAudiencePerception < 0.7,
            "beta empire should keep observer perception bounded below the alpha case");

        const auto alphaState = builder.build(alphaBrief, alpha, 0.58);
        const auto alphaJson = strategic_nexus::serializeIntegratedEmpireState(alphaState);
        const auto betaJson = strategic_nexus::serializeIntegratedEmpireState(state);
        requireCondition(alphaJson != betaJson, "same evidence should still serialize differently for different empires");
    }

    {
        const auto baselineState = builder.build(alphaBrief, alpha, 0.58);
        const auto profileBackedState = profileBackedBuilder.build(alphaBrief, alpha, 0.58);
        requireCondition(profileBackedState.ok, "profile-backed integrated state should still build successfully");
        requireCondition(profileBackedState.personalityProfileApplied, "loaded personality profile should be surfaced in the integrated state");
        requireCondition(profileBackedState.personalityProfileSourceSchemaVersion == 1, "loaded personality profile should preserve schema version");
        requireCondition(profileBackedState.personalityProfileSchemaCompatibilityState == "current", "loaded personality profile should preserve compatibility state");
        requireCondition(profileBackedState.personalityProfileValidatedUpdateSummary == "profile-backed defensive caution", "loaded personality profile should expose the validated update summary");
        requireCondition(profileBackedState.personalityProfileSourceSaveDate == "2026-06-10", "loaded personality profile should expose the source save date");
        requireCondition(!profileBackedState.personalityProfileZeroHistoryBootstrap, "loaded profile fixture should not be zero-history bootstrap");
        requireCondition(
            profileBackedState.internalPressure.securityAnxiety > baselineState.internalPressure.securityAnxiety,
            "loaded personality profile should increase security anxiety for the stored empire");
        requireCondition(
            profileBackedState.strategicReputation.privateSelfImage < baselineState.strategicReputation.privateSelfImage,
            "loaded personality profile should reduce private self-image for the stored empire");
        const auto profileJson = strategic_nexus::serializeIntegratedEmpireState(profileBackedState);
        requireCondition(profileJson.find("\"applied\": true") != std::string::npos, "profile-backed JSON should expose applied profile state");
        requireCondition(profileJson.find("\"validated_update_summary\": \"profile-backed defensive caution\"") != std::string::npos, "profile-backed JSON should expose the validated update summary");
        requireCondition(profileJson.find("\"schema_compatibility_state\": \"current\"") != std::string::npos, "profile-backed JSON should expose the current compatibility state");
        requireCondition(profileJson.find("\"prompt_output_note\": \"validated personality profile available for prompt-output consumers\"") != std::string::npos, "profile-backed JSON should expose prompt-output provenance");
    }

    {
        strategic_nexus::EmpireState mismatchEmpire = alpha;
        mismatchEmpire.id = "empire_beta";
        const auto mismatch = builder.build(alphaBrief, mismatchEmpire, 0.58);
        requireCondition(!mismatch.ok, "mismatched empire id should fail closed");
        requireCondition(mismatch.reason == "mismatched empire id", "mismatched empire id should be explained");
    }

    {
        const auto missingEmpireId = builder.build(alphaBrief, strategic_nexus::EmpireState{}, 0.58);
        requireCondition(!missingEmpireId.ok, "missing empire id should fail closed");
        requireCondition(missingEmpireId.reason == "missing empire id", "missing empire id should be explained");
    }

    {
        const auto invalidConfidence = builder.build(alphaBrief, alpha, 1.5);
        requireCondition(!invalidConfidence.ok, "out-of-range confidence should fail closed");
        requireCondition(invalidConfidence.reason == "invalid confidence", "invalid confidence should be explained");
    }

    std::cout << "integrated empire state builder tests passed.\n";
    return 0;
}
