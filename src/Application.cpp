// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "Application.h"

#include "ArchiveMinistryInputBuilder.h"
#include "AutosaveArchiver.h"
#include "AutosaveArchiveVerifier.h"
#include "AutosaveArchiveSummarizer.h"
#include "CampaignLibraryPlanner.h"
#include "CampaignSaveScanner.h"
#include "DaemonRunner.h"
#include "PostPlayPackageBuilder.h"
#include "RequestFileReader.h"
#include "SeasonDeltaLedgerBuilder.h"
#include "SeasonEmpireBriefBuilder.h"
#include "SaveParser.h"
#include "SaveEntryPointAnalyzer.h"
#include "SncCandidateDecisionPackageBuilder.h"
#include "SncDecisionInputPackageBuilder.h"
#include "SncDslDraftPackageBuilder.h"
#include "SncGeneratedOverlayPublishGate.h"
#include "SncGeneratedOverlayStager.h"
#include "StellarisProcessDetector.h"
#include "StellarisSavePathResolver.h"
#include "StrategicNexusCompanion.h"
#include "StrategicWorker.h"
#include "LocalLlmRuntimeAdapter.h"
#include "bridge_core/BridgeCorePipeline.h"
#include "common/FileUtil.h"
#include "generated_overlay/DslParser.h"
#include "generated_overlay/DslValidator.h"
#include "generated_overlay/GeneratedOverlayPublisher.h"
#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/MpOverlayPackage.h"
#include "generated_overlay/OverlayCompiler.h"
#include "strategic_pipeline/EmpireProcessingQueue.h"
#include "strategic_pipeline/MinistryInputReader.h"
#include "strategic_pipeline/PipelineJsonWriter.h"
#include "strategic_pipeline/V0StrategicPipeline.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace strategic_nexus {
namespace {

std::int64_t currentUnixMs()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::int64_t parseInt64Arg(const char* value, const std::int64_t fallback)
{
    try {
        return std::stoll(value);
    }
    catch (...) {
        return fallback;
    }
}

bool parseBoolArg(const char* value, const bool fallback)
{
    if (value == nullptr) {
        return fallback;
    }

    const std::string text = value;
    if (text == "true" || text == "1" || text == "yes") {
        return true;
    }
    if (text == "false" || text == "0" || text == "no") {
        return false;
    }
    return fallback;
}

std::string formatSessionToken(const std::chrono::system_clock::time_point now)
{
    const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (localtime_s(&localTime, &timeValue) != 0) {
        return "unknown-time";
    }

    std::ostringstream output;
    output << std::setw(4) << std::setfill('0') << (localTime.tm_year + 1900)
           << std::setw(2) << std::setfill('0') << (localTime.tm_mon + 1)
           << std::setw(2) << std::setfill('0') << localTime.tm_mday
           << "T"
           << std::setw(2) << std::setfill('0') << localTime.tm_hour
           << std::setw(2) << std::setfill('0') << localTime.tm_min
           << std::setw(2) << std::setfill('0') << localTime.tm_sec;
    return output.str();
}

std::string sanitizeCliValue(const std::string& value)
{
    std::ostringstream output;
    for (const char ch : value) {
        switch (ch) {
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                output << ' ';
            } else {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

bool hasNonWhitespace(const std::string& value)
{
    for (const char ch : value) {
        if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')) {
            return true;
        }
    }
    return false;
}

std::string readStatusTextField(const std::string& statusText, const std::string& key)
{
    if (key.empty()) {
        return std::string();
    }
    const std::string needle = key + ":";
    std::istringstream stream(statusText);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind(needle, 0) != 0) {
            continue;
        }
        std::string value = line.substr(needle.size());
        const auto first = value.find_first_not_of(" \t");
        if (first == std::string::npos) {
            return std::string();
        }
        value.erase(0, first);
        const auto last = value.find_last_not_of(" \t\r");
        if (last != std::string::npos) {
            value.erase(last + 1);
        }
        return value;
    }
    return std::string();
}

std::vector<std::string> readStatusTextFields(const std::string& statusText, const std::string& key)
{
    std::vector<std::string> values;
    if (key.empty()) {
        return values;
    }

    const std::string needle = key + ":";
    std::istringstream stream(statusText);
    std::set<std::string> seen;
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind(needle, 0) != 0) {
            continue;
        }

        std::string value = line.substr(needle.size());
        const auto first = value.find_first_not_of(" \t");
        if (first == std::string::npos) {
            continue;
        }
        value.erase(0, first);
        const auto last = value.find_last_not_of(" \t\r");
        if (last != std::string::npos) {
            value.erase(last + 1);
        }
        if (!value.empty() && seen.insert(value).second) {
            values.push_back(value);
        }
    }

    return values;
}

bool isIdentityMismatchWarningCode(const std::string& warningCode)
{
    return warningCode == "package_campaign_id_mismatch" ||
           warningCode == "package_overlay_version_mismatch" ||
           warningCode == "package_game_version_mismatch" ||
           warningCode == "package_mod_version_mismatch" ||
           warningCode == "package_manifest_hash_mismatch" ||
           warningCode == "mp_overlay_package_files_mismatch_manifest";
}

bool parseLatestArchivedSaveHeadline(
    const AutosaveArchiveSummary& summary,
    SaveParserSummary& outSummary)
{
    if (!summary.ok || summary.sessionArchiveDirectory.empty() || summary.lastArchivedPath.empty()) {
        return false;
    }

    const std::filesystem::path latestArchivedSave = summary.sessionArchiveDirectory / summary.lastArchivedPath;
    SaveParser parser;
    outSummary = parser.parseSummary(latestArchivedSave);
    return true;
}

} // namespace

RunConfig parseRunConfig(int argc, char* argv[])
{
    RunConfig config;

    if (argc > 1 && std::string(argv[1]) == "--bridge-pipeline") {
        config.bridgePipelineMode = true;

        if (argc > 2) {
            config.bridgeDecisionPath = argv[2];
        }

        if (argc > 3) {
            config.bridgeEffectBatchPath = argv[3];
        }

        if (argc > 4) {
            config.bridgeSequenceStatePath = argv[4];
        }

        config.bridgeNowUnixMs = argc > 5 ? parseInt64Arg(argv[5], currentUnixMs()) : currentUnixMs();
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--v0-pipeline") {
        config.v0PipelineMode = true;

        if (argc > 2) {
            config.v0InputContextPath = argv[2];
        }

        if (argc > 3) {
            config.v0DecisionOutputPath = argv[3];
        }

        config.v0SequenceId = argc > 4 ? parseInt64Arg(argv[4], 1) : 1;
        config.v0CreatedUnixMs = argc > 5 ? parseInt64Arg(argv[5], currentUnixMs()) : currentUnixMs();
        config.v0TtlMs = argc > 6 ? parseInt64Arg(argv[6], 30000) : 30000;
        if (argc > 7) {
            config.v0AuditOutputPath = argv[7];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--v0-priority-queue") {
        config.v0PriorityQueueMode = true;

        if (argc > 2) {
            config.v0PriorityQueueOutputPath = argv[2];
        }

        for (int index = 3; index < argc; ++index) {
            config.v0PriorityQueueInputPaths.push_back(argv[index]);
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--prepare-local-llm-model") {
        config.prepareLocalLlmModelMode = true;

        if (argc > 2) {
            config.localLlmPrepareModelId = argv[2];
        }
        if (argc > 3) {
            config.localLlmPrepareStateOutputPath = argv[3];
        }
        if (argc > 4) {
            config.localLlmPrepareUserLicenseAccepted = std::string(argv[4]) == "accept-license";
        }
        if (argc > 5) {
            const std::string value = argv[5];
            config.localLlmPrepareAllowDownload = value == "download" || parseBoolArg(argv[5], false);
        }
        if (argc > 6) {
            config.localLlmPrepareRuntimeUrl = argv[6];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--compile-generated-overlay") {
        config.generatedOverlayCompileMode = true;

        if (argc > 2) {
            config.generatedOverlayDslInputPath = argv[2];
        }

        if (argc > 3) {
            config.generatedOverlayOutputDirectory = argv[3];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--verify-generated-overlay") {
        config.generatedOverlayVerifyMode = true;

        if (argc > 2) {
            config.generatedOverlayVerifyDirectory = argv[2];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--export-mp-overlay-package") {
        config.exportMpOverlayPackageMode = true;

        if (argc > 2) {
            config.mpOverlaySourceDirectory = argv[2];
        }
        if (argc > 3) {
            config.mpOverlayCampaignId = argv[3];
        }
        if (argc > 4) {
            config.mpOverlayOverlayVersion = argv[4];
        }
        if (argc > 5) {
            config.mpOverlayGameVersion = argv[5];
        }
        if (argc > 6) {
            config.mpOverlayStrategicNexusModVersion = argv[6];
        }
        if (argc > 7) {
            config.mpOverlayPackageDirectory = argv[7];
        }
        config.mpOverlayPreviousHostAvailable = argc > 8 ? std::string(argv[8]) != "false" : true;
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--verify-mp-overlay-package") {
        config.verifyMpOverlayPackageMode = true;

        if (argc > 2) {
            config.mpOverlayPackageDirectory = argv[2];
        }
        if (argc > 3) {
            config.mpOverlayExpectedCampaignId = argv[3];
        }
        if (argc > 4) {
            config.mpOverlayExpectedOverlayVersion = argv[4];
        }
        if (argc > 5) {
            config.mpOverlayExpectedGameVersion = argv[5];
        }
        if (argc > 6) {
            config.mpOverlayExpectedStrategicNexusModVersion = argv[6];
        }
        if (argc > 7) {
            config.mpOverlayExpectedManifestHash = argv[7];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--import-mp-overlay-package") {
        config.importMpOverlayPackageMode = true;

        if (argc > 2) {
            config.mpOverlayPackageDirectory = argv[2];
        }
        if (argc > 3) {
            config.mpOverlayImportTargetDirectory = argv[3];
        }
        if (argc > 4) {
            config.mpOverlayExpectedCampaignId = argv[4];
        }
        if (argc > 5) {
            config.mpOverlayExpectedOverlayVersion = argv[5];
        }
        if (argc > 6) {
            config.mpOverlayExpectedGameVersion = argv[6];
        }
        if (argc > 7) {
            config.mpOverlayExpectedStrategicNexusModVersion = argv[7];
        }
        if (argc > 8) {
            config.mpOverlayExpectedManifestHash = argv[8];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--snc-status-snapshot") {
        config.sncStatusSnapshotMode = true;

        if (argc > 2) {
            config.sncArchiveRoot = argv[2];
        }
        if (argc > 3) {
            config.sncGeneratedOverlayDirectory = argv[3];
        }
        if (argc > 4) {
            config.sncStatusOutputPath = argv[4];
        }
        config.sncStartWithWindowsEnabled = argc > 5 ? std::string(argv[5]) == "true" : false;
        if (argc > 6) {
            const std::string value = argv[6];
            if (value == "true" || value == "false") {
                config.sncUseDetectedStellarisState = false;
                config.sncStellarisRunningOverride = value == "true";
            } else {
                config.mpOverlayPackageDirectory = argv[6];
            }
        }
        if (argc > 7) {
            const std::string value = argv[7];
            if (value == "true" || value == "false") {
                config.sncUseDetectedStellarisState = false;
                config.sncStellarisRunningOverride = value == "true";
            }
        }
        if (argc > 8) {
            config.sncGeneratedOverlayPublishStagingStatusPath = argv[8];
        }
        if (argc > 9) {
            config.sncGeneratedOverlayPublishActiveDirectory = argv[9];
        }
        if (argc > 10) {
            config.sncGeneratedOverlayPublishStatusOutputPath = argv[10];
        }
        if (argc > 11) {
            config.sncGeneratedOverlayPublishBackupRootDirectory = argv[11];
        }
        if (argc > 12) {
            config.sncMpOverlayPackageZipPath = argv[12];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--snc-live-autosave-monitor") {
        config.sncLiveAutosaveMonitorMode = true;

        if (argc > 2) {
            const std::string saveRoot = argv[2];
            if (saveRoot != "auto" && saveRoot != "AUTO") {
                config.sncLiveAutosaveSaveRoots.push_back(argv[2]);
            }
        }
        if (argc > 3) {
            config.sncLiveAutosaveArchiveRoot = argv[3];
        }
        if (argc > 4) {
            config.sncLiveAutosaveSessionId = argv[4];
        }
        config.sncLiveAutosavePollIntervalMs = argc > 5 ? parseInt64Arg(argv[5], 1000) : 1000;
        config.sncLiveAutosaveStabilityDelayMs = argc > 6 ? parseInt64Arg(argv[6], 250) : 250;
        config.sncLiveAutosaveMaxIterations = argc > 7 ? parseInt64Arg(argv[7], 1) : 1;
        config.sncLiveAutosaveUseDetectedStellarisState = argc > 8 ? parseBoolArg(argv[8], true) : true;
        config.sncLiveAutosaveStellarisRunningOverride = argc > 9 ? parseBoolArg(argv[9], false) : false;
        config.sncLiveAutosaveCaptureWhenStellarisNotRunning = argc > 10 ? parseBoolArg(argv[10], false) : false;
        if (argc > 11) {
            config.sncLiveAutosaveStatusOutputPath = argv[11];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--run-snc-session-capture") {
        config.sncLiveAutosaveMonitorMode = true;
        config.sncSessionCaptureMode = true;
        config.sncLiveAutosaveArchiveRoot = "dist/snc_live_autosave_archive";
        config.sncLiveAutosaveStatusOutputPath = "dist/private_reports/snc_live_autosave_monitor_status.json";
        config.sncLiveAutosaveSessionId = "snc-session-" + formatSessionToken(std::chrono::system_clock::now());
        config.sncLiveAutosavePollIntervalMs = 1000;
        config.sncLiveAutosaveStabilityDelayMs = 250;
        config.sncLiveAutosaveMaxIterations = 0;
        config.sncLiveAutosaveUseDetectedStellarisState = true;
        config.sncLiveAutosaveCaptureWhenStellarisNotRunning = false;

        if (argc > 2) {
            config.sncLiveAutosaveArchiveRoot = argv[2];
        }
        if (argc > 3) {
            config.sncLiveAutosaveStatusOutputPath = argv[3];
        }
        if (argc > 4) {
            config.sncLiveAutosaveSessionId = argv[4];
        }
        config.sncLiveAutosavePollIntervalMs = argc > 5 ? parseInt64Arg(argv[5], config.sncLiveAutosavePollIntervalMs) : config.sncLiveAutosavePollIntervalMs;
        config.sncLiveAutosaveStabilityDelayMs = argc > 6 ? parseInt64Arg(argv[6], config.sncLiveAutosaveStabilityDelayMs) : config.sncLiveAutosaveStabilityDelayMs;
        config.sncLiveAutosaveMaxIterations = argc > 7 ? parseInt64Arg(argv[7], config.sncLiveAutosaveMaxIterations) : config.sncLiveAutosaveMaxIterations;
        config.sncLiveAutosaveCaptureWhenStellarisNotRunning = argc > 8 ? parseBoolArg(argv[8], false) : false;
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--publish-generated-overlay") {
        config.generatedOverlayPublishMode = true;

        if (argc > 2) {
            config.generatedOverlayPublishStagedDirectory = argv[2];
        }
        if (argc > 3) {
            config.generatedOverlayPublishActiveDirectory = argv[3];
        }
        if (argc > 4) {
            const std::string value = argv[4];
            if (value == "true" || value == "false") {
                config.generatedOverlayPublishUseDetectedStellarisState = false;
                config.generatedOverlayPublishStellarisRunning = value == "true";
            }
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--detect-stellaris-running") {
        config.detectStellarisRunningMode = true;

        if (argc > 2) {
            config.stellarisRunningOutputPath = argv[2];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--parse-stellaris-save") {
        config.parseStellarisSaveMode = true;

        if (argc > 2) {
            config.stellarisSaveParseInputPath = argv[2];
        }
        if (argc > 3) {
            config.stellarisSaveParseOutputPath = argv[3];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--analyze-save-entry-points") {
        config.analyzeSaveEntryPointsMode = true;

        if (argc > 2) {
            config.saveEntryPointArchiveDirectory = argv[2];
        }
        if (argc > 3) {
            config.saveEntryPointAnalysisOutputPath = argv[3];
        }
        for (int index = 4; index < argc; ++index) {
            config.saveEntryPointRoots.push_back(argv[index]);
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--build-post-play-package") {
        config.buildPostPlayPackageMode = true;

        if (argc > 2) {
            config.postPlayPackageArchiveDirectory = argv[2];
        }
        if (argc > 3) {
            config.postPlayPackageOutputPath = argv[3];
        }
        for (int index = 4; index < argc; ++index) {
            config.postPlayPackageSaveRoots.push_back(argv[index]);
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--build-snc-decision-input-package") {
        config.buildSncDecisionInputPackageMode = true;

        if (argc > 2) {
            config.sncDecisionInputPostPlayPackagePath = argv[2];
        }
        if (argc > 3) {
            config.sncDecisionInputOutputPath = argv[3];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--build-snc-candidate-decision-package") {
        config.buildSncCandidateDecisionPackageMode = true;

        if (argc > 2) {
            config.sncCandidateDecisionInputPackagePath = argv[2];
        }
        if (argc > 3) {
            config.sncCandidateDecisionOutputPath = argv[3];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--build-snc-dsl-draft-package") {
        config.buildSncDslDraftPackageMode = true;

        if (argc > 2) {
            config.sncDslDraftCandidatePackagePath = argv[2];
        }
        if (argc > 3) {
            config.sncDslDraftOutputPath = argv[3];
        }
        if (argc > 4) {
            config.sncDslDraftAuditOutputPath = argv[4];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--stage-snc-generated-overlay") {
        config.stageSncGeneratedOverlayMode = true;

        if (argc > 2) {
            config.sncGeneratedOverlayDslPath = argv[2];
        }
        if (argc > 3) {
            config.sncGeneratedOverlayStagingDirectory = argv[3];
        }
        if (argc > 4) {
            config.sncGeneratedOverlayStagingStatusOutputPath = argv[4];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--publish-snc-generated-overlay") {
        config.publishSncGeneratedOverlayMode = true;

        if (argc > 2) {
            config.sncGeneratedOverlayPublishStagingStatusPath = argv[2];
        }
        if (argc > 3) {
            config.sncGeneratedOverlayPublishActiveDirectory = argv[3];
        }
        if (argc > 4) {
            config.sncGeneratedOverlayPublishStatusOutputPath = argv[4];
        }
        if (argc > 5) {
            config.sncGeneratedOverlayOwnerApprovalToken = argv[5];
        }
        if (argc > 6) {
            const std::string value = argv[6];
            if (value == "true" || value == "false") {
                config.sncGeneratedOverlayPublishUseDetectedStellarisState = false;
                config.sncGeneratedOverlayPublishStellarisRunning = value == "true";
            } else {
                config.sncGeneratedOverlayPublishBackupRootDirectory = argv[6];
            }
        }
        if (argc > 7) {
            config.sncGeneratedOverlayPublishBackupRootDirectory = argv[7];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--run-offline-spine") {
        config.offlineSpineMode = true;

        if (argc > 2) {
            config.offlineSpineArchiveDirectory = argv[2];
        }
        if (argc > 3) {
            config.offlineSpineCampaignId = argv[3];
        }
        if (argc > 4) {
            config.offlineSpineEmpireId = argv[4];
        }
        if (argc > 5) {
            config.offlineSpineDslInputPath = argv[5];
        }
        if (argc > 6) {
            config.offlineSpineWorkDirectory = argv[6];
        }
        if (argc > 7) {
            config.offlineSpineGeneratedOverlayDirectory = argv[7];
        }
        if (argc > 8) {
            config.offlineSpineStatusOutputPath = argv[8];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--archive-stable-saves") {
        config.archiveStableSavesMode = true;

        if (argc > 2) {
            config.autosaveArchiveSourceRoot = argv[2];
        }

        if (argc > 3) {
            config.autosaveArchiveRoot = argv[3];
        }

        if (argc > 4) {
            config.autosaveArchiveSessionId = argv[4];
        }

        config.autosaveArchiveStabilityDelayMs = argc > 5 ? parseInt64Arg(argv[5], 250) : 250;
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--archive-live-saves") {
        config.archiveLiveSavesMode = true;

        if (argc > 2) {
            config.autosaveArchiveSourceRoot = argv[2];
        }

        if (argc > 3) {
            config.autosaveArchiveRoot = argv[3];
        }

        if (argc > 4) {
            config.autosaveArchiveSessionId = argv[4];
        }

        config.autosaveArchiveStabilityDelayMs = argc > 5 ? parseInt64Arg(argv[5], 250) : 250;
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--verify-autosave-archive") {
        config.verifyAutosaveArchiveMode = true;

        if (argc > 2) {
            config.autosaveArchiveVerifyDirectory = argv[2];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--summarize-autosave-archive") {
        config.summarizeAutosaveArchiveMode = true;

        if (argc > 2) {
            config.autosaveArchiveSummaryDirectory = argv[2];
        }

        if (argc > 3) {
            config.autosaveArchiveSummaryOutputPath = argv[3];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--build-ministry-input-from-archive") {
        config.buildMinistryInputFromArchiveMode = true;

        if (argc > 2) {
            config.archiveMinistryInputDirectory = argv[2];
        }

        if (argc > 3) {
            config.archiveMinistryInputCampaignId = argv[3];
        }

        if (argc > 4) {
            config.archiveMinistryInputEmpireId = argv[4];
        }

        if (argc > 5) {
            config.archiveMinistryInputMinistry = argv[5];
        }

        if (argc > 6) {
            config.archiveMinistryInputOutputPath = argv[6];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--v0-pipeline-from-archive") {
        config.v0PipelineFromArchiveMode = true;

        if (argc > 2) {
            config.archivePipelineDirectory = argv[2];
        }

        if (argc > 3) {
            config.archivePipelineCampaignId = argv[3];
        }

        if (argc > 4) {
            config.archivePipelineEmpireId = argv[4];
        }

        if (argc > 5) {
            config.archivePipelineMinistry = argv[5];
        }

        if (argc > 6) {
            config.archivePipelineMinistryInputOutputPath = argv[6];
        }

        if (argc > 7) {
            config.archivePipelineDecisionOutputPath = argv[7];
        }

        config.v0SequenceId = argc > 8 ? parseInt64Arg(argv[8], 1) : 1;
        config.v0CreatedUnixMs = argc > 9 ? parseInt64Arg(argv[9], currentUnixMs()) : currentUnixMs();
        config.v0TtlMs = argc > 10 ? parseInt64Arg(argv[10], 30000) : 30000;
        if (argc > 11) {
            config.archivePipelineAuditOutputPath = argv[11];
        }
        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--build-season-delta-ledger") {
        config.buildSeasonDeltaLedgerMode = true;

        if (argc > 2) {
            config.seasonDeltaLedgerDirectory = argv[2];
        }

        if (argc > 3) {
            config.seasonDeltaLedgerCampaignId = argv[3];
        }

        if (argc > 4) {
            config.seasonDeltaLedgerOutputPath = argv[4];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--build-empire-brief-from-archive") {
        config.buildEmpireBriefFromArchiveMode = true;

        if (argc > 2) {
            config.archiveEmpireBriefDirectory = argv[2];
        }

        if (argc > 3) {
            config.archiveEmpireBriefCampaignId = argv[3];
        }

        if (argc > 4) {
            config.archiveEmpireBriefEmpireId = argv[4];
        }

        if (argc > 5) {
            config.archiveEmpireBriefOutputPath = argv[5];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--scan-save-campaigns") {
        config.scanSaveCampaignsMode = true;

        if (argc > 2) {
            config.saveCampaignScanRoot = argv[2];
        }

        if (argc > 3) {
            config.saveCampaignScanOutputPath = argv[3];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--diff-save-campaigns") {
        config.diffSaveCampaignsMode = true;

        if (argc > 2) {
            config.saveCampaignPreviousRoot = argv[2];
        }

        if (argc > 3) {
            config.saveCampaignCurrentRoot = argv[3];
        }

        if (argc > 4) {
            config.saveCampaignDiffOutputPath = argv[4];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--plan-campaign-library") {
        config.planCampaignLibraryMode = true;

        if (argc > 2) {
            config.campaignLibraryPlanRoot = argv[2];
        }

        config.campaignLibraryMaxCampaigns = argc > 3 ? parseInt64Arg(argv[3], 16) : 16;

        if (argc > 4) {
            config.campaignLibraryPlanOutputPath = argv[4];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--compile-campaign-library-overlay") {
        config.compileCampaignLibraryOverlayMode = true;

        if (argc > 2) {
            config.campaignLibraryOverlayDslInputPath = argv[2];
        }

        if (argc > 3) {
            config.campaignLibraryOverlaySaveRoot = argv[3];
        }

        config.campaignLibraryOverlayMaxCampaigns = argc > 4 ? parseInt64Arg(argv[4], 16) : 16;

        if (argc > 5) {
            config.campaignLibraryOverlayOutputDirectory = argv[5];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--discover-stellaris-save-roots") {
        config.discoverStellarisSaveRootsMode = true;

        if (argc > 2) {
            config.stellarisSaveRootsOutputPath = argv[2];
        }

        return config;
    }

    if (argc > 1 && std::string(argv[1]) == "--daemon") {
        config.daemonMode = true;

        if (argc > 2) {
            config.exchangeDirectory = argv[2];
        }

        if (argc > 3) {
            config.daemonMaxIterations = std::stoi(argv[3]);
        }

        return config;
    }

    if (argc > 2 && std::string(argv[1]) == "--request") {
        const RequestFileReader reader;
        config.request = reader.read(argv[2]);
        return config;
    }

    if (argc > 1) {
        config.request.snapshotPath = argv[1];
    }

    if (argc > 2) {
        config.request.outputDoctrinePath = argv[2];
    }

    if (argc > 3) {
        config.request.trigger = requestTriggerFromString(argv[3]);
    }

    return config;
}

int Application::run(const RunConfig& config) const
{
    try {
        if (config.daemonMode) {
            const DaemonRunner daemon;
            return daemon.run({
                config.exchangeDirectory,
                config.daemonPollInterval,
                config.daemonMaxIterations
            });
        }

        if (config.prepareLocalLlmModelMode) {
            const auto result = prepareLocalLlmModel({
                config.localLlmPrepareModelId,
                config.localLlmPrepareStateOutputPath,
                config.localLlmPrepareRuntimeUrl,
                config.localLlmPrepareUserLicenseAccepted,
                config.localLlmPrepareAllowDownload
            });

            std::cout << "local_llm_prepare_ok=" << (result.ok ? "true" : "false") << "\n";
            std::cout << "local_llm_prepare_state=" << sanitizeCliValue(result.state) << "\n";
            std::cout << "local_llm_prepare_reason=" << sanitizeCliValue(result.reason) << "\n";
            std::cout << "local_llm_prepare_model_id=" << sanitizeCliValue(result.modelId) << "\n";
            std::cout << "local_llm_prepare_runtime=" << sanitizeCliValue(result.runtime) << "\n";
            std::cout << "local_llm_prepare_runtime_model_name=" << sanitizeCliValue(result.runtimeModelName) << "\n";
            std::cout << "local_llm_prepare_runtime_url=" << sanitizeCliValue(result.runtimeUrl) << "\n";
            std::cout << "local_llm_prepare_runtime_available=" << (result.runtimeAvailable ? "true" : "false") << "\n";
            std::cout << "local_llm_prepare_runtime_model_present=" << (result.runtimeModelPresent ? "true" : "false") << "\n";
            std::cout << "local_llm_prepare_download_attempted=" << (result.downloadAttempted ? "true" : "false") << "\n";
            std::cout << "local_llm_prepare_download_succeeded=" << (result.downloadSucceeded ? "true" : "false") << "\n";
            std::cout << "local_llm_prepare_state_written=" << (result.stateWritten ? "true" : "false") << "\n";
            std::cout << "local_llm_prepare_state_output_path=" << sanitizeCliValue(result.stateOutputPath.generic_string()) << "\n";
            return result.ok ? 0 : 2;
        }

        if (config.bridgePipelineMode) {
            const bridge_core::BridgeCorePipeline pipeline;
            const bridge_core::BridgeCorePipelineResult result = pipeline.processOnce({
                config.bridgeDecisionPath,
                config.bridgeEffectBatchPath,
                config.bridgeSequenceStatePath,
                config.bridgeNowUnixMs
            });

            std::cout << "pipeline_accepted=" << (result.accepted ? "true" : "false") << "\n";
            std::cout << "pipeline_reason=" << sanitizeCliValue(result.reason) << "\n";
            std::cout << "pipeline_sequence_id=" << result.sequenceId << "\n";
            std::cout << "pipeline_previous_sequence_id=" << result.previousSequenceId << "\n";
            std::cout << "pipeline_current_sequence_id=" << result.currentSequenceId << "\n";
            std::cout << "pipeline_effect_batch_written=" << (result.effectBatchWritten ? "true" : "false") << "\n";
            std::cout << "pipeline_sequence_advanced=" << (result.sequenceAdvanced ? "true" : "false") << "\n";
            return 0;
        }

        if (config.v0PipelineMode) {
            const strategic_pipeline::V0StrategicPipeline pipeline;
            const strategic_pipeline::PipelineRunResult result = pipeline.run({
                config.v0InputContextPath,
                config.v0DecisionOutputPath,
                config.v0AuditOutputPath,
                config.v0SequenceId,
                config.v0CreatedUnixMs,
                config.v0TtlMs
            });

            std::cout << "v0_pipeline_success=" << (result.success ? "true" : "false") << "\n";
            std::cout << "v0_pipeline_reason=" << sanitizeCliValue(result.reason) << "\n";
            std::cout << "v0_pipeline_ministry=" << result.input.ministry << "\n";
            std::cout << "v0_pipeline_campaign_id=" << result.payload.campaignId << "\n";
            std::cout << "v0_pipeline_empire_id=" << result.payload.empireId << "\n";
            std::cout << "v0_pipeline_military_posture=" << result.payload.militaryPosture << "\n";
            std::cout << "v0_pipeline_research_bias=" << result.payload.researchBias << "\n";
            std::cout << "v0_pipeline_confidence=" << result.payload.confidence << "\n";
            std::cout << "v0_pipeline_decision_written=" << (result.decisionOutputWritten ? "true" : "false") << "\n";
            std::cout << "v0_pipeline_audit_requested=" << (result.auditOutputRequested ? "true" : "false") << "\n";
            std::cout << "v0_pipeline_audit_written=" << (result.auditOutputWritten ? "true" : "false") << "\n";
            if (!result.auditReason.empty()) {
                std::cout << "v0_pipeline_audit_reason=" << sanitizeCliValue(result.auditReason) << "\n";
            }
            return result.success ? 0 : 1;
        }

        if (config.v0PriorityQueueMode) {
            std::vector<strategic_pipeline::MinistryInputContext> inputs;
            std::vector<std::string> invalidInputs;
            strategic_pipeline::MinistryInputReader reader;

            for (const auto& inputPath : config.v0PriorityQueueInputPaths) {
                const strategic_pipeline::MinistryInputReadResult readResult = reader.read(inputPath);
                if (readResult.ok) {
                    inputs.push_back(readResult.input);
                } else {
                    invalidInputs.push_back(inputPath.string() + ": " + readResult.error);
                }
            }

            const strategic_pipeline::EmpireProcessingQueue queue;
            const auto entries = queue.build(std::move(inputs));
            const bool written = common::writeTextFileAtomically(
                config.v0PriorityQueueOutputPath,
                strategic_pipeline::serializeProcessingQueue(entries, invalidInputs));

            std::cout << "v0_priority_queue_success=" << (written && !entries.empty() ? "true" : "false") << "\n";
            std::cout << "v0_priority_queue_entries=" << entries.size() << "\n";
            std::cout << "v0_priority_queue_invalid_inputs=" << invalidInputs.size() << "\n";
            std::cout << "v0_priority_queue_output_written=" << (written ? "true" : "false") << "\n";

            return written && !entries.empty() ? 0 : 1;
        }

        if (config.generatedOverlayCompileMode) {
            if (config.generatedOverlayDslInputPath.empty() || config.generatedOverlayOutputDirectory.empty()) {
                std::cout << "generated_overlay_success=false\n";
                std::cout << "generated_overlay_reason=missing input or output path\n";
                return 1;
            }

            if (!std::filesystem::exists(config.generatedOverlayDslInputPath)) {
                std::cout << "generated_overlay_success=false\n";
                std::cout << "generated_overlay_reason=missing DSL input\n";
                return 1;
            }

            const auto outputDirectory = config.generatedOverlayOutputDirectory;
            {
                std::error_code ec;
                if (std::filesystem::exists(outputDirectory, ec)) {
                    if (ec) {
                        std::cout << "generated_overlay_success=false\n";
                        std::cout << "generated_overlay_reason=failed to inspect output directory\n";
                        return 1;
                    }

                    if (!std::filesystem::is_directory(outputDirectory, ec) || ec) {
                        std::cout << "generated_overlay_success=false\n";
                        std::cout << "generated_overlay_reason=output path is not a directory\n";
                        return 1;
                    }

                    if (!std::filesystem::is_empty(outputDirectory, ec) || ec) {
                        std::cout << "generated_overlay_success=false\n";
                        std::cout << "generated_overlay_reason=output directory must be empty\n";
                        return 1;
                    }
                }
            }

            const generated_overlay::DslParser parser;
            const auto parseResult = parser.parse(common::readTextFile(config.generatedOverlayDslInputPath));
            if (!parseResult.ok) {
                std::cout << "generated_overlay_success=false\n";
                std::cout << "generated_overlay_reason=parse failed: " << sanitizeCliValue(parseResult.error) << "\n";
                return 1;
            }

            const generated_overlay::DslValidator validator;
            const auto validation = validator.validate(parseResult.program);
            if (!validation.ok) {
                std::cout << "generated_overlay_success=false\n";
                std::cout << "generated_overlay_reason=validation failed\n";
                for (const auto& error : validation.errors) {
                    std::cout << "generated_overlay_error=" << sanitizeCliValue(error) << "\n";
                }
                return 1;
            }

            const auto runtimeConditionErrors =
                generated_overlay::findUnsupportedRuntimeConditionErrors(parseResult.program);
            if (!runtimeConditionErrors.empty()) {
                std::cout << "generated_overlay_success=false\n";
                std::cout << "generated_overlay_reason=runtime conditions unsupported\n";
                for (const auto& error : runtimeConditionErrors) {
                    std::cout << "generated_overlay_error=" << sanitizeCliValue(error) << "\n";
                }
                return 1;
            }

            const generated_overlay::OverlayCompiler compiler;
            const auto files = compiler.compile(parseResult.program);
            {
                std::error_code ec;
                std::filesystem::create_directories(outputDirectory, ec);
                if (ec) {
                    std::cout << "generated_overlay_success=false\n";
                    std::cout << "generated_overlay_reason=failed to create output directory\n";
                    return 1;
                }
            }
            const bool eventsWritten = common::writeTextFileAtomically(
                outputDirectory / "events" / "strategic_nexus_generated_events.txt",
                files.eventsText);
            const bool effectsWritten = common::writeTextFileAtomically(
                outputDirectory / "common" / "scripted_effects" / "strategic_nexus_generated_effects.txt",
                files.scriptedEffectsText);
            const bool triggersWritten = common::writeTextFileAtomically(
                outputDirectory / "common" / "scripted_triggers" / "strategic_nexus_generated_triggers.txt",
                files.scriptedTriggersText);
            const bool manifestWritten = common::writeTextFileAtomically(
                outputDirectory / "strategic_nexus_generated_manifest.json",
                files.manifestText);

            const bool success = eventsWritten && effectsWritten && triggersWritten && manifestWritten;
            std::cout << "generated_overlay_success=" << (success ? "true" : "false") << "\n";
            std::cout << "generated_overlay_rule_count=" << parseResult.program.rules.size() << "\n";
            std::cout << "generated_overlay_output_path=" << sanitizeCliValue(outputDirectory.generic_string()) << "\n";
            std::cout << "generated_overlay_events_written=" << (eventsWritten ? "true" : "false") << "\n";
            std::cout << "generated_overlay_effects_written=" << (effectsWritten ? "true" : "false") << "\n";
            std::cout << "generated_overlay_triggers_written=" << (triggersWritten ? "true" : "false") << "\n";
            std::cout << "generated_overlay_manifest_written=" << (manifestWritten ? "true" : "false") << "\n";
            if (!success) {
                std::cout << "generated_overlay_reason=failed to write generated overlay files\n";
            }
            return success ? 0 : 1;
        }

        if (config.generatedOverlayVerifyMode) {
            const generated_overlay::ManifestVerifier verifier;
            const auto result = verifier.verify(config.generatedOverlayVerifyDirectory);

            std::cout << "generated_overlay_manifest_ok=" << (result.ok ? "true" : "false") << "\n";
            std::cout << "generated_overlay_manifest_reason=" << sanitizeCliValue(result.reason) << "\n";
            if (!result.manifestHash.empty()) {
                std::cout << "generated_overlay_manifest_hash=" << sanitizeCliValue(result.manifestHash) << "\n";
            }
            std::cout << "generated_overlay_manifest_file_count=" << result.files.size() << "\n";
            for (const auto& file : result.files) {
                std::cout << "generated_overlay_manifest_file=" << sanitizeCliValue(file.path)
                          << ";exists=" << (file.exists ? "true" : "false")
                          << ";hash_matches=" << (file.hashMatches ? "true" : "false")
                          << ";byte_count_matches=" << (file.byteCountMatches ? "true" : "false")
                          << "\n";
            }
            return result.ok ? 0 : 1;
        }

        if (config.generatedOverlayPublishMode) {
            bool stellarisRunning = config.generatedOverlayPublishStellarisRunning;
            bool detectionAvailable = false;
            std::string stellarisRunningReason =
                config.generatedOverlayPublishStellarisRunning ? "explicit true" : "explicit false";
            if (config.generatedOverlayPublishUseDetectedStellarisState) {
                const StellarisProcessDetector detector;
                const auto processStatus = detector.detectFromSystem();
                stellarisRunning = processStatus.running;
                detectionAvailable = processStatus.detectionAvailable;
                stellarisRunningReason = processStatus.reason;
                if (!processStatus.detectionAvailable) {
                    stellarisRunning = true;
                    stellarisRunningReason += "; publish blocked because process detection is unavailable";
                }
            }

            const generated_overlay::GeneratedOverlayPublisher publisher;
            const auto result = publisher.publish(
                config.generatedOverlayPublishStagedDirectory,
                config.generatedOverlayPublishActiveDirectory,
                stellarisRunning);

            std::cout << "generated_overlay_publish_ok=" << (result.ok ? "true" : "false") << "\n";
            std::cout << "generated_overlay_publish_reason=" << sanitizeCliValue(result.reason) << "\n";
            std::cout << "generated_overlay_publish_stellaris_detection_available="
                      << (detectionAvailable ? "true" : "false") << "\n";
            std::cout << "generated_overlay_publish_stellaris_running=" << (stellarisRunning ? "true" : "false") << "\n";
            std::cout << "generated_overlay_publish_stellaris_reason="
                      << sanitizeCliValue(stellarisRunningReason) << "\n";
            std::cout << "generated_overlay_publish_source_path="
                      << sanitizeCliValue(result.sourceDirectory.generic_string()) << "\n";
            std::cout << "generated_overlay_publish_active_path="
                      << sanitizeCliValue(result.activeDirectory.generic_string()) << "\n";
            if (!result.sourceManifestHash.empty()) {
                std::cout << "generated_overlay_publish_source_manifest_hash="
                          << sanitizeCliValue(result.sourceManifestHash) << "\n";
            }
            if (!result.publishedManifestHash.empty()) {
                std::cout << "generated_overlay_publish_manifest_hash="
                          << sanitizeCliValue(result.publishedManifestHash) << "\n";
            }
            std::cout << "generated_overlay_publish_file_count=" << result.files.size() << "\n";
            for (const auto& file : result.files) {
                std::cout << "generated_overlay_publish_file=" << sanitizeCliValue(file.path)
                          << ";exists=" << (file.exists ? "true" : "false")
                          << ";hash_matches=" << (file.hashMatches ? "true" : "false")
                          << ";byte_count_matches=" << (file.byteCountMatches ? "true" : "false")
                          << "\n";
            }
            return result.ok ? 0 : 1;
        }

        if (config.exportMpOverlayPackageMode) {
            const generated_overlay::MpOverlayPackageExporter exporter;
            const auto result = exporter.exportPackage(
                config.mpOverlaySourceDirectory,
                config.mpOverlayCampaignId,
                config.mpOverlayOverlayVersion,
                config.mpOverlayGameVersion,
                config.mpOverlayStrategicNexusModVersion,
                config.mpOverlayPackageDirectory,
                config.mpOverlayPreviousHostAvailable);

            std::cout << "mp_overlay_package_export_ok=" << (result.ok ? "true" : "false") << "\n";
            std::cout << "mp_overlay_package_export_reason=" << sanitizeCliValue(result.reason) << "\n";
            if (result.ok) {
                const generated_overlay::MpOverlayPackageVerifier verifier;
                const auto verificationResult = verifier.verify(config.mpOverlayPackageDirectory);
                std::vector<std::string> exportWarnings;
                bool hasExportFileHashMismatch = false;
                bool hasExportFileByteCountMismatch = false;
                bool hasExportMissingExpectedFiles = false;
                for (const auto& file : verificationResult.files) {
                    if (!file.exists) {
                        hasExportMissingExpectedFiles = true;
                    }
                    if (!file.hashMatches) {
                        hasExportFileHashMismatch = true;
                    }
                    if (!file.byteCountMatches) {
                        hasExportFileByteCountMismatch = true;
                    }
                }
                if (hasExportMissingExpectedFiles) {
                    exportWarnings.push_back("missing_expected_files");
                }
                if (hasExportFileHashMismatch) {
                    exportWarnings.push_back("generated_file_hash_mismatch");
                }
                if (hasExportFileByteCountMismatch) {
                    exportWarnings.push_back("generated_file_byte_count_mismatch");
                }
                if (!verificationResult.unexpectedFiles.empty()) {
                    exportWarnings.push_back("unexpected_files_present");
                }
                if (!verificationResult.ok && verificationResult.readiness == "not_ready" &&
                    !verificationResult.statusText.empty()) {
                    exportWarnings.push_back("package_identity_or_version_mismatch");
                }
                if (!verificationResult.campaignId.empty()) {
                    std::cout << "mp_overlay_package_export_campaign_id="
                              << sanitizeCliValue(verificationResult.campaignId) << "\n";
                }
                if (!verificationResult.overlayVersion.empty()) {
                    std::cout << "mp_overlay_package_export_overlay_version="
                              << sanitizeCliValue(verificationResult.overlayVersion) << "\n";
                }
                if (!verificationResult.gameVersion.empty()) {
                    std::cout << "mp_overlay_package_export_game_version="
                              << sanitizeCliValue(verificationResult.gameVersion) << "\n";
                }
                if (!verificationResult.strategicNexusModVersion.empty()) {
                    std::cout << "mp_overlay_package_export_strategic_nexus_mod_version="
                              << sanitizeCliValue(verificationResult.strategicNexusModVersion) << "\n";
                }
                if (!verificationResult.handoffStatus.empty()) {
                    std::cout << "mp_overlay_package_export_handoff_status="
                              << sanitizeCliValue(verificationResult.handoffStatus) << "\n";
                }
                if (!verificationResult.readiness.empty()) {
                    std::cout << "mp_overlay_package_export_readiness="
                              << sanitizeCliValue(verificationResult.readiness) << "\n";
                }
                if (!verificationResult.packageManifestHash.empty()) {
                    std::cout << "mp_overlay_package_export_manifest_hash="
                              << sanitizeCliValue(verificationResult.packageManifestHash) << "\n";
                }
                if (!verificationResult.statusText.empty()) {
                    std::cout << "mp_overlay_package_export_status_text="
                              << sanitizeCliValue(verificationResult.statusText) << "\n";
                    const auto verifyCommand = readStatusTextField(verificationResult.statusText, "verify_command");
                    if (!verifyCommand.empty()) {
                        std::cout << "mp_overlay_package_export_verify_command=" << sanitizeCliValue(verifyCommand)
                                  << "\n";
                    }
                    const auto importCommand = readStatusTextField(verificationResult.statusText, "import_command");
                    if (!importCommand.empty()) {
                        std::cout << "mp_overlay_package_export_import_command=" << sanitizeCliValue(importCommand)
                                  << "\n";
                    }
                    const auto strictVerifyCommand =
                        readStatusTextField(verificationResult.statusText, "strict_verify_command");
                    if (!strictVerifyCommand.empty()) {
                        std::cout << "mp_overlay_package_export_strict_verify_command="
                                  << sanitizeCliValue(strictVerifyCommand) << "\n";
                    }
                    const auto strictImportCommand =
                        readStatusTextField(verificationResult.statusText, "strict_import_command");
                    if (!strictImportCommand.empty()) {
                        std::cout << "mp_overlay_package_export_strict_import_command="
                                  << sanitizeCliValue(strictImportCommand) << "\n";
                    }
                    const auto hostReadiness = readStatusTextField(verificationResult.statusText, "host_readiness");
                    if (!hostReadiness.empty()) {
                        std::cout << "mp_overlay_package_export_host_readiness=" << sanitizeCliValue(hostReadiness)
                                  << "\n";
                    }
                    const auto clientReadiness =
                        readStatusTextField(verificationResult.statusText, "client_readiness_gate");
                    if (!clientReadiness.empty()) {
                        std::cout << "mp_overlay_package_export_client_readiness_gate="
                                  << sanitizeCliValue(clientReadiness) << "\n";
                    }
                    const auto hostNextStep = readStatusTextField(verificationResult.statusText, "host_next_step");
                    if (!hostNextStep.empty()) {
                        std::cout << "mp_overlay_package_export_host_next_step=" << sanitizeCliValue(hostNextStep)
                                  << "\n";
                    }
                    const auto clientNextStep = readStatusTextField(verificationResult.statusText, "client_next_step");
                    if (!clientNextStep.empty()) {
                        std::cout << "mp_overlay_package_export_client_next_step="
                                  << sanitizeCliValue(clientNextStep) << "\n";
                    }
                }
                const auto statusWarningCodes = readStatusTextFields(verificationResult.statusText, "warning_code");
                for (const auto& warningCode : statusWarningCodes) {
                    exportWarnings.push_back(warningCode);
                }
                std::set<std::string> uniqueExportWarnings(exportWarnings.begin(), exportWarnings.end());
                std::vector<std::string> exportWarningCodes(uniqueExportWarnings.begin(), uniqueExportWarnings.end());
                std::cout << "mp_overlay_package_export_warning_count=" << exportWarningCodes.size() << "\n";
                for (const auto& warning : exportWarningCodes) {
                    std::cout << "mp_overlay_package_export_warning=" << sanitizeCliValue(warning) << "\n";
                    std::cout << "mp_overlay_package_export_warning_code=" << sanitizeCliValue(warning) << "\n";
                }
                bool hasExportIdentityMismatchWarning = false;
                for (const auto& warning : exportWarningCodes) {
                    if (isIdentityMismatchWarningCode(warning)) {
                        hasExportIdentityMismatchWarning = true;
                        break;
                    }
                }
                std::cout << "mp_overlay_package_export_identity_mismatch_warning="
                          << (hasExportIdentityMismatchWarning ? "true" : "false") << "\n";
                for (const auto& warning : exportWarningCodes) {
                    if (isIdentityMismatchWarningCode(warning)) {
                        std::cout << "mp_overlay_package_export_identity_mismatch_warning_code="
                                  << sanitizeCliValue(warning) << "\n";
                    }
                }
            }
            std::cout << "mp_overlay_package_export_file_count=" << result.files.size() << "\n";
            for (const auto& file : result.files) {
                std::cout << "mp_overlay_package_export_file=" << sanitizeCliValue(file.path)
                          << ";exists=" << (file.exists ? "true" : "false")
                          << ";hash_matches=" << (file.hashMatches ? "true" : "false")
                          << ";byte_count_matches=" << (file.byteCountMatches ? "true" : "false")
                          << "\n";
            }
            return result.ok ? 0 : 1;
        }

        if (config.verifyMpOverlayPackageMode) {
            const generated_overlay::MpOverlayPackageVerifier verifier;
            const auto result = verifier.verify(config.mpOverlayPackageDirectory);
            std::vector<std::string> warnings;
            bool hasFileHashMismatch = false;
            bool hasFileByteCountMismatch = false;
            bool hasMissingExpectedFiles = false;
            for (const auto& file : result.files) {
                if (!file.exists) {
                    hasMissingExpectedFiles = true;
                }
                if (!file.hashMatches) {
                    hasFileHashMismatch = true;
                }
                if (!file.byteCountMatches) {
                    hasFileByteCountMismatch = true;
                }
            }
            if (hasMissingExpectedFiles) {
                warnings.push_back("missing_expected_files");
            }
            if (hasFileHashMismatch) {
                warnings.push_back("generated_file_hash_mismatch");
            }
            if (hasFileByteCountMismatch) {
                warnings.push_back("generated_file_byte_count_mismatch");
            }
            if (!result.unexpectedFiles.empty()) {
                warnings.push_back("unexpected_files_present");
            }
            if (!result.ok && result.readiness == "not_ready" && !result.statusText.empty()) {
                warnings.push_back("package_identity_or_version_mismatch");
            }
            const auto addMismatchWarning = [&warnings](
                                                const std::string& expected,
                                                const std::string& actual,
                                                const char* warning) {
                if (!expected.empty() && expected != actual) {
                    warnings.push_back(warning);
                }
            };
            addMismatchWarning(
                config.mpOverlayExpectedCampaignId,
                result.campaignId,
                "package_campaign_id_mismatch");
            addMismatchWarning(
                config.mpOverlayExpectedOverlayVersion,
                result.overlayVersion,
                "package_overlay_version_mismatch");
            addMismatchWarning(
                config.mpOverlayExpectedGameVersion,
                result.gameVersion,
                "package_game_version_mismatch");
            addMismatchWarning(
                config.mpOverlayExpectedStrategicNexusModVersion,
                result.strategicNexusModVersion,
                "package_mod_version_mismatch");
            addMismatchWarning(
                config.mpOverlayExpectedManifestHash,
                result.packageManifestHash,
                "package_manifest_hash_mismatch");

            std::cout << "mp_overlay_package_ok=" << (result.ok ? "true" : "false") << "\n";
            std::cout << "mp_overlay_package_reason=" << sanitizeCliValue(result.reason) << "\n";
            if (!result.campaignId.empty()) {
                std::cout << "mp_overlay_package_campaign_id=" << sanitizeCliValue(result.campaignId) << "\n";
            }
            if (!result.overlayVersion.empty()) {
                std::cout << "mp_overlay_package_overlay_version=" << sanitizeCliValue(result.overlayVersion) << "\n";
            }
            if (!result.gameVersion.empty()) {
                std::cout << "mp_overlay_package_game_version=" << sanitizeCliValue(result.gameVersion) << "\n";
            }
            if (!result.strategicNexusModVersion.empty()) {
                std::cout << "mp_overlay_package_strategic_nexus_mod_version="
                          << sanitizeCliValue(result.strategicNexusModVersion) << "\n";
            }
            if (!result.handoffStatus.empty()) {
                std::cout << "mp_overlay_package_handoff_status=" << sanitizeCliValue(result.handoffStatus) << "\n";
            }
            if (!result.readiness.empty()) {
                std::cout << "mp_overlay_package_readiness=" << sanitizeCliValue(result.readiness) << "\n";
            }
            if (!result.packageManifestHash.empty()) {
                std::cout << "mp_overlay_package_manifest_hash=" << sanitizeCliValue(result.packageManifestHash) << "\n";
            }
            if (!result.provenanceState.empty()) {
                std::cout << "mp_overlay_package_provenance_state=" << sanitizeCliValue(result.provenanceState) << "\n";
            }
            std::cout << "mp_overlay_package_source_quality_count=" << result.sourceQualities.size() << "\n";
            for (const auto& sourceQuality : result.sourceQualities) {
                std::cout << "mp_overlay_package_source_quality=" << sanitizeCliValue(sourceQuality) << "\n";
            }
            std::cout << "mp_overlay_package_bootstrap_campaign_count=" << result.bootstrapCampaignCount << "\n";
            if (!result.statusText.empty()) {
                std::cout << "mp_overlay_package_status_text=" << sanitizeCliValue(result.statusText) << "\n";
                const auto verifyCommand = readStatusTextField(result.statusText, "verify_command");
                if (!verifyCommand.empty()) {
                    std::cout << "mp_overlay_package_verify_command=" << sanitizeCliValue(verifyCommand) << "\n";
                }
                const auto importCommand = readStatusTextField(result.statusText, "import_command");
                if (!importCommand.empty()) {
                    std::cout << "mp_overlay_package_import_command=" << sanitizeCliValue(importCommand) << "\n";
                }
                const auto strictVerifyCommand = readStatusTextField(result.statusText, "strict_verify_command");
                if (!strictVerifyCommand.empty()) {
                    std::cout << "mp_overlay_package_strict_verify_command=" << sanitizeCliValue(strictVerifyCommand) << "\n";
                }
                const auto strictImportCommand = readStatusTextField(result.statusText, "strict_import_command");
                if (!strictImportCommand.empty()) {
                    std::cout << "mp_overlay_package_strict_import_command=" << sanitizeCliValue(strictImportCommand) << "\n";
                }
                const auto hostReadiness = readStatusTextField(result.statusText, "host_readiness");
                if (!hostReadiness.empty()) {
                    std::cout << "mp_overlay_package_host_readiness=" << sanitizeCliValue(hostReadiness) << "\n";
                }
                const auto clientReadinessGate = readStatusTextField(result.statusText, "client_readiness_gate");
                if (!clientReadinessGate.empty()) {
                    std::cout << "mp_overlay_package_client_readiness_gate="
                              << sanitizeCliValue(clientReadinessGate) << "\n";
                }
                const auto hostNextStep = readStatusTextField(result.statusText, "host_next_step");
                if (!hostNextStep.empty()) {
                    std::cout << "mp_overlay_package_host_next_step=" << sanitizeCliValue(hostNextStep) << "\n";
                }
                const auto clientNextStep = readStatusTextField(result.statusText, "client_next_step");
                if (!clientNextStep.empty()) {
                    std::cout << "mp_overlay_package_client_next_step=" << sanitizeCliValue(clientNextStep) << "\n";
                }
            }
            std::cout << "mp_overlay_package_file_count=" << result.files.size() << "\n";
            for (const auto& file : result.files) {
                std::cout << "mp_overlay_package_file=" << sanitizeCliValue(file.path)
                          << ";exists=" << (file.exists ? "true" : "false")
                          << ";hash_matches=" << (file.hashMatches ? "true" : "false")
                          << ";byte_count_matches=" << (file.byteCountMatches ? "true" : "false")
                          << "\n";
            }
            for (const auto& file : result.unexpectedFiles) {
                std::cout << "mp_overlay_package_unexpected_file=" << sanitizeCliValue(file) << "\n";
            }
            std::cout << "mp_overlay_package_warning_count=" << warnings.size() << "\n";
            std::vector<std::string> identityMismatchWarningCodes;
            for (const auto& warning : warnings) {
                if (isIdentityMismatchWarningCode(warning)) {
                    identityMismatchWarningCodes.push_back(warning);
                }
            }
            for (const auto& warning : warnings) {
                std::cout << "mp_overlay_package_warning=" << sanitizeCliValue(warning) << "\n";
                std::cout << "mp_overlay_package_warning_code=" << sanitizeCliValue(warning) << "\n";
            }
            std::cout << "mp_overlay_package_identity_mismatch_warning="
                      << (!identityMismatchWarningCodes.empty() ? "true" : "false") << "\n";
            for (const auto& warningCode : identityMismatchWarningCodes) {
                std::cout << "mp_overlay_package_identity_mismatch_warning_code="
                          << sanitizeCliValue(warningCode) << "\n";
            }
            return result.ok ? 0 : 1;
        }

        if (config.importMpOverlayPackageMode) {
            generated_overlay::MpOverlayPackageImporter importer;
            const auto result = importer.importPackage(
                config.mpOverlayPackageDirectory,
                config.mpOverlayImportTargetDirectory);
            std::vector<std::string> warnings;
            const auto addMismatchWarning = [&warnings](
                                                const std::string& expected,
                                                const std::string& actual,
                                                const char* warning) {
                if (!expected.empty() && expected != actual) {
                    warnings.push_back(warning);
                }
            };
            addMismatchWarning(
                config.mpOverlayExpectedCampaignId,
                result.campaignId,
                "package_campaign_id_mismatch");
            addMismatchWarning(
                config.mpOverlayExpectedOverlayVersion,
                result.overlayVersion,
                "package_overlay_version_mismatch");
            addMismatchWarning(
                config.mpOverlayExpectedGameVersion,
                result.gameVersion,
                "package_game_version_mismatch");
            addMismatchWarning(
                config.mpOverlayExpectedStrategicNexusModVersion,
                result.strategicNexusModVersion,
                "package_mod_version_mismatch");
            addMismatchWarning(
                config.mpOverlayExpectedManifestHash,
                result.packageManifestHash,
                "package_manifest_hash_mismatch");

            std::cout << "mp_overlay_package_import_ok=" << (result.ok ? "true" : "false") << "\n";
            std::cout << "mp_overlay_package_import_reason=" << sanitizeCliValue(result.reason) << "\n";
            if (!result.campaignId.empty()) {
                std::cout << "mp_overlay_package_import_campaign_id="
                          << sanitizeCliValue(result.campaignId) << "\n";
            }
            if (!result.overlayVersion.empty()) {
                std::cout << "mp_overlay_package_import_overlay_version="
                          << sanitizeCliValue(result.overlayVersion) << "\n";
            }
            if (!result.gameVersion.empty()) {
                std::cout << "mp_overlay_package_import_game_version="
                          << sanitizeCliValue(result.gameVersion) << "\n";
            }
            if (!result.strategicNexusModVersion.empty()) {
                std::cout << "mp_overlay_package_import_strategic_nexus_mod_version="
                          << sanitizeCliValue(result.strategicNexusModVersion) << "\n";
            }
            if (!result.handoffStatus.empty()) {
                std::cout << "mp_overlay_package_import_handoff_status="
                          << sanitizeCliValue(result.handoffStatus) << "\n";
            }
            if (!result.packageManifestHash.empty()) {
                std::cout << "mp_overlay_package_import_manifest_hash="
                          << sanitizeCliValue(result.packageManifestHash) << "\n";
            }
            if (!result.provenanceState.empty()) {
                std::cout << "mp_overlay_package_import_provenance_state="
                          << sanitizeCliValue(result.provenanceState) << "\n";
            }
            std::cout << "mp_overlay_package_import_source_quality_count=" << result.sourceQualities.size() << "\n";
            for (const auto& sourceQuality : result.sourceQualities) {
                std::cout << "mp_overlay_package_import_source_quality=" << sanitizeCliValue(sourceQuality) << "\n";
            }
            std::cout << "mp_overlay_package_import_bootstrap_campaign_count=" << result.bootstrapCampaignCount << "\n";
            if (!result.readiness.empty()) {
                std::cout << "mp_overlay_package_import_readiness="
                          << sanitizeCliValue(result.readiness) << "\n";
            }
            if (!result.statusText.empty()) {
                std::cout << "mp_overlay_package_import_status_text="
                          << sanitizeCliValue(result.statusText) << "\n";
                const auto verifyCommand = readStatusTextField(result.statusText, "verify_command");
                if (!verifyCommand.empty()) {
                    std::cout << "mp_overlay_package_import_verify_command=" << sanitizeCliValue(verifyCommand) << "\n";
                }
                const auto importCommand = readStatusTextField(result.statusText, "import_command");
                if (!importCommand.empty()) {
                    std::cout << "mp_overlay_package_import_command=" << sanitizeCliValue(importCommand) << "\n";
                }
                const auto strictVerifyCommand = readStatusTextField(result.statusText, "strict_verify_command");
                if (!strictVerifyCommand.empty()) {
                    std::cout << "mp_overlay_package_import_strict_verify_command="
                              << sanitizeCliValue(strictVerifyCommand) << "\n";
                }
                const auto strictImportCommand = readStatusTextField(result.statusText, "strict_import_command");
                if (!strictImportCommand.empty()) {
                    std::cout << "mp_overlay_package_import_strict_import_command="
                              << sanitizeCliValue(strictImportCommand) << "\n";
                }
                const auto hostReadiness = readStatusTextField(result.statusText, "host_readiness");
                if (!hostReadiness.empty()) {
                    std::cout << "mp_overlay_package_import_host_readiness=" << sanitizeCliValue(hostReadiness) << "\n";
                }
                const auto clientReadinessGate = readStatusTextField(result.statusText, "client_readiness_gate");
                if (!clientReadinessGate.empty()) {
                    std::cout << "mp_overlay_package_import_client_readiness_gate="
                              << sanitizeCliValue(clientReadinessGate) << "\n";
                }
                const auto hostNextStep = readStatusTextField(result.statusText, "host_next_step");
                if (!hostNextStep.empty()) {
                    std::cout << "mp_overlay_package_import_host_next_step=" << sanitizeCliValue(hostNextStep) << "\n";
                }
                const auto clientNextStep = readStatusTextField(result.statusText, "client_next_step");
                if (!clientNextStep.empty()) {
                    std::cout << "mp_overlay_package_import_client_next_step=" << sanitizeCliValue(clientNextStep) << "\n";
                }
            }
            std::cout << "mp_overlay_package_imported_file_count=" << result.importedFiles.size() << "\n";
            for (const auto& file : result.importedFiles) {
                std::cout << "mp_overlay_package_imported_file=" << sanitizeCliValue(file.path) << "\n";
            }
            std::cout << "mp_overlay_package_import_warning_count=" << warnings.size() << "\n";
            std::vector<std::string> identityMismatchWarningCodes;
            for (const auto& warning : warnings) {
                if (isIdentityMismatchWarningCode(warning)) {
                    identityMismatchWarningCodes.push_back(warning);
                }
            }
            for (const auto& warning : warnings) {
                std::cout << "mp_overlay_package_import_warning=" << sanitizeCliValue(warning) << "\n";
                std::cout << "mp_overlay_package_import_warning_code=" << sanitizeCliValue(warning) << "\n";
            }
            std::cout << "mp_overlay_package_import_identity_mismatch_warning="
                      << (!identityMismatchWarningCodes.empty() ? "true" : "false") << "\n";
            for (const auto& warningCode : identityMismatchWarningCodes) {
                std::cout << "mp_overlay_package_import_identity_mismatch_warning_code="
                          << sanitizeCliValue(warningCode) << "\n";
            }
            return result.ok ? 0 : 1;
        }

        if (config.sncStatusSnapshotMode) {
            const StrategicNexusCompanion companion;
            CompanionStatusConfig statusConfig;
            statusConfig.archiveRoot = config.sncArchiveRoot;
            statusConfig.generatedOverlayDirectory = config.sncGeneratedOverlayDirectory;
            statusConfig.mpOverlayPackageDirectory = config.mpOverlayPackageDirectory;
            statusConfig.mpOverlayPackageZipPath = config.sncMpOverlayPackageZipPath;
            statusConfig.startWithWindowsEnabled = config.sncStartWithWindowsEnabled;
            statusConfig.useDetectedStellarisState = config.sncUseDetectedStellarisState;
            statusConfig.stellarisRunningOverride = config.sncStellarisRunningOverride;
            statusConfig.generatedOverlayStagingStatusPath =
                config.sncGeneratedOverlayPublishStagingStatusPath;
            statusConfig.postPlayGeneratedOverlayStagingStatusPath =
                config.sncGeneratedOverlayPublishStagingStatusPath;
            statusConfig.generatedOverlayActiveDirectory =
                config.sncGeneratedOverlayPublishActiveDirectory;
            statusConfig.generatedOverlayPublishStatusPath =
                config.sncGeneratedOverlayPublishStatusOutputPath;
            statusConfig.generatedOverlayPublishBackupRootDirectory =
                config.sncGeneratedOverlayPublishBackupRootDirectory;
            const auto snapshot = companion.buildStatusSnapshot(statusConfig);
            const auto json = serializeCompanionStatusSnapshot(snapshot);
            const bool outputRequested = !config.sncStatusOutputPath.empty();
            const bool written = outputRequested
                ? common::writeTextFileAtomically(config.sncStatusOutputPath, json)
                : true;
            const bool success = written;

            const auto stdoutPath = [](const std::filesystem::path& path) {
                return path.empty() ? std::string() : path.generic_string();
            };
            std::cout << "snc_status_success=" << (success ? "true" : "false") << "\n";
            std::cout << "snc_app_name=" << sanitizeCliValue(snapshot.appName) << "\n";
            std::cout << "snc_abbreviation=" << sanitizeCliValue(snapshot.abbreviation) << "\n";
            std::cout << "snc_startup_lifecycle_state="
                      << sanitizeCliValue(
                             snapshot.lifecycle.startWithWindowsEnabled
                                 ? "owner_enabled_start_with_windows"
                                 : "manual_start_only")
                      << "\n";
            std::cout << "snc_start_with_windows_enabled="
                      << (snapshot.lifecycle.startWithWindowsEnabled ? "true" : "false") << "\n";
            std::cout << "snc_window_close_behavior="
                      << sanitizeCliValue(snapshot.lifecycle.windowCloseBehavior) << "\n";
            std::cout << "snc_explicit_exit_behavior="
                      << sanitizeCliValue(snapshot.lifecycle.explicitExitBehavior) << "\n";
            std::cout << "snc_crash_restart_policy="
                      << sanitizeCliValue(snapshot.lifecycle.crashRestartPolicy) << "\n";
            std::cout << "snc_archive_state=" << sanitizeCliValue(snapshot.archive.state) << "\n";
            std::cout << "snc_archive_reason=" << sanitizeCliValue(snapshot.archive.reason) << "\n";
            std::cout << "snc_archive_path=" << sanitizeCliValue(stdoutPath(snapshot.archive.path)) << "\n";
            std::cout << "snc_generated_overlay_state=" << sanitizeCliValue(snapshot.generatedOverlay.state) << "\n";
            std::cout << "snc_generated_overlay_reason=" << sanitizeCliValue(snapshot.generatedOverlay.reason) << "\n";
            std::cout << "snc_generated_overlay_path=" << sanitizeCliValue(stdoutPath(snapshot.generatedOverlay.path)) << "\n";
            std::cout << "snc_generated_overlay_manifest_hash=" << sanitizeCliValue(snapshot.generatedOverlay.manifestHash) << "\n";
            std::cout << "snc_generated_overlay_reactive_capability="
                      << sanitizeCliValue(snapshot.generatedOverlay.reactivePolicyPackCapability.empty()
                                             ? "unknown"
                                             : snapshot.generatedOverlay.reactivePolicyPackCapability)
                      << "\n";
            std::cout << "snc_generated_overlay_event_family_count="
                      << snapshot.generatedOverlay.eventFamilies.size() << "\n";
            for (const auto& eventFamily : snapshot.generatedOverlay.eventFamilies) {
                std::cout << "snc_generated_overlay_event_family="
                          << sanitizeCliValue(eventFamily) << "\n";
            }
            std::cout << "snc_generated_overlay_source_quality_count="
                      << snapshot.generatedOverlay.sourceQualities.size() << "\n";
            for (const auto& sourceQuality : snapshot.generatedOverlay.sourceQualities) {
                std::cout << "snc_generated_overlay_source_quality="
                          << sanitizeCliValue(sourceQuality) << "\n";
            }
            std::cout << "snc_generated_overlay_bootstrap_campaign_count="
                      << snapshot.generatedOverlay.bootstrapCampaignCount << "\n";
            std::cout << "snc_generated_overlay_publish_gate_state="
                      << sanitizeCliValue(snapshot.generatedOverlayPublishGate.state) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_reason="
                      << sanitizeCliValue(snapshot.generatedOverlayPublishGate.reason) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_path="
                      << sanitizeCliValue(stdoutPath(snapshot.generatedOverlayPublishGate.path)) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_staging_status_path="
                      << sanitizeCliValue(stdoutPath(snapshot.generatedOverlayPublishGate.stagingStatusPath)) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_staged_overlay_path="
                      << sanitizeCliValue(stdoutPath(snapshot.generatedOverlayPublishGate.stagedOverlayDirectory)) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_active_overlay_path="
                      << sanitizeCliValue(stdoutPath(snapshot.generatedOverlayPublishGate.activeOverlayDirectory)) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_status_output_path="
                      << sanitizeCliValue(stdoutPath(snapshot.generatedOverlayPublishGate.publishStatusPath)) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_backup_root_path="
                      << sanitizeCliValue(stdoutPath(snapshot.generatedOverlayPublishGate.backupRootDirectory)) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_manifest_hash="
                      << sanitizeCliValue(snapshot.generatedOverlayPublishGate.manifestHash) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_rule_count="
                      << snapshot.generatedOverlayPublishGate.dslRuleCount << "\n";
            std::cout << "snc_generated_overlay_publish_gate_owner_approval_required="
                      << (snapshot.generatedOverlayPublishGate.ownerApprovalRequired ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_gate_can_publish="
                      << (snapshot.generatedOverlayPublishGate.canPublish ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_gate_active_overlay_exists="
                      << (snapshot.generatedOverlayPublishGate.activeOverlayExists ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_gate_backup_before_replace="
                      << (snapshot.generatedOverlayPublishGate.backupBeforeReplace ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_gate_command="
                      << sanitizeCliValue(snapshot.generatedOverlayPublishGate.publishCommand) << "\n";
            std::cout << "snc_mp_overlay_package_state=" << sanitizeCliValue(snapshot.mpOverlayPackage.state) << "\n";
            std::cout << "snc_mp_overlay_package_reason=" << sanitizeCliValue(snapshot.mpOverlayPackage.reason) << "\n";
            std::cout << "snc_mp_overlay_package_path=" << sanitizeCliValue(stdoutPath(snapshot.mpOverlayPackage.path)) << "\n";
            std::cout << "snc_mp_overlay_package_zip_state="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.packageZipState) << "\n";
            std::cout << "snc_mp_overlay_package_zip_reason="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.packageZipReason) << "\n";
            std::cout << "snc_mp_overlay_package_zip_path="
                      << sanitizeCliValue(stdoutPath(snapshot.mpOverlayPackage.packageZipPath)) << "\n";
            std::cout << "snc_mp_overlay_package_zip_hash="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.packageZipHash) << "\n";
            std::cout << "snc_mp_overlay_package_zip_bytes=" << snapshot.mpOverlayPackage.packageZipBytes << "\n";
            std::cout << "snc_mp_overlay_package_campaign_id=" << sanitizeCliValue(snapshot.mpOverlayPackage.campaignId) << "\n";
            std::cout << "snc_mp_overlay_package_overlay_version=" << sanitizeCliValue(snapshot.mpOverlayPackage.overlayVersion) << "\n";
            std::cout << "snc_mp_overlay_package_game_version=" << sanitizeCliValue(snapshot.mpOverlayPackage.gameVersion) << "\n";
            std::cout << "snc_mp_overlay_package_strategic_nexus_mod_version="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.strategicNexusModVersion) << "\n";
            std::cout << "snc_mp_overlay_package_handoff_status=" << sanitizeCliValue(snapshot.mpOverlayPackage.handoffStatus) << "\n";
            std::cout << "snc_mp_overlay_package_previous_host_available="
                      << (snapshot.mpOverlayPackage.previousHostAvailableKnown
                              ? (snapshot.mpOverlayPackage.previousHostAvailable ? "true" : "false")
                              : "unknown")
                      << "\n";
            std::cout << "snc_mp_overlay_package_previous_host_available_known="
                      << (snapshot.mpOverlayPackage.previousHostAvailableKnown ? "true" : "false") << "\n";
            std::cout << "snc_mp_overlay_package_readiness=" << sanitizeCliValue(snapshot.mpOverlayPackage.readiness) << "\n";
            std::cout << "snc_mp_overlay_package_host_readiness="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.hostReadiness) << "\n";
            std::cout << "snc_mp_overlay_package_client_readiness_gate="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.clientReadinessGate) << "\n";
            std::cout << "snc_mp_overlay_package_host_next_step="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.hostNextStep) << "\n";
            std::cout << "snc_mp_overlay_package_client_next_step="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.clientNextStep) << "\n";
            std::cout << "snc_mp_overlay_package_manifest_hash=" << sanitizeCliValue(snapshot.mpOverlayPackage.packageManifestHash) << "\n";
            std::cout << "snc_mp_overlay_package_provenance_state="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.provenanceState) << "\n";
            std::cout << "snc_mp_overlay_package_source_quality_count="
                      << snapshot.mpOverlayPackage.sourceQualities.size() << "\n";
            for (const auto& sourceQuality : snapshot.mpOverlayPackage.sourceQualities) {
                std::cout << "snc_mp_overlay_package_source_quality=" << sanitizeCliValue(sourceQuality) << "\n";
            }
            std::cout << "snc_mp_overlay_package_bootstrap_campaign_count="
                      << snapshot.mpOverlayPackage.bootstrapCampaignCount << "\n";
            std::cout << "snc_mp_overlay_package_verify_command=" << sanitizeCliValue(snapshot.mpOverlayPackage.verifyCommand) << "\n";
            std::cout << "snc_mp_overlay_package_import_command=" << sanitizeCliValue(snapshot.mpOverlayPackage.importCommand) << "\n";
            std::cout << "snc_mp_overlay_package_strict_verify_command="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.strictVerifyCommand) << "\n";
            std::cout << "snc_mp_overlay_package_strict_import_command="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.strictImportCommand) << "\n";
            std::cout << "snc_mp_overlay_package_status_text=" << sanitizeCliValue(snapshot.mpOverlayPackage.statusText) << "\n";
            std::vector<std::string> mpWarningCodes = snapshot.mpOverlayPackage.warningCodes;
            if (mpWarningCodes.empty()) {
                mpWarningCodes = readStatusTextFields(snapshot.mpOverlayPackage.statusText, "warning_code");
            }
            std::cout << "snc_mp_overlay_package_warning_count=" << snapshot.mpOverlayPackage.warningCount << "\n";
            for (const auto& warningCode : mpWarningCodes) {
                std::cout << "snc_mp_overlay_package_warning=" << sanitizeCliValue(warningCode) << "\n";
                std::cout << "snc_mp_overlay_package_warning_code=" << sanitizeCliValue(warningCode) << "\n";
            }
            std::cout << "snc_mp_overlay_package_identity_mismatch_warning="
                      << (snapshot.mpOverlayPackage.identityMismatchWarning ? "true" : "false") << "\n";
            for (const auto& warningCode : snapshot.mpOverlayPackage.identityMismatchWarningCodes) {
                std::cout << "snc_mp_overlay_package_identity_mismatch_warning_code="
                          << sanitizeCliValue(warningCode) << "\n";
            }
            std::cout << "snc_mp_overlay_package_mismatch_warning_state="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.mismatchWarningState) << "\n";
            std::cout << "snc_mp_overlay_package_mismatch_warning_reason="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.mismatchWarningReason) << "\n";
            for (const auto& warningCode : snapshot.mpOverlayPackage.mismatchWarningCodes) {
                std::cout << "snc_mp_overlay_package_mismatch_warning_code=" << sanitizeCliValue(warningCode) << "\n";
            }
            if (!snapshot.mpOverlayPackage.identityMismatchAlert.empty()) {
                std::cout << "snc_mp_overlay_package_identity_mismatch_alert="
                          << sanitizeCliValue(snapshot.mpOverlayPackage.identityMismatchAlert) << "\n";
            }
            std::cout << "snc_post_play_pipeline_state=" << sanitizeCliValue(snapshot.postPlayPipeline.state) << "\n";
            std::cout << "snc_post_play_pipeline_reason=" << sanitizeCliValue(snapshot.postPlayPipeline.reason) << "\n";
            std::cout << "snc_post_play_entry_point_analysis_path="
                      << sanitizeCliValue(stdoutPath(snapshot.postPlayPipeline.entryPointAnalysisPath)) << "\n";
            std::cout << "snc_post_play_entry_point_readiness="
                      << sanitizeCliValue(snapshot.postPlayPipeline.entryPointReadiness) << "\n";
            std::cout << "snc_post_play_entry_point_count=" << snapshot.postPlayPipeline.entryPointCount << "\n";
            std::cout << "snc_post_play_branch_ambiguity_detected="
                      << (snapshot.postPlayPipeline.branchAmbiguityDetected ? "true" : "false") << "\n";
            std::cout << "snc_post_play_package_path="
                      << sanitizeCliValue(stdoutPath(snapshot.postPlayPipeline.postPlayPackagePath)) << "\n";
            std::cout << "snc_post_play_package_readiness="
                      << sanitizeCliValue(snapshot.postPlayPipeline.postPlayPackageReadiness) << "\n";
            std::cout << "snc_post_play_decision_ready_entry_count="
                      << snapshot.postPlayPipeline.postPlayDecisionReadyEntryCount << "\n";
            std::cout << "snc_decision_input_package_path="
                      << sanitizeCliValue(stdoutPath(snapshot.postPlayPipeline.decisionInputPackagePath)) << "\n";
            std::cout << "snc_decision_input_package_readiness="
                      << sanitizeCliValue(snapshot.postPlayPipeline.decisionInputPackageReadiness) << "\n";
            std::cout << "snc_decision_input_count=" << snapshot.postPlayPipeline.decisionInputCount << "\n";
            std::cout << "snc_candidate_decision_package_path="
                      << sanitizeCliValue(stdoutPath(snapshot.postPlayPipeline.candidateDecisionPackagePath)) << "\n";
            std::cout << "snc_candidate_decision_package_readiness="
                      << sanitizeCliValue(snapshot.postPlayPipeline.candidateDecisionPackageReadiness) << "\n";
            std::cout << "snc_candidate_decision_count=" << snapshot.postPlayPipeline.candidateDecisionCount << "\n";
            std::cout << "snc_candidate_decision_validator_passed="
                      << (snapshot.postPlayPipeline.candidateDecisionValidatorPassed ? "true" : "false") << "\n";
            std::cout << "snc_dsl_draft_path="
                      << sanitizeCliValue(stdoutPath(snapshot.postPlayPipeline.dslDraftPath)) << "\n";
            std::cout << "snc_dsl_draft_audit_path="
                      << sanitizeCliValue(stdoutPath(snapshot.postPlayPipeline.dslDraftAuditPath)) << "\n";
            std::cout << "snc_dsl_draft_readiness="
                      << sanitizeCliValue(snapshot.postPlayPipeline.dslDraftReadiness) << "\n";
            std::cout << "snc_dsl_draft_rule_count=" << snapshot.postPlayPipeline.dslDraftRuleCount << "\n";
            std::cout << "snc_dsl_draft_validator_passed="
                      << (snapshot.postPlayPipeline.dslDraftValidatorPassed ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_staging_status_path="
                      << sanitizeCliValue(stdoutPath(snapshot.postPlayPipeline.generatedOverlayStagingStatusPath)) << "\n";
            std::cout << "snc_generated_overlay_staging_readiness="
                      << sanitizeCliValue(snapshot.postPlayPipeline.generatedOverlayStagingReadiness) << "\n";
            std::cout << "snc_generated_overlay_staging_rule_count="
                      << snapshot.postPlayPipeline.generatedOverlayStagingRuleCount << "\n";
            std::cout << "snc_generated_overlay_manifest_verified="
                      << (snapshot.postPlayPipeline.generatedOverlayManifestVerified ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_allowed="
                      << (snapshot.postPlayPipeline.generatedOverlayPublishAllowed ? "true" : "false") << "\n";
            std::cout << "snc_campaign_library_plan_present="
                      << (snapshot.postPlayPipeline.campaignLibraryPlanPresent ? "true" : "false") << "\n";
            std::cout << "snc_campaign_library_plan_path="
                      << sanitizeCliValue(stdoutPath(snapshot.postPlayPipeline.campaignLibraryPlanPath)) << "\n";
            std::cout << "snc_campaign_library_plan_source="
                      << sanitizeCliValue(snapshot.postPlayPipeline.campaignLibraryPlanSource) << "\n";
            std::cout << "snc_campaign_library_plan_readiness="
                      << sanitizeCliValue(snapshot.postPlayPipeline.campaignLibraryPlanReadiness) << "\n";
            std::cout << "snc_campaign_library_plan_reason="
                      << sanitizeCliValue(snapshot.postPlayPipeline.campaignLibraryPlanReason) << "\n";
            std::cout << "snc_campaign_library_limit_reached="
                      << (snapshot.postPlayPipeline.campaignLibraryLimitReached ? "true" : "false") << "\n";
            std::cout << "snc_campaign_library_skipped_due_to_limit_count="
                      << snapshot.postPlayPipeline.campaignLibrarySkippedDueToLimitCount << "\n";
            std::cout << "snc_gameplay_acceptance_state=" << sanitizeCliValue(snapshot.gameplayAcceptance.state) << "\n";
            std::cout << "snc_gameplay_acceptance_reason=" << sanitizeCliValue(snapshot.gameplayAcceptance.reason) << "\n";
            std::cout << "snc_gameplay_acceptance_path=" << sanitizeCliValue(stdoutPath(snapshot.gameplayAcceptance.path)) << "\n";
            std::cout << "snc_status_center_state=" << sanitizeCliValue(snapshot.statusCenter.state) << "\n";
            std::cout << "snc_status_center_reason=" << sanitizeCliValue(snapshot.statusCenter.reason) << "\n";
            std::cout << "snc_status_center_path=" << sanitizeCliValue(stdoutPath(snapshot.statusCenter.path)) << "\n";
            std::cout << "snc_owner_test_playbook_path="
                      << sanitizeCliValue(stdoutPath(snapshot.ownerTestPlaybookPath)) << "\n";
            std::cout << "snc_status_center_summary_text=" << sanitizeCliValue(snapshot.statusCenterSummaryText) << "\n";
            std::cout << "snc_status_output_written=" << (outputRequested && written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "snc_status_reason=failed to write status snapshot\n";
            }
            return success ? 0 : 1;
        }

        if (config.sncLiveAutosaveMonitorMode) {
            if (config.sncLiveAutosaveArchiveRoot.empty() || config.sncLiveAutosaveSessionId.empty()) {
                std::cout << "snc_live_autosave_monitor_success=false\n";
                std::cout << "snc_live_autosave_monitor_reason=missing archive root or session id\n";
                return 1;
            }

            const auto pollMs = config.sncLiveAutosavePollIntervalMs < 0
                ? 0
                : config.sncLiveAutosavePollIntervalMs;
            const auto stabilityDelayMs = config.sncLiveAutosaveStabilityDelayMs < 0
                ? 0
                : static_cast<std::uint32_t>(config.sncLiveAutosaveStabilityDelayMs);
            const auto maxIterations = config.sncLiveAutosaveMaxIterations < 0
                ? 0
                : static_cast<int>(config.sncLiveAutosaveMaxIterations);

            if (config.sncSessionCaptureMode) {
                std::cout << "snc_session_capture_started=true\n";
                std::cout << "snc_session_capture_session_id="
                          << sanitizeCliValue(config.sncLiveAutosaveSessionId) << "\n";
                std::cout << "snc_session_capture_archive_root="
                          << sanitizeCliValue(config.sncLiveAutosaveArchiveRoot.generic_string()) << "\n";
                std::cout << "snc_session_capture_status_path="
                          << sanitizeCliValue(config.sncLiveAutosaveStatusOutputPath.generic_string()) << "\n";
                std::cout << std::flush;
            }

            const auto result = runCompanionLiveAutosaveMonitor({
                config.sncLiveAutosaveSaveRoots,
                config.sncLiveAutosaveArchiveRoot,
                config.sncLiveAutosaveStatusOutputPath,
                config.sncLiveAutosaveSessionId,
                std::chrono::milliseconds(pollMs),
                stabilityDelayMs,
                maxIterations,
                config.sncLiveAutosaveUseDetectedStellarisState,
                config.sncLiveAutosaveStellarisRunningOverride,
                config.sncLiveAutosaveCaptureWhenStellarisNotRunning
            });

            if (config.sncSessionCaptureMode) {
                std::cout << "snc_session_capture_mode=true\n";
            }
            std::cout << "snc_live_autosave_monitor_success=" << (result.ok ? "true" : "false") << "\n";
            std::cout << "snc_live_autosave_monitor_reason=" << sanitizeCliValue(result.reason) << "\n";
            std::cout << "snc_live_autosave_monitor_iterations=" << result.iterationsRun << "\n";
            std::cout << "snc_live_autosave_monitor_candidate_roots=" << result.candidateRootCount << "\n";
            std::cout << "snc_live_autosave_monitor_existing_roots=" << result.existingRootCount << "\n";
            std::cout << "snc_live_autosave_monitor_copied=" << result.copiedCount << "\n";
            std::cout << "snc_live_autosave_monitor_skipped=" << result.skippedCount << "\n";
            std::cout << "snc_live_autosave_monitor_stellaris_running="
                      << (result.lastStellarisRunning ? "true" : "false") << "\n";
            std::cout << "snc_live_autosave_monitor_session_dir="
                      << sanitizeCliValue(result.archiveSessionDirectory.generic_string()) << "\n";
            std::cout << "snc_live_autosave_monitor_status_path="
                      << sanitizeCliValue(result.statusOutputPath.generic_string()) << "\n";
            std::cout << "snc_live_autosave_monitor_status_written="
                      << (result.statusOutputWritten ? "true" : "false") << "\n";
            return result.ok ? 0 : 1;
        }

        if (config.offlineSpineMode) {
            if (config.offlineSpineArchiveDirectory.empty() ||
                config.offlineSpineCampaignId.empty() ||
                config.offlineSpineEmpireId.empty() ||
                config.offlineSpineDslInputPath.empty() ||
                config.offlineSpineWorkDirectory.empty() ||
                config.offlineSpineGeneratedOverlayDirectory.empty() ||
                config.offlineSpineStatusOutputPath.empty()) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=missing archive directory, identity, DSL input, work directory, overlay directory, or status output\n";
                return 1;
            }

            const AutosaveArchiveVerifier archiveVerifier;
            const auto archiveVerification = archiveVerifier.verify(config.offlineSpineArchiveDirectory);
            if (!archiveVerification.ok) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=archive verification failed: " << sanitizeCliValue(archiveVerification.reason) << "\n";
                return 1;
            }

            if (!std::filesystem::exists(config.offlineSpineDslInputPath)) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=missing DSL input\n";
                return 1;
            }

            const auto overlayDirectory = config.offlineSpineGeneratedOverlayDirectory;
            {
                const auto containsAnyRegularFile = [](const std::filesystem::path& directory) {
                    std::error_code iterateError;
                    std::filesystem::recursive_directory_iterator it(
                        directory,
                        std::filesystem::directory_options::skip_permission_denied,
                        iterateError);
                    if (iterateError) {
                        return true;
                    }
                    for (const auto& entry : it) {
                        std::error_code entryError;
                        if (entry.is_regular_file(entryError)) {
                            return true;
                        }
                        if (entryError) {
                            return true;
                        }
                    }
                    return false;
                };

                std::error_code ec;
                if (std::filesystem::exists(overlayDirectory, ec)) {
                    if (ec || !std::filesystem::is_directory(overlayDirectory, ec)) {
                        std::cout << "offline_spine_success=false\n";
                        std::cout << "offline_spine_reason=overlay output path is not a usable directory\n";
                        return 1;
                    }
                    if (containsAnyRegularFile(overlayDirectory)) {
                        std::cout << "offline_spine_success=false\n";
                        std::cout << "offline_spine_reason=overlay output directory must not contain files\n";
                        return 1;
                    }
                }
            }

            const AutosaveArchiveSummarizer summarizer;
            const auto summary = summarizer.summarize(config.offlineSpineArchiveDirectory);
            if (!summary.ok) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=archive summary failed: " << sanitizeCliValue(summary.reason) << "\n";
                return 1;
            }

            SaveParserSummary parsedHeadline;
            const bool hasParsedHeadline = parseLatestArchivedSaveHeadline(summary, parsedHeadline);
            const SeasonDeltaLedgerBuilder ledgerBuilder;
            const auto ledger = ledgerBuilder.build(
                summary,
                config.offlineSpineCampaignId,
                hasParsedHeadline ? &parsedHeadline : nullptr);
            const auto ledgerPath = config.offlineSpineWorkDirectory / "season_delta_ledger.json";
            const bool ledgerWritten = common::writeTextFileAtomically(ledgerPath, serializeSeasonDeltaLedger(ledger));
            if (!ledger.ok || !ledgerWritten) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=" << sanitizeCliValue(!ledger.ok ? ledger.reason : "failed to write season delta ledger") << "\n";
                return 1;
            }

            const SeasonEmpireBriefBuilder briefBuilder;
            const auto brief = briefBuilder.build(ledger, config.offlineSpineEmpireId);
            const auto briefPath = config.offlineSpineWorkDirectory / "empire_brief.json";
            const bool briefWritten = common::writeTextFileAtomically(briefPath, serializeSeasonEmpireBrief(brief));
            if (!brief.ok || !briefWritten) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=" << sanitizeCliValue(!brief.ok ? brief.reason : "failed to write empire brief") << "\n";
                return 1;
            }

            const generated_overlay::DslParser parser;
            const auto parseResult = parser.parse(common::readTextFile(config.offlineSpineDslInputPath));
            if (!parseResult.ok) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=parse failed: " << sanitizeCliValue(parseResult.error) << "\n";
                return 1;
            }

            const generated_overlay::DslValidator validator;
            const auto validation = validator.validate(parseResult.program);
            if (!validation.ok) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=validation failed\n";
                for (const auto& error : validation.errors) {
                    std::cout << "offline_spine_error=" << sanitizeCliValue(error) << "\n";
                }
                return 1;
            }

            const auto runtimeConditionErrors =
                generated_overlay::findUnsupportedRuntimeConditionErrors(parseResult.program);
            if (!runtimeConditionErrors.empty()) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=runtime conditions unsupported\n";
                for (const auto& error : runtimeConditionErrors) {
                    std::cout << "offline_spine_error=" << sanitizeCliValue(error) << "\n";
                }
                return 1;
            }

            const generated_overlay::OverlayCompiler compiler;
            const auto files = compiler.compile(parseResult.program);
            const bool eventsWritten = common::writeTextFileAtomically(
                overlayDirectory / "events" / "strategic_nexus_generated_events.txt",
                files.eventsText);
            const bool effectsWritten = common::writeTextFileAtomically(
                overlayDirectory / "common" / "scripted_effects" / "strategic_nexus_generated_effects.txt",
                files.scriptedEffectsText);
            const bool triggersWritten = common::writeTextFileAtomically(
                overlayDirectory / "common" / "scripted_triggers" / "strategic_nexus_generated_triggers.txt",
                files.scriptedTriggersText);
            const bool manifestWritten = common::writeTextFileAtomically(
                overlayDirectory / "strategic_nexus_generated_manifest.json",
                files.manifestText);
            if (!(eventsWritten && effectsWritten && triggersWritten && manifestWritten)) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=failed to write generated overlay files\n";
                return 1;
            }

            const generated_overlay::ManifestVerifier overlayVerifier;
            const auto overlayVerification = overlayVerifier.verify(overlayDirectory);
            if (!overlayVerification.ok) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=generated overlay verification failed: " << sanitizeCliValue(overlayVerification.reason) << "\n";
                return 1;
            }

            const StrategicNexusCompanion companion;
            const auto snapshot = companion.buildStatusSnapshot({
                config.offlineSpineArchiveDirectory,
                overlayDirectory,
                std::filesystem::path(),
                config.sncStartWithWindowsEnabled
            });
            const bool statusWritten = common::writeTextFileAtomically(
                config.offlineSpineStatusOutputPath,
                serializeCompanionStatusSnapshot(snapshot));
            if (!statusWritten) {
                std::cout << "offline_spine_success=false\n";
                std::cout << "offline_spine_reason=failed to write status snapshot\n";
                return 1;
            }

            std::cout << "offline_spine_success=true\n";
            std::cout << "offline_spine_archive_verified=true\n";
            std::cout << "offline_spine_save_count=" << summary.copiedSaveCount << "\n";
            std::cout << "offline_spine_ledger_written=true\n";
            std::cout << "offline_spine_brief_written=true\n";
            std::cout << "offline_spine_overlay_verified=true\n";
            std::cout << "offline_spine_status_center_state=" << snapshot.statusCenter.state << "\n";
            std::cout << "offline_spine_work_directory=" << sanitizeCliValue(config.offlineSpineWorkDirectory.generic_string()) << "\n";
            std::cout << "offline_spine_ledger_output_path=" << sanitizeCliValue(ledgerPath.generic_string()) << "\n";
            std::cout << "offline_spine_brief_output_path=" << sanitizeCliValue(briefPath.generic_string()) << "\n";
            std::cout << "offline_spine_overlay_output_path=" << sanitizeCliValue(overlayDirectory.generic_string()) << "\n";
            std::cout << "offline_spine_status_output_path=" << sanitizeCliValue(config.offlineSpineStatusOutputPath.generic_string()) << "\n";
            std::cout << "offline_spine_status_generated_at_local=" << sanitizeCliValue(snapshot.generatedAtLocal) << "\n";
            std::cout << "offline_spine_status_center_summary_text=" << sanitizeCliValue(snapshot.statusCenterSummaryText) << "\n";
            std::cout << "offline_spine_status_output_written=true\n";
            return 0;
        }

        if (config.archiveStableSavesMode) {
            if (config.autosaveArchiveSourceRoot.empty() ||
                config.autosaveArchiveRoot.empty() ||
                config.autosaveArchiveSessionId.empty()) {
                std::cout << "autosave_archive_success=false\n";
                std::cout << "autosave_archive_reason=missing source root, archive root, or session id\n";
                return 1;
            }

            const auto delayMs = config.autosaveArchiveStabilityDelayMs < 0
                ? 0
                : static_cast<std::uint32_t>(config.autosaveArchiveStabilityDelayMs);
            const AutosaveArchiver archiver;
            const auto result = archiver.archiveStableSaves(
                config.autosaveArchiveSourceRoot,
                config.autosaveArchiveRoot,
                config.autosaveArchiveSessionId,
                delayMs);
            const bool success = result.sourceExists;

            std::cout << "autosave_archive_success=" << (success ? "true" : "false") << "\n";
            std::cout << "autosave_archive_source_exists=" << (result.sourceExists ? "true" : "false") << "\n";
            std::cout << "autosave_archive_copied=" << result.copiedCount << "\n";
            std::cout << "autosave_archive_skipped=" << result.skippedCount << "\n";
            if (!success) {
                std::cout << "autosave_archive_reason=source root missing\n";
            }
            return success ? 0 : 1;
        }

        if (config.archiveLiveSavesMode) {
            if (config.autosaveArchiveSourceRoot.empty() ||
                config.autosaveArchiveRoot.empty() ||
                config.autosaveArchiveSessionId.empty()) {
                std::cout << "live_autosave_archive_success=false\n";
                std::cout << "live_autosave_archive_reason=missing source root, archive root, or session id\n";
                return 1;
            }

            const auto delayMs = config.autosaveArchiveStabilityDelayMs < 0
                ? 0
                : static_cast<std::uint32_t>(config.autosaveArchiveStabilityDelayMs);
            const AutosaveArchiver archiver;
            const auto result = archiver.archiveLiveSaves(
                config.autosaveArchiveSourceRoot,
                config.autosaveArchiveRoot,
                config.autosaveArchiveSessionId,
                delayMs);
            const bool success = result.sourceExists;

            std::cout << "live_autosave_archive_success=" << (success ? "true" : "false") << "\n";
            std::cout << "live_autosave_archive_source_exists=" << (result.sourceExists ? "true" : "false") << "\n";
            std::cout << "live_autosave_archive_copied=" << result.copiedCount << "\n";
            std::cout << "live_autosave_archive_skipped=" << result.skippedCount << "\n";
            std::cout << "live_autosave_archive_session_dir=" << sanitizeCliValue((config.autosaveArchiveRoot / config.autosaveArchiveSessionId).generic_string()) << "\n";
            if (!success) {
                std::cout << "live_autosave_archive_reason=source root missing\n";
            }
            return success ? 0 : 1;
        }

        if (config.verifyAutosaveArchiveMode) {
            const AutosaveArchiveVerifier verifier;
            const auto result = verifier.verify(config.autosaveArchiveVerifyDirectory);

            std::cout << "autosave_archive_manifest_ok=" << (result.ok ? "true" : "false") << "\n";
            std::cout << "autosave_archive_manifest_reason=" << sanitizeCliValue(result.reason) << "\n";
            std::cout << "autosave_archive_manifest_file_count=" << result.files.size() << "\n";
            for (const auto& file : result.files) {
                std::cout << "autosave_archive_manifest_file=" << file.path
                          << ";exists=" << (file.exists ? "true" : "false")
                          << ";hash_matches=" << (file.hashMatches ? "true" : "false")
                          << ";byte_count_matches=" << (file.byteCountMatches ? "true" : "false")
                          << "\n";
            }
            return result.ok ? 0 : 1;
        }

        if (config.summarizeAutosaveArchiveMode) {
            if (config.autosaveArchiveSummaryDirectory.empty() ||
                config.autosaveArchiveSummaryOutputPath.empty()) {
                std::cout << "autosave_archive_summary_success=false\n";
                std::cout << "autosave_archive_summary_reason=missing archive directory or output path\n";
                return 1;
            }

            const AutosaveArchiveSummarizer summarizer;
            const auto summary = summarizer.summarize(config.autosaveArchiveSummaryDirectory);
            const bool written = common::writeTextFileAtomically(
                config.autosaveArchiveSummaryOutputPath,
                serializeAutosaveArchiveSummary(summary));
            const bool success = summary.ok && written;

            std::cout << "autosave_archive_summary_success=" << (success ? "true" : "false") << "\n";
            std::cout << "autosave_archive_summary_reason=" << sanitizeCliValue(summary.reason) << "\n";
            std::cout << "autosave_archive_summary_save_count=" << summary.copiedSaveCount << "\n";
            std::cout << "autosave_archive_summary_output_written=" << (written ? "true" : "false") << "\n";
            return success ? 0 : 1;
        }

        if (config.buildMinistryInputFromArchiveMode) {
            if (config.archiveMinistryInputDirectory.empty() ||
                !hasNonWhitespace(config.archiveMinistryInputCampaignId) ||
                !hasNonWhitespace(config.archiveMinistryInputEmpireId) ||
                !hasNonWhitespace(config.archiveMinistryInputMinistry) ||
                config.archiveMinistryInputOutputPath.empty()) {
                std::cout << "archive_ministry_input_success=false\n";
                std::cout << "archive_ministry_input_reason=missing archive directory, identity, ministry, or output path\n";
                return 1;
            }

            const AutosaveArchiveSummarizer summarizer;
            const auto summary = summarizer.summarize(config.archiveMinistryInputDirectory);
            if (!summary.ok) {
                std::cout << "archive_ministry_input_success=false\n";
                std::cout << "archive_ministry_input_reason=" << sanitizeCliValue(summary.reason) << "\n";
                return 1;
            }

            SaveParserSummary parsedHeadline;
            const bool hasParsedHeadline = parseLatestArchivedSaveHeadline(summary, parsedHeadline);
            const SeasonDeltaLedgerBuilder ledgerBuilder;
            const auto ledger = ledgerBuilder.build(
                summary,
                config.archiveMinistryInputCampaignId,
                hasParsedHeadline ? &parsedHeadline : nullptr);
            if (!ledger.ok) {
                std::cout << "archive_ministry_input_success=false\n";
                std::cout << "archive_ministry_input_reason=" << sanitizeCliValue(ledger.reason) << "\n";
                return 1;
            }

            const ArchiveMinistryInputBuilder builder;
            const auto input = builder.build(
                ledger,
                config.archiveMinistryInputEmpireId,
                config.archiveMinistryInputMinistry);
            const bool written = common::writeTextFileAtomically(
                config.archiveMinistryInputOutputPath,
                strategic_pipeline::serializeMinistryInputContext(input));

            std::cout << "archive_ministry_input_success=" << (written ? "true" : "false") << "\n";
            std::cout << "archive_ministry_input_campaign_id=" << input.campaignId << "\n";
            std::cout << "archive_ministry_input_empire_id=" << input.empireId << "\n";
            std::cout << "archive_ministry_input_ministry=" << input.ministry << "\n";
            std::cout << "archive_ministry_input_output_written=" << (written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "archive_ministry_input_reason=failed to write ministry input context\n";
            }
            return written ? 0 : 1;
        }

        if (config.v0PipelineFromArchiveMode) {
            if (config.archivePipelineDirectory.empty() ||
                !hasNonWhitespace(config.archivePipelineCampaignId) ||
                !hasNonWhitespace(config.archivePipelineEmpireId) ||
                !hasNonWhitespace(config.archivePipelineMinistry) ||
                config.archivePipelineMinistryInputOutputPath.empty() ||
                config.archivePipelineDecisionOutputPath.empty()) {
                std::cout << "archive_v0_pipeline_success=false\n";
                std::cout << "archive_v0_pipeline_reason=missing archive directory, identity, ministry, ministry input output, or decision output\n";
                return 1;
            }

            const AutosaveArchiveSummarizer summarizer;
            const auto summary = summarizer.summarize(config.archivePipelineDirectory);
            if (!summary.ok) {
                std::cout << "archive_v0_pipeline_success=false\n";
                std::cout << "archive_v0_pipeline_reason=" << sanitizeCliValue(summary.reason) << "\n";
                return 1;
            }

            SaveParserSummary parsedHeadline;
            const bool hasParsedHeadline = parseLatestArchivedSaveHeadline(summary, parsedHeadline);
            const SeasonDeltaLedgerBuilder ledgerBuilder;
            const auto ledger = ledgerBuilder.build(
                summary,
                config.archivePipelineCampaignId,
                hasParsedHeadline ? &parsedHeadline : nullptr);
            if (!ledger.ok) {
                std::cout << "archive_v0_pipeline_success=false\n";
                std::cout << "archive_v0_pipeline_reason=" << sanitizeCliValue(ledger.reason) << "\n";
                return 1;
            }

            const ArchiveMinistryInputBuilder builder;
            const auto input = builder.build(
                ledger,
                config.archivePipelineEmpireId,
                config.archivePipelineMinistry);
            const bool inputWritten = common::writeTextFileAtomically(
                config.archivePipelineMinistryInputOutputPath,
                strategic_pipeline::serializeMinistryInputContext(input));
            if (!inputWritten) {
                std::cout << "archive_v0_pipeline_success=false\n";
                std::cout << "archive_v0_pipeline_reason=failed to write ministry input context\n";
                return 1;
            }

            const strategic_pipeline::V0StrategicPipeline pipeline;
            const strategic_pipeline::PipelineRunResult result = pipeline.run({
                config.archivePipelineMinistryInputOutputPath,
                config.archivePipelineDecisionOutputPath,
                config.archivePipelineAuditOutputPath,
                config.v0SequenceId,
                config.v0CreatedUnixMs,
                config.v0TtlMs
            });

            std::cout << "archive_v0_pipeline_success=" << (result.success ? "true" : "false") << "\n";
            std::cout << "archive_v0_pipeline_reason=" << sanitizeCliValue(result.reason) << "\n";
            std::cout << "archive_v0_pipeline_ministry_input_written=true\n";
            std::cout << "archive_v0_pipeline_decision_written=" << (result.decisionOutputWritten ? "true" : "false") << "\n";
            std::cout << "archive_v0_pipeline_audit_requested=" << (result.auditOutputRequested ? "true" : "false") << "\n";
            std::cout << "archive_v0_pipeline_audit_written=" << (result.auditOutputWritten ? "true" : "false") << "\n";
            std::cout << "archive_v0_pipeline_campaign_id=" << sanitizeCliValue(result.input.campaignId) << "\n";
            std::cout << "archive_v0_pipeline_empire_id=" << sanitizeCliValue(result.input.empireId) << "\n";
            std::cout << "archive_v0_pipeline_military_posture=" << sanitizeCliValue(result.payload.militaryPosture) << "\n";
            std::cout << "archive_v0_pipeline_research_bias=" << sanitizeCliValue(result.payload.researchBias) << "\n";
            if (!result.auditReason.empty()) {
                std::cout << "archive_v0_pipeline_audit_reason=" << sanitizeCliValue(result.auditReason) << "\n";
            }
            return result.success ? 0 : 1;
        }

	        if (config.buildSeasonDeltaLedgerMode) {
	            if (config.seasonDeltaLedgerDirectory.empty() ||
	                !hasNonWhitespace(config.seasonDeltaLedgerCampaignId) ||
	                config.seasonDeltaLedgerOutputPath.empty()) {
	                std::cout << "season_delta_ledger_success=false\n";
	                std::cout << "season_delta_ledger_reason=missing archive directory, campaign id, or output path\n";
	                return 1;
	            }

	            const AutosaveArchiveSummarizer summarizer;
	            const auto summary = summarizer.summarize(config.seasonDeltaLedgerDirectory);

	            SaveParserSummary parsedHeadline;
	            const bool hasParsedHeadline = parseLatestArchivedSaveHeadline(summary, parsedHeadline);
	            const SeasonDeltaLedgerBuilder builder;
	            const auto ledger = builder.build(
                    summary,
                    config.seasonDeltaLedgerCampaignId,
                    hasParsedHeadline ? &parsedHeadline : nullptr);
	            const bool written = common::writeTextFileAtomically(
	                config.seasonDeltaLedgerOutputPath,
                serializeSeasonDeltaLedger(ledger));

            const bool success = ledger.ok && written;
            std::string reason = ledger.reason;
            if (ledger.ok && !written) {
                reason = "failed to write season delta ledger";
            }
            std::cout << "season_delta_ledger_success=" << (success ? "true" : "false") << "\n";
            std::cout << "season_delta_ledger_reason=" << sanitizeCliValue(reason) << "\n";
            std::cout << "season_delta_ledger_campaign_id=" << sanitizeCliValue(ledger.campaignId) << "\n";
            std::cout << "season_delta_ledger_save_count=" << ledger.copiedSaveCount << "\n";
            std::cout << "season_delta_ledger_output_path=" << sanitizeCliValue(config.seasonDeltaLedgerOutputPath.generic_string()) << "\n";
            std::cout << "season_delta_ledger_output_written=" << (written ? "true" : "false") << "\n";
            return success ? 0 : 1;
        }

	        if (config.buildEmpireBriefFromArchiveMode) {
	            if (config.archiveEmpireBriefDirectory.empty() ||
	                !hasNonWhitespace(config.archiveEmpireBriefCampaignId) ||
	                !hasNonWhitespace(config.archiveEmpireBriefEmpireId) ||
	                config.archiveEmpireBriefOutputPath.empty()) {
	                std::cout << "archive_empire_brief_success=false\n";
	                std::cout << "archive_empire_brief_reason=missing archive directory, identity, or output path\n";
	                return 1;
	            }

	            const AutosaveArchiveSummarizer summarizer;
	            const auto summary = summarizer.summarize(config.archiveEmpireBriefDirectory);

	            SaveParserSummary parsedHeadline;
	            const bool hasParsedHeadline = parseLatestArchivedSaveHeadline(summary, parsedHeadline);
	            const SeasonDeltaLedgerBuilder ledgerBuilder;
	            const auto ledger = ledgerBuilder.build(
                    summary,
                    config.archiveEmpireBriefCampaignId,
                    hasParsedHeadline ? &parsedHeadline : nullptr);
	            const SeasonEmpireBriefBuilder briefBuilder;
            const auto brief = briefBuilder.build(ledger, config.archiveEmpireBriefEmpireId);
            const bool written = common::writeTextFileAtomically(
                config.archiveEmpireBriefOutputPath,
                serializeSeasonEmpireBrief(brief));

            const bool success = brief.ok && written;
            std::string reason = brief.reason;
            if (brief.ok && !written) {
                reason = "failed to write empire brief";
            }
            std::cout << "archive_empire_brief_success=" << (success ? "true" : "false") << "\n";
            std::cout << "archive_empire_brief_reason=" << sanitizeCliValue(reason) << "\n";
            std::cout << "archive_empire_brief_campaign_id=" << sanitizeCliValue(brief.campaignId) << "\n";
            std::cout << "archive_empire_brief_empire_id=" << sanitizeCliValue(brief.empireId) << "\n";
            std::cout << "archive_empire_brief_output_path=" << sanitizeCliValue(config.archiveEmpireBriefOutputPath.generic_string()) << "\n";
            std::cout << "archive_empire_brief_output_written=" << (written ? "true" : "false") << "\n";
            return success ? 0 : 1;
        }

        if (config.scanSaveCampaignsMode) {
            if (config.saveCampaignScanRoot.empty() || config.saveCampaignScanOutputPath.empty()) {
                std::cout << "save_campaign_scan_success=false\n";
                std::cout << "save_campaign_scan_reason=missing save root or output path\n";
                return 1;
            }

            const CampaignSaveScanner scanner;
            const auto inventory = scanner.scan(config.saveCampaignScanRoot);
            const bool written = common::writeTextFileAtomically(
                config.saveCampaignScanOutputPath,
                serializeCampaignSaveInventory(inventory));

            std::cout << "save_campaign_scan_success=" << (written ? "true" : "false") << "\n";
            std::cout << "save_campaign_scan_root_exists=" << (inventory.rootExists ? "true" : "false") << "\n";
            std::cout << "save_campaign_scan_campaign_count=" << inventory.entries.size() << "\n";
            std::cout << "save_campaign_scan_output_written=" << (written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "save_campaign_scan_reason=failed to write campaign inventory\n";
            }
            return written ? 0 : 1;
        }

        if (config.diffSaveCampaignsMode) {
            if (config.saveCampaignPreviousRoot.empty() ||
                config.saveCampaignCurrentRoot.empty() ||
                config.saveCampaignDiffOutputPath.empty()) {
                std::cout << "save_campaign_diff_success=false\n";
                std::cout << "save_campaign_diff_reason=missing previous root, current root, or output path\n";
                return 1;
            }

            const CampaignSaveScanner scanner;
            const auto previous = scanner.scan(config.saveCampaignPreviousRoot);
            const auto current = scanner.scan(config.saveCampaignCurrentRoot);
            const auto diff = diffCampaignSaveInventories(previous, current);
            const bool written = common::writeTextFileAtomically(
                config.saveCampaignDiffOutputPath,
                serializeCampaignSaveInventoryDiff(diff));

            std::cout << "save_campaign_diff_success=" << (written ? "true" : "false") << "\n";
            std::cout << "save_campaign_diff_added=" << diff.addedCount << "\n";
            std::cout << "save_campaign_diff_removed=" << diff.removedCount << "\n";
            std::cout << "save_campaign_diff_renamed=" << diff.renamedCount << "\n";
            std::cout << "save_campaign_diff_restored=" << diff.restoredCount << "\n";
            std::cout << "save_campaign_diff_changed=" << diff.changedCount << "\n";
            std::cout << "save_campaign_diff_unchanged=" << diff.unchangedCount << "\n";
            std::cout << "save_campaign_diff_output_written=" << (written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "save_campaign_diff_reason=failed to write campaign inventory diff\n";
            }
            return written ? 0 : 1;
        }

        if (config.planCampaignLibraryMode) {
            if (config.campaignLibraryPlanRoot.empty() || config.campaignLibraryPlanOutputPath.empty()) {
                std::cout << "campaign_library_plan_success=false\n";
                std::cout << "campaign_library_plan_reason=missing save root or output path\n";
                return 1;
            }

            const auto maxCampaigns = config.campaignLibraryMaxCampaigns < 0
                ? 0
                : static_cast<std::size_t>(config.campaignLibraryMaxCampaigns);
            const CampaignSaveScanner scanner;
            const CampaignLibraryPlanner planner;
            const auto inventory = scanner.scan(config.campaignLibraryPlanRoot);
            const auto plan = planner.build(inventory, maxCampaigns);
            const bool written = common::writeTextFileAtomically(
                config.campaignLibraryPlanOutputPath,
                serializeCampaignLibraryPlan(plan));

            std::cout << "campaign_library_plan_success=" << (written ? "true" : "false") << "\n";
            std::cout << "campaign_library_plan_root_exists=" << (inventory.rootExists ? "true" : "false") << "\n";
            std::cout << "campaign_library_plan_limit_reached=" << (plan.limitReached ? "true" : "false") << "\n";
            std::cout << "campaign_library_plan_included=" << plan.includedCount << "\n";
            std::cout << "campaign_library_plan_skipped=" << plan.skippedCount << "\n";
            std::cout << "campaign_library_plan_skipped_due_to_limit=" << plan.skippedDueToLimitCount << "\n";
            std::cout << "campaign_library_plan_output_written=" << (written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "campaign_library_plan_reason=failed to write campaign library plan\n";
            }
            return written ? 0 : 1;
        }

        if (config.compileCampaignLibraryOverlayMode) {
            if (config.campaignLibraryOverlayDslInputPath.empty() ||
                config.campaignLibraryOverlaySaveRoot.empty() ||
                config.campaignLibraryOverlayOutputDirectory.empty()) {
                std::cout << "campaign_library_overlay_success=false\n";
                std::cout << "campaign_library_overlay_reason=missing DSL input, save root, or output directory\n";
                return 1;
            }

            if (!std::filesystem::exists(config.campaignLibraryOverlayDslInputPath)) {
                std::cout << "campaign_library_overlay_success=false\n";
                std::cout << "campaign_library_overlay_reason=missing DSL input\n";
                return 1;
            }

            const auto outputDirectory = config.campaignLibraryOverlayOutputDirectory;
            {
                std::error_code ec;
                if (std::filesystem::exists(outputDirectory, ec)) {
                    if (ec) {
                        std::cout << "campaign_library_overlay_success=false\n";
                        std::cout << "campaign_library_overlay_reason=failed to inspect output directory\n";
                        return 1;
                    }

                    if (!std::filesystem::is_directory(outputDirectory, ec) || ec) {
                        std::cout << "campaign_library_overlay_success=false\n";
                        std::cout << "campaign_library_overlay_reason=output path is not a directory\n";
                        return 1;
                    }

                    if (!std::filesystem::is_empty(outputDirectory, ec) || ec) {
                        std::cout << "campaign_library_overlay_success=false\n";
                        std::cout << "campaign_library_overlay_reason=output directory must be empty\n";
                        return 1;
                    }
                }
            }

            const auto maxCampaigns = config.campaignLibraryOverlayMaxCampaigns < 0
                ? 0
                : static_cast<std::size_t>(config.campaignLibraryOverlayMaxCampaigns);
            const CampaignSaveScanner scanner;
            const CampaignLibraryPlanner planner;
            const auto inventory = scanner.scan(config.campaignLibraryOverlaySaveRoot);
            if (!inventory.rootExists) {
                std::cout << "campaign_library_overlay_success=false\n";
                std::cout << "campaign_library_overlay_reason=save root unavailable\n";
                return 1;
            }
            const auto plan = planner.build(inventory, maxCampaigns);
            std::set<std::string> includedCampaignKeys;
            for (const auto& entry : plan.entries) {
                if (entry.status == "included") {
                    includedCampaignKeys.insert(entry.campaignKey);
                }
            }

            const generated_overlay::DslParser parser;
            const auto parseResult = parser.parse(common::readTextFile(config.campaignLibraryOverlayDslInputPath));
            if (!parseResult.ok) {
                std::cout << "campaign_library_overlay_success=false\n";
                std::cout << "campaign_library_overlay_reason=parse failed: " << sanitizeCliValue(parseResult.error) << "\n";
                return 1;
            }

            generated_overlay::DslProgram filteredProgram;
            std::size_t skippedRules = 0;
            for (const auto& rule : parseResult.program.rules) {
                if (includedCampaignKeys.find(rule.campaignId) != includedCampaignKeys.end()) {
                    filteredProgram.rules.push_back(rule);
                } else {
                    ++skippedRules;
                }
            }

            const generated_overlay::DslValidator validator;
            const auto validation = validator.validate(filteredProgram);
            if (!validation.ok) {
                std::cout << "campaign_library_overlay_success=false\n";
                std::cout << "campaign_library_overlay_reason=validation failed\n";
                for (const auto& error : validation.errors) {
                    std::cout << "campaign_library_overlay_error=" << sanitizeCliValue(error) << "\n";
                }
                return 1;
            }

            const auto runtimeConditionErrors =
                generated_overlay::findUnsupportedRuntimeConditionErrors(filteredProgram);
            if (!runtimeConditionErrors.empty()) {
                std::cout << "campaign_library_overlay_success=false\n";
                std::cout << "campaign_library_overlay_reason=runtime conditions unsupported\n";
                for (const auto& error : runtimeConditionErrors) {
                    std::cout << "campaign_library_overlay_error=" << sanitizeCliValue(error) << "\n";
                }
                return 1;
            }

            const generated_overlay::OverlayCompiler compiler;
            const auto files = compiler.compile(filteredProgram);
            {
                std::error_code ec;
                std::filesystem::create_directories(outputDirectory, ec);
                if (ec) {
                    std::cout << "campaign_library_overlay_success=false\n";
                    std::cout << "campaign_library_overlay_reason=failed to create output directory\n";
                    return 1;
                }
            }
            const bool eventsWritten = common::writeTextFileAtomically(
                outputDirectory / "events" / "strategic_nexus_generated_events.txt",
                files.eventsText);
            const bool effectsWritten = common::writeTextFileAtomically(
                outputDirectory / "common" / "scripted_effects" / "strategic_nexus_generated_effects.txt",
                files.scriptedEffectsText);
            const bool triggersWritten = common::writeTextFileAtomically(
                outputDirectory / "common" / "scripted_triggers" / "strategic_nexus_generated_triggers.txt",
                files.scriptedTriggersText);
            const bool manifestWritten = common::writeTextFileAtomically(
                outputDirectory / "strategic_nexus_generated_manifest.json",
                files.manifestText);
            const bool planWritten = common::writeTextFileAtomically(
                outputDirectory / "strategic_nexus_campaign_library_plan.json",
                serializeCampaignLibraryPlan(plan));

            const bool success = eventsWritten && effectsWritten && triggersWritten && manifestWritten && planWritten;
            std::cout << "campaign_library_overlay_success=" << (success ? "true" : "false") << "\n";
            std::cout << "campaign_library_overlay_rules_included=" << filteredProgram.rules.size() << "\n";
            std::cout << "campaign_library_overlay_rules_skipped=" << skippedRules << "\n";
            std::cout << "campaign_library_overlay_limit_reached=" << (plan.limitReached ? "true" : "false") << "\n";
            std::cout << "campaign_library_overlay_campaigns_included=" << plan.includedCount << "\n";
            std::cout << "campaign_library_overlay_campaigns_skipped_due_to_limit=" << plan.skippedDueToLimitCount << "\n";
            std::cout << "campaign_library_overlay_plan_written=" << (planWritten ? "true" : "false") << "\n";
            if (!success) {
                std::cout << "campaign_library_overlay_reason=failed to write campaign library overlay files\n";
            }
            return success ? 0 : 1;
        }

        if (config.discoverStellarisSaveRootsMode) {
            if (config.stellarisSaveRootsOutputPath.empty()) {
                std::cout << "stellaris_save_roots_success=false\n";
                std::cout << "stellaris_save_roots_reason=missing output path\n";
                return 1;
            }

            const StellarisSavePathResolver resolver;
            const auto discovery = resolver.discoverFromEnvironment();
            const bool written = common::writeTextFileAtomically(
                config.stellarisSaveRootsOutputPath,
                serializeStellarisSaveRootDiscovery(discovery));

            std::cout << "stellaris_save_roots_success=" << (written ? "true" : "false") << "\n";
            std::cout << "stellaris_save_roots_candidate_count=" << discovery.candidates.size() << "\n";
            std::cout << "stellaris_save_roots_output_written=" << (written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "stellaris_save_roots_reason=failed to write save root discovery\n";
            }
            return written ? 0 : 1;
        }

        if (config.detectStellarisRunningMode) {
            const StellarisProcessDetector detector;
            const auto status = detector.detectFromSystem();
            const auto json = serializeStellarisProcessStatus(status);
            const bool outputRequested = !config.stellarisRunningOutputPath.empty();
            const bool written = outputRequested ? common::writeTextFileAtomically(config.stellarisRunningOutputPath, json) : true;

            std::cout << "stellaris_process_detection_available="
                      << (status.detectionAvailable ? "true" : "false") << "\n";
            std::cout << "stellaris_running=" << (status.running ? "true" : "false") << "\n";
            std::cout << "stellaris_running_reason=" << sanitizeCliValue(status.reason) << "\n";
            std::cout << "stellaris_running_match_count=" << status.matchedProcessNames.size() << "\n";
            for (const auto& processName : status.matchedProcessNames) {
                std::cout << "stellaris_running_match=" << sanitizeCliValue(processName) << "\n";
            }
            std::cout << "stellaris_running_output_written=" << (outputRequested && written ? "true" : "false") << "\n";
            return written ? 0 : 1;
        }

        if (config.parseStellarisSaveMode) {
            if (config.stellarisSaveParseInputPath.empty() || config.stellarisSaveParseOutputPath.empty()) {
                std::cout << "stellaris_save_parse_success=false\n";
                std::cout << "stellaris_save_parse_reason=missing save input or output path\n";
                return 1;
            }

            const SaveParser parser;
            const SaveParserSummary summary = parser.parseSummary(config.stellarisSaveParseInputPath);
            const bool written = common::writeTextFileAtomically(
                config.stellarisSaveParseOutputPath,
                parser.serializeSummaryJson(summary));
            const bool success = summary.ok && written;

            std::cout << "stellaris_save_parse_success=" << (success ? "true" : "false") << "\n";
            std::cout << "stellaris_save_parse_reason=" << sanitizeCliValue(summary.reason) << "\n";
            std::cout << "stellaris_save_parse_player_country_id=" << sanitizeCliValue(summary.playerCountryId) << "\n";
            std::cout << "stellaris_save_parse_empire_name=" << sanitizeCliValue(summary.empireName) << "\n";
            std::cout << "stellaris_save_parse_save_date=" << sanitizeCliValue(summary.saveDate) << "\n";
            std::cout << "stellaris_save_parse_output_written=" << (written ? "true" : "false") << "\n";
            return success ? 0 : 1;
        }

        if (config.analyzeSaveEntryPointsMode) {
            if (config.saveEntryPointArchiveDirectory.empty() ||
                config.saveEntryPointAnalysisOutputPath.empty()) {
                std::cout << "save_entry_point_analysis_success=false\n";
                std::cout << "save_entry_point_analysis_reason=missing archive directory or output path\n";
                return 1;
            }

            std::vector<std::filesystem::path> roots = config.saveEntryPointRoots;
            if (roots.empty()) {
                const StellarisSavePathResolver resolver;
                const auto discovery = resolver.discoverFromEnvironment();
                for (const auto& candidate : discovery.candidates) {
                    if (candidate.exists) {
                        roots.push_back(candidate.path);
                    }
                }
            }

            const SaveEntryPointAnalyzer analyzer;
            const auto analysis = analyzer.analyze(config.saveEntryPointArchiveDirectory, roots);
            const bool written = common::writeTextFileAtomically(
                config.saveEntryPointAnalysisOutputPath,
                serializeSaveEntryPointAnalysis(analysis));
            const bool success = analysis.ok && written;

            std::cout << "save_entry_point_analysis_success=" << (success ? "true" : "false") << "\n";
            std::cout << "save_entry_point_analysis_reason=" << sanitizeCliValue(analysis.reason) << "\n";
            std::cout << "save_entry_point_analysis_readiness=" << sanitizeCliValue(analysis.readiness) << "\n";
            std::cout << "save_entry_point_analysis_entry_points=" << analysis.entryPointCount << "\n";
            std::cout << "save_entry_point_analysis_archived_evidence=" << analysis.archivedEvidenceCount << "\n";
            std::cout << "save_entry_point_analysis_branch_ambiguity="
                      << (analysis.branchAmbiguityDetected ? "true" : "false") << "\n";
            std::cout << "save_entry_point_analysis_output_written=" << (written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "save_entry_point_analysis_write_reason=failed to write entry point analysis\n";
            }
            return success ? 0 : 1;
        }

        if (config.buildPostPlayPackageMode) {
            if (config.postPlayPackageArchiveDirectory.empty() ||
                config.postPlayPackageOutputPath.empty()) {
                std::cout << "post_play_package_success=false\n";
                std::cout << "post_play_package_reason=missing archive directory or output path\n";
                return 1;
            }

            std::vector<std::filesystem::path> roots = config.postPlayPackageSaveRoots;
            if (roots.empty()) {
                const StellarisSavePathResolver resolver;
                const auto discovery = resolver.discoverFromEnvironment();
                for (const auto& candidate : discovery.candidates) {
                    if (candidate.exists) {
                        roots.push_back(candidate.path);
                    }
                }
            }

            const AutosaveArchiveSummarizer summarizer;
            const SaveEntryPointAnalyzer analyzer;
            const PostPlayPackageBuilder builder;
            const auto archiveSummary = summarizer.summarize(config.postPlayPackageArchiveDirectory);
            const auto analysis = analyzer.analyze(config.postPlayPackageArchiveDirectory, roots);
            const auto package = builder.build(archiveSummary, analysis);
            const bool written = common::writeTextFileAtomically(
                config.postPlayPackageOutputPath,
                serializePostPlayPackage(package));
            const bool success = package.ok && written;

            std::cout << "post_play_package_success=" << (success ? "true" : "false") << "\n";
            std::cout << "post_play_package_reason=" << sanitizeCliValue(package.reason) << "\n";
            std::cout << "post_play_package_readiness=" << sanitizeCliValue(package.readiness) << "\n";
            std::cout << "post_play_package_entry_points=" << package.entryPointCount << "\n";
            std::cout << "post_play_package_decision_ready_entries=" << package.decisionReadyEntryCount << "\n";
            std::cout << "post_play_package_archived_evidence=" << package.archivedEvidenceCount << "\n";
            std::cout << "post_play_package_dry_run_only=" << (package.dryRunOnly ? "true" : "false") << "\n";
            std::cout << "post_play_package_output_written=" << (written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "post_play_package_write_reason=failed to write post-play package\n";
            }
            return success ? 0 : 1;
        }

        if (config.buildSncDecisionInputPackageMode) {
            if (config.sncDecisionInputPostPlayPackagePath.empty() ||
                config.sncDecisionInputOutputPath.empty()) {
                std::cout << "snc_decision_input_package_success=false\n";
                std::cout << "snc_decision_input_package_reason=missing post-play package path or output path\n";
                return 1;
            }

            std::string postPlayJson;
            if (!common::tryReadTextFile(config.sncDecisionInputPostPlayPackagePath, postPlayJson)) {
                std::cout << "snc_decision_input_package_success=false\n";
                std::cout << "snc_decision_input_package_reason=failed to read post-play package\n";
                return 1;
            }

            const auto readResult = parsePostPlayPackageJson(postPlayJson);
            if (!readResult.ok) {
                std::cout << "snc_decision_input_package_success=false\n";
                std::cout << "snc_decision_input_package_reason=" << sanitizeCliValue(readResult.reason) << "\n";
                return 1;
            }

            const SncDecisionInputPackageBuilder builder;
            const auto package = builder.build(
                readResult.package,
                config.sncDecisionInputPostPlayPackagePath);
            const bool written = common::writeTextFileAtomically(
                config.sncDecisionInputOutputPath,
                serializeSncDecisionInputPackage(package));
            const bool success = package.ok && written;

            std::cout << "snc_decision_input_package_success=" << (success ? "true" : "false") << "\n";
            std::cout << "snc_decision_input_package_reason=" << sanitizeCliValue(package.reason) << "\n";
            std::cout << "snc_decision_input_package_readiness=" << sanitizeCliValue(package.readiness) << "\n";
            std::cout << "snc_decision_input_package_entry_points=" << package.entryPointCount << "\n";
            std::cout << "snc_decision_input_package_decision_inputs=" << package.decisionInputCount << "\n";
            std::cout << "snc_decision_input_package_blocked_entries=" << package.blockedEntryCount << "\n";
            std::cout << "snc_decision_input_package_dry_run_only=" << (package.dryRunOnly ? "true" : "false") << "\n";
            std::cout << "snc_decision_input_package_output_written=" << (written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "snc_decision_input_package_write_reason=failed to write decision input package\n";
            }
            return success ? 0 : 1;
        }

        if (config.buildSncCandidateDecisionPackageMode) {
            if (config.sncCandidateDecisionInputPackagePath.empty() ||
                config.sncCandidateDecisionOutputPath.empty()) {
                std::cout << "snc_candidate_decision_package_success=false\n";
                std::cout << "snc_candidate_decision_package_reason=missing decision input package path or output path\n";
                return 1;
            }

            std::string decisionInputJson;
            if (!common::tryReadTextFile(config.sncCandidateDecisionInputPackagePath, decisionInputJson)) {
                std::cout << "snc_candidate_decision_package_success=false\n";
                std::cout << "snc_candidate_decision_package_reason=failed to read decision input package\n";
                return 1;
            }

            const auto readResult = parseSncDecisionInputPackageJson(decisionInputJson);
            if (!readResult.ok) {
                std::cout << "snc_candidate_decision_package_success=false\n";
                std::cout << "snc_candidate_decision_package_reason=" << sanitizeCliValue(readResult.reason) << "\n";
                return 1;
            }

            const SncCandidateDecisionPackageBuilder builder;
            const auto package = builder.build(
                readResult.package,
                config.sncCandidateDecisionInputPackagePath);
            const bool written = common::writeTextFileAtomically(
                config.sncCandidateDecisionOutputPath,
                serializeSncCandidateDecisionPackage(package));
            const bool success = package.ok && written;

            std::cout << "snc_candidate_decision_package_success=" << (success ? "true" : "false") << "\n";
            std::cout << "snc_candidate_decision_package_reason=" << sanitizeCliValue(package.reason) << "\n";
            std::cout << "snc_candidate_decision_package_readiness=" << sanitizeCliValue(package.readiness) << "\n";
            std::cout << "snc_candidate_decision_package_candidate_decisions=" << package.candidateDecisionCount << "\n";
            std::cout << "snc_candidate_decision_package_accepted_candidates=" << package.acceptedCandidateCount << "\n";
            std::cout << "snc_candidate_decision_package_rejected_candidates=" << package.rejectedCandidateCount << "\n";
            std::cout << "snc_candidate_decision_package_blocked_source_entries=" << package.blockedSourceEntryCount << "\n";
            std::cout << "snc_candidate_decision_package_validator_passed=" << (package.validatorPassed ? "true" : "false") << "\n";
            std::cout << "snc_candidate_decision_package_dry_run_only=" << (package.dryRunOnly ? "true" : "false") << "\n";
            std::cout << "snc_candidate_decision_package_output_written=" << (written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "snc_candidate_decision_package_write_reason=failed to write candidate decision package\n";
            }
            return success ? 0 : 1;
        }

        if (config.buildSncDslDraftPackageMode) {
            if (config.sncDslDraftCandidatePackagePath.empty() ||
                config.sncDslDraftOutputPath.empty() ||
                config.sncDslDraftAuditOutputPath.empty()) {
                std::cout << "snc_dsl_draft_package_success=false\n";
                std::cout << "snc_dsl_draft_package_reason=missing candidate package path, DSL output path, or audit output path\n";
                return 1;
            }

            std::string candidatePackageJson;
            if (!common::tryReadTextFile(config.sncDslDraftCandidatePackagePath, candidatePackageJson)) {
                std::cout << "snc_dsl_draft_package_success=false\n";
                std::cout << "snc_dsl_draft_package_reason=failed to read candidate decision package\n";
                return 1;
            }

            const auto readResult = parseSncCandidateDecisionPackageJson(candidatePackageJson);
            if (!readResult.ok) {
                std::cout << "snc_dsl_draft_package_success=false\n";
                std::cout << "snc_dsl_draft_package_reason=" << sanitizeCliValue(readResult.reason) << "\n";
                return 1;
            }

            const SncDslDraftPackageBuilder builder;
            const auto package = builder.build(
                readResult.package,
                config.sncDslDraftCandidatePackagePath);
            const bool dslWritten = package.ok && common::writeTextFileAtomically(
                config.sncDslDraftOutputPath,
                package.dslText);
            const bool auditWritten = common::writeTextFileAtomically(
                config.sncDslDraftAuditOutputPath,
                serializeSncDslDraftPackage(package));
            const bool success = package.ok && dslWritten && auditWritten;

            std::cout << "snc_dsl_draft_package_success=" << (success ? "true" : "false") << "\n";
            std::cout << "snc_dsl_draft_package_reason=" << sanitizeCliValue(package.reason) << "\n";
            std::cout << "snc_dsl_draft_package_readiness=" << sanitizeCliValue(package.readiness) << "\n";
            std::cout << "snc_dsl_draft_package_candidate_decisions=" << package.candidateDecisionCount << "\n";
            std::cout << "snc_dsl_draft_package_eligible_candidates=" << package.eligibleCandidateCount << "\n";
            std::cout << "snc_dsl_draft_package_skipped_candidates=" << package.skippedCandidateCount << "\n";
            std::cout << "snc_dsl_draft_package_dsl_rules=" << package.dslRuleCount << "\n";
            std::cout << "snc_dsl_draft_package_validator_passed=" << (package.validatorPassed ? "true" : "false") << "\n";
            std::cout << "snc_dsl_draft_package_dry_run_only=" << (package.dryRunOnly ? "true" : "false") << "\n";
            std::cout << "snc_dsl_draft_package_publishes_overlay=" << (package.publishesOverlay ? "true" : "false") << "\n";
            std::cout << "snc_dsl_draft_package_overlay_compile_allowed=" << (package.overlayCompileAllowed ? "true" : "false") << "\n";
            std::cout << "snc_dsl_draft_package_dsl_output_written=" << (dslWritten ? "true" : "false") << "\n";
            std::cout << "snc_dsl_draft_package_audit_output_written=" << (auditWritten ? "true" : "false") << "\n";
            if (package.ok && !dslWritten) {
                std::cout << "snc_dsl_draft_package_write_reason=failed to write DSL draft\n";
            }
            if (!auditWritten) {
                std::cout << "snc_dsl_draft_package_audit_write_reason=failed to write DSL draft audit\n";
            }
            return success ? 0 : 1;
        }

        if (config.stageSncGeneratedOverlayMode) {
            if (config.sncGeneratedOverlayDslPath.empty() ||
                config.sncGeneratedOverlayStagingDirectory.empty() ||
                config.sncGeneratedOverlayStagingStatusOutputPath.empty()) {
                std::cout << "snc_generated_overlay_stage_success=false\n";
                std::cout << "snc_generated_overlay_stage_reason=missing DSL path, staging directory, or status output path\n";
                return 1;
            }

            const SncGeneratedOverlayStager stager;
            const auto status = stager.stage(
                config.sncGeneratedOverlayDslPath,
                config.sncGeneratedOverlayStagingDirectory);
            const bool statusWritten = common::writeTextFileAtomically(
                config.sncGeneratedOverlayStagingStatusOutputPath,
                serializeSncGeneratedOverlayStagingStatus(status));
            const bool success = status.ok && statusWritten;

            std::cout << "snc_generated_overlay_stage_success=" << (success ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_stage_reason=" << sanitizeCliValue(status.reason) << "\n";
            std::cout << "snc_generated_overlay_stage_readiness=" << sanitizeCliValue(status.readiness) << "\n";
            std::cout << "snc_generated_overlay_stage_dsl_rules=" << status.dslRuleCount << "\n";
            std::cout << "snc_generated_overlay_stage_validator_passed=" << (status.validatorPassed ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_stage_manifest_verified=" << (status.manifestVerified ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_stage_manifest_hash=" << sanitizeCliValue(status.manifestHash) << "\n";
            std::cout << "snc_generated_overlay_stage_publish_allowed=" << (status.publishAllowed ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_stage_publishes_overlay=" << (status.publishesOverlay ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_stage_status_output_written=" << (statusWritten ? "true" : "false") << "\n";
            if (!statusWritten) {
                std::cout << "snc_generated_overlay_stage_status_write_reason=failed to write staging status\n";
            }
            return success ? 0 : 1;
        }

        if (config.publishSncGeneratedOverlayMode) {
            if (config.sncGeneratedOverlayPublishStagingStatusPath.empty() ||
                config.sncGeneratedOverlayPublishActiveDirectory.empty() ||
                config.sncGeneratedOverlayPublishStatusOutputPath.empty()) {
                std::cout << "snc_generated_overlay_publish_success=false\n";
                std::cout << "snc_generated_overlay_publish_reason=missing staging status path, active directory, or status output path\n";
                return 1;
            }

            bool stellarisRunning = config.sncGeneratedOverlayPublishStellarisRunning;
            bool detectionAvailable = false;
            std::string stellarisRunningReason =
                config.sncGeneratedOverlayPublishStellarisRunning ? "explicit true" : "explicit false";
            if (config.sncGeneratedOverlayPublishUseDetectedStellarisState) {
                const StellarisProcessDetector detector;
                const auto processStatus = detector.detectFromSystem();
                stellarisRunning = processStatus.running;
                detectionAvailable = processStatus.detectionAvailable;
                stellarisRunningReason = processStatus.reason;
                if (!processStatus.detectionAvailable) {
                    stellarisRunning = true;
                    stellarisRunningReason += "; SNC generated overlay publish blocked because process detection is unavailable";
                }
            }

            SncGeneratedOverlayPublishGateRequest request;
            request.stagingStatusPath = config.sncGeneratedOverlayPublishStagingStatusPath;
            request.activeOverlayDirectory = config.sncGeneratedOverlayPublishActiveDirectory;
            request.backupRootDirectory = config.sncGeneratedOverlayPublishBackupRootDirectory;
            request.ownerApprovalToken = config.sncGeneratedOverlayOwnerApprovalToken;
            request.stellarisRunning = stellarisRunning;

            const SncGeneratedOverlayPublishGate gate;
            const auto result = gate.publish(request);
            const bool statusWritten = common::writeTextFileAtomically(
                config.sncGeneratedOverlayPublishStatusOutputPath,
                serializeSncGeneratedOverlayPublishGateResult(result));
            const bool success = result.ok && statusWritten;

            std::cout << "snc_generated_overlay_publish_success=" << (success ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_reason=" << sanitizeCliValue(result.reason) << "\n";
            std::cout << "snc_generated_overlay_publish_readiness=" << sanitizeCliValue(result.readiness) << "\n";
            std::cout << "snc_generated_overlay_publish_owner_approved=" << (result.ownerApproved ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_published=" << (result.published ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_backup_created=" << (result.backupCreated ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_stellaris_detection_available="
                      << (detectionAvailable ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_stellaris_running=" << (stellarisRunning ? "true" : "false") << "\n";
            std::cout << "snc_generated_overlay_publish_stellaris_reason="
                      << sanitizeCliValue(stellarisRunningReason) << "\n";
            std::cout << "snc_generated_overlay_publish_staging_status_path="
                      << sanitizeCliValue(result.stagingStatusPath.generic_string()) << "\n";
            std::cout << "snc_generated_overlay_publish_staged_overlay_path="
                      << sanitizeCliValue(result.stagedOverlayDirectory.generic_string()) << "\n";
            std::cout << "snc_generated_overlay_publish_active_overlay_path="
                      << sanitizeCliValue(result.activeOverlayDirectory.generic_string()) << "\n";
            std::cout << "snc_generated_overlay_publish_backup_path="
                      << sanitizeCliValue(result.backupDirectory.generic_string()) << "\n";
            if (!result.sourceManifestHash.empty()) {
                std::cout << "snc_generated_overlay_publish_source_manifest_hash="
                          << sanitizeCliValue(result.sourceManifestHash) << "\n";
            }
            if (!result.publishedManifestHash.empty()) {
                std::cout << "snc_generated_overlay_publish_manifest_hash="
                          << sanitizeCliValue(result.publishedManifestHash) << "\n";
            }
            std::cout << "snc_generated_overlay_publish_file_count=" << result.publishedFileCount << "\n";
            std::cout << "snc_generated_overlay_publish_status_output_written=" << (statusWritten ? "true" : "false") << "\n";
            if (!statusWritten) {
                std::cout << "snc_generated_overlay_publish_status_write_reason=failed to write publish status\n";
            }
            return success ? 0 : 1;
        }

        const StrategicWorker worker;
        return worker.processRequestSafely(config.request);
    }
    catch (const std::exception& error) {
        std::cerr << "Strategic Nexus failed: " << error.what() << "\n";
        return 1;
    }
}

} // namespace strategic_nexus

