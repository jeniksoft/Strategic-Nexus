// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "IntegratedEmpireStateBuilder.h"

#include "PersonalityProfileStore.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

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

double clamp01(const double value)
{
    return std::clamp(value, 0.0, 1.0);
}

double normalizedRank(const int rank)
{
    return clamp01(static_cast<double>(std::max(rank, 0)) / 4.0);
}

double normalizedPower(const int power)
{
    return clamp01(static_cast<double>(std::max(power, 0)) / 200.0);
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

void writeAvailabilityArray(
    std::ostringstream& json,
    const std::vector<IntegratedEmpireFieldAvailability>& values)
{
    json << "  \"field_availability\": [\n";
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto& value = values[index];
        json << "    {\n";
        json << "      \"field_group\": \"" << jsonEscape(value.fieldGroup) << "\",\n";
        json << "      \"source_quality\": \"" << jsonEscape(value.sourceQuality) << "\",\n";
        json << "      \"available\": " << (value.available ? "true" : "false") << ",\n";
        json << "      \"missing_reasons\": [\n";
        for (std::size_t reasonIndex = 0; reasonIndex < value.missingReasons.size(); ++reasonIndex) {
            json << "        \"" << jsonEscape(value.missingReasons[reasonIndex]) << "\"";
            if (reasonIndex + 1 < value.missingReasons.size()) {
                json << ",";
            }
            json << "\n";
        }
        json << "      ]\n";
        json << "    }";
        if (index + 1 < values.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ],\n";
}

std::string bandFor(const double value)
{
    if (value >= 0.75) {
        return "high";
    }
    if (value >= 0.45) {
        return "moderate";
    }
    return "low";
}

std::string describePressureBand(const std::string& label, const double value)
{
    return label + " band=" + bandFor(value);
}

std::string describeReputationBand(const std::string& label, const double value)
{
    return label + " band=" + bandFor(value);
}

std::string describeInertiaBand(const std::string& label, const double value)
{
    return label + " band=" + bandFor(value);
}

} // namespace

IntegratedEmpireStateBuilder::IntegratedEmpireStateBuilder(
    std::filesystem::path personalityProfileRoot)
    : personalityProfileRoot_(std::move(personalityProfileRoot))
{
}

IntegratedEmpireState IntegratedEmpireStateBuilder::build(
    const SeasonEmpireBrief& brief,
    const EmpireState& empire,
    const double confidence) const
{
    EmpireState effectiveEmpire = empire;
    if (!personalityProfileRoot_.empty() && hasNonWhitespace(brief.campaignId) && hasNonWhitespace(empire.id)) {
        const PersonalityProfileStore store;
        const PersonalityProfileRecord storedProfile = store.load(personalityProfileRoot_, brief.campaignId, empire.id);
        if (storedProfile.ok) {
            effectiveEmpire.personality = storedProfile.personality;
        }
    }

    IntegratedEmpireState state;
    state.campaignId = brief.campaignId;
    state.empireId = effectiveEmpire.id;
    state.sourceBriefQuality = brief.sourceLedgerQuality;
    state.sourceEmpireStateQuality = "summary_only_empire_state_contract";
    state.confidence = confidence;
    state.summaryOnly = true;
    state.evidenceReferences = brief.relevantFacts;
    state.evidenceReferences.insert(
        state.evidenceReferences.end(),
        brief.explicitUncertainties.begin(),
        brief.explicitUncertainties.end());

    if (!brief.ok) {
        state.reason = brief.reason;
        return state;
    }
    if (!hasNonWhitespace(state.campaignId)) {
        state.reason = "missing campaign id";
        return state;
    }
    if (!hasNonWhitespace(effectiveEmpire.id)) {
        state.reason = "missing empire id";
        return state;
    }
    if (brief.empireId != effectiveEmpire.id) {
        state.reason = "mismatched empire id";
        return state;
    }
    if (confidence < 0.0 || confidence > 1.0) {
        state.reason = "invalid confidence";
        return state;
    }
    if (state.evidenceReferences.empty()) {
        state.reason = "missing evidence references";
        return state;
    }

    const double fleetScore = normalizedPower(effectiveEmpire.power.fleetPower);
    const double economicScore = normalizedRank(effectiveEmpire.power.economicRank);
    const double techScore = normalizedRank(effectiveEmpire.power.technologyRank);
    const double diplomaticScore = normalizedPower(effectiveEmpire.power.diplomaticInfluence);
    const double paranoia = clamp01(effectiveEmpire.personality.paranoia);
    const double honor = clamp01(effectiveEmpire.personality.honor);
    const double boldness = clamp01(effectiveEmpire.personality.boldness);
    const double opportunism = clamp01(effectiveEmpire.personality.opportunism);
    const double fear = clamp01(effectiveEmpire.adaptiveState.fearOfPlayer);
    const double trauma = clamp01(effectiveEmpire.adaptiveState.warTrauma);
    const double federationTrust = clamp01(effectiveEmpire.adaptiveState.trustInFederations);

    state.internalPressure.securityAnxiety = clamp01(
        0.18 + paranoia * 0.40 + fear * 0.35 + (fleetScore < 0.25 ? 0.10 : 0.0));
    state.internalPressure.economicPressure = clamp01(
        0.22 + (1.0 - economicScore) * 0.45 + trauma * 0.10);
    state.internalPressure.publicTrauma = clamp01(
        0.12 + trauma * 0.75 + (state.evidenceReferences.size() > 3 ? 0.02 : 0.0));
    state.internalPressure.eliteAgenda = clamp01(
        0.20 + opportunism * 0.45 + boldness * 0.10);
    state.internalPressure.prestigeNeed = clamp01(
        0.15 + boldness * 0.30 + (diplomaticScore < 0.30 ? 0.10 : 0.0));
    state.internalPressure.diplomaticFlexibility = clamp01(
        0.20 + federationTrust * 0.45 + opportunism * 0.10 - paranoia * 0.08);
    state.internalPressure.warExhaustionMemory = clamp01(0.10 + trauma * 0.80);
    state.internalPressure.summary =
        describePressureBand("internal pressure", state.internalPressure.securityAnxiety);

    state.strategicReputation.privateSelfImage = clamp01(
        0.30 + honor * 0.30 + boldness * 0.15 + opportunism * 0.10 - paranoia * 0.05);
    state.strategicReputation.observerAudiencePerception = clamp01(
        0.25 + fleetScore * 0.25 + economicScore * 0.20 + techScore * 0.15 + diplomaticScore * 0.15
        - fear * 0.10);
    state.strategicReputation.privateSelfImageSummary =
        describeReputationBand("private self-image", state.strategicReputation.privateSelfImage);
    state.strategicReputation.observerAudiencePerceptionSummary =
        describeReputationBand(
            "observer perception",
            state.strategicReputation.observerAudiencePerception);

    state.doctrineInertia.inertia = clamp01(
        0.20 + honor * 0.20 + trauma * 0.25 + (economicScore < 0.40 ? 0.10 : 0.0) + paranoia * 0.05);
    state.doctrineInertia.reformCost = clamp01(
        0.15 + state.doctrineInertia.inertia * 0.60 + (fear > 0.65 ? 0.10 : 0.0));
    state.doctrineInertia.inertiaSummary =
        describeInertiaBand("doctrine inertia", state.doctrineInertia.inertia);
    state.doctrineInertia.reformCostSummary =
        describeInertiaBand("reform cost", state.doctrineInertia.reformCost);

    state.fieldAvailability = {
        { "identity", "summary_only_empire_state_contract", true, {} },
        { "ethics", "summary_only_empire_state_contract", false, { "ethics distribution not extracted yet" } },
        { "population_ethics_distribution", "summary_only_empire_state_contract", false, { "population ethics distribution not extracted yet" } },
        { "government", "summary_only_empire_state_contract", false, { "government not extracted yet" } },
        { "civics", "summary_only_empire_state_contract", false, { "civics not extracted yet" } },
        { "resources", "summary_only_empire_state_contract", false, { "resource trajectories not extracted yet" } },
        { "income_trajectory", "summary_only_empire_state_contract", false, { "income trajectory not extracted yet" } },
        { "capability", "summary_only_empire_state_contract", true, {} },
        { "memory", "summary_only_empire_state_contract", true, {} },
        { "relationships", "summary_only_empire_state_contract", false, { "relationship deltas not extracted yet" } },
        { "internal_pressure", "summary_only_empire_state_contract", true, {} },
        { "reputation", "summary_only_empire_state_contract", true, {} },
        { "doctrine_inertia", "summary_only_empire_state_contract", true, {} }
    };

    state.missingInformation = {
        "ethics_distribution_not_extracted_yet",
        "population_ethics_distribution_not_extracted_yet",
        "government_not_extracted_yet",
        "civics_not_extracted_yet",
        "resource_trajectory_not_extracted_yet",
        "income_trajectory_not_extracted_yet",
        "relationship_deltas_not_extracted_yet",
        "generated_overlay_rules_not_emitted_from_summary_only_contract"
    };

    state.compressionNotes = {
        "integrated_empire_state_summary_only_contract",
        "do_not_treat_internal_pressure_as_hidden_bonus",
        "do_not_treat_strategic_reputation_as_objective_truth",
        "coarse_nudges_only_until_parser_coverage_exists"
    };

    state.ok = true;
    state.reason = "accepted summary-only integrated empire state";
    return state;
}

std::string serializeIntegratedEmpireState(const IntegratedEmpireState& state)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (state.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(state.reason) << "\",\n";
    json << "  \"campaign_id\": \"" << jsonEscape(state.campaignId) << "\",\n";
    json << "  \"empire_id\": \"" << jsonEscape(state.empireId) << "\",\n";
    json << "  \"source_brief_quality\": \"" << jsonEscape(state.sourceBriefQuality) << "\",\n";
    json << "  \"source_empire_state_quality\": \"" << jsonEscape(state.sourceEmpireStateQuality) << "\",\n";
    json << "  \"confidence\": " << state.confidence << ",\n";
    json << "  \"summary_only\": " << (state.summaryOnly ? "true" : "false") << ",\n";
    writeStringArray(json, "evidence_references", state.evidenceReferences, true);
    json << "  \"internal_pressure\": {\n";
    json << "    \"security_anxiety\": " << state.internalPressure.securityAnxiety << ",\n";
    json << "    \"economic_pressure\": " << state.internalPressure.economicPressure << ",\n";
    json << "    \"public_trauma\": " << state.internalPressure.publicTrauma << ",\n";
    json << "    \"elite_agenda\": " << state.internalPressure.eliteAgenda << ",\n";
    json << "    \"prestige_need\": " << state.internalPressure.prestigeNeed << ",\n";
    json << "    \"diplomatic_flexibility\": " << state.internalPressure.diplomaticFlexibility << ",\n";
    json << "    \"war_exhaustion_memory\": " << state.internalPressure.warExhaustionMemory << ",\n";
    json << "    \"summary\": \"" << jsonEscape(state.internalPressure.summary) << "\"\n";
    json << "  },\n";
    json << "  \"strategic_reputation\": {\n";
    json << "    \"private_self_image\": " << state.strategicReputation.privateSelfImage << ",\n";
    json << "    \"observer_audience_perception\": " << state.strategicReputation.observerAudiencePerception << ",\n";
    json << "    \"private_self_image_summary\": \""
         << jsonEscape(state.strategicReputation.privateSelfImageSummary) << "\",\n";
    json << "    \"observer_audience_perception_summary\": \""
         << jsonEscape(state.strategicReputation.observerAudiencePerceptionSummary) << "\"\n";
    json << "  },\n";
    json << "  \"doctrine_inertia\": {\n";
    json << "    \"inertia\": " << state.doctrineInertia.inertia << ",\n";
    json << "    \"reform_cost\": " << state.doctrineInertia.reformCost << ",\n";
    json << "    \"inertia_summary\": \"" << jsonEscape(state.doctrineInertia.inertiaSummary) << "\",\n";
    json << "    \"reform_cost_summary\": \"" << jsonEscape(state.doctrineInertia.reformCostSummary) << "\"\n";
    json << "  },\n";
    writeAvailabilityArray(json, state.fieldAvailability);
    writeStringArray(json, "missing_information", state.missingInformation, true);
    writeStringArray(json, "compression_notes", state.compressionNotes, false);
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
