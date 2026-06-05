// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace strategic_nexus {

struct CompanionStatusConfig {
    std::filesystem::path archiveRoot;
    std::filesystem::path generatedOverlayDirectory;
    std::filesystem::path mpOverlayPackageDirectory;
    bool startWithWindowsEnabled = false;
    bool useDetectedStellarisState = true;
    bool stellarisRunningOverride = false;
    std::filesystem::path gameplayAcceptanceReportPath =
        "dist/private_reports/generated_overlay_gameplay_acceptance_v0.json";
    std::filesystem::path generatedOverlayStagingStatusPath;
    std::filesystem::path generatedOverlayActiveDirectory;
    std::filesystem::path generatedOverlayPublishStatusPath;
    std::filesystem::path generatedOverlayPublishBackupRootDirectory;
    std::filesystem::path entryPointAnalysisPath =
        "dist/private_reports/snc_entry_point_analysis.json";
    std::filesystem::path postPlayPackagePath =
        "dist/private_reports/snc_post_play_package.json";
    std::filesystem::path decisionInputPackagePath =
        "dist/private_reports/snc_decision_input_package.json";
    std::filesystem::path candidateDecisionPackagePath =
        "dist/private_reports/snc_candidate_decision_package.json";
    std::filesystem::path dslDraftPath =
        "dist/private_reports/snc_validated_dsl_draft.dsl";
    std::filesystem::path dslDraftAuditPath =
        "dist/private_reports/snc_dsl_draft_package.json";
    std::filesystem::path postPlayGeneratedOverlayStagingStatusPath =
        "dist/private_reports/snc_generated_overlay_staging_status.json";
    std::filesystem::path mpOverlayPackageZipPath;
};

struct CompanionStatusLoopConfig {
    CompanionStatusConfig statusConfig;
    std::filesystem::path statusOutputPath;
    std::chrono::milliseconds pollInterval = std::chrono::milliseconds(1000);
    int maxIterations = 1;
};

struct CompanionStatusLoopResult {
    bool ok = false;
    int iterationsRun = 0;
    std::string reason;
};

struct CompanionLiveAutosaveMonitorConfig {
    std::vector<std::filesystem::path> saveRoots;
    std::filesystem::path archiveRoot;
    std::filesystem::path statusOutputPath;
    std::string sessionId;
    std::chrono::milliseconds pollInterval = std::chrono::milliseconds(1000);
    std::uint32_t stabilityDelayMs = 250;
    int maxIterations = 1;
    bool useDetectedStellarisState = true;
    bool stellarisRunningOverride = false;
    bool captureWhenStellarisNotRunning = false;
};

struct CompanionLiveAutosaveMonitorResult {
    bool ok = false;
    int iterationsRun = 0;
    std::size_t candidateRootCount = 0;
    std::size_t existingRootCount = 0;
    std::size_t copiedCount = 0;
    std::size_t skippedCount = 0;
    bool lastStellarisRunning = false;
    std::filesystem::path archiveSessionDirectory;
    std::filesystem::path statusOutputPath;
    bool statusOutputWritten = false;
    std::string reason;
};

struct CompanionSubsystemStatus {
    std::string state;
    std::string reason;
    std::filesystem::path path;
    std::string manifestHash;
    std::string reactivePolicyPackCapability;
    std::vector<std::string> eventFamilies;
    std::vector<std::string> sourceQualities;
    std::size_t bootstrapCampaignCount = 0;
};

struct CompanionMpOverlayPackageStatus {
    std::string state;
    std::string reason;
    std::filesystem::path path;
    std::string campaignId;
    std::string overlayVersion;
    std::string gameVersion;
    std::string strategicNexusModVersion;
    std::string handoffStatus;
    bool previousHostAvailable = false;
    bool previousHostAvailableKnown = false;
    std::string readiness;
    std::string hostReadiness;
    std::string clientReadinessGate;
    std::string hostNextStep;
    std::string clientNextStep;
    std::string packageManifestHash;
    std::string verifyCommand;
    std::string importCommand;
    std::string strictVerifyCommand;
    std::string strictImportCommand;
    std::string statusText;
    std::vector<std::string> warningCodes;
    std::size_t warningCount = 0;
    bool identityMismatchWarning = false;
    std::vector<std::string> identityMismatchWarningCodes;
    std::string mismatchWarningState = "no_mismatch";
    std::string mismatchWarningReason = "no_identity_mismatch_detected";
    std::vector<std::string> mismatchWarningCodes;
    std::string identityMismatchAlert;
    std::string packageZipState;
    std::string packageZipReason;
    std::filesystem::path packageZipPath;
    std::string packageZipHash;
    std::uintmax_t packageZipBytes = 0;
};

struct CompanionGeneratedOverlayPublishGateStatus {
    std::string state;
    std::string reason;
    std::filesystem::path path;
    std::filesystem::path stagingStatusPath;
    std::filesystem::path stagedOverlayDirectory;
    std::filesystem::path activeOverlayDirectory;
    std::filesystem::path publishStatusPath;
    std::filesystem::path backupRootDirectory;
    std::filesystem::path backupDirectory;
    std::string manifestHash;
    std::string publishedManifestHash;
    std::string publishCommand;
    std::size_t dslRuleCount = 0;
    std::size_t publishedFileCount = 0;
    bool ownerApprovalRequired = false;
    bool canPublish = false;
    bool activeOverlayExists = false;
    bool backupBeforeReplace = false;
    bool published = false;
    bool backupCreated = false;
};

struct CompanionPostPlayPipelineStatus {
    std::string state;
    std::string reason;
    std::filesystem::path entryPointAnalysisPath;
    std::string entryPointReadiness;
    std::string entryPointReason;
    std::size_t entryPointCount = 0;
    bool branchAmbiguityDetected = false;
    std::filesystem::path postPlayPackagePath;
    std::string postPlayPackageReadiness;
    std::string postPlayPackageReason;
    std::size_t postPlayDecisionReadyEntryCount = 0;
    std::size_t postPlayCampaignCount = 0;
    std::size_t postPlayReadyCampaignCount = 0;
    std::size_t postPlayPartialCampaignCount = 0;
    std::size_t postPlayBlockedCampaignCount = 0;
    std::vector<std::string> postPlayCampaignReadinessSummaries;
    std::filesystem::path decisionInputPackagePath;
    std::string decisionInputPackageReadiness;
    std::string decisionInputPackageReason;
    std::size_t decisionInputCount = 0;
    std::size_t decisionInputBlockedEntryCount = 0;
    std::filesystem::path candidateDecisionPackagePath;
    std::string candidateDecisionPackageReadiness;
    std::string candidateDecisionPackageReason;
    std::size_t candidateDecisionCount = 0;
    std::size_t candidateDecisionBlockedSourceEntryCount = 0;
    bool candidateDecisionValidatorPassed = false;
    std::filesystem::path dslDraftPath;
    std::filesystem::path dslDraftAuditPath;
    std::string dslDraftReadiness;
    std::string dslDraftReason;
    std::size_t dslDraftRuleCount = 0;
    std::size_t dslDraftEligibleCandidateCount = 0;
    std::size_t dslDraftSkippedCandidateCount = 0;
    bool dslDraftValidatorPassed = false;
    std::filesystem::path generatedOverlayStagingStatusPath;
    std::string generatedOverlayStagingReadiness;
    std::string generatedOverlayStagingReason;
    std::size_t generatedOverlayStagingRuleCount = 0;
    bool generatedOverlayManifestVerified = false;
    bool generatedOverlayPublishAllowed = false;
    std::filesystem::path campaignLibraryPlanPath;
    bool campaignLibraryPlanPresent = false;
    bool campaignLibraryLimitReached = false;
    std::size_t campaignLibrarySkippedDueToLimitCount = 0;
    std::string campaignLibraryPlanSource;
    std::string campaignLibraryPlanReadiness;
    std::string campaignLibraryPlanReason;
};

struct CompanionLifecycleStatus {
    bool startWithWindowsEnabled = false;
    std::string windowCloseBehavior = "minimize_to_tray";
    std::string explicitExitBehavior = "stop_without_restart";
    std::string crashRestartPolicy = "bounded_backoff_with_crash_loop_guard";
};

struct CompanionStatusSnapshot {
    std::string appName = "Strategic Nexus Companion";
    std::string abbreviation = "SNC";
    std::string generatedAtLocal;
    CompanionLifecycleStatus lifecycle;
    CompanionSubsystemStatus saveDiscovery;
    CompanionSubsystemStatus archive;
    CompanionSubsystemStatus generatedOverlay;
    CompanionGeneratedOverlayPublishGateStatus generatedOverlayPublishGate;
    CompanionMpOverlayPackageStatus mpOverlayPackage;
    CompanionPostPlayPipelineStatus postPlayPipeline;
    CompanionSubsystemStatus gameplayAcceptance;
    CompanionSubsystemStatus statusCenter;
    std::string nextAction;
    std::string nextActionReason;
    std::string nextActionCommandHint;
    std::string nextActionCommandHintSource;
    std::filesystem::path nextActionPath;
    std::string statusCenterSummaryText;
};

class StrategicNexusCompanion {
public:
    CompanionStatusSnapshot buildStatusSnapshot(const CompanionStatusConfig& config) const;
};

std::string serializeCompanionStatusSnapshot(const CompanionStatusSnapshot& snapshot);

CompanionStatusLoopResult runCompanionStatusLoop(const StrategicNexusCompanion& companion, const CompanionStatusLoopConfig& config);

CompanionLiveAutosaveMonitorResult runCompanionLiveAutosaveMonitor(const CompanionLiveAutosaveMonitorConfig& config);

} // namespace strategic_nexus
