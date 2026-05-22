#include "BridgeValidator.h"
#include "BridgeCorePipeline.h"
#include "DecisionFileReader.h"
#include "EffectBatchEmitter.h"
#include "EffectBatchFileWriter.h"
#include "ScriptIntentMapper.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

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
    } catch (...) {
        return fallback;
    }
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace strategic_nexus::bridge_core;

    if (argc >= 2 && std::string(argv[1]) == "--pipeline") {
        if (argc < 6) {
            std::cerr << "Usage: bridge_core_test --pipeline <decision.json> <effect_batch_output> <sequence_state> <now_unix_ms>\n";
            return 2;
        }

        BridgeCorePipelineConfig config;
        config.decisionPath = argv[2];
        config.effectBatchPath = argv[3];
        config.sequenceStatePath = argv[4];
        config.nowUnixMs = parseInt64Arg(argv[5], currentUnixMs());

        BridgeCorePipeline pipeline;
        const BridgeCorePipelineResult result = pipeline.processOnce(config);
        std::cout << "pipeline_accepted=" << (result.accepted ? "true" : "false") << "\n";
        std::cout << "pipeline_reason=" << result.reason << "\n";
        std::cout << "pipeline_sequence_id=" << result.sequenceId << "\n";
        std::cout << "pipeline_previous_sequence_id=" << result.previousSequenceId << "\n";
        std::cout << "pipeline_current_sequence_id=" << result.currentSequenceId << "\n";
        std::cout << "pipeline_effect_batch_written=" << (result.effectBatchWritten ? "true" : "false") << "\n";
        std::cout << "pipeline_sequence_advanced=" << (result.sequenceAdvanced ? "true" : "false") << "\n";
        return 0;
    }

    if (argc < 2) {
        std::cerr << "Usage: bridge_core_test <decision.json> [now_unix_ms] [last_sequence_id] [effect_batch_output]\n";
        std::cerr << "       bridge_core_test --pipeline <decision.json> <effect_batch_output> <sequence_state> <now_unix_ms>\n";
        return 2;
    }

    const std::filesystem::path decisionPath = argv[1];
    const std::int64_t nowUnixMs = argc >= 3 ? parseInt64Arg(argv[2], currentUnixMs()) : currentUnixMs();
    const std::int64_t lastSequenceId = argc >= 4 ? parseInt64Arg(argv[3], 0) : 0;
    const std::filesystem::path effectBatchOutputPath = argc >= 5 ? std::filesystem::path(argv[4]) : std::filesystem::path();

    DecisionFileReader reader;
    const DecisionReadResult readResult = reader.read(decisionPath);
    if (!readResult.ok) {
        const ValidatedBridgeState fallback = makeFallbackState(readResult.error);
        std::cout << "valid=false\n";
        std::cout << "reason=" << fallback.reason << "\n";
        std::cout << "action=" << toString(fallback.action) << "\n";
        return 0;
    }

    BridgeValidator validator;
    const ValidatedBridgeState state = validator.validate(readResult.payload, nowUnixMs, lastSequenceId);

    std::cout << "valid=" << (state.valid ? "true" : "false") << "\n";
    std::cout << "reason=" << state.reason << "\n";
    std::cout << "sequence_id=" << state.sequenceId << "\n";
    std::cout << "valid_dimensions=" << state.validDimensionCount << "\n";
    std::cout << "invalid_dimensions=" << state.invalidDimensionCount << "\n";
    std::cout << "confidence=" << state.confidence << "\n";
    std::cout << "action=" << toString(state.action) << "\n";

    ScriptIntentMapper intentMapper;
    const ScriptIntent intent = intentMapper.map(state);
    std::cout << "script_event=" << intent.eventId << "\n";
    std::cout << "mapped_dimensions=" << intent.mappedDimensionCount << "\n";
    std::cout << "unmapped_dimensions=" << intent.unmappedDimensionCount << "\n";
    for (const std::string& flag : intent.globalFlags) {
        std::cout << "script_global_flag=" << flag << "\n";
    }
    for (const std::string& flag : intent.countryFlags) {
        std::cout << "script_country_flag=" << flag << "\n";
    }

    EffectBatchEmitter emitter;
    const EffectBatch batch = emitter.emit(intent);
    std::cout << "effect_batch_valid=" << (batch.valid ? "true" : "false") << "\n";
    for (const std::string& line : batch.lines) {
        std::cout << "effect_batch_line=" << line << "\n";
    }
    if (!effectBatchOutputPath.empty()) {
        EffectBatchFileWriter writer;
        const EffectBatchWriteResult writeResult = writer.writeAtomically(batch, effectBatchOutputPath);
        std::cout << "effect_batch_written=" << (writeResult.written ? "true" : "false") << "\n";
        std::cout << "effect_batch_write_reason=" << writeResult.reason << "\n";
        std::cout << "effect_batch_path=" << writeResult.path.string() << "\n";
    }

    if (state.valid) {
        if (state.dimensions.hasMilitaryPosture) {
            std::cout << "military_posture=" << toString(state.dimensions.militaryPosture) << "\n";
        }
        if (state.dimensions.hasFleetPhilosophy) {
            std::cout << "fleet_philosophy=" << toString(state.dimensions.fleetPhilosophy) << "\n";
        }
        if (state.dimensions.hasBorderStrategy) {
            std::cout << "border_strategy=" << toString(state.dimensions.borderStrategy) << "\n";
        }
        if (state.dimensions.hasExpansionPolicy) {
            std::cout << "expansion_policy=" << toString(state.dimensions.expansionPolicy) << "\n";
        }
        if (state.dimensions.hasDiplomaticBehavior) {
            std::cout << "diplomatic_behavior=" << toString(state.dimensions.diplomaticBehavior) << "\n";
        }
        if (state.dimensions.hasResearchBias) {
            std::cout << "research_bias=" << toString(state.dimensions.researchBias) << "\n";
        }
        if (state.dimensions.hasEconomicFocus) {
            std::cout << "economic_focus=" << toString(state.dimensions.economicFocus) << "\n";
        }
        if (state.dimensions.hasCrisisPolicy) {
            std::cout << "crisis_policy=" << toString(state.dimensions.crisisPolicy) << "\n";
        }
        if (state.dimensions.hasEspionagePolicy) {
            std::cout << "espionage_policy=" << toString(state.dimensions.espionagePolicy) << "\n";
        }
    }
    return 0;
}
