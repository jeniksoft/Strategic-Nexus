// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "ObserverTargetProfileBuilder.h"

#include <cmath>
#include <iomanip>
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

void writeStringArray(
    std::ostringstream& json,
    const char* name,
    const std::vector<std::string>& values,
    const bool trailingComma)
{
    json << "  \"" << name << "\": [\n";
    for (std::size_t index = 0; index < values.size(); ++index) {
        json << "    \"" << jsonEscape(values[index]) << "\"";
        if (index + 1 < values.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]";
    if (trailingComma) {
        json << ",";
    }
    json << "\n";
}

void appendEvidenceReference(
    std::vector<std::string>& references,
    const std::vector<std::string>& values)
{
    references.insert(references.end(), values.begin(), values.end());
}

void writeObjectArray(
    std::ostringstream& json,
    const char* name,
    const std::vector<ObserverTargetFieldAvailability>& values,
    const bool trailingComma)
{
    json << "  \"" << name << "\": [\n";
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto& value = values[index];
        json << "    {"
             << "\"field_group\":\"" << jsonEscape(value.fieldGroup) << "\","
             << "\"source_quality\":\"" << jsonEscape(value.sourceQuality) << "\","
             << "\"available\":" << (value.available ? "true" : "false") << ","
             << "\"missing_reasons\":[";
        for (std::size_t reasonIndex = 0; reasonIndex < value.missingReasons.size(); ++reasonIndex) {
            if (reasonIndex > 0) {
                json << ", ";
            }
            json << "\"" << jsonEscape(value.missingReasons[reasonIndex]) << "\"";
        }
        json << "]}";
        if (index + 1 < values.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]";
    if (trailingComma) {
        json << ",";
    }
    json << "\n";
}

std::vector<ObserverTargetFieldAvailability> buildFieldAvailability(
    const ObserverTargetProfile& profile,
    const SeasonEmpireBrief& observerBrief)
{
    std::vector<ObserverTargetFieldAvailability> availability;
    availability.push_back({
        "observer_identity",
        "observer_brief_identity",
        observerBrief.ok && hasNonWhitespace(profile.observerEmpireId),
        observerBrief.ok && hasNonWhitespace(profile.observerEmpireId)
            ? std::vector<std::string>{}
            : std::vector<std::string>{ "observer empire identity unavailable from observer brief" }
    });
    availability.push_back({
        "target_identity",
        "target_scoped_profile_contract",
        hasNonWhitespace(profile.targetEmpireId),
        hasNonWhitespace(profile.targetEmpireId)
            ? std::vector<std::string>{}
            : std::vector<std::string>{ "target empire identity missing" }
    });
    availability.push_back({ "diplomacy", "summary_only_profile_contract", false, { "diplomatic evidence not extracted yet" } });
    availability.push_back({ "war", "summary_only_profile_contract", false, { "war evidence not extracted yet" } });
    availability.push_back({ "subject", "summary_only_profile_contract", false, { "subject evidence not extracted yet" } });
    availability.push_back({ "federation", "summary_only_profile_contract", false, { "federation evidence not extracted yet" } });
    availability.push_back({ "border", "summary_only_profile_contract", false, { "border evidence not extracted yet" } });
    availability.push_back({ "intel", "summary_only_profile_contract", false, { "intel evidence not extracted yet" } });
    availability.push_back({ "internal_pressure", "summary_only_profile_contract", false, { "internal pressure evidence not extracted yet" } });
    availability.push_back({ "strategic_reputation", "summary_only_profile_contract", false, { "strategic reputation evidence not extracted yet" } });
    return availability;
}

std::string confidenceBandFor(const double confidence)
{
    if (confidence >= 0.75) {
        return "high";
    }

    if (confidence >= 0.45) {
        return "moderate";
    }

    return "low";
}

std::string buildTargetMemorySummary(
    const ObserverTargetProfile& profile,
    const std::size_t evidenceCount)
{
    std::ostringstream output;
    output << "summary_only target memory for "
           << (hasNonWhitespace(profile.targetEmpireId) ? profile.targetEmpireId : "unknown_target")
           << "; evidence_count=" << evidenceCount
           << "; source_quality=" << profile.sourceBriefQuality
           << "; confidence_band=" << profile.targetMemorySummaryConfidenceBand
           << "; gameplay_rule_candidates=blocked";
    return output.str();
}

} // namespace

bool ObserverTargetRuleCandidateValidation::allowsDomain(const std::string& domain) const
{
    if (!ready) {
        return false;
    }

    for (const auto& candidateDomain : candidateDomains) {
        if (candidateDomain == domain) {
            return true;
        }
    }

    return false;
}

ObserverTargetProfile ObserverTargetProfileBuilder::build(
    const SeasonEmpireBrief& observerBrief,
    const std::string& targetEmpireId,
    const double confidence) const
{
    ObserverTargetProfile profile;
    profile.campaignId = observerBrief.campaignId;
    profile.observerEmpireId = observerBrief.empireId;
    profile.targetEmpireId = targetEmpireId;
    profile.sourceBriefQuality = observerBrief.sourceLedgerQuality;
    profile.confidence = confidence;
    profile.summaryOnly = true;
    profile.relationshipDelta.generalTrustSummary = "summary_only_contract_no_relationship_delta_yet";
    profile.relationshipDelta.predictedFutureBehaviorSummary = "summary_only_contract_no_predictive_profile_yet";
    profile.allowedRuleDomains = { "diplomacy", "war", "border", "intel" };
    profile.ruleCandidateValidation.candidateDomains = profile.allowedRuleDomains;
    profile.ruleCandidateValidation.requiredSignals = {
        "target_scoped_relationship_delta",
        "confidence_scored_memory_summary",
        "gameplay_safe_generation_validation"
    };
    profile.ruleCandidateValidation.blockedReasons = {
        "summary_only_contract",
        "target_specific_rule_generation_not_implemented_yet",
        "missing_gameplay_safe_validation_path"
    };

    if (!observerBrief.ok) {
        profile.reason = observerBrief.reason.empty()
            ? "observer brief unavailable"
            : ("observer brief unavailable: " + observerBrief.reason);
        return profile;
    }

    if (!hasNonWhitespace(targetEmpireId)) {
        profile.reason = "missing target empire id";
        return profile;
    }

    if (!std::isfinite(confidence) || confidence < 0.0 || confidence > 1.0) {
        profile.reason = "invalid confidence";
        return profile;
    }

    if (observerBrief.sourceLedgerQuality != "metadata_only" &&
        observerBrief.sourceLedgerQuality != "metadata_plus_save_headline") {
        profile.reason = "unsupported source brief quality";
        return profile;
    }

    appendEvidenceReference(profile.evidenceReferences, observerBrief.relevantFacts);
    appendEvidenceReference(profile.evidenceReferences, observerBrief.explicitUncertainties);
    if (profile.evidenceReferences.empty()) {
        profile.reason = "missing evidence references";
        return profile;
    }

    profile.ok = true;
    profile.reason = "accepted summary-only observer-target profile";
    profile.targetMemorySummaryConfidence = confidence;
    profile.targetMemorySummaryConfidenceBand = confidenceBandFor(confidence);
    profile.targetMemorySummary = buildTargetMemorySummary(profile, profile.evidenceReferences.size());
    profile.missingInformation = observerBrief.missingInformation;
    profile.missingInformation.push_back("observer_target_relationship_delta_not_generated_yet");
    profile.missingInformation.push_back("target_specific_rule_generation_not_implemented_yet");
    profile.missingInformation.push_back("gameplay_affecting_target_rules_require_later_validation");
    profile.missingInformation.push_back("internal_pressure_not_generated_yet");
    profile.missingInformation.push_back("strategic_reputation_not_generated_yet");

    profile.compressionNotes = observerBrief.compressionNotes;
    profile.compressionNotes.push_back("observer_target_profile_summary_only_contract");
    profile.compressionNotes.push_back("do_not_infer_target_specific_rules_from_this_contract_alone");
    profile.ruleCandidateValidation.ready = false;
    profile.fieldAvailability = buildFieldAvailability(profile, observerBrief);
    return profile;
}

std::string serializeObserverTargetProfile(const ObserverTargetProfile& profile)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (profile.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(profile.reason) << "\",\n";
    json << "  \"campaign_id\": \"" << jsonEscape(profile.campaignId) << "\",\n";
    json << "  \"observer_empire_id\": \"" << jsonEscape(profile.observerEmpireId) << "\",\n";
    json << "  \"target_empire_id\": \"" << jsonEscape(profile.targetEmpireId) << "\",\n";
    json << "  \"source_brief_quality\": \"" << jsonEscape(profile.sourceBriefQuality) << "\",\n";
    json << "  \"confidence\": " << profile.confidence << ",\n";
    json << "  \"summary_only\": " << (profile.summaryOnly ? "true" : "false") << ",\n";
    json << "  \"relationship_delta\": {\n";
    json << "    \"general_trust\": " << profile.relationshipDelta.generalTrust << ",\n";
    json << "    \"predicted_future_behavior\": " << profile.relationshipDelta.predictedFutureBehavior << ",\n";
    json << "    \"general_trust_summary\": \"" << jsonEscape(profile.relationshipDelta.generalTrustSummary) << "\",\n";
    json << "    \"predicted_future_behavior_summary\": \"" << jsonEscape(profile.relationshipDelta.predictedFutureBehaviorSummary) << "\"\n";
    json << "  },\n";
    json << "  \"target_memory_summary\": \"" << jsonEscape(profile.targetMemorySummary) << "\",\n";
    json << "  \"target_memory_summary_confidence\": " << profile.targetMemorySummaryConfidence << ",\n";
    json << "  \"target_memory_summary_confidence_band\": \"" << jsonEscape(profile.targetMemorySummaryConfidenceBand) << "\",\n";
    json << "  \"rule_candidate_validation\": {\n";
    json << "    \"ready\": " << (profile.ruleCandidateValidation.ready ? "true" : "false") << ",\n";
    writeStringArray(json, "candidate_domains", profile.ruleCandidateValidation.candidateDomains, true);
    writeStringArray(json, "required_signals", profile.ruleCandidateValidation.requiredSignals, true);
    writeStringArray(json, "blocked_reasons", profile.ruleCandidateValidation.blockedReasons, false);
    json << "  },\n";
    writeObjectArray(json, "field_availability", profile.fieldAvailability, true);
    writeStringArray(json, "evidence_references", profile.evidenceReferences, true);
    writeStringArray(json, "allowed_rule_domains", profile.allowedRuleDomains, true);
    writeStringArray(json, "target_specific_rule_candidates", profile.targetSpecificRuleCandidates, true);
    writeStringArray(json, "missing_information", profile.missingInformation, true);
    writeStringArray(json, "compression_notes", profile.compressionNotes, false);
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
