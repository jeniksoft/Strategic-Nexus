// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SncDslDraftPackageBuilder.h"

#include "generated_overlay/DslParser.h"
#include "generated_overlay/DslValidator.h"

#include <cstdlib>
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

strategic_nexus::SncCandidateDecision buildEligibleCandidate()
{
    strategic_nexus::SncCandidateDecision candidate;
    candidate.candidateDecisionId = "candidate-decision::alpha";
    candidate.decisionInputId = "decision-input::alpha";
    candidate.packageEntryId = "entry::alpha";
    candidate.campaignKey = "Alpha Campaign";
    candidate.ruleScope = "Alpha Campaign::autosave_2200.03.01::abcdef1234567890";
    candidate.sourceKind = "current_loadable_save";
    candidate.saveName = "autosave_2200.03.01.sav";
    candidate.saveDate = "2200.03.01";
    candidate.contentHash = "abcdef1234567890";
    candidate.parseStatus = "headline_parsed";
    candidate.playerCountryId = "0";
    candidate.empireName = "Aeel Corp";
    candidate.recommendedAction = "observe_only";
    candidate.militaryPosture = "defensive";
    candidate.researchBias = "economy";
    candidate.decisionBoundary = "dry_run_no_overlay_publish";
    candidate.validationState = "validated_dry_run_candidate";
    candidate.confidencePercent = 55;
    candidate.empireState.parsed = true;
    candidate.empireState.parseStatus = "headline_parsed";
    candidate.empireState.playerCountryId = "0";
    candidate.empireState.empireName = "Aeel Corp";
    candidate.empireState.government = "oligarchic";
    candidate.empireState.authority = "auth_oligarchic";
    return candidate;
}

strategic_nexus::SncCandidateDecisionPackage buildCandidatePackage()
{
    strategic_nexus::SncCandidateDecisionPackage package;
    package.ok = true;
    package.reason = "candidate decision package built";
    package.readiness = "ready";
    package.validatorPassed = true;
    package.dryRunOnly = true;
    package.publishesOverlay = false;
    package.modelOutputUsed = false;
    package.modelOutputTrusted = false;
    package.validationRequired = true;
    package.candidateDecisions.push_back(buildEligibleCandidate());
    package.candidateDecisionCount = package.candidateDecisions.size();
    package.acceptedCandidateCount = package.candidateDecisionCount;
    return package;
}

} // namespace

int main()
{
    const strategic_nexus::SncDslDraftPackageBuilder builder;
    const auto candidatePackage = buildCandidatePackage();

    const auto candidateJson = strategic_nexus::serializeSncCandidateDecisionPackage(candidatePackage);
    const auto readResult = strategic_nexus::parseSncCandidateDecisionPackageJson(candidateJson);
    requireCondition(readResult.ok, "serialized candidate package should parse");
    requireCondition(readResult.package.candidateDecisions.size() == 1, "candidate parser should preserve candidate count");
    requireCondition(readResult.package.candidateDecisions.front().empireState.parsed, "candidate parser should preserve parsed empire state");

    const auto package = builder.build(readResult.package, std::filesystem::path("dist/snc_candidate_decision_package.json"));
    requireCondition(package.ok, "DSL draft package should build from eligible candidate");
    requireCondition(package.validatorPassed, "DSL draft package should pass DSL validation");
    requireCondition(package.readiness == "ready", "eligible candidate package should produce ready DSL draft");
    requireCondition(package.dryRunOnly, "DSL draft package should be dry-run only");
    requireCondition(!package.publishesOverlay, "DSL draft package must not publish overlay");
    requireCondition(!package.overlayCompileAllowed, "DSL draft package should not allow overlay compile/publish yet");
    requireCondition(package.dslRuleCount == 1, "one eligible candidate should create one DSL rule");
    requireCondition(package.eligibleCandidateCount == 1, "eligible count should be one");
    requireCondition(package.skippedCandidateCount == 0, "no eligible candidate should be skipped");
    requireCondition(package.dslText.find("campaign \"alpha_campaign\"") != std::string::npos, "DSL should carry safe campaign id");
    requireCondition(package.dslText.find("empire \"country_0\"") != std::string::npos, "DSL should carry safe empire id");
    requireCondition(package.dslText.find("when known.save_fingerprint = h_abcdef123456") != std::string::npos, "DSL should gate on save fingerprint");
    requireCondition(package.dslText.find("when known.save_date = d_2200_03_01") != std::string::npos, "DSL should gate on save date");
    requireCondition(package.dslText.find("prefer military_posture defensive intensity 0.8") != std::string::npos, "DSL should emit bounded military posture");
    requireCondition(package.dslText.find("prefer research_bias economy intensity 0.7") != std::string::npos, "DSL should emit bounded research bias");
    requireCondition(package.dslText.find("prefer doctrine_inertia high intensity 0.8") != std::string::npos, "DSL should retain conservative doctrine inertia");

    const strategic_nexus::generated_overlay::DslParser parser;
    const auto parseResult = parser.parse(package.dslText);
    requireCondition(parseResult.ok, "generated DSL draft should parse");
    const strategic_nexus::generated_overlay::DslValidator validator;
    const auto validation = validator.validate(parseResult.program);
    requireCondition(validation.ok, "generated DSL draft should validate");

    const auto json = strategic_nexus::serializeSncDslDraftPackage(package);
    requireCondition(json.find("\"dsl_rule_count\": 1") != std::string::npos, "audit JSON should expose DSL rule count");
    requireCondition(json.find("\"overlay_compile_allowed\": false") != std::string::npos, "audit JSON should keep overlay compile disabled");
    requireCondition(json.find("\"publishes_overlay\": true") == std::string::npos, "audit JSON must never publish overlay in this slice");

    auto ineligibleSource = candidatePackage;
    ineligibleSource.candidateDecisions.front().empireState.parsed = false;
    const auto ineligiblePackage = builder.build(ineligibleSource, std::filesystem::path("dist/snc_candidate_decision_package.json"));
    requireCondition(!ineligiblePackage.ok, "missing parsed identity should block DSL draft");
    requireCondition(ineligiblePackage.readiness == "needs_identity", "missing parsed identity should request identity");
    requireCondition(ineligiblePackage.dslRuleCount == 0, "blocked identity should produce no DSL rules");

    std::cout << "SNC DSL draft package builder tests passed.\n";
    return 0;
}
