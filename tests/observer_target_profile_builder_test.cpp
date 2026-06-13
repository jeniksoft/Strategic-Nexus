// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "ObserverTargetProfileBuilder.h"

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

strategic_nexus::SeasonEmpireBrief makeObserverBrief()
{
    strategic_nexus::SeasonDeltaLedger ledger;
    ledger.ok = true;
    ledger.reason = "accepted";
    ledger.campaignId = "campaign_001";
    ledger.deltaQuality = "metadata_plus_save_headline";
    ledger.facts.push_back("archive_verified:true");
    ledger.facts.push_back("player_country_id:42");
    ledger.uncertainties.push_back("save_content_not_parsed_yet");

    strategic_nexus::SeasonEmpireBriefBuilder briefBuilder;
    return briefBuilder.build(ledger, "observer_empire_001");
}

} // namespace

int main()
{
    const strategic_nexus::ObserverTargetProfileBuilder builder;
    const auto observerBrief = makeObserverBrief();

    {
        const auto profile = builder.build(observerBrief, "target_empire_002", 0.42);
        requireCondition(profile.ok, "profile should accept valid observer brief and target id");
        requireCondition(profile.campaignId == "campaign_001", "profile should preserve campaign id");
        requireCondition(profile.observerEmpireId == "observer_empire_001", "profile should preserve observer empire id");
        requireCondition(profile.targetEmpireId == "target_empire_002", "profile should preserve target empire id");
        requireCondition(profile.sourceBriefQuality == "metadata_plus_save_headline", "profile should preserve source brief quality");
        requireCondition(profile.summaryOnly, "profile should remain summary-only at contract stage");
        requireCondition(profile.evidenceReferences.size() == 3, "profile should gather evidence references from brief facts and uncertainties");
        requireCondition(profile.targetMemorySummaryConfidence == 0.42, "profile should carry the target memory summary confidence");
        requireCondition(profile.targetMemorySummaryConfidenceBand == "low", "profile should classify the summary confidence band");
        requireCondition(profile.targetMemorySummary.find("target_empire_002") != std::string::npos, "profile should describe the target empire in the memory summary");
        requireCondition(profile.targetMemorySummary.find("confidence_band=low") != std::string::npos, "profile should describe the confidence band in the memory summary");
        requireCondition(profile.predictiveDimensions.size() == 1, "profile should infer one bounded predictive dimension from the shared event evidence");
        requireCondition(profile.predictiveDimensions[0] == "breaks_agreements", "profile should interpret trade breakdown evidence as agreement-breaking risk");
        requireCondition(profile.predictiveDimensionsSummary.find("breaks_agreements") != std::string::npos, "profile should summarize inferred predictive dimensions");
        requireCondition(profile.allowedRuleDomains.size() == 4, "profile should advertise bounded allowed rule domains");
        requireCondition(profile.targetSpecificRuleCandidates.empty(), "profile should not emit gameplay rule candidates yet");
        requireCondition(!profile.ruleCandidateValidation.ready, "profile should keep candidate validation locked at summary-only stage");
        requireCondition(profile.ruleCandidateValidation.candidateDomains.size() == 4, "profile should expose bounded candidate-domain hooks");
        requireCondition(profile.ruleCandidateValidation.blockedReasons.size() == 3, "profile should explain why candidate generation is blocked");
        requireCondition(!profile.ruleCandidateValidation.allowsDomain("diplomacy"), "profile should fail closed on candidate-domain checks");
        requireCondition(profile.fieldAvailability.size() == 10, "profile should expose a bounded field availability map");
        requireCondition(profile.fieldAvailabilityAvailableCount == 2, "profile should count available field groups");
        requireCondition(profile.fieldAvailabilityMissingCount == 8, "profile should count missing field groups");
        requireCondition(
            profile.fieldAvailabilitySummary ==
                "available=observer_identity,target_identity; missing=diplomacy,war,subject,federation,border,intel,internal_pressure,strategic_reputation",
            "profile should summarize field availability");
        requireCondition(profile.missingInformation.size() == 10, "profile should carry brief missing information plus contract gaps");
        requireCondition(profile.compressionNotes.size() == 4, "profile should carry brief compression notes plus contract notes");
        requireCondition(
            profile.relationshipDelta.generalTrust > 0.0 && profile.relationshipDelta.generalTrust < 1.0,
            "profile should populate a bounded general trust delta");
        requireCondition(
            profile.relationshipDelta.predictedFutureBehavior > 0.0 &&
                profile.relationshipDelta.predictedFutureBehavior < 1.0,
            "profile should populate a bounded predictive behavior delta");
        requireCondition(
            profile.relationshipDelta.generalTrustSummary.find("summary_only general trust signal") != std::string::npos,
            "profile should summarize the general trust signal");
        requireCondition(
            profile.relationshipDelta.generalTrustSummary.find("confidence_band=low") != std::string::npos,
            "profile should describe the trust confidence band");
        requireCondition(
            profile.relationshipDelta.predictedFutureBehaviorSummary.find("summary_only predicted future behavior signal") != std::string::npos,
            "profile should summarize the predictive behavior signal");
        requireCondition(
            profile.relationshipDelta.predictedFutureBehaviorSummary.find("gameplay_rule_candidates=blocked") != std::string::npos,
            "profile should keep gameplay rule candidates blocked");

        const auto json = strategic_nexus::serializeObserverTargetProfile(profile);
        requireCondition(json.find("\"observer_empire_id\": \"observer_empire_001\"") != std::string::npos, "profile JSON should include observer empire id");
        requireCondition(json.find("\"target_empire_id\": \"target_empire_002\"") != std::string::npos, "profile JSON should include target empire id");
        requireCondition(json.find("\"summary_only\": true") != std::string::npos, "profile JSON should preserve summary-only contract");
        requireCondition(json.find("\"target_memory_summary\": \"summary_only target memory for target_empire_002") != std::string::npos, "profile JSON should include target memory summary");
        requireCondition(json.find("\"target_memory_summary_confidence\": 0.42") != std::string::npos, "profile JSON should include target memory summary confidence");
        requireCondition(json.find("\"target_memory_summary_confidence_band\": \"low\"") != std::string::npos, "profile JSON should include target memory summary confidence band");
        requireCondition(json.find("\"predictive_dimensions\": [") != std::string::npos, "profile JSON should include predictive dimensions");
        requireCondition(json.find("\"breaks_agreements\"") != std::string::npos, "profile JSON should include the inferred predictive dimension");
        requireCondition(json.find("\"predictive_dimensions_summary\": \"summary_only predictive dimensions; confidence_band=low; inferred=breaks_agreements; gameplay_rule_candidates=blocked\"") != std::string::npos, "profile JSON should include the predictive dimensions summary");
        requireCondition(json.find("\"field_availability_available_count\": 2") != std::string::npos, "profile JSON should include available field count");
        requireCondition(json.find("\"field_availability_missing_count\": 8") != std::string::npos, "profile JSON should include missing field count");
        requireCondition(
            json.find("\"field_availability_summary\": \"available=observer_identity,target_identity; missing=diplomacy,war,subject,federation,border,intel,internal_pressure,strategic_reputation\"") != std::string::npos,
            "profile JSON should include field availability summary");
        requireCondition(json.find("\"relationship_delta\": {") != std::string::npos, "profile JSON should include relationship delta");
        requireCondition(json.find("\"general_trust\": ") != std::string::npos, "profile JSON should include general trust");
        requireCondition(json.find("\"predicted_future_behavior\": ") != std::string::npos, "profile JSON should include predicted future behavior");
        requireCondition(json.find("\"field_availability\": [") != std::string::npos, "profile JSON should include field availability");
        requireCondition(json.find("\"internal_pressure\"") != std::string::npos, "profile JSON should include internal pressure availability");
        requireCondition(json.find("\"strategic_reputation\"") != std::string::npos, "profile JSON should include strategic reputation availability");
        requireCondition(json.find("\"rule_candidate_validation\": {") != std::string::npos, "profile JSON should include candidate validation scaffold");
        requireCondition(json.find("\"allowed_rule_domains\": [") != std::string::npos, "profile JSON should include allowed rule domains");
        requireCondition(json.find("\"target_specific_rule_candidates\": [") != std::string::npos, "profile JSON should include candidate rules field");
        requireCondition(json.find("\"blocked_reasons\": [") != std::string::npos, "profile JSON should include blocked reasons");
        requireCondition(json.find("observer_target_profile_summary_only_contract") != std::string::npos, "profile JSON should record contract note");
    }

    {
        const auto missingTarget = builder.build(observerBrief, " \t\r\n", 0.42);
        requireCondition(!missingTarget.ok, "profile should reject missing target empire id");
        requireCondition(missingTarget.reason == "missing target empire id", "profile should explain missing target empire id");
    }

    {
        strategic_nexus::SeasonEmpireBrief badBrief = observerBrief;
        badBrief.sourceLedgerQuality = "future_quality";
        const auto rejected = builder.build(badBrief, "target_empire_002", 0.42);
        requireCondition(!rejected.ok, "profile should reject unsupported brief quality");
        requireCondition(rejected.reason == "unsupported source brief quality", "profile should explain unsupported source brief quality");
    }

    {
        const auto invalidConfidence = builder.build(observerBrief, "target_empire_002", 1.5);
        requireCondition(!invalidConfidence.ok, "profile should reject out-of-range confidence");
        requireCondition(invalidConfidence.reason == "invalid confidence", "profile should explain invalid confidence");
    }

    {
        strategic_nexus::SeasonDeltaLedger emptyFactsLedger;
        emptyFactsLedger.ok = true;
        emptyFactsLedger.reason = "accepted";
        emptyFactsLedger.campaignId = "campaign_001";
        emptyFactsLedger.deltaQuality = "metadata_only";
        strategic_nexus::SeasonEmpireBriefBuilder briefBuilder;
        const auto emptyBrief = briefBuilder.build(emptyFactsLedger, "observer_empire_003");
        const auto rejected = builder.build(emptyBrief, "target_empire_002", 0.42);
        requireCondition(!rejected.ok, "profile should reject observer briefs without evidence");
        requireCondition(rejected.reason == "missing evidence references", "profile should explain missing evidence references");
    }

    {
        strategic_nexus::SeasonDeltaLedger sharedLedger;
        sharedLedger.ok = true;
        sharedLedger.reason = "accepted";
        sharedLedger.campaignId = "campaign_001";
        sharedLedger.deltaQuality = "metadata_plus_save_headline";
        sharedLedger.facts.push_back("archive_verified:true");
        sharedLedger.facts.push_back("shared_event:trade_breakdown");

        strategic_nexus::SeasonEmpireBriefBuilder briefBuilder;
        const auto observerOneBrief = briefBuilder.build(sharedLedger, "observer_empire_001");
        const auto observerTwoBrief = briefBuilder.build(sharedLedger, "observer_empire_002");
        const auto observerOneProfile = builder.build(observerOneBrief, "target_empire_002", 0.68);
        const auto observerTwoProfile = builder.build(observerTwoBrief, "target_empire_002", 0.68);

        requireCondition(observerOneProfile.ok && observerTwoProfile.ok, "profile should accept both observer-specific briefs");
        requireCondition(
            observerOneProfile.observerEmpireId != observerTwoProfile.observerEmpireId,
            "profiles should preserve different observer empire ids");
        requireCondition(
            observerOneProfile.evidenceReferences == observerTwoProfile.evidenceReferences,
            "shared event evidence should remain consistent across observer profiles");
        requireCondition(
            observerOneProfile.predictiveDimensions == observerTwoProfile.predictiveDimensions,
            "shared event evidence should infer the same predictive dimensions across observer profiles");

        const auto observerOneJson = strategic_nexus::serializeObserverTargetProfile(observerOneProfile);
        const auto observerTwoJson = strategic_nexus::serializeObserverTargetProfile(observerTwoProfile);
        requireCondition(
            observerOneJson != observerTwoJson,
            "same event should still serialize as distinct observer-target memories");
    }

    std::cout << "observer target profile builder tests passed.\n";
    return 0;
}
