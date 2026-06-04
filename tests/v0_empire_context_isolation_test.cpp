// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "strategic_pipeline/V0StrategicPipeline.h"
#include "common/FileUtil.h"

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

std::string makeInputJson(
    const std::string& campaignId,
    const std::string& empireId,
    const std::string& ministry,
    const bool atWar,
    const double strategicPressure,
    const std::string& militaryPosture,
    const std::string& researchBias)
{
    return
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"context_id\": \"archive_session_summary_v0\",\n"
        "  \"campaign_id\": \"" + campaignId + "\",\n"
        "  \"empire_id\": \"" + empireId + "\",\n"
        "  \"ministry\": \"" + ministry + "\",\n"
        "  \"turn_context\": {\"year\": 2230, \"is_at_war\": " + std::string(atWar ? "true" : "false") +
        ", \"strategic_pressure\": " + std::to_string(strategicPressure) + "},\n"
        "  \"known_facts\": [\"archive_verified:true\", \"save_headline_parsed:true\"],\n"
        "  \"uncertainties\": [\"empire_state_not_extracted_yet\"],\n"
        "  \"current_agenda\": {\"military_posture\": \"" + militaryPosture + "\", \"research_bias\": \"" +
        researchBias + "\"}\n"
        "}\n";
}

} // namespace

int main()
{
    namespace pipeline = strategic_nexus::strategic_pipeline;

    const std::filesystem::path root = "dist/v0_empire_context_isolation";
    std::filesystem::create_directories(root);

    const std::string sharedCampaignId = "campaign_shared";
    const std::string empireAlphaId = "empire_alpha";
    const std::string empireBetaId = "empire_beta";

    const std::filesystem::path alphaInputPath = root / "alpha_input.json";
    const std::filesystem::path alphaDecisionPath = root / "alpha_decision.json";
    const std::filesystem::path alphaAuditPath = root / "alpha_audit.json";
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(
            alphaInputPath,
            makeInputJson(sharedCampaignId, empireAlphaId, "military", true, 0.85, "defensive", "economy")),
        "alpha input fixture should be written");

    const pipeline::V0StrategicPipeline pipelineRunner;
    pipeline::PipelineRunConfig alphaConfig;
    alphaConfig.inputContextPath = alphaInputPath;
    alphaConfig.decisionOutputPath = alphaDecisionPath;
    alphaConfig.auditOutputPath = alphaAuditPath;
    alphaConfig.sequenceId = 11;
    alphaConfig.createdUnixMs = 1111;
    alphaConfig.ttlMs = 60000;

    const auto alphaResult = pipelineRunner.run(alphaConfig);
    requireCondition(alphaResult.success, "alpha empire pipeline should succeed");
    requireCondition(alphaResult.payload.campaignId == sharedCampaignId, "alpha payload should preserve shared campaign id");
    requireCondition(alphaResult.payload.empireId == empireAlphaId, "alpha payload should preserve alpha empire id");
    requireCondition(alphaResult.proposal.empireId == empireAlphaId, "alpha proposal should preserve alpha empire id");
    requireCondition(alphaResult.brief.empireId == empireAlphaId, "alpha brief should preserve alpha empire id");
    requireCondition(alphaResult.auditOutputWritten, "alpha audit output should be written");

    const std::string alphaDecisionJson = strategic_nexus::common::readTextFile(alphaDecisionPath);
    const std::string alphaAuditJson = strategic_nexus::common::readTextFile(alphaAuditPath);
    requireCondition(
        alphaDecisionJson.find("\"campaign_id\": \"campaign_shared\"") != std::string::npos,
        "alpha decision JSON should contain shared campaign id");
    requireCondition(
        alphaDecisionJson.find("\"empire_id\": \"empire_alpha\"") != std::string::npos,
        "alpha decision JSON should contain alpha empire id");
    requireCondition(
        alphaDecisionJson.find("\"empire_id\": \"empire_beta\"") == std::string::npos,
        "alpha decision JSON must not leak beta empire id");
    requireCondition(
        alphaAuditJson.find("\"empire_id\": \"empire_alpha\"") != std::string::npos,
        "alpha audit JSON should contain alpha empire id");
    requireCondition(
        alphaAuditJson.find("\"empire_id\": \"empire_beta\"") == std::string::npos,
        "alpha audit JSON must not leak beta empire id");

    const std::filesystem::path betaInputPath = root / "beta_input.json";
    const std::filesystem::path betaDecisionPath = root / "beta_decision.json";
    const std::filesystem::path betaAuditPath = root / "beta_audit.json";
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(
            betaInputPath,
            makeInputJson(sharedCampaignId, empireBetaId, "research", false, 0.20, "defensive", "economy")),
        "beta input fixture should be written");

    pipeline::PipelineRunConfig betaConfig;
    betaConfig.inputContextPath = betaInputPath;
    betaConfig.decisionOutputPath = betaDecisionPath;
    betaConfig.auditOutputPath = betaAuditPath;
    betaConfig.sequenceId = 12;
    betaConfig.createdUnixMs = 2222;
    betaConfig.ttlMs = 60000;

    const auto betaResult = pipelineRunner.run(betaConfig);
    requireCondition(betaResult.success, "beta empire pipeline should succeed");
    requireCondition(betaResult.payload.campaignId == sharedCampaignId, "beta payload should preserve shared campaign id");
    requireCondition(betaResult.payload.empireId == empireBetaId, "beta payload should preserve beta empire id");
    requireCondition(betaResult.proposal.empireId == empireBetaId, "beta proposal should preserve beta empire id");
    requireCondition(betaResult.brief.empireId == empireBetaId, "beta brief should preserve beta empire id");
    requireCondition(betaResult.auditOutputWritten, "beta audit output should be written");

    const std::string betaDecisionJson = strategic_nexus::common::readTextFile(betaDecisionPath);
    const std::string betaAuditJson = strategic_nexus::common::readTextFile(betaAuditPath);
    requireCondition(
        betaDecisionJson.find("\"campaign_id\": \"campaign_shared\"") != std::string::npos,
        "beta decision JSON should contain shared campaign id");
    requireCondition(
        betaDecisionJson.find("\"empire_id\": \"empire_beta\"") != std::string::npos,
        "beta decision JSON should contain beta empire id");
    requireCondition(
        betaDecisionJson.find("\"empire_id\": \"empire_alpha\"") == std::string::npos,
        "beta decision JSON must not leak alpha empire id");
    requireCondition(
        betaAuditJson.find("\"empire_id\": \"empire_beta\"") != std::string::npos,
        "beta audit JSON should contain beta empire id");
    requireCondition(
        betaAuditJson.find("\"empire_id\": \"empire_alpha\"") == std::string::npos,
        "beta audit JSON must not leak alpha empire id");

    const std::filesystem::path missingEmpireInputPath = root / "missing_empire_input.json";
    const std::filesystem::path missingEmpireDecisionPath = root / "missing_empire_decision.json";
    requireCondition(
        strategic_nexus::common::writeTextFileAtomically(
            missingEmpireInputPath,
            "{\n"
            "  \"schema_version\": 1,\n"
            "  \"campaign_id\": \"campaign_shared\",\n"
            "  \"ministry\": \"military\"\n"
            "}\n"),
        "missing-empire fixture should be written");

    pipeline::PipelineRunConfig missingEmpireConfig;
    missingEmpireConfig.inputContextPath = missingEmpireInputPath;
    missingEmpireConfig.decisionOutputPath = missingEmpireDecisionPath;

    const auto missingEmpireResult = pipelineRunner.run(missingEmpireConfig);
    requireCondition(!missingEmpireResult.success, "missing empire input should fail closed");
    requireCondition(
        missingEmpireResult.reason == "missing required identity fields",
        "missing empire input should expose explicit identity failure reason");
    requireCondition(
        !std::filesystem::exists(missingEmpireDecisionPath),
        "missing empire input must not write decision output");

    std::cout << "v0 empire context isolation tests passed.\n";
    return 0;
}
