// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "StrategicRequest.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace strategic_nexus {

struct RunConfig {
    bool daemonMode = false;
    bool bridgePipelineMode = false;
    bool v0PipelineMode = false;
    bool v0PriorityQueueMode = false;
    bool generatedOverlayCompileMode = false;
    bool generatedOverlayVerifyMode = false;
    bool generatedOverlayPublishMode = false;
    bool archiveStableSavesMode = false;
    bool verifyAutosaveArchiveMode = false;
    bool summarizeAutosaveArchiveMode = false;
    bool buildSeasonDeltaLedgerMode = false;
    bool buildEmpireBriefFromArchiveMode = false;
    bool buildMinistryInputFromArchiveMode = false;
    bool v0PipelineFromArchiveMode = false;
    bool scanSaveCampaignsMode = false;
    bool diffSaveCampaignsMode = false;
    bool planCampaignLibraryMode = false;
    bool compileCampaignLibraryOverlayMode = false;
    bool discoverStellarisSaveRootsMode = false;
    bool exportMpOverlayPackageMode = false;
    bool verifyMpOverlayPackageMode = false;
    bool sncStatusSnapshotMode = false;
    bool offlineSpineMode = false;
    std::filesystem::path exchangeDirectory = "exchange";
    std::chrono::milliseconds daemonPollInterval = std::chrono::milliseconds(1000);
    int daemonMaxIterations = 0;
    std::filesystem::path bridgeDecisionPath;
    std::filesystem::path bridgeEffectBatchPath;
    std::filesystem::path bridgeSequenceStatePath;
    std::int64_t bridgeNowUnixMs = 0;
    std::filesystem::path v0InputContextPath;
    std::filesystem::path v0DecisionOutputPath;
    std::filesystem::path v0AuditOutputPath;
    std::filesystem::path v0PriorityQueueOutputPath;
    std::filesystem::path generatedOverlayDslInputPath;
    std::filesystem::path generatedOverlayOutputDirectory;
    std::filesystem::path generatedOverlayVerifyDirectory;
    std::filesystem::path generatedOverlayPublishStagedDirectory;
    std::filesystem::path generatedOverlayPublishActiveDirectory;
    std::filesystem::path autosaveArchiveSourceRoot;
    std::filesystem::path autosaveArchiveRoot;
    std::filesystem::path autosaveArchiveVerifyDirectory;
    std::filesystem::path autosaveArchiveSummaryDirectory;
    std::filesystem::path autosaveArchiveSummaryOutputPath;
    std::filesystem::path seasonDeltaLedgerDirectory;
    std::filesystem::path seasonDeltaLedgerOutputPath;
    std::filesystem::path archiveEmpireBriefDirectory;
    std::filesystem::path archiveEmpireBriefOutputPath;
    std::filesystem::path archiveMinistryInputDirectory;
    std::filesystem::path archiveMinistryInputOutputPath;
    std::filesystem::path archivePipelineDirectory;
    std::filesystem::path archivePipelineMinistryInputOutputPath;
    std::filesystem::path archivePipelineDecisionOutputPath;
    std::filesystem::path archivePipelineAuditOutputPath;
    std::string archiveMinistryInputCampaignId;
    std::string archivePipelineCampaignId;
    std::string seasonDeltaLedgerCampaignId;
    std::string archiveEmpireBriefCampaignId;
    std::string archiveEmpireBriefEmpireId;
    std::string archiveMinistryInputEmpireId;
    std::string archiveMinistryInputMinistry;
    std::string archivePipelineEmpireId;
    std::string archivePipelineMinistry;
    std::string autosaveArchiveSessionId;
    std::filesystem::path saveCampaignScanRoot;
    std::filesystem::path saveCampaignScanOutputPath;
    std::filesystem::path saveCampaignPreviousRoot;
    std::filesystem::path saveCampaignCurrentRoot;
    std::filesystem::path saveCampaignDiffOutputPath;
    std::filesystem::path campaignLibraryPlanRoot;
    std::filesystem::path campaignLibraryPlanOutputPath;
    std::filesystem::path campaignLibraryOverlayDslInputPath;
    std::filesystem::path campaignLibraryOverlaySaveRoot;
    std::filesystem::path campaignLibraryOverlayOutputDirectory;
    std::filesystem::path stellarisSaveRootsOutputPath;
    std::filesystem::path mpOverlaySourceDirectory;
    std::filesystem::path mpOverlayPackageDirectory;
    std::filesystem::path sncArchiveRoot;
    std::filesystem::path sncGeneratedOverlayDirectory;
    std::filesystem::path sncStatusOutputPath;
    std::filesystem::path offlineSpineArchiveDirectory;
    std::filesystem::path offlineSpineDslInputPath;
    std::filesystem::path offlineSpineWorkDirectory;
    std::filesystem::path offlineSpineGeneratedOverlayDirectory;
    std::filesystem::path offlineSpineStatusOutputPath;
    std::string mpOverlayCampaignId;
    std::string mpOverlayOverlayVersion;
    std::string mpOverlayGameVersion;
    std::string mpOverlayStrategicNexusModVersion;
    std::string offlineSpineCampaignId;
    std::string offlineSpineEmpireId;
    bool mpOverlayPreviousHostAvailable = true;
    bool sncStartWithWindowsEnabled = false;
    bool generatedOverlayPublishStellarisRunning = false;
    std::vector<std::filesystem::path> v0PriorityQueueInputPaths;
    std::int64_t v0SequenceId = 1;
    std::int64_t v0CreatedUnixMs = 0;
    std::int64_t v0TtlMs = 30000;
    std::int64_t campaignLibraryMaxCampaigns = 16;
    std::int64_t campaignLibraryOverlayMaxCampaigns = 16;
    std::int64_t autosaveArchiveStabilityDelayMs = 250;
    StrategicRequest request;
};

class Application {
public:
    int run(const RunConfig& config) const;
};

RunConfig parseRunConfig(int argc, char* argv[]);

} // namespace strategic_nexus

