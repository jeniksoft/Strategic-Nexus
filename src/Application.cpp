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
#include "RequestFileReader.h"
#include "SeasonDeltaLedgerBuilder.h"
#include "SeasonEmpireBriefBuilder.h"
#include "StellarisProcessDetector.h"
#include "StellarisSavePathResolver.h"
#include "StrategicNexusCompanion.h"
#include "StrategicWorker.h"
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
#include <exception>
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
            if (!result.statusText.empty()) {
                std::cout << "mp_overlay_package_status_text=" << sanitizeCliValue(result.statusText) << "\n";
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
            return result.ok ? 0 : 1;
        }

        if (config.sncStatusSnapshotMode) {
            const StrategicNexusCompanion companion;
            const auto snapshot = companion.buildStatusSnapshot({
                config.sncArchiveRoot,
                config.sncGeneratedOverlayDirectory,
                config.mpOverlayPackageDirectory,
                config.sncStartWithWindowsEnabled,
                config.sncUseDetectedStellarisState,
                config.sncStellarisRunningOverride
            });
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
            std::cout << "snc_archive_state=" << sanitizeCliValue(snapshot.archive.state) << "\n";
            std::cout << "snc_archive_reason=" << sanitizeCliValue(snapshot.archive.reason) << "\n";
            std::cout << "snc_archive_path=" << sanitizeCliValue(stdoutPath(snapshot.archive.path)) << "\n";
            std::cout << "snc_generated_overlay_state=" << sanitizeCliValue(snapshot.generatedOverlay.state) << "\n";
            std::cout << "snc_generated_overlay_reason=" << sanitizeCliValue(snapshot.generatedOverlay.reason) << "\n";
            std::cout << "snc_generated_overlay_path=" << sanitizeCliValue(stdoutPath(snapshot.generatedOverlay.path)) << "\n";
            std::cout << "snc_generated_overlay_manifest_hash=" << sanitizeCliValue(snapshot.generatedOverlay.manifestHash) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_state="
                      << sanitizeCliValue(snapshot.generatedOverlayPublishGate.state) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_reason="
                      << sanitizeCliValue(snapshot.generatedOverlayPublishGate.reason) << "\n";
            std::cout << "snc_generated_overlay_publish_gate_path="
                      << sanitizeCliValue(stdoutPath(snapshot.generatedOverlayPublishGate.path)) << "\n";
            std::cout << "snc_mp_overlay_package_state=" << sanitizeCliValue(snapshot.mpOverlayPackage.state) << "\n";
            std::cout << "snc_mp_overlay_package_reason=" << sanitizeCliValue(snapshot.mpOverlayPackage.reason) << "\n";
            std::cout << "snc_mp_overlay_package_path=" << sanitizeCliValue(stdoutPath(snapshot.mpOverlayPackage.path)) << "\n";
            std::cout << "snc_mp_overlay_package_campaign_id=" << sanitizeCliValue(snapshot.mpOverlayPackage.campaignId) << "\n";
            std::cout << "snc_mp_overlay_package_overlay_version=" << sanitizeCliValue(snapshot.mpOverlayPackage.overlayVersion) << "\n";
            std::cout << "snc_mp_overlay_package_game_version=" << sanitizeCliValue(snapshot.mpOverlayPackage.gameVersion) << "\n";
            std::cout << "snc_mp_overlay_package_strategic_nexus_mod_version="
                      << sanitizeCliValue(snapshot.mpOverlayPackage.strategicNexusModVersion) << "\n";
            std::cout << "snc_mp_overlay_package_handoff_status=" << sanitizeCliValue(snapshot.mpOverlayPackage.handoffStatus) << "\n";
            std::cout << "snc_mp_overlay_package_readiness=" << sanitizeCliValue(snapshot.mpOverlayPackage.readiness) << "\n";
            std::cout << "snc_mp_overlay_package_manifest_hash=" << sanitizeCliValue(snapshot.mpOverlayPackage.packageManifestHash) << "\n";
            std::cout << "snc_mp_overlay_package_status_text=" << sanitizeCliValue(snapshot.mpOverlayPackage.statusText) << "\n";
            std::cout << "snc_status_center_state=" << sanitizeCliValue(snapshot.statusCenter.state) << "\n";
            std::cout << "snc_status_center_reason=" << sanitizeCliValue(snapshot.statusCenter.reason) << "\n";
            std::cout << "snc_status_center_path=" << sanitizeCliValue(stdoutPath(snapshot.statusCenter.path)) << "\n";
            std::cout << "snc_status_center_summary_text=" << sanitizeCliValue(snapshot.statusCenterSummaryText) << "\n";
            std::cout << "snc_status_output_written=" << (outputRequested && written ? "true" : "false") << "\n";
            if (!written) {
                std::cout << "snc_status_reason=failed to write status snapshot\n";
            }
            return success ? 0 : 1;
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

            const SeasonDeltaLedgerBuilder ledgerBuilder;
            const auto ledger = ledgerBuilder.build(summary, config.offlineSpineCampaignId);
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
                config.archiveMinistryInputCampaignId.empty() ||
                config.archiveMinistryInputEmpireId.empty() ||
                config.archiveMinistryInputMinistry.empty() ||
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

            const ArchiveMinistryInputBuilder builder;
            const auto input = builder.build(
                summary,
                config.archiveMinistryInputCampaignId,
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
                config.archivePipelineCampaignId.empty() ||
                config.archivePipelineEmpireId.empty() ||
                config.archivePipelineMinistry.empty() ||
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

            const ArchiveMinistryInputBuilder builder;
            const auto input = builder.build(
                summary,
                config.archivePipelineCampaignId,
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
	                config.seasonDeltaLedgerCampaignId.empty() ||
	                config.seasonDeltaLedgerOutputPath.empty()) {
	                std::cout << "season_delta_ledger_success=false\n";
	                std::cout << "season_delta_ledger_reason=missing archive directory, campaign id, or output path\n";
	                return 1;
	            }

	            const AutosaveArchiveSummarizer summarizer;
	            const auto summary = summarizer.summarize(config.seasonDeltaLedgerDirectory);

	            const SeasonDeltaLedgerBuilder builder;
	            const auto ledger = builder.build(summary, config.seasonDeltaLedgerCampaignId);
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
	                config.archiveEmpireBriefCampaignId.empty() ||
	                config.archiveEmpireBriefEmpireId.empty() ||
	                config.archiveEmpireBriefOutputPath.empty()) {
	                std::cout << "archive_empire_brief_success=false\n";
	                std::cout << "archive_empire_brief_reason=missing archive directory, identity, or output path\n";
	                return 1;
	            }

	            const AutosaveArchiveSummarizer summarizer;
	            const auto summary = summarizer.summarize(config.archiveEmpireBriefDirectory);

	            const SeasonDeltaLedgerBuilder ledgerBuilder;
	            const auto ledger = ledgerBuilder.build(summary, config.archiveEmpireBriefCampaignId);
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
            std::cout << "campaign_library_plan_included=" << plan.includedCount << "\n";
            std::cout << "campaign_library_plan_skipped=" << plan.skippedCount << "\n";
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

            const auto maxCampaigns = config.campaignLibraryOverlayMaxCampaigns < 0
                ? 0
                : static_cast<std::size_t>(config.campaignLibraryOverlayMaxCampaigns);
            const CampaignSaveScanner scanner;
            const CampaignLibraryPlanner planner;
            const auto inventory = scanner.scan(config.campaignLibraryOverlaySaveRoot);
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

            const generated_overlay::OverlayCompiler compiler;
            const auto files = compiler.compile(filteredProgram);
            const auto outputDirectory = config.campaignLibraryOverlayOutputDirectory;
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
            std::cout << "campaign_library_overlay_campaigns_included=" << plan.includedCount << "\n";
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

        const StrategicWorker worker;
        return worker.processRequestSafely(config.request);
    }
    catch (const std::exception& error) {
        std::cerr << "Strategic Nexus failed: " << error.what() << "\n";
        return 1;
    }
}

} // namespace strategic_nexus

