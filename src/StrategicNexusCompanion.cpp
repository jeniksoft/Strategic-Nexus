// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicNexusCompanion.h"

#include "AutosaveArchiver.h"
#include "common/FileUtil.h"
#include "common/JsonExtract.h"
#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/MpOverlayPackage.h"
#include "StellarisProcessDetector.h"
#include "StellarisSavePathResolver.h"

#include <filesystem>
#include <cctype>
#include <iomanip>
#include <ctime>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace strategic_nexus {
namespace {

std::string escapeJson(const std::string& value)
{
    std::ostringstream output;
    for (const unsigned char raw : value) {
        const char ch = static_cast<char>(raw);
        switch (ch) {
        case '\\':
            output << "\\\\";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        default:
            if (raw < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(raw) << std::dec << std::setw(0);
            } else {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

std::string jsonString(const std::string& value)
{
    return "\"" + escapeJson(value) + "\"";
}

std::string trimWhitespace(const std::string& value)
{
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string toWarningCode(const std::string& value)
{
    std::string code;
    code.reserve(value.size());
    bool lastWasUnderscore = false;
    for (const unsigned char raw : value) {
        const char lower = static_cast<char>(std::tolower(raw));
        if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9')) {
            code.push_back(lower);
            lastWasUnderscore = false;
            continue;
        }
        if (!lastWasUnderscore) {
            code.push_back('_');
            lastWasUnderscore = true;
        }
    }

    while (!code.empty() && code.front() == '_') {
        code.erase(code.begin());
    }
    while (!code.empty() && code.back() == '_') {
        code.pop_back();
    }
    if (code.empty()) {
        return "mp_overlay_package_warning_unknown";
    }
    return code;
}

std::vector<std::string> extractWarningCodesFromStatusText(const std::string& statusText)
{
    std::vector<std::string> warnings;
    std::unordered_set<std::string> seen;
    std::istringstream input(statusText);
    std::string line;
    while (std::getline(input, line)) {
        constexpr std::string_view prefix = "warning_code:";
        if (line.rfind(prefix.data(), 0) != 0) {
            continue;
        }
        const auto warningCode = trimWhitespace(line.substr(prefix.size()));
        if (warningCode.empty() || seen.find(warningCode) != seen.end()) {
            continue;
        }
        seen.insert(warningCode);
        warnings.push_back(warningCode);
    }
    return warnings;
}

std::unordered_map<std::string, std::string> extractGameplayAcceptanceCaseResults(const std::string& reportJson)
{
    std::unordered_map<std::string, std::string> results;
    const std::regex casePattern(
        "\"case_id\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"[\\s\\S]*?\"result\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"");
    auto begin = std::sregex_iterator(reportJson.begin(), reportJson.end(), casePattern);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const auto caseId = common::decodeJsonStringLiteral((*it)[1].str());
        const auto result = common::decodeJsonStringLiteral((*it)[2].str());
        if (!caseId.empty()) {
            results[caseId] = result;
        }
    }
    return results;
}

bool hasVerifiedGameplayAcceptanceCoverage(const std::string& reportJson)
{
    static const std::vector<std::string> expectedCaseIds = {
        "case_a_defensive_military_posture",
        "case_b_aggressive_military_posture",
        "case_c_economy_research_bias",
        "case_d_military_industry_research_bias",
        "case_e_invalid_tactical_domain",
        "case_f_manifest_drift_before_publish"
    };

    const auto caseResults = extractGameplayAcceptanceCaseResults(reportJson);
    for (const auto& caseId : expectedCaseIds) {
        const auto it = caseResults.find(caseId);
        if (it == caseResults.end() || it->second != "pass") {
            return false;
        }
    }
    return true;
}

bool isIdentityMismatchWarningCode(const std::string& warningCode)
{
    return warningCode == "package_campaign_id_mismatch" || warningCode == "package_overlay_version_mismatch" ||
           warningCode == "package_game_version_mismatch" || warningCode == "package_mod_version_mismatch" ||
           warningCode == "package_manifest_hash_mismatch" ||
           warningCode == "mp_overlay_package_files_mismatch_manifest";
}

void finalizeMpOverlayPackageWarnings(CompanionMpOverlayPackageStatus& status)
{
    status.warningCount = status.warningCodes.size();
    status.identityMismatchWarningCodes.clear();
    status.mismatchWarningCodes.clear();
    status.identityMismatchWarning = false;
    status.identityMismatchAlert.clear();

    for (const auto& warningCode : status.warningCodes) {
        if (!isIdentityMismatchWarningCode(warningCode)) {
            continue;
        }
        status.identityMismatchWarning = true;
        status.identityMismatchWarningCodes.push_back(warningCode);
        status.mismatchWarningCodes.push_back(warningCode);
    }

    if (status.identityMismatchWarning) {
        status.mismatchWarningState = "warning";
        status.mismatchWarningReason = "package_identity_mismatch_detected";
        status.identityMismatchAlert =
            "package identity mismatch detected (campaign/version/mod/hash); run strict verify/import before MP join";
    } else {
        status.mismatchWarningState = "no_mismatch";
        status.mismatchWarningReason = "no_identity_mismatch_detected";
    }
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

std::string pathString(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::string normalizedPathKey(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::vector<std::filesystem::path> resolveLiveAutosaveMonitorRoots(
    const std::vector<std::filesystem::path>& configuredRoots,
    std::size_t& candidateRootCount)
{
    std::vector<std::filesystem::path> roots;
    std::set<std::string> seen;

    const auto addRoot = [&](const std::filesystem::path& root) {
        if (root.empty()) {
            return;
        }

        const auto key = normalizedPathKey(root);
        if (!seen.insert(key).second) {
            return;
        }

        roots.push_back(root);
    };

    if (!configuredRoots.empty()) {
        for (const auto& root : configuredRoots) {
            addRoot(root);
        }
        candidateRootCount = roots.size();
        return roots;
    }

    const StellarisSavePathResolver resolver;
    const auto discovery = resolver.discoverFromEnvironment();
    candidateRootCount = discovery.candidates.size();
    for (const auto& candidate : discovery.candidates) {
        if (candidate.exists) {
            addRoot(candidate.path);
        }
    }
    return roots;
}

bool directoryExists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error) && std::filesystem::is_directory(path, error);
}

std::string quoteCliPathArg(const std::filesystem::path& path)
{
    const std::string value = pathString(path);
    if (value.empty()) {
        return "\"\"";
    }
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        if (ch == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return "\"" + escaped + "\"";
}

std::optional<bool> extractJsonBool(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }
    return match[1].str() == "true";
}

std::optional<std::string> extractArrayBody(const std::string& json, const char* key)
{
    const std::regex arrayStartPattern("\"" + std::string(key) + "\"\\s*:\\s*\\[");
    std::smatch match;
    if (!std::regex_search(json, match, arrayStartPattern)) {
        return std::nullopt;
    }

    const std::size_t bodyStart = static_cast<std::size_t>(match.position()) + match.length();
    int depth = 1;
    bool inString = false;
    bool escaped = false;

    for (std::size_t index = bodyStart; index < json.size(); ++index) {
        const char ch = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return json.substr(bodyStart, index - bodyStart);
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string> extractObjectBodies(const std::string& arrayBody)
{
    std::vector<std::string> objects;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    std::size_t objectStart = std::string::npos;

    for (std::size_t index = 0; index < arrayBody.size(); ++index) {
        const char ch = arrayBody[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
            continue;
        }
        if (ch == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                objects.push_back(arrayBody.substr(objectStart, index - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }

    return objects;
}

std::optional<std::size_t> extractJsonSize(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }

    try {
        return static_cast<std::size_t>(std::stoull(match[1].str()));
    }
    catch (...) {
        return std::nullopt;
    }
}

bool tryApplyCurrentPublishedOverlayStatus(
    const std::string& publishStatusJson,
    const CompanionSubsystemStatus& generatedOverlay,
    CompanionGeneratedOverlayPublishGateStatus& status)
{
    const auto ok = extractJsonBool(publishStatusJson, "ok");
    const auto published = extractJsonBool(publishStatusJson, "published");
    const auto readiness = common::extractJsonString(publishStatusJson, "readiness");
    const auto sourceManifestHash = common::extractJsonString(publishStatusJson, "source_manifest_hash");
    const auto publishedManifestHash = common::extractJsonString(publishStatusJson, "published_manifest_hash");

    if (!ok.value_or(false) || !published.value_or(false) || !readiness.has_value() || *readiness != "published" ||
        !sourceManifestHash.has_value() || sourceManifestHash->empty() || !publishedManifestHash.has_value() ||
        publishedManifestHash->empty()) {
        return false;
    }

    if (status.manifestHash.empty() || generatedOverlay.manifestHash.empty()) {
        return false;
    }

    if (*sourceManifestHash != status.manifestHash || *publishedManifestHash != generatedOverlay.manifestHash) {
        return false;
    }

    status.state = "published";
    status.reason = "current staged generated overlay already published";
    status.path = status.activeOverlayDirectory;
    status.published = true;
    status.canPublish = false;
    status.ownerApprovalRequired = false;
    status.publishCommand.clear();
    status.publishedManifestHash = *publishedManifestHash;
    status.publishedFileCount = extractJsonSize(publishStatusJson, "published_file_count").value_or(0);
    status.backupCreated = extractJsonBool(publishStatusJson, "backup_created").value_or(false);

    const auto backupDirectory = common::extractJsonString(publishStatusJson, "backup_directory");
    if (backupDirectory.has_value() && !backupDirectory->empty()) {
        status.backupDirectory = *backupDirectory;
    }

    return true;
}

void parsePostPlayCampaignSummaries(const std::string& json, CompanionPostPlayPipelineStatus& status)
{
    const auto campaignsBody = extractArrayBody(json, "campaigns");
    if (!campaignsBody.has_value()) {
        return;
    }

    for (const auto& object : extractObjectBodies(*campaignsBody)) {
        const auto campaignKey = common::extractJsonString(object, "campaign_key").value_or("");
        const auto readiness = common::extractJsonString(object, "readiness").value_or("");
        const auto entryPointCount = extractJsonSize(object, "entry_point_count").value_or(0);
        const auto decisionReadyEntryCount = extractJsonSize(object, "decision_ready_entry_count").value_or(0);
        const auto branchAmbiguityDetected = extractJsonBool(object, "branch_ambiguity_detected").value_or(false);

        if (campaignKey.empty() && readiness.empty() && entryPointCount == 0 && decisionReadyEntryCount == 0 &&
            !branchAmbiguityDetected) {
            continue;
        }

        ++status.postPlayCampaignCount;
        if (readiness.rfind("ready_partial", 0) == 0) {
            ++status.postPlayPartialCampaignCount;
        } else if (readiness.rfind("ready", 0) == 0) {
            ++status.postPlayReadyCampaignCount;
        } else {
            ++status.postPlayBlockedCampaignCount;
        }

        std::ostringstream summary;
        summary << (campaignKey.empty() ? "unknown_campaign" : campaignKey)
                << ": " << (readiness.empty() ? "unknown" : readiness)
                << " (" << decisionReadyEntryCount << "/" << entryPointCount << " ready";
        if (branchAmbiguityDetected) {
            summary << ", ambiguous";
        }
        summary << ")";
        status.postPlayCampaignReadinessSummaries.push_back(summary.str());
    }
}

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.rfind(prefix, 0) == 0;
}

std::string stateFromReadiness(const std::string& readiness)
{
    if (startsWith(readiness, "ready")) {
        return "ready";
    }
    if (startsWith(readiness, "needs") || startsWith(readiness, "blocked") || startsWith(readiness, "failed")) {
        return "needs_attention";
    }
    return "starting";
}

std::string reasonFromReadiness(const std::string& label, const std::string& readiness)
{
    if (readiness.empty()) {
        return "waiting for " + label;
    }
    return label + " " + readiness;
}

std::string stateFromGeneratedOverlayStagingReadiness(const std::string& readiness)
{
    if (readiness == "staged_verified") {
        return "ready";
    }
    return stateFromReadiness(readiness);
}

std::string reasonFromGeneratedOverlayStagingReadiness(const std::string& readiness)
{
    if (readiness.empty()) {
        return "waiting for generated overlay staging";
    }
    if (readiness == "staged_verified") {
        return "generated overlay staging verified";
    }
    return "generated overlay staging " + readiness;
}

std::filesystem::path postPlayPipelineFocusPath(const CompanionPostPlayPipelineStatus& status)
{
    const auto hasPrefix = [](const std::string& value, const std::string& prefix) {
        return value.rfind(prefix, 0) == 0;
    };

    if (hasPrefix(status.reason, "generated overlay staging ") ||
        !status.generatedOverlayStagingReadiness.empty() ||
        !status.generatedOverlayStagingReason.empty()) {
        return status.generatedOverlayStagingStatusPath;
    }
    if (hasPrefix(status.reason, "dsl draft ") ||
        !status.dslDraftReadiness.empty() ||
        !status.dslDraftReason.empty()) {
        return status.dslDraftAuditPath;
    }
    if (hasPrefix(status.reason, "candidate decision package ") ||
        !status.candidateDecisionPackageReadiness.empty() ||
        !status.candidateDecisionPackageReason.empty()) {
        return status.candidateDecisionPackagePath;
    }
    if (hasPrefix(status.reason, "decision input package ") ||
        !status.decisionInputPackageReadiness.empty() ||
        !status.decisionInputPackageReason.empty()) {
        return status.decisionInputPackagePath;
    }
    if (hasPrefix(status.reason, "post-play package ") ||
        !status.postPlayPackageReadiness.empty() ||
        !status.postPlayPackageReason.empty()) {
        return status.postPlayPackagePath;
    }
    return status.entryPointAnalysisPath;
}

std::string buildGeneratedOverlayPublishCommand(const CompanionGeneratedOverlayPublishGateStatus& status)
{
    if (status.stagingStatusPath.empty() || status.activeOverlayDirectory.empty() ||
        status.publishStatusPath.empty()) {
        return std::string();
    }

    std::ostringstream command;
    command << "Strategic Nexus.exe --publish-snc-generated-overlay "
            << quoteCliPathArg(status.stagingStatusPath) << " "
            << quoteCliPathArg(status.activeOverlayDirectory) << " "
            << quoteCliPathArg(status.publishStatusPath) << " owner-approved";
    if (!status.backupRootDirectory.empty()) {
        command << " " << quoteCliPathArg(status.backupRootDirectory);
    }
    return command.str();
}

void populatePublishGateProcessStatus(
    CompanionGeneratedOverlayPublishGateStatus& status,
    const CompanionStatusConfig& config)
{
    StellarisProcessStatus processStatus;
    if (config.useDetectedStellarisState) {
        const StellarisProcessDetector detector;
        processStatus = detector.detectFromSystem();
    } else {
        processStatus.detectionAvailable = true;
        processStatus.running = config.stellarisRunningOverride;
        processStatus.reason =
            config.stellarisRunningOverride ? "explicit Stellaris running override" : "explicit Stellaris not running override";
    }

    if (!processStatus.detectionAvailable) {
        status.state = "needs_attention";
        status.reason = "Stellaris process detection unavailable; generated overlay publish blocked";
        return;
    }

    if (processStatus.running) {
        status.state = "blocked";
        status.reason = "Stellaris is running; generated overlay publish deferred";
        return;
    }

    status.state = "ready";
    status.reason = "Stellaris is not running; generated overlay publish allowed";
}

std::string formatLocalTimestamp(std::chrono::system_clock::time_point now)
{
    const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (localtime_s(&localTime, &timeValue) != 0) {
        return std::string();
    }

    std::ostringstream output;
    output << localTime.tm_mday << "." << (localTime.tm_mon + 1) << "." << (localTime.tm_year + 1900) << "  "
           << localTime.tm_hour << ":" << std::setw(2) << std::setfill('0') << localTime.tm_min << ":"
           << std::setw(2) << std::setfill('0') << localTime.tm_sec;
    return output.str();
}

std::string serializeLiveAutosaveMonitorStatus(
    const CompanionLiveAutosaveMonitorResult& result,
    const std::string& state)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"schema_version\": 1,\n";
    output << "  \"state\": " << jsonString(state) << ",\n";
    output << "  \"reason\": " << jsonString(result.reason) << ",\n";
    output << "  \"updated_at_local\": "
           << jsonString(formatLocalTimestamp(std::chrono::system_clock::now())) << ",\n";
    output << "  \"iterations_run\": " << result.iterationsRun << ",\n";
    output << "  \"candidate_root_count\": " << result.candidateRootCount << ",\n";
    output << "  \"existing_root_count\": " << result.existingRootCount << ",\n";
    output << "  \"copied_count\": " << result.copiedCount << ",\n";
    output << "  \"skipped_count\": " << result.skippedCount << ",\n";
    output << "  \"stellaris_running\": " << (result.lastStellarisRunning ? "true" : "false") << ",\n";
    output << "  \"archive_session_directory\": "
           << jsonString(pathString(result.archiveSessionDirectory)) << "\n";
    output << "}\n";
    return output.str();
}

bool writeLiveAutosaveMonitorStatus(
    CompanionLiveAutosaveMonitorResult& result,
    const std::string& state)
{
    if (result.statusOutputPath.empty()) {
        return true;
    }

    const bool written = common::writeTextFileAtomically(
        result.statusOutputPath,
        serializeLiveAutosaveMonitorStatus(result, state));
    result.statusOutputWritten = written;
    return written;
}

CompanionSubsystemStatus buildSaveDiscoveryStatus()
{
    CompanionSubsystemStatus status;
    status.state = "starting";
    status.reason = "discovering Stellaris save roots";

    const StellarisSavePathResolver resolver;
    const auto discovery = resolver.discoverFromEnvironment();

    if (discovery.candidates.empty()) {
        status.state = "needs_setup";
        status.reason = "save discovery unavailable";
        return status;
    }

    status.path = discovery.candidates.front().path;

    for (const auto& candidate : discovery.candidates) {
        if (candidate.exists) {
            status.state = "ready";
            status.reason = "Stellaris save root discovered";
            status.path = candidate.path;
            return status;
        }
    }

    status.state = "needs_attention";
    status.reason = "no Stellaris save roots found";
    return status;
}

CompanionSubsystemStatus buildArchiveStatus(const std::filesystem::path& archiveRoot)
{
    CompanionSubsystemStatus status;
    status.path = archiveRoot;

    if (archiveRoot.empty()) {
        status.state = "needs_setup";
        status.reason = "archive root not configured";
        return status;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(archiveRoot, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "archive root inaccessible";
        return status;
    }

    if (!exists) {
        status.state = "needs_attention";
        status.reason = "archive root missing";
        return status;
    }

    const bool isDirectory = std::filesystem::is_directory(archiveRoot, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "archive root inaccessible";
        return status;
    }

    if (!isDirectory) {
        status.state = "needs_attention";
        status.reason = "archive root is not a directory";
        return status;
    }

    if (std::filesystem::exists(archiveRoot / "manifest.json", error) && !error) {
        status.state = "ready";
        status.reason = "archive session manifest available";
        return status;
    }

    bool hasSessions = false;
    for (const auto& entry : std::filesystem::directory_iterator(archiveRoot, error)) {
        if (error) {
            break;
        }
        if (!entry.is_directory(error)) {
            continue;
        }
        if (std::filesystem::exists(entry.path() / "manifest.json", error) && !error) {
            hasSessions = true;
            break;
        }
    }

    if (!hasSessions) {
        status.state = "starting";
        status.reason = "no archive sessions yet";
        return status;
    }

    status.state = "ready";
    status.reason = "archive sessions available";
    return status;
}

CompanionSubsystemStatus buildGeneratedOverlayStatus(const std::filesystem::path& overlayDirectory)
{
    CompanionSubsystemStatus status;
    status.path = overlayDirectory;

    if (overlayDirectory.empty()) {
        status.state = "needs_setup";
        status.reason = "generated overlay directory not configured";
        return status;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(overlayDirectory, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "generated overlay directory inaccessible";
        return status;
    }

    if (!exists) {
        status.state = "needs_attention";
        status.reason = "generated overlay directory missing";
        return status;
    }

    const bool isDirectory = std::filesystem::is_directory(overlayDirectory, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "generated overlay directory inaccessible";
        return status;
    }

    if (!isDirectory) {
        status.state = "needs_attention";
        status.reason = "generated overlay path is not a directory";
        return status;
    }

    const generated_overlay::ManifestVerifier verifier;
    const auto verification = verifier.verify(overlayDirectory);
    status.manifestHash = verification.manifestHash;
    if (verification.ok) {
        status.state = "ready";
        status.reason = "generated overlay verified";
        return status;
    }

    if (verification.reason == "missing generated overlay manifest") {
        std::error_code iterError;
        const bool hasAnyEntry =
            std::filesystem::directory_iterator(overlayDirectory, iterError) != std::filesystem::directory_iterator();
        if (iterError) {
            status.state = "needs_attention";
            status.reason = "generated overlay directory inaccessible";
            return status;
        }

        if (!hasAnyEntry) {
            status.state = "starting";
            status.reason = "no generated overlay yet";
            return status;
        }

        status.state = "needs_attention";
        status.reason = "missing generated overlay manifest";
        return status;
    }

    status.state = "needs_attention";
    status.reason = verification.reason.empty() ? "generated overlay verification failed" : verification.reason;
    return status;
}

CompanionMpOverlayPackageStatus buildMpOverlayPackageStatus(const std::filesystem::path& packageDirectory)
{
    CompanionMpOverlayPackageStatus status;
    status.path = packageDirectory;
    const auto setFailureStatusText = [&status]() {
        const std::string warningCode = toWarningCode(status.reason);
        std::ostringstream text;
        text << "readiness: not_ready\n";
        text << "warning_code: " << warningCode << "\n";
        text << "next_step: re-export package on host and import+verify before multiplayer join\n";
        if (!status.verifyCommand.empty()) {
            text << "verify_command: " << status.verifyCommand << "\n";
        }
        if (!status.importCommand.empty()) {
            text << "import_command: " << status.importCommand << "\n";
        }
        if (!status.strictVerifyCommand.empty()) {
            text << "strict_verify_command: " << status.strictVerifyCommand << "\n";
        }
        if (!status.strictImportCommand.empty()) {
            text << "strict_import_command: " << status.strictImportCommand << "\n";
        }
        status.statusText = text.str();
    };

    if (packageDirectory.empty()) {
        status.state = "disabled";
        status.reason = "mp overlay package directory not configured";
        status.warningCodes.push_back(toWarningCode(status.reason));
        finalizeMpOverlayPackageWarnings(status);
        return status;
    }
    status.verifyCommand = "Strategic Nexus.exe --verify-mp-overlay-package " + quoteCliPathArg(packageDirectory);
    status.importCommand =
        "Strategic Nexus.exe --import-mp-overlay-package " + quoteCliPathArg(packageDirectory) +
        " <target_overlay_dir>";

    std::error_code error;
    const bool exists = std::filesystem::exists(packageDirectory, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "mp overlay package directory inaccessible";
        status.warningCodes.push_back(toWarningCode(status.reason));
        setFailureStatusText();
        finalizeMpOverlayPackageWarnings(status);
        return status;
    }

    if (!exists) {
        status.state = "needs_attention";
        status.reason = "mp overlay package directory missing";
        status.warningCodes.push_back(toWarningCode(status.reason));
        setFailureStatusText();
        finalizeMpOverlayPackageWarnings(status);
        return status;
    }

    const bool isDirectory = std::filesystem::is_directory(packageDirectory, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "mp overlay package directory inaccessible";
        status.warningCodes.push_back(toWarningCode(status.reason));
        setFailureStatusText();
        finalizeMpOverlayPackageWarnings(status);
        return status;
    }

    if (!isDirectory) {
        status.state = "needs_attention";
        status.reason = "mp overlay package path is not a directory";
        status.warningCodes.push_back(toWarningCode(status.reason));
        setFailureStatusText();
        finalizeMpOverlayPackageWarnings(status);
        return status;
    }

    const generated_overlay::MpOverlayPackageVerifier verifier;
    const auto verification = verifier.verify(packageDirectory);
    status.campaignId = verification.campaignId;
    status.overlayVersion = verification.overlayVersion;
    status.gameVersion = verification.gameVersion;
    status.strategicNexusModVersion = verification.strategicNexusModVersion;
    status.handoffStatus = verification.handoffStatus;
    status.readiness = verification.readiness;
    status.hostReadiness = trimWhitespace(readStatusTextField(verification.statusText, "host_readiness"));
    status.clientReadinessGate = trimWhitespace(readStatusTextField(verification.statusText, "client_readiness_gate"));
    status.hostNextStep = trimWhitespace(readStatusTextField(verification.statusText, "host_next_step"));
    status.clientNextStep = trimWhitespace(readStatusTextField(verification.statusText, "client_next_step"));
    status.packageManifestHash = verification.packageManifestHash;
    if (!status.campaignId.empty() && !status.overlayVersion.empty() && !status.gameVersion.empty() &&
        !status.strategicNexusModVersion.empty() && !status.packageManifestHash.empty()) {
        status.strictVerifyCommand =
            status.verifyCommand + " " + status.campaignId + " " + status.overlayVersion + " " + status.gameVersion + " " +
            status.strategicNexusModVersion + " " + status.packageManifestHash;
        status.strictImportCommand =
            status.importCommand + " " + status.campaignId + " " + status.overlayVersion + " " + status.gameVersion + " " +
            status.strategicNexusModVersion + " " + status.packageManifestHash;
    }
    if (verification.ok) {
        status.state = "ready";
        status.reason = "mp overlay package verified";
        status.statusText = verification.statusText;
        const auto warningCodes = extractWarningCodesFromStatusText(status.statusText);
        if (!warningCodes.empty()) {
            status.warningCodes = warningCodes;
        }
        finalizeMpOverlayPackageWarnings(status);
        return status;
    }

    status.state = "needs_attention";
    status.reason = verification.reason.empty() ? "mp overlay package verification failed" : verification.reason;
    status.warningCodes.push_back(toWarningCode(status.reason));
    if (!verification.statusText.empty()) {
        status.statusText = verification.statusText;
    } else {
        setFailureStatusText();
    }
    const auto warningCodes = extractWarningCodesFromStatusText(status.statusText);
    if (!warningCodes.empty()) {
        status.warningCodes = warningCodes;
    }
    finalizeMpOverlayPackageWarnings(status);
    return status;
}

void populateMpOverlayPackageZipStatus(
    CompanionMpOverlayPackageStatus& status,
    const std::filesystem::path& packageZipPath)
{
    status.packageZipPath = packageZipPath;
    if (packageZipPath.empty()) {
        return;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(packageZipPath, error);
    if (error) {
        status.packageZipState = "needs_attention";
        status.packageZipReason = "mp overlay package zip inaccessible";
        status.state = "needs_attention";
        status.reason = status.packageZipReason;
        return;
    }
    if (!exists) {
        status.packageZipState = "needs_attention";
        status.packageZipReason = "mp overlay package zip missing";
        status.state = "needs_attention";
        status.reason = status.packageZipReason;
        return;
    }

    const bool isRegularFile = std::filesystem::is_regular_file(packageZipPath, error);
    if (error) {
        status.packageZipState = "needs_attention";
        status.packageZipReason = "mp overlay package zip inaccessible";
        status.state = "needs_attention";
        status.reason = status.packageZipReason;
        return;
    }
    if (!isRegularFile) {
        status.packageZipState = "needs_attention";
        status.packageZipReason = "mp overlay package zip path is not a file";
        status.state = "needs_attention";
        status.reason = status.packageZipReason;
        return;
    }

    const auto byteCount = std::filesystem::file_size(packageZipPath, error);
    if (error) {
        status.packageZipState = "needs_attention";
        status.packageZipReason = "mp overlay package zip byte count unavailable";
        status.state = "needs_attention";
        status.reason = status.packageZipReason;
        return;
    }

    status.packageZipBytes = byteCount;
    status.packageZipState = "ready";
    status.packageZipReason = "mp overlay package zip ready for handoff";
}

CompanionSubsystemStatus buildGameplayAcceptanceStatus(const std::filesystem::path& reportPath)
{
    CompanionSubsystemStatus status;
    status.path = reportPath;

    if (reportPath.empty()) {
        status.state = "starting";
        status.reason = "gameplay acceptance pending";
        return status;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(reportPath, error);
    if (error) {
        status.state = "needs_attention";
        status.reason = "gameplay acceptance report inaccessible";
        return status;
    }
    if (!exists) {
        status.state = "starting";
        status.reason = "gameplay acceptance pending";
        return status;
    }

    const bool isRegularFile = std::filesystem::is_regular_file(reportPath, error);
    if (error || !isRegularFile) {
        status.state = "needs_attention";
        status.reason = "gameplay acceptance report path is not a file";
        return status;
    }

    std::string reportJson;
    if (!common::tryReadTextFile(reportPath, reportJson)) {
        status.state = "needs_attention";
        status.reason = "gameplay acceptance report unreadable";
        return status;
    }

    const auto setReasonFromSummaryOrFallback = [&](const std::string& fallback) {
        if (const auto summary = common::extractJsonString(reportJson, "summary"); summary.has_value() && !summary->empty()) {
            status.reason = *summary;
        } else {
            status.reason = fallback;
        }
    };

    const auto acceptanceState = common::extractJsonString(reportJson, "acceptance_state");
    if (!acceptanceState.has_value()) {
        status.state = "needs_attention";
        status.reason = "gameplay acceptance report malformed";
        return status;
    }

    if (acceptanceState.value() == "verified_for_v0_domains") {
        if (!hasVerifiedGameplayAcceptanceCoverage(reportJson)) {
            status.state = "needs_attention";
            status.reason = "gameplay acceptance report incomplete for verified state";
            return status;
        }
        status.state = "ready";
        setReasonFromSummaryOrFallback("gameplay acceptance verified for v0 domains");
        return status;
    }
    if (acceptanceState.value() == "pending") {
        status.state = "starting";
        setReasonFromSummaryOrFallback("gameplay acceptance pending");
        return status;
    }
    if (acceptanceState.value() == "failed") {
        status.state = "needs_attention";
        setReasonFromSummaryOrFallback("gameplay acceptance failed");
        return status;
    }

    status.state = "needs_attention";
    status.reason = "gameplay acceptance report has unknown state";
    return status;
}

CompanionGeneratedOverlayPublishGateStatus buildGeneratedOverlayPublishGateStatus(
    const CompanionSubsystemStatus& generatedOverlay,
    const CompanionStatusConfig& config)
{
    CompanionGeneratedOverlayPublishGateStatus status;

    if (!config.generatedOverlayStagingStatusPath.empty()) {
        status.stagingStatusPath = config.generatedOverlayStagingStatusPath;
        status.activeOverlayDirectory = config.generatedOverlayActiveDirectory.empty()
            ? config.generatedOverlayDirectory
            : config.generatedOverlayActiveDirectory;
        status.publishStatusPath = config.generatedOverlayPublishStatusPath;
        status.backupRootDirectory = config.generatedOverlayPublishBackupRootDirectory;
        status.path = status.activeOverlayDirectory;
        status.ownerApprovalRequired = true;

        if (status.activeOverlayDirectory.empty()) {
            status.state = "needs_setup";
            status.reason = "active generated overlay directory not configured";
            return status;
        }
        if (status.publishStatusPath.empty()) {
            status.state = "needs_setup";
            status.reason = "generated overlay publish status path not configured";
            return status;
        }

        std::error_code activeError;
        status.activeOverlayExists =
            std::filesystem::exists(status.activeOverlayDirectory, activeError) &&
            !activeError &&
            std::filesystem::is_directory(status.activeOverlayDirectory, activeError) &&
            !activeError;
        status.backupBeforeReplace = status.activeOverlayExists;

        std::string stagingStatusJson;
        if (!common::tryReadTextFile(status.stagingStatusPath, stagingStatusJson)) {
            status.state = "starting";
            status.reason = "waiting for staged generated overlay status";
            return status;
        }

        const auto ok = extractJsonBool(stagingStatusJson, "ok");
        if (!ok.has_value() || !*ok) {
            status.state = "needs_attention";
            status.reason = "staged generated overlay status is not successful";
            return status;
        }

        const auto readiness = common::extractJsonString(stagingStatusJson, "readiness");
        if (!readiness.has_value() || *readiness != "staged_verified") {
            status.state = "needs_attention";
            status.reason = "staged generated overlay is not staged_verified";
            return status;
        }

        const auto manifestVerified = extractJsonBool(stagingStatusJson, "manifest_verified");
        if (!manifestVerified.has_value() || !*manifestVerified) {
            status.state = "needs_attention";
            status.reason = "staged generated overlay manifest is not verified";
            return status;
        }

        const auto publishAllowed = extractJsonBool(stagingStatusJson, "publish_allowed");
        const auto publishesOverlay = extractJsonBool(stagingStatusJson, "publishes_overlay");
        if (publishAllowed.value_or(false) || publishesOverlay.value_or(false)) {
            status.state = "needs_attention";
            status.reason = "staged generated overlay unexpectedly claims direct publish";
            return status;
        }

        const auto stagedOverlayDirectory = common::extractJsonString(stagingStatusJson, "staged_overlay_directory");
        if (!stagedOverlayDirectory.has_value() || stagedOverlayDirectory->empty()) {
            status.state = "needs_attention";
            status.reason = "staged generated overlay directory missing from status";
            return status;
        }
        status.stagedOverlayDirectory = *stagedOverlayDirectory;

        const auto manifestHash = common::extractJsonString(stagingStatusJson, "manifest_hash");
        if (!manifestHash.has_value() || manifestHash->empty()) {
            status.state = "needs_attention";
            status.reason = "staged generated overlay manifest hash missing";
            return status;
        }
        status.manifestHash = *manifestHash;
        status.dslRuleCount = extractJsonSize(stagingStatusJson, "dsl_rule_count").value_or(0);

        std::error_code stagedError;
        if (!std::filesystem::is_directory(status.stagedOverlayDirectory, stagedError) || stagedError) {
            status.state = "needs_attention";
            status.reason = "staged generated overlay directory is missing";
            return status;
        }

        if (!status.publishStatusPath.empty()) {
            std::string publishStatusJson;
            if (common::tryReadTextFile(status.publishStatusPath, publishStatusJson) &&
                tryApplyCurrentPublishedOverlayStatus(publishStatusJson, generatedOverlay, status)) {
                return status;
            }
        }

        populatePublishGateProcessStatus(status, config);
        if (status.state == "ready") {
            status.reason = "staged generated overlay ready; owner approval required before publish";
            status.canPublish = true;
            status.publishCommand = buildGeneratedOverlayPublishCommand(status);
        }
        return status;
    }

    status.path = generatedOverlay.path;

    if (generatedOverlay.state == "needs_setup" || generatedOverlay.state == "needs_attention") {
        status.state = "needs_attention";
        status.reason = "generated overlay is not publishable";
        return status;
    }

    if (generatedOverlay.state == "starting") {
        status.state = "starting";
        status.reason = "waiting for generated overlay before publish";
        return status;
    }

    populatePublishGateProcessStatus(status, config);
    return status;
}

CompanionPostPlayPipelineStatus buildPostPlayPipelineStatus(const CompanionStatusConfig& config)
{
    CompanionPostPlayPipelineStatus status;
    status.entryPointAnalysisPath = config.entryPointAnalysisPath;
    status.postPlayPackagePath = config.postPlayPackagePath;
    status.decisionInputPackagePath = config.decisionInputPackagePath;
    status.candidateDecisionPackagePath = config.candidateDecisionPackagePath;
    status.dslDraftPath = config.dslDraftPath;
    status.dslDraftAuditPath = config.dslDraftAuditPath;
    status.generatedOverlayStagingStatusPath = config.postPlayGeneratedOverlayStagingStatusPath;

    auto inspectJsonFile = [](const std::filesystem::path& path, std::string& json, std::string& errorReason) {
        if (path.empty()) {
            errorReason = "path not configured";
            return false;
        }

        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        if (error) {
            errorReason = "path inaccessible";
            return false;
        }
        if (!exists) {
            errorReason = "missing";
            return false;
        }
        const bool isRegularFile = std::filesystem::is_regular_file(path, error);
        if (error || !isRegularFile) {
            errorReason = "path is not a file";
            return false;
        }
        if (!common::tryReadTextFile(path, json)) {
            errorReason = "unreadable";
            return false;
        }
        errorReason.clear();
        return true;
    };

    std::string json;
    std::string fileError;
    bool candidateAvailable = false;
    bool decisionInputAvailable = false;
    bool postPlayAvailable = false;
    bool entryPointAvailable = false;
    bool dslDraftAvailable = false;
    bool generatedOverlayStagingAvailable = false;
    if (inspectJsonFile(status.generatedOverlayStagingStatusPath, json, fileError)) {
        generatedOverlayStagingAvailable = true;
        status.generatedOverlayStagingReadiness = common::extractJsonString(json, "readiness").value_or("");
        status.generatedOverlayStagingReason = common::extractJsonString(json, "reason").value_or("");
        status.generatedOverlayStagingRuleCount = extractJsonSize(json, "dsl_rule_count").value_or(0);
        status.generatedOverlayManifestVerified = extractJsonBool(json, "manifest_verified").value_or(false);
        status.generatedOverlayPublishAllowed = extractJsonBool(json, "publish_allowed").value_or(false);
    } else if (!status.generatedOverlayStagingStatusPath.empty() && fileError != "missing") {
        status.state = "needs_attention";
        status.reason = "generated overlay staging status " + fileError;
        return status;
    }

    if (inspectJsonFile(status.dslDraftAuditPath, json, fileError)) {
        dslDraftAvailable = true;
        status.dslDraftReadiness = common::extractJsonString(json, "readiness").value_or("");
        status.dslDraftReason = common::extractJsonString(json, "reason").value_or("");
        status.dslDraftRuleCount = extractJsonSize(json, "dsl_rule_count").value_or(0);
        status.dslDraftEligibleCandidateCount = extractJsonSize(json, "eligible_candidate_count").value_or(0);
        status.dslDraftSkippedCandidateCount = extractJsonSize(json, "skipped_candidate_count").value_or(0);
        status.dslDraftValidatorPassed = extractJsonBool(json, "validator_passed").value_or(false);
    } else if (!status.dslDraftAuditPath.empty() && fileError != "missing") {
        status.state = "needs_attention";
        status.reason = "dsl draft audit " + fileError;
        return status;
    }

    if (inspectJsonFile(status.candidateDecisionPackagePath, json, fileError)) {
        candidateAvailable = true;
        status.candidateDecisionPackageReadiness = common::extractJsonString(json, "readiness").value_or("");
        status.candidateDecisionPackageReason = common::extractJsonString(json, "reason").value_or("");
        status.candidateDecisionCount = extractJsonSize(json, "candidate_decision_count").value_or(0);
        status.candidateDecisionBlockedSourceEntryCount = extractJsonSize(json, "blocked_source_entry_count").value_or(0);
        status.candidateDecisionValidatorPassed = extractJsonBool(json, "validator_passed").value_or(false);
    } else if (!status.candidateDecisionPackagePath.empty() && fileError != "missing") {
        status.state = "needs_attention";
        status.reason = "candidate decision package " + fileError;
        return status;
    }

    if (inspectJsonFile(status.decisionInputPackagePath, json, fileError)) {
        decisionInputAvailable = true;
        status.decisionInputPackageReadiness = common::extractJsonString(json, "readiness").value_or("");
        status.decisionInputPackageReason = common::extractJsonString(json, "reason").value_or("");
        status.decisionInputCount = extractJsonSize(json, "decision_input_count").value_or(0);
        status.decisionInputBlockedEntryCount = extractJsonSize(json, "blocked_entry_count").value_or(0);
    } else if (!status.decisionInputPackagePath.empty() && fileError != "missing") {
        status.state = "needs_attention";
        status.reason = "decision input package " + fileError;
        return status;
    }

    if (inspectJsonFile(status.postPlayPackagePath, json, fileError)) {
        postPlayAvailable = true;
        status.postPlayPackageReadiness = common::extractJsonString(json, "readiness").value_or("");
        status.postPlayPackageReason = common::extractJsonString(json, "reason").value_or("");
        status.postPlayDecisionReadyEntryCount = extractJsonSize(json, "decision_ready_entry_count").value_or(0);
        parsePostPlayCampaignSummaries(json, status);
    } else if (!status.postPlayPackagePath.empty() && fileError != "missing") {
        status.state = "needs_attention";
        status.reason = "post-play package " + fileError;
        return status;
    }

    if (inspectJsonFile(status.entryPointAnalysisPath, json, fileError)) {
        entryPointAvailable = true;
        status.entryPointReadiness = common::extractJsonString(json, "readiness").value_or("");
        status.entryPointReason = common::extractJsonString(json, "reason").value_or("");
        status.entryPointCount = extractJsonSize(json, "entry_point_count").value_or(0);
        status.branchAmbiguityDetected = extractJsonBool(json, "branch_ambiguity_detected").value_or(false);
    } else if (!status.entryPointAnalysisPath.empty() && fileError != "missing") {
        status.state = "needs_attention";
        status.reason = "entry point analysis " + fileError;
        return status;
    }

    if (generatedOverlayStagingAvailable) {
        status.state = stateFromGeneratedOverlayStagingReadiness(status.generatedOverlayStagingReadiness);
        status.reason = reasonFromGeneratedOverlayStagingReadiness(status.generatedOverlayStagingReadiness);
        return status;
    }
    if (dslDraftAvailable) {
        status.state = stateFromReadiness(status.dslDraftReadiness);
        status.reason = reasonFromReadiness("dsl draft", status.dslDraftReadiness);
        return status;
    }
    if (candidateAvailable) {
        status.state = stateFromReadiness(status.candidateDecisionPackageReadiness);
        status.reason = reasonFromReadiness("candidate decision package", status.candidateDecisionPackageReadiness);
        return status;
    }
    if (decisionInputAvailable) {
        status.state = stateFromReadiness(status.decisionInputPackageReadiness);
        status.reason = reasonFromReadiness("decision input package", status.decisionInputPackageReadiness);
        return status;
    }
    if (postPlayAvailable) {
        status.state = stateFromReadiness(status.postPlayPackageReadiness);
        status.reason = reasonFromReadiness("post-play package", status.postPlayPackageReadiness);
        return status;
    }
    if (entryPointAvailable) {
        if (status.branchAmbiguityDetected) {
            status.state = "needs_attention";
            status.reason = "entry point analysis branch ambiguity detected";
        } else {
            status.state = stateFromReadiness(status.entryPointReadiness);
            status.reason = reasonFromReadiness("entry point analysis", status.entryPointReadiness);
        }
        return status;
    }

    status.state = "starting";
    status.reason = "waiting for post-play pipeline artifacts";
    return status;
}

CompanionSubsystemStatus buildStatusCenterStatus(
    const CompanionSubsystemStatus& saveDiscovery,
    const CompanionSubsystemStatus& archive,
    const CompanionSubsystemStatus& generatedOverlay,
    const CompanionGeneratedOverlayPublishGateStatus& generatedOverlayPublishGate,
    const CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const CompanionPostPlayPipelineStatus& postPlayPipeline,
    const CompanionSubsystemStatus& gameplayAcceptance)
{
    CompanionSubsystemStatus status;
    status.state = "ready";
    status.reason = "no attention required";

    const bool archiveNeedsAttention =
        archive.state == "needs_setup" || archive.state == "needs_attention";
    const bool overlayNeedsAttention =
        generatedOverlay.state == "needs_setup" || generatedOverlay.state == "needs_attention";
    const bool publishGateNeedsAttention =
        generatedOverlayPublishGate.state == "needs_setup" || generatedOverlayPublishGate.state == "needs_attention";
    const bool mpNeedsAttention = mpOverlayPackage.state == "needs_attention";
    const bool postPlayNeedsAttention = postPlayPipeline.state == "needs_attention";
    const bool gameplayAcceptanceNeedsAttention = gameplayAcceptance.state == "needs_attention";

    if (archiveNeedsAttention || overlayNeedsAttention || publishGateNeedsAttention || mpNeedsAttention ||
        postPlayNeedsAttention || gameplayAcceptanceNeedsAttention) {
        status.state = "attention_required";
        if (archiveNeedsAttention && overlayNeedsAttention && publishGateNeedsAttention && mpNeedsAttention) {
            status.reason = "archive, generated overlay, publish gate, and mp overlay package need attention";
            return status;
        }
        if (archiveNeedsAttention && overlayNeedsAttention) {
            status.reason = "archive and generated overlay need attention";
            return status;
        }
        if (archiveNeedsAttention && mpNeedsAttention) {
            status.reason = "archive and mp overlay package need attention";
            return status;
        }
        if (overlayNeedsAttention && mpNeedsAttention) {
            status.reason = "generated overlay and mp overlay package need attention";
            return status;
        }
        if (publishGateNeedsAttention && mpNeedsAttention) {
            status.reason = "generated overlay publish gate and mp overlay package need attention";
            return status;
        }
        if (archiveNeedsAttention) {
            status.reason = "archive needs attention";
            status.path = archive.path;
            if (archive.state == "needs_setup" && !saveDiscovery.path.empty()) {
                status.path = saveDiscovery.path;
            }
            return status;
        }
        if (overlayNeedsAttention) {
            status.reason = "generated overlay needs attention";
            status.path = generatedOverlay.path;
            return status;
        }
        if (publishGateNeedsAttention) {
            status.reason = "generated overlay publish gate needs attention";
            status.path = generatedOverlayPublishGate.path;
            return status;
        }
        if (postPlayNeedsAttention && gameplayAcceptanceNeedsAttention) {
            status.reason = "post-play pipeline and gameplay acceptance need attention";
            status.path = postPlayPipelineFocusPath(postPlayPipeline);
            if (status.path.empty()) {
                status.path = gameplayAcceptance.path;
            }
            return status;
        }
        if (postPlayNeedsAttention) {
            status.reason = "post-play pipeline needs attention";
            status.path = postPlayPipelineFocusPath(postPlayPipeline);
            return status;
        }
        if (gameplayAcceptanceNeedsAttention) {
            status.reason = "gameplay acceptance needs attention";
            status.path = gameplayAcceptance.path;
            return status;
        }
        status.reason = "mp overlay package needs attention";
        status.path = mpOverlayPackage.path;
        return status;
    }

    const bool archiveStarting = archive.state == "starting";
    const bool overlayStarting = generatedOverlay.state == "starting";
    const bool publishGateStarting = generatedOverlayPublishGate.state == "starting";
    const bool postPlayStarting = postPlayPipeline.state == "starting";
    const bool gameplayAcceptanceStarting = gameplayAcceptance.state == "starting";

    if (generatedOverlayPublishGate.state == "blocked") {
        status.state = "waiting";
        status.reason = generatedOverlayPublishGate.reason;
        status.path = generatedOverlayPublishGate.path;
        return status;
    }

    if (archiveStarting || overlayStarting || publishGateStarting || postPlayStarting || gameplayAcceptanceStarting) {
        status.state = "starting";
        if (archiveStarting && overlayStarting && publishGateStarting) {
            status.reason = "waiting for archive, generated overlay, and publish gate to become ready";
            return status;
        }
        if (archiveStarting && overlayStarting) {
            status.reason = "waiting for archive and generated overlay to become ready";
            return status;
        }
        if (archiveStarting && publishGateStarting) {
            status.reason = "waiting for archive and generated overlay publish gate to become ready";
            return status;
        }
        if (overlayStarting && publishGateStarting) {
            status.reason = "waiting for generated overlay and publish gate to become ready";
            return status;
        }
        if (archiveStarting) {
            status.reason = "waiting for archive to become ready";
            status.path = archive.path;
            return status;
        }
        if (publishGateStarting) {
            status.reason = "waiting for generated overlay publish gate to become ready";
            status.path = generatedOverlayPublishGate.path;
            return status;
        }
        if (postPlayStarting && gameplayAcceptanceStarting) {
            status.reason = "waiting for post-play pipeline and gameplay acceptance";
            status.path = postPlayPipelineFocusPath(postPlayPipeline);
            if (status.path.empty()) {
                status.path = gameplayAcceptance.path;
            }
            return status;
        }
        if (postPlayStarting) {
            status.reason = "waiting for post-play pipeline";
            status.path = postPlayPipelineFocusPath(postPlayPipeline);
            return status;
        }
        if (gameplayAcceptanceStarting) {
            status.reason = "waiting for gameplay acceptance";
            status.path = gameplayAcceptance.path;
            return status;
        }
        status.reason = "waiting for generated overlay to become ready";
        status.path = generatedOverlay.path;
        return status;
    }

    return status;
}

std::string buildStatusCenterSummaryText(
    const std::string& generatedAtLocal,
    const CompanionSubsystemStatus& saveDiscovery,
    const CompanionSubsystemStatus& archive,
    const CompanionSubsystemStatus& generatedOverlay,
    const CompanionGeneratedOverlayPublishGateStatus& generatedOverlayPublishGate,
    const CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const CompanionPostPlayPipelineStatus& postPlayPipeline,
    const CompanionSubsystemStatus& gameplayAcceptance,
    const CompanionSubsystemStatus& statusCenter)
{
    std::ostringstream text;
    text << "Strategic Nexus Status Center\n";
    if (!generatedAtLocal.empty()) {
        text << "generated_at_local: " << generatedAtLocal << "\n";
    }
    text << "stav: " << statusCenter.state << " - " << statusCenter.reason << "\n";
    text << "nalezeni_uloziste: " << saveDiscovery.state << " - " << saveDiscovery.reason << "\n";
    if (!saveDiscovery.path.empty()) {
        text << "nalezeni_uloziste_cesta: " << pathString(saveDiscovery.path) << "\n";
    }
    text << "archiv: " << archive.state << " - " << archive.reason << "\n";
    if (!archive.path.empty()) {
        text << "archiv_cesta: " << pathString(archive.path) << "\n";
    }
    text << "generovany_overlay: " << generatedOverlay.state << " - " << generatedOverlay.reason << "\n";
    if (!generatedOverlay.path.empty()) {
        text << "generovany_overlay_cesta: " << pathString(generatedOverlay.path) << "\n";
    }
    text << "publish_gate: " << generatedOverlayPublishGate.state << " - " << generatedOverlayPublishGate.reason << "\n";
    if (!generatedOverlayPublishGate.path.empty()) {
        text << "publish_gate_cesta: " << pathString(generatedOverlayPublishGate.path) << "\n";
    }
    if (!generatedOverlayPublishGate.stagingStatusPath.empty()) {
        text << "publish_gate_staging_status: " << pathString(generatedOverlayPublishGate.stagingStatusPath) << "\n";
    }
    if (!generatedOverlayPublishGate.stagedOverlayDirectory.empty()) {
        text << "publish_gate_staged_overlay: " << pathString(generatedOverlayPublishGate.stagedOverlayDirectory) << "\n";
    }
    if (!generatedOverlayPublishGate.activeOverlayDirectory.empty()) {
        text << "publish_gate_active_overlay: " << pathString(generatedOverlayPublishGate.activeOverlayDirectory) << "\n";
    }
    if (!generatedOverlayPublishGate.publishStatusPath.empty()) {
        text << "publish_gate_status_output: " << pathString(generatedOverlayPublishGate.publishStatusPath) << "\n";
    }
    if (!generatedOverlayPublishGate.backupRootDirectory.empty()) {
        text << "publish_gate_backup_root: " << pathString(generatedOverlayPublishGate.backupRootDirectory) << "\n";
    }
    if (!generatedOverlayPublishGate.backupDirectory.empty()) {
        text << "publish_gate_backup_directory: " << pathString(generatedOverlayPublishGate.backupDirectory) << "\n";
    }
    if (!generatedOverlayPublishGate.manifestHash.empty()) {
        text << "publish_gate_manifest_hash: " << generatedOverlayPublishGate.manifestHash << "\n";
    }
    if (!generatedOverlayPublishGate.publishedManifestHash.empty()) {
        text << "publish_gate_published_manifest_hash: " << generatedOverlayPublishGate.publishedManifestHash << "\n";
    }
    text << "publish_gate_rule_count: " << generatedOverlayPublishGate.dslRuleCount << "\n";
    text << "publish_gate_published: " << (generatedOverlayPublishGate.published ? "true" : "false") << "\n";
    text << "publish_gate_backup_created: " << (generatedOverlayPublishGate.backupCreated ? "true" : "false") << "\n";
    text << "publish_gate_published_file_count: " << generatedOverlayPublishGate.publishedFileCount << "\n";
    text << "publish_gate_owner_approval_required: "
         << (generatedOverlayPublishGate.ownerApprovalRequired ? "true" : "false") << "\n";
    text << "publish_gate_can_publish: " << (generatedOverlayPublishGate.canPublish ? "true" : "false") << "\n";
    text << "publish_gate_active_overlay_exists: "
         << (generatedOverlayPublishGate.activeOverlayExists ? "true" : "false") << "\n";
    text << "publish_gate_backup_before_replace: "
         << (generatedOverlayPublishGate.backupBeforeReplace ? "true" : "false") << "\n";
    if (!generatedOverlayPublishGate.publishCommand.empty()) {
        text << "publish_gate_command: " << generatedOverlayPublishGate.publishCommand << "\n";
    }
    text << "mp_overlay_balicek: " << mpOverlayPackage.state << " - " << mpOverlayPackage.reason << "\n";
    if (!mpOverlayPackage.path.empty()) {
        text << "mp_overlay_balicek_cesta: " << pathString(mpOverlayPackage.path) << "\n";
    }
    if (!mpOverlayPackage.packageZipState.empty()) {
        text << "mp_package_zip_state: " << mpOverlayPackage.packageZipState << "\n";
    }
    if (!mpOverlayPackage.packageZipReason.empty()) {
        text << "mp_package_zip_reason: " << mpOverlayPackage.packageZipReason << "\n";
    }
    if (!mpOverlayPackage.packageZipPath.empty()) {
        text << "mp_package_zip_path: " << pathString(mpOverlayPackage.packageZipPath) << "\n";
    }
    if (!mpOverlayPackage.packageZipState.empty()) {
        text << "mp_package_zip_bytes: " << mpOverlayPackage.packageZipBytes << "\n";
    }
    text << "post_play_pipeline: " << postPlayPipeline.state << " - " << postPlayPipeline.reason << "\n";
    if (!postPlayPipeline.entryPointAnalysisPath.empty()) {
        text << "entry_point_analysis_path: " << pathString(postPlayPipeline.entryPointAnalysisPath) << "\n";
    }
    if (!postPlayPipeline.entryPointReadiness.empty()) {
        text << "entry_point_readiness: " << postPlayPipeline.entryPointReadiness << "\n";
    }
    if (!postPlayPipeline.entryPointReason.empty()) {
        text << "entry_point_reason: " << postPlayPipeline.entryPointReason << "\n";
    }
    text << "entry_point_count: " << postPlayPipeline.entryPointCount << "\n";
    text << "branch_ambiguity_detected: " << (postPlayPipeline.branchAmbiguityDetected ? "true" : "false") << "\n";
    if (!postPlayPipeline.postPlayPackagePath.empty()) {
        text << "post_play_package_path: " << pathString(postPlayPipeline.postPlayPackagePath) << "\n";
    }
    if (!postPlayPipeline.postPlayPackageReadiness.empty()) {
        text << "post_play_package_readiness: " << postPlayPipeline.postPlayPackageReadiness << "\n";
    }
    if (!postPlayPipeline.postPlayPackageReason.empty()) {
        text << "post_play_package_reason: " << postPlayPipeline.postPlayPackageReason << "\n";
    }
    text << "post_play_decision_ready_entry_count: " << postPlayPipeline.postPlayDecisionReadyEntryCount << "\n";
    text << "post_play_campaign_count: " << postPlayPipeline.postPlayCampaignCount << "\n";
    text << "post_play_ready_campaign_count: " << postPlayPipeline.postPlayReadyCampaignCount << "\n";
    text << "post_play_partial_campaign_count: " << postPlayPipeline.postPlayPartialCampaignCount << "\n";
    text << "post_play_blocked_campaign_count: " << postPlayPipeline.postPlayBlockedCampaignCount << "\n";
    for (const auto& summary : postPlayPipeline.postPlayCampaignReadinessSummaries) {
        text << "post_play_campaign_summary: " << summary << "\n";
    }
    if (!postPlayPipeline.decisionInputPackagePath.empty()) {
        text << "decision_input_package_path: " << pathString(postPlayPipeline.decisionInputPackagePath) << "\n";
    }
    if (!postPlayPipeline.decisionInputPackageReadiness.empty()) {
        text << "decision_input_package_readiness: " << postPlayPipeline.decisionInputPackageReadiness << "\n";
    }
    if (!postPlayPipeline.decisionInputPackageReason.empty()) {
        text << "decision_input_package_reason: " << postPlayPipeline.decisionInputPackageReason << "\n";
    }
    text << "decision_input_count: " << postPlayPipeline.decisionInputCount << "\n";
    text << "decision_input_blocked_entry_count: " << postPlayPipeline.decisionInputBlockedEntryCount << "\n";
    if (!postPlayPipeline.candidateDecisionPackagePath.empty()) {
        text << "candidate_decision_package_path: " << pathString(postPlayPipeline.candidateDecisionPackagePath) << "\n";
    }
    if (!postPlayPipeline.candidateDecisionPackageReadiness.empty()) {
        text << "candidate_decision_package_readiness: " << postPlayPipeline.candidateDecisionPackageReadiness << "\n";
    }
    if (!postPlayPipeline.candidateDecisionPackageReason.empty()) {
        text << "candidate_decision_package_reason: " << postPlayPipeline.candidateDecisionPackageReason << "\n";
    }
    text << "candidate_decision_count: " << postPlayPipeline.candidateDecisionCount << "\n";
    text << "candidate_decision_blocked_source_entry_count: "
         << postPlayPipeline.candidateDecisionBlockedSourceEntryCount << "\n";
    text << "candidate_decision_validator_passed: "
         << (postPlayPipeline.candidateDecisionValidatorPassed ? "true" : "false") << "\n";
    if (!postPlayPipeline.dslDraftPath.empty()) {
        text << "dsl_draft_path: " << pathString(postPlayPipeline.dslDraftPath) << "\n";
    }
    if (!postPlayPipeline.dslDraftAuditPath.empty()) {
        text << "dsl_draft_audit_path: " << pathString(postPlayPipeline.dslDraftAuditPath) << "\n";
    }
    if (!postPlayPipeline.dslDraftReadiness.empty()) {
        text << "dsl_draft_readiness: " << postPlayPipeline.dslDraftReadiness << "\n";
    }
    if (!postPlayPipeline.dslDraftReason.empty()) {
        text << "dsl_draft_reason: " << postPlayPipeline.dslDraftReason << "\n";
    }
    text << "dsl_draft_rule_count: " << postPlayPipeline.dslDraftRuleCount << "\n";
    text << "dsl_draft_eligible_candidate_count: " << postPlayPipeline.dslDraftEligibleCandidateCount << "\n";
    text << "dsl_draft_skipped_candidate_count: " << postPlayPipeline.dslDraftSkippedCandidateCount << "\n";
    text << "dsl_draft_validator_passed: "
         << (postPlayPipeline.dslDraftValidatorPassed ? "true" : "false") << "\n";
    if (!postPlayPipeline.generatedOverlayStagingStatusPath.empty()) {
        text << "generated_overlay_staging_status_path: "
             << pathString(postPlayPipeline.generatedOverlayStagingStatusPath) << "\n";
    }
    if (!postPlayPipeline.generatedOverlayStagingReadiness.empty()) {
        text << "generated_overlay_staging_readiness: "
             << postPlayPipeline.generatedOverlayStagingReadiness << "\n";
    }
    if (!postPlayPipeline.generatedOverlayStagingReason.empty()) {
        text << "generated_overlay_staging_reason: "
             << postPlayPipeline.generatedOverlayStagingReason << "\n";
    }
    text << "generated_overlay_staging_rule_count: "
         << postPlayPipeline.generatedOverlayStagingRuleCount << "\n";
    text << "generated_overlay_manifest_verified: "
         << (postPlayPipeline.generatedOverlayManifestVerified ? "true" : "false") << "\n";
    text << "generated_overlay_publish_allowed: "
         << (postPlayPipeline.generatedOverlayPublishAllowed ? "true" : "false") << "\n";

    if (!generatedOverlay.manifestHash.empty()) {
        text << "generated_overlay_manifest_hash: " << generatedOverlay.manifestHash << "\n";
    }
    if (!mpOverlayPackage.campaignId.empty()) {
        text << "campaign_id: " << mpOverlayPackage.campaignId << "\n";
    }
    if (!mpOverlayPackage.overlayVersion.empty()) {
        text << "overlay_version: " << mpOverlayPackage.overlayVersion << "\n";
    }
    if (!mpOverlayPackage.gameVersion.empty()) {
        text << "game_version: " << mpOverlayPackage.gameVersion << "\n";
    }
    if (!mpOverlayPackage.strategicNexusModVersion.empty()) {
        text << "strategic_nexus_mod_version: " << mpOverlayPackage.strategicNexusModVersion << "\n";
    }
    if (!mpOverlayPackage.handoffStatus.empty()) {
        text << "handoff_status: " << mpOverlayPackage.handoffStatus << "\n";
    }
    if (!mpOverlayPackage.readiness.empty()) {
        text << "mp_readiness: " << mpOverlayPackage.readiness << "\n";
    }
    if (!mpOverlayPackage.hostReadiness.empty()) {
        text << "mp_host_readiness: " << mpOverlayPackage.hostReadiness << "\n";
    }
    if (!mpOverlayPackage.clientReadinessGate.empty()) {
        text << "mp_client_readiness_gate: " << mpOverlayPackage.clientReadinessGate << "\n";
    }
    if (!mpOverlayPackage.hostNextStep.empty()) {
        text << "mp_host_next_step: " << mpOverlayPackage.hostNextStep << "\n";
    }
    if (!mpOverlayPackage.clientNextStep.empty()) {
        text << "mp_client_next_step: " << mpOverlayPackage.clientNextStep << "\n";
    }
    if (!mpOverlayPackage.packageManifestHash.empty()) {
        text << "package_manifest_hash: " << mpOverlayPackage.packageManifestHash << "\n";
    }
    if (!mpOverlayPackage.verifyCommand.empty()) {
        text << "mp_verify_command: " << mpOverlayPackage.verifyCommand << "\n";
    }
    if (!mpOverlayPackage.importCommand.empty()) {
        text << "mp_import_command: " << mpOverlayPackage.importCommand << "\n";
    }
    if (!mpOverlayPackage.strictVerifyCommand.empty()) {
        text << "mp_strict_verify_command: " << mpOverlayPackage.strictVerifyCommand << "\n";
    }
    if (!mpOverlayPackage.strictImportCommand.empty()) {
        text << "mp_strict_import_command: " << mpOverlayPackage.strictImportCommand << "\n";
    }
    if (!mpOverlayPackage.statusText.empty()) {
        text << "mp_overlay_package_status_text: " << mpOverlayPackage.statusText << "\n";
    }
    text << "mp_warning_count: " << mpOverlayPackage.warningCount << "\n";
    for (const auto& warningCode : mpOverlayPackage.warningCodes) {
        text << "mp_warning_code: " << warningCode << "\n";
    }
    text << "mp_identity_mismatch_warning: " << (mpOverlayPackage.identityMismatchWarning ? "true" : "false") << "\n";
    for (const auto& warningCode : mpOverlayPackage.identityMismatchWarningCodes) {
        text << "mp_identity_mismatch_warning_code: " << warningCode << "\n";
    }
    text << "mp_mismatch_warning_state: " << mpOverlayPackage.mismatchWarningState << "\n";
    text << "mp_mismatch_warning_reason: " << mpOverlayPackage.mismatchWarningReason << "\n";
    for (const auto& warningCode : mpOverlayPackage.mismatchWarningCodes) {
        text << "mp_mismatch_warning_code: " << warningCode << "\n";
    }
    if (!mpOverlayPackage.identityMismatchAlert.empty()) {
        text << "mp_identity_mismatch_alert: " << mpOverlayPackage.identityMismatchAlert << "\n";
    }
    text << "gameplay_acceptance: " << gameplayAcceptance.state << " - " << gameplayAcceptance.reason << "\n";
    if (!gameplayAcceptance.path.empty()) {
        text << "gameplay_acceptance_report_path: " << pathString(gameplayAcceptance.path) << "\n";
    }
    return text.str();
}

void writeSubsystemJson(
    std::ostringstream& output,
    const CompanionSubsystemStatus& status,
    const std::string& indent,
    const bool includeManifestHash = false)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"path\": " << jsonString(pathString(status.path));
    if (includeManifestHash) {
        output << ",\n";
        output << indent << "  \"manifest_hash\": " << jsonString(status.manifestHash) << "\n";
    } else {
        output << "\n";
    }
    output << indent << "}";
}

void writeGeneratedOverlayPublishGateJson(
    std::ostringstream& output,
    const CompanionGeneratedOverlayPublishGateStatus& status,
    const std::string& indent)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"path\": " << jsonString(pathString(status.path)) << ",\n";
    output << indent << "  \"staging_status_path\": " << jsonString(pathString(status.stagingStatusPath)) << ",\n";
    output << indent << "  \"staged_overlay_directory\": "
           << jsonString(pathString(status.stagedOverlayDirectory)) << ",\n";
    output << indent << "  \"active_overlay_directory\": "
           << jsonString(pathString(status.activeOverlayDirectory)) << ",\n";
    output << indent << "  \"publish_status_path\": "
           << jsonString(pathString(status.publishStatusPath)) << ",\n";
    output << indent << "  \"backup_root_directory\": "
           << jsonString(pathString(status.backupRootDirectory)) << ",\n";
    output << indent << "  \"backup_directory\": "
           << jsonString(pathString(status.backupDirectory)) << ",\n";
    output << indent << "  \"manifest_hash\": " << jsonString(status.manifestHash) << ",\n";
    output << indent << "  \"published_manifest_hash\": " << jsonString(status.publishedManifestHash) << ",\n";
    output << indent << "  \"dsl_rule_count\": " << status.dslRuleCount << ",\n";
    output << indent << "  \"published_file_count\": " << status.publishedFileCount << ",\n";
    output << indent << "  \"published\": " << (status.published ? "true" : "false") << ",\n";
    output << indent << "  \"backup_created\": " << (status.backupCreated ? "true" : "false") << ",\n";
    output << indent << "  \"owner_approval_required\": "
           << (status.ownerApprovalRequired ? "true" : "false") << ",\n";
    output << indent << "  \"can_publish\": " << (status.canPublish ? "true" : "false") << ",\n";
    output << indent << "  \"active_overlay_exists\": "
           << (status.activeOverlayExists ? "true" : "false") << ",\n";
    output << indent << "  \"backup_before_replace\": "
           << (status.backupBeforeReplace ? "true" : "false") << ",\n";
    output << indent << "  \"publish_command\": " << jsonString(status.publishCommand) << "\n";
    output << indent << "}";
}

void writeMpOverlayPackageJson(std::ostringstream& output, const CompanionMpOverlayPackageStatus& status, const std::string& indent)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"path\": " << jsonString(pathString(status.path)) << ",\n";
    output << indent << "  \"package_zip_state\": " << jsonString(status.packageZipState) << ",\n";
    output << indent << "  \"package_zip_reason\": " << jsonString(status.packageZipReason) << ",\n";
    output << indent << "  \"package_zip_path\": " << jsonString(pathString(status.packageZipPath)) << ",\n";
    output << indent << "  \"package_zip_bytes\": " << status.packageZipBytes << ",\n";
    output << indent << "  \"campaign_id\": " << jsonString(status.campaignId) << ",\n";
    output << indent << "  \"overlay_version\": " << jsonString(status.overlayVersion) << ",\n";
    output << indent << "  \"game_version\": " << jsonString(status.gameVersion) << ",\n";
    output << indent << "  \"strategic_nexus_mod_version\": " << jsonString(status.strategicNexusModVersion) << ",\n";
    output << indent << "  \"handoff_status\": " << jsonString(status.handoffStatus) << ",\n";
    output << indent << "  \"readiness\": " << jsonString(status.readiness) << ",\n";
    output << indent << "  \"host_readiness\": " << jsonString(status.hostReadiness) << ",\n";
    output << indent << "  \"client_readiness_gate\": " << jsonString(status.clientReadinessGate) << ",\n";
    output << indent << "  \"host_next_step\": " << jsonString(status.hostNextStep) << ",\n";
    output << indent << "  \"client_next_step\": " << jsonString(status.clientNextStep) << ",\n";
    output << indent << "  \"package_manifest_hash\": " << jsonString(status.packageManifestHash) << ",\n";
    output << indent << "  \"verify_command\": " << jsonString(status.verifyCommand) << ",\n";
    output << indent << "  \"import_command\": " << jsonString(status.importCommand) << ",\n";
    output << indent << "  \"strict_verify_command\": " << jsonString(status.strictVerifyCommand) << ",\n";
    output << indent << "  \"strict_import_command\": " << jsonString(status.strictImportCommand) << ",\n";
    output << indent << "  \"status_text\": " << jsonString(status.statusText) << ",\n";
    output << indent << "  \"warning_count\": " << status.warningCount << ",\n";
    output << indent << "  \"warning_codes\": [";
    for (std::size_t index = 0; index < status.warningCodes.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << jsonString(status.warningCodes[index]);
    }
    output << "],\n";
    output << indent << "  \"identity_mismatch_warning\": "
           << (status.identityMismatchWarning ? "true" : "false") << ",\n";
    output << indent << "  \"identity_mismatch_warning_codes\": [";
    for (std::size_t index = 0; index < status.identityMismatchWarningCodes.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << jsonString(status.identityMismatchWarningCodes[index]);
    }
    output << "],\n";
    output << indent << "  \"mismatch_warning_state\": " << jsonString(status.mismatchWarningState) << ",\n";
    output << indent << "  \"mismatch_warning_reason\": " << jsonString(status.mismatchWarningReason) << ",\n";
    output << indent << "  \"mismatch_warning_codes\": [";
    for (std::size_t index = 0; index < status.mismatchWarningCodes.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << jsonString(status.mismatchWarningCodes[index]);
    }
    output << "],\n";
    output << indent << "  \"identity_mismatch_alert\": " << jsonString(status.identityMismatchAlert) << "\n";
    output << indent << "}";
}

void writePostPlayPipelineJson(
    std::ostringstream& output,
    const CompanionPostPlayPipelineStatus& status,
    const std::string& indent)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"entry_point_analysis_path\": " << jsonString(pathString(status.entryPointAnalysisPath)) << ",\n";
    output << indent << "  \"entry_point_readiness\": " << jsonString(status.entryPointReadiness) << ",\n";
    output << indent << "  \"entry_point_reason\": " << jsonString(status.entryPointReason) << ",\n";
    output << indent << "  \"entry_point_count\": " << status.entryPointCount << ",\n";
    output << indent << "  \"branch_ambiguity_detected\": "
           << (status.branchAmbiguityDetected ? "true" : "false") << ",\n";
    output << indent << "  \"post_play_package_path\": " << jsonString(pathString(status.postPlayPackagePath)) << ",\n";
    output << indent << "  \"post_play_package_readiness\": " << jsonString(status.postPlayPackageReadiness) << ",\n";
    output << indent << "  \"post_play_package_reason\": " << jsonString(status.postPlayPackageReason) << ",\n";
    output << indent << "  \"post_play_decision_ready_entry_count\": " << status.postPlayDecisionReadyEntryCount << ",\n";
    output << indent << "  \"post_play_campaign_count\": " << status.postPlayCampaignCount << ",\n";
    output << indent << "  \"post_play_ready_campaign_count\": " << status.postPlayReadyCampaignCount << ",\n";
    output << indent << "  \"post_play_partial_campaign_count\": " << status.postPlayPartialCampaignCount << ",\n";
    output << indent << "  \"post_play_blocked_campaign_count\": " << status.postPlayBlockedCampaignCount << ",\n";
    output << indent << "  \"post_play_campaign_summaries\": [";
    for (std::size_t index = 0; index < status.postPlayCampaignReadinessSummaries.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << jsonString(status.postPlayCampaignReadinessSummaries[index]);
    }
    output << "],\n";
    output << indent << "  \"decision_input_package_path\": " << jsonString(pathString(status.decisionInputPackagePath)) << ",\n";
    output << indent << "  \"decision_input_package_readiness\": " << jsonString(status.decisionInputPackageReadiness) << ",\n";
    output << indent << "  \"decision_input_package_reason\": " << jsonString(status.decisionInputPackageReason) << ",\n";
    output << indent << "  \"decision_input_count\": " << status.decisionInputCount << ",\n";
    output << indent << "  \"decision_input_blocked_entry_count\": " << status.decisionInputBlockedEntryCount << ",\n";
    output << indent << "  \"candidate_decision_package_path\": "
           << jsonString(pathString(status.candidateDecisionPackagePath)) << ",\n";
    output << indent << "  \"candidate_decision_package_readiness\": "
           << jsonString(status.candidateDecisionPackageReadiness) << ",\n";
    output << indent << "  \"candidate_decision_package_reason\": "
           << jsonString(status.candidateDecisionPackageReason) << ",\n";
    output << indent << "  \"candidate_decision_count\": " << status.candidateDecisionCount << ",\n";
    output << indent << "  \"candidate_decision_blocked_source_entry_count\": "
           << status.candidateDecisionBlockedSourceEntryCount << ",\n";
    output << indent << "  \"candidate_decision_validator_passed\": "
           << (status.candidateDecisionValidatorPassed ? "true" : "false") << ",\n";
    output << indent << "  \"dsl_draft_path\": " << jsonString(pathString(status.dslDraftPath)) << ",\n";
    output << indent << "  \"dsl_draft_audit_path\": " << jsonString(pathString(status.dslDraftAuditPath)) << ",\n";
    output << indent << "  \"dsl_draft_readiness\": " << jsonString(status.dslDraftReadiness) << ",\n";
    output << indent << "  \"dsl_draft_reason\": " << jsonString(status.dslDraftReason) << ",\n";
    output << indent << "  \"dsl_draft_rule_count\": " << status.dslDraftRuleCount << ",\n";
    output << indent << "  \"dsl_draft_eligible_candidate_count\": " << status.dslDraftEligibleCandidateCount << ",\n";
    output << indent << "  \"dsl_draft_skipped_candidate_count\": " << status.dslDraftSkippedCandidateCount << ",\n";
    output << indent << "  \"dsl_draft_validator_passed\": "
           << (status.dslDraftValidatorPassed ? "true" : "false") << ",\n";
    output << indent << "  \"generated_overlay_staging_status_path\": "
           << jsonString(pathString(status.generatedOverlayStagingStatusPath)) << ",\n";
    output << indent << "  \"generated_overlay_staging_readiness\": "
           << jsonString(status.generatedOverlayStagingReadiness) << ",\n";
    output << indent << "  \"generated_overlay_staging_reason\": "
           << jsonString(status.generatedOverlayStagingReason) << ",\n";
    output << indent << "  \"generated_overlay_staging_rule_count\": "
           << status.generatedOverlayStagingRuleCount << ",\n";
    output << indent << "  \"generated_overlay_manifest_verified\": "
           << (status.generatedOverlayManifestVerified ? "true" : "false") << ",\n";
    output << indent << "  \"generated_overlay_publish_allowed\": "
           << (status.generatedOverlayPublishAllowed ? "true" : "false") << "\n";
    output << indent << "}";
}

} // namespace

CompanionStatusSnapshot StrategicNexusCompanion::buildStatusSnapshot(const CompanionStatusConfig& config) const
{
    CompanionStatusSnapshot snapshot;
    snapshot.lifecycle.startWithWindowsEnabled = config.startWithWindowsEnabled;
    snapshot.generatedAtLocal = formatLocalTimestamp(std::chrono::system_clock::now());
    snapshot.saveDiscovery = buildSaveDiscoveryStatus();
    snapshot.archive = buildArchiveStatus(config.archiveRoot);
    snapshot.generatedOverlay = buildGeneratedOverlayStatus(config.generatedOverlayDirectory);
    snapshot.generatedOverlayPublishGate = buildGeneratedOverlayPublishGateStatus(snapshot.generatedOverlay, config);
    snapshot.mpOverlayPackage = buildMpOverlayPackageStatus(config.mpOverlayPackageDirectory);
    populateMpOverlayPackageZipStatus(snapshot.mpOverlayPackage, config.mpOverlayPackageZipPath);
    snapshot.postPlayPipeline = buildPostPlayPipelineStatus(config);
    snapshot.gameplayAcceptance = buildGameplayAcceptanceStatus(config.gameplayAcceptanceReportPath);
    snapshot.statusCenter =
        buildStatusCenterStatus(
            snapshot.saveDiscovery,
            snapshot.archive,
            snapshot.generatedOverlay,
            snapshot.generatedOverlayPublishGate,
            snapshot.mpOverlayPackage,
            snapshot.postPlayPipeline,
            snapshot.gameplayAcceptance);
    snapshot.statusCenterSummaryText = buildStatusCenterSummaryText(
        snapshot.generatedAtLocal,
        snapshot.saveDiscovery,
        snapshot.archive,
        snapshot.generatedOverlay,
        snapshot.generatedOverlayPublishGate,
        snapshot.mpOverlayPackage,
        snapshot.postPlayPipeline,
        snapshot.gameplayAcceptance,
        snapshot.statusCenter);
    return snapshot;
}

std::string serializeCompanionStatusSnapshot(const CompanionStatusSnapshot& snapshot)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"schema_version\": 1,\n";
    output << "  \"app_name\": " << jsonString(snapshot.appName) << ",\n";
    output << "  \"abbreviation\": " << jsonString(snapshot.abbreviation) << ",\n";
    output << "  \"generated_at_local\": " << jsonString(snapshot.generatedAtLocal) << ",\n";
    output << "  \"lifecycle\": {\n";
    output << "    \"start_with_windows_enabled\": "
           << (snapshot.lifecycle.startWithWindowsEnabled ? "true" : "false") << ",\n";
    output << "    \"window_close_behavior\": "
           << jsonString(snapshot.lifecycle.windowCloseBehavior) << ",\n";
    output << "    \"explicit_exit_behavior\": "
           << jsonString(snapshot.lifecycle.explicitExitBehavior) << ",\n";
    output << "    \"crash_restart_policy\": "
           << jsonString(snapshot.lifecycle.crashRestartPolicy) << "\n";
    output << "  },\n";
    output << "  \"archive_status\": ";
    writeSubsystemJson(output, snapshot.archive, "  ");
    output << ",\n";
    output << "  \"save_discovery_status\": ";
    writeSubsystemJson(output, snapshot.saveDiscovery, "  ");
    output << ",\n";
    output << "  \"generated_overlay_status\": ";
    writeSubsystemJson(output, snapshot.generatedOverlay, "  ", true);
    output << ",\n";
    output << "  \"generated_overlay_publish_gate_status\": ";
    writeGeneratedOverlayPublishGateJson(output, snapshot.generatedOverlayPublishGate, "  ");
    output << ",\n";
    output << "  \"mp_overlay_package_status\": ";
    writeMpOverlayPackageJson(output, snapshot.mpOverlayPackage, "  ");
    output << ",\n";
    output << "  \"post_play_pipeline_status\": ";
    writePostPlayPipelineJson(output, snapshot.postPlayPipeline, "  ");
    output << ",\n";
    output << "  \"gameplay_acceptance_status\": ";
    writeSubsystemJson(output, snapshot.gameplayAcceptance, "  ");
    output << ",\n";
    output << "  \"status_center\": ";
    writeSubsystemJson(output, snapshot.statusCenter, "  ");
    output << ",\n";
    output << "  \"status_center_summary_text\": " << jsonString(snapshot.statusCenterSummaryText) << "\n";
    output << "}\n";
    return output.str();
}

CompanionStatusLoopResult runCompanionStatusLoop(const StrategicNexusCompanion& companion, const CompanionStatusLoopConfig& config)
{
    CompanionStatusLoopResult result;

    if (config.statusOutputPath.empty()) {
        result.reason = "status output path not configured";
        return result;
    }

    std::error_code outputTypeError;
    const bool outputIsDirectory = std::filesystem::is_directory(config.statusOutputPath, outputTypeError);
    if (outputTypeError) {
        const bool missingPath = outputTypeError == std::errc::no_such_file_or_directory;
        if (!missingPath) {
            result.reason = "status output path inaccessible: " + config.statusOutputPath.generic_string();
            return result;
        }
        outputTypeError.clear();
    }
    if (outputIsDirectory) {
        result.reason = "status output path is a directory: " + config.statusOutputPath.generic_string();
        return result;
    }

    const int iterations = config.maxIterations < 0 ? 0 : config.maxIterations;
    if (iterations == 0) {
        result.ok = true;
        result.reason = "no iterations requested";
        return result;
    }

    for (int i = 0; i < iterations; i++) {
        const auto snapshot = companion.buildStatusSnapshot(config.statusConfig);
        const auto json = serializeCompanionStatusSnapshot(snapshot);
        const bool written = common::writeTextFileAtomically(config.statusOutputPath, json);
        if (!written) {
            result.reason = "failed to write status snapshot: " + config.statusOutputPath.generic_string();
            return result;
        }

        result.iterationsRun++;
        if (i + 1 < iterations && config.pollInterval.count() > 0) {
            std::this_thread::sleep_for(config.pollInterval);
        }
    }

    result.ok = true;
    result.reason = "ok";
    return result;
}

CompanionLiveAutosaveMonitorResult runCompanionLiveAutosaveMonitor(const CompanionLiveAutosaveMonitorConfig& config)
{
    CompanionLiveAutosaveMonitorResult result;
    result.archiveSessionDirectory = config.archiveRoot / config.sessionId;
    result.statusOutputPath = config.statusOutputPath;

    if (config.archiveRoot.empty()) {
        result.reason = "archive root not configured";
        return result;
    }
    if (config.sessionId.empty()) {
        result.reason = "session id not configured";
        return result;
    }

    const bool autoDiscoverSaveRoots = config.saveRoots.empty();
    const int maxIterations = config.maxIterations < 0 ? 0 : config.maxIterations;
    const bool runForever = maxIterations == 0;
    if (runForever && config.pollInterval.count() <= 0) {
        result.reason = "continuous monitor requires a positive poll interval";
        writeLiveAutosaveMonitorStatus(result, "failed");
        return result;
    }

    int iteration = 0;
    bool sawExistingRoot = false;
    std::string lastWaitingReason = "waiting for Stellaris save root";

    while (runForever || iteration < maxIterations) {
        std::size_t candidateRootCount = 0;
        const auto roots = resolveLiveAutosaveMonitorRoots(config.saveRoots, candidateRootCount);
        result.candidateRootCount = candidateRootCount;

        StellarisProcessStatus processStatus;
        if (config.useDetectedStellarisState) {
            const StellarisProcessDetector detector;
            processStatus = detector.detectFromSystem();
        } else {
            processStatus.detectionAvailable = true;
            processStatus.running = config.stellarisRunningOverride;
            processStatus.reason =
                config.stellarisRunningOverride ? "explicit Stellaris running override" : "explicit Stellaris not running override";
        }
        result.lastStellarisRunning = processStatus.running;

        const bool shouldSweep =
            iteration == 0 ||
            config.captureWhenStellarisNotRunning ||
            !processStatus.detectionAvailable ||
            processStatus.running;

        if (shouldSweep) {
            const AutosaveArchiver archiver;
            std::size_t existingRootsThisIteration = 0;
            for (const auto& root : roots) {
                if (!directoryExists(root)) {
                    continue;
                }

                ++existingRootsThisIteration;
                sawExistingRoot = true;
                const auto archiveResult = archiver.archiveLiveSaves(
                    root,
                    config.archiveRoot,
                    config.sessionId,
                    config.stabilityDelayMs);
                result.copiedCount += archiveResult.copiedCount;
                result.skippedCount += archiveResult.skippedCount;
            }
            if (existingRootsThisIteration > 1) {
                archiver.writeLiveArchiveManifest(
                    config.archiveRoot,
                    config.sessionId,
                    std::filesystem::path("multiple_stellaris_save_roots"),
                    true);
            }
            result.existingRootCount = existingRootsThisIteration;
            if (existingRootsThisIteration == 0) {
                lastWaitingReason = autoDiscoverSaveRoots
                    ? "waiting for discovered Stellaris save root"
                    : "configured Stellaris save root missing";
            } else {
                lastWaitingReason = "running";
            }
        } else {
            lastWaitingReason = "Stellaris is not running; live autosave sweep deferred";
        }

        ++iteration;
        result.iterationsRun = iteration;
        result.reason = lastWaitingReason;
        if (!writeLiveAutosaveMonitorStatus(result, "running")) {
            result.ok = false;
            result.reason = "failed to write live autosave monitor status";
            return result;
        }

        if ((runForever || iteration < maxIterations) && config.pollInterval.count() > 0) {
            std::this_thread::sleep_for(config.pollInterval);
        }
    }

    if (!sawExistingRoot && !autoDiscoverSaveRoots) {
        result.ok = false;
        result.reason = lastWaitingReason;
        writeLiveAutosaveMonitorStatus(result, "failed");
        return result;
    }

    result.ok = true;
    result.reason = sawExistingRoot ? "ok" : lastWaitingReason;
    writeLiveAutosaveMonitorStatus(result, "completed");
    return result;
}

} // namespace strategic_nexus
