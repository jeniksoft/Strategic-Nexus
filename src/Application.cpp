#include "Application.h"

#include "DaemonRunner.h"
#include "RequestFileReader.h"
#include "StrategicWorker.h"
#include "bridge_core/BridgeCorePipeline.h"
#include "common/FileUtil.h"
#include "strategic_pipeline/EmpireProcessingQueue.h"
#include "strategic_pipeline/MinistryInputReader.h"
#include "strategic_pipeline/PipelineJsonWriter.h"
#include "strategic_pipeline/V0StrategicPipeline.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
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
            std::cout << "pipeline_reason=" << result.reason << "\n";
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
            std::cout << "v0_pipeline_reason=" << result.reason << "\n";
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
                std::cout << "v0_pipeline_audit_reason=" << result.auditReason << "\n";
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

        const StrategicWorker worker;
        return worker.processRequestSafely(config.request);
    }
    catch (const std::exception& error) {
        std::cerr << "Strategic Nexus failed: " << error.what() << "\n";
        return 1;
    }
}

} // namespace strategic_nexus
