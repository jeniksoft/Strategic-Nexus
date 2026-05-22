#include "ModIntegration.h"

#include "BridgeContract.h"
#include "DoctrinePlanner.h"
#include "common/FileUtil.h"

#include <filesystem>
#include <iomanip>
#include <stdexcept>
#include <sstream>

namespace strategic_nexus {

namespace {

void writeAtomically(const std::filesystem::path& outputPath, const std::string& content)
{
    if (!common::writeTextFileAtomically(outputPath, content)) {
        throw std::runtime_error("Unable to publish doctrine output file: " + outputPath.string());
    }
}

std::string escapeJsonString(const std::string& value)
{
    std::ostringstream escaped;
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped << "\\\\";
            break;
        case '"':
            escaped << "\\\"";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        default:
            escaped << character;
            break;
        }
    }

    return escaped.str();
}

} // namespace

std::string ModIntegration::renderDoctrineJson(const DoctrineDecision& decision) const
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"doctrine\": \"" << toString(decision.type) << "\",\n";
    output << "  \"confidence\": " << std::fixed << std::setprecision(2) << decision.confidence << ",\n";
    output << "  \"safe_mode\": true\n";
    output << "}\n";
    return output.str();
}

void ModIntegration::writeDoctrineJson(const std::filesystem::path& outputPath, const DoctrineDecision& decision) const
{
    writeAtomically(outputPath, renderDoctrineJson(decision));
}

std::string ModIntegration::renderDoctrineJson(const StrategicRequest& request, const DoctrineDecision& decision) const
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"status\": \"ok\",\n";
    output << "  \"request\": {\n";
    output << "    \"request_id\": \"" << escapeJsonString(request.requestId) << "\",\n";
    output << "    \"trigger\": \"" << toString(request.trigger) << "\",\n";
    output << "    \"async\": " << (request.asynchronous ? "true" : "false") << "\n";
    output << "  },\n";
    output << "  \"decision\": {\n";
    output << "    \"doctrine\": \"" << toString(decision.type) << "\",\n";
    output << "    \"confidence\": " << std::fixed << std::setprecision(2) << decision.confidence << ",\n";
    output << "    \"safe_mode\": true\n";
    output << "  },\n";
    output << BridgeContract().renderJson(BridgeContract().buildValues(decision)) << "\n";
    output << "}\n";
    return output.str();
}

void ModIntegration::writeDoctrineJson(
    const std::filesystem::path& outputPath,
    const StrategicRequest& request,
    const DoctrineDecision& decision) const
{
    writeAtomically(outputPath, renderDoctrineJson(request, decision));
}

std::string ModIntegration::renderErrorJson(const StrategicRequest& request, const std::string& message) const
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"status\": \"error\",\n";
    output << "  \"request\": {\n";
    output << "    \"request_id\": \"" << escapeJsonString(request.requestId) << "\",\n";
    output << "    \"trigger\": \"" << toString(request.trigger) << "\",\n";
    output << "    \"async\": " << (request.asynchronous ? "true" : "false") << "\n";
    output << "  },\n";
    output << "  \"error\": {\n";
    output << "    \"message\": \"" << escapeJsonString(message) << "\"\n";
    output << "  },\n";
    output << "  \"bridge\": {\n";
    output << "    \"available\": false,\n";
    output << "    \"doctrine\": \"fallback_scripted\",\n";
    output << "    \"strategic_pressure\": 0.00,\n";
    output << "    \"fallback_required\": true\n";
    output << "  }\n";
    output << "}\n";
    return output.str();
}

void ModIntegration::writeErrorJson(
    const std::filesystem::path& outputPath,
    const StrategicRequest& request,
    const std::string& message) const
{
    writeAtomically(outputPath, renderErrorJson(request, message));
}

} // namespace strategic_nexus
