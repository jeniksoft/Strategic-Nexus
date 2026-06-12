// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StrategicNexusCompanion.h"

#include "AutosaveArchiver.h"
#include "CampaignLibraryPlanner.h"
#include "common/FileUtil.h"
#include "common/JsonExtract.h"
#include "generated_overlay/ManifestVerifier.h"
#include "generated_overlay/MpOverlayPackage.h"
#include "LocalLlmModelManager.h"
#include "SncFriendPackage.h"
#include "StellarisProcessDetector.h"
#include "StellarisSavePathResolver.h"

#include <filesystem>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <ctime>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace strategic_nexus {

std::string buildCampaignLibraryPinCommandTemplate(const std::filesystem::path& pinPath);

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

std::string joinStrings(const std::vector<std::string>& values, const std::string_view delimiter)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            output << delimiter;
        }
        output << values[index];
    }
    return output.str();
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

std::size_t countBootstrapCampaignSignals(const std::string& manifestJson)
{
    const std::regex bootstrapSeedPattern("\"bootstrap_rotation_seed_id\"\\s*:");
    return static_cast<std::size_t>(std::distance(
        std::sregex_iterator(manifestJson.begin(), manifestJson.end(), bootstrapSeedPattern),
        std::sregex_iterator()));
}

void populateGeneratedOverlayManifestSignals(
    CompanionSubsystemStatus& status,
    const std::filesystem::path& overlayDirectory)
{
    status.reactivePolicyPackCapability = "unknown";
    status.eventFamilies.clear();
    status.sourceQualities.clear();
    status.bootstrapCampaignCount = 0;

    if (overlayDirectory.empty()) {
        return;
    }

    std::string manifestJson;
    if (!common::tryReadTextFile(
            overlayDirectory / "strategic_nexus_generated_manifest.json",
            manifestJson)) {
        return;
    }

    if (const auto capability = common::extractJsonString(manifestJson, "reactive_policy_pack_capability");
        capability.has_value() && !capability->empty()) {
        status.reactivePolicyPackCapability = *capability;
    }
    status.eventFamilies = common::extractJsonStringArray(manifestJson, "event_families");
    status.sourceQualities = common::extractJsonStringArray(manifestJson, "source_qualities");
    status.bootstrapCampaignCount = countBootstrapCampaignSignals(manifestJson);
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

bool hasDegradedMpHandoffContinuity(const CompanionStatusSnapshot& snapshot)
{
    return snapshot.mpOverlayPackage.state == "ready" &&
           snapshot.mpOverlayPackage.handoffStatus == "degraded_previous_host_unavailable";
}

bool memoryRecoveryNeedsAttention(const CompanionStatusSnapshot& snapshot)
{
    return snapshot.postPlayPipeline.memoryRecovery.warningVisible ||
           snapshot.postPlayPipeline.memoryRecovery.state == "degraded" ||
           snapshot.postPlayPipeline.memoryRecovery.state == "needs_attention";
}

bool archiveNeedsAttention(const CompanionStatusSnapshot& snapshot)
{
    return snapshot.archive.state == "needs_setup" || snapshot.archive.state == "needs_attention";
}

std::filesystem::path archiveAttentionPath(const CompanionStatusSnapshot& snapshot)
{
    if (snapshot.archive.state == "needs_setup" && !snapshot.saveDiscovery.path.empty()) {
        return snapshot.saveDiscovery.path;
    }
    return snapshot.archive.path;
}

bool tryParseStatusTextBoolField(const std::string& statusText, const std::string& key, bool& value)
{
    const auto rawValue = trimWhitespace(readStatusTextField(statusText, key));
    if (rawValue == "true") {
        value = true;
        return true;
    }
    if (rawValue == "false") {
        value = false;
        return true;
    }
    return false;
}

std::string pathString(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::string buildLocalLlmCatalogSummary(const std::vector<LocalLlmCatalogEntry>& catalog)
{
    std::ostringstream output;
    output << "podporovane modely (" << catalog.size() << "): ";
    if (catalog.empty()) {
        output << "zadne";
        return output.str();
    }

    for (std::size_t index = 0; index < catalog.size(); ++index) {
        if (index > 0) {
            output << "; ";
        }
        const auto& entry = catalog[index];
        output << entry.displayName;
        if (!entry.modelId.empty()) {
            output << " (" << entry.modelId << ")";
        }
        if (!entry.status.empty()) {
            output << " [" << entry.status << "]";
        }
    }

    return output.str();
}

std::filesystem::path buildDefaultSncStartupShortcutPath()
{
    const char* appData = std::getenv("APPDATA");
    if (appData == nullptr || *appData == '\0') {
        return {};
    }

    return std::filesystem::path(appData) / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Startup" /
           "Strategic Nexus Companion.lnk";
}

std::string buildInstallSncStartupShortcutCommandHint()
{
    return R"(cmd /c tools\install_snc_tray_startup_shortcut.cmd)";
}

std::string buildRemoveSncStartupShortcutCommandHint()
{
    return R"(cmd /c tools\remove_snc_tray_startup_shortcut.cmd)";
}

std::string buildPrepareSncSupportReportCommandHint()
{
    return R"(powershell -NoProfile -ExecutionPolicy Bypass -File tools\prepare_snc_support_report.ps1)";
}

CompanionLifecycleStatus buildLifecycleStatus(const CompanionStatusConfig& config)
{
    CompanionLifecycleStatus lifecycle;
    lifecycle.startWithWindowsEnableCommandHint = buildInstallSncStartupShortcutCommandHint();
    lifecycle.startWithWindowsDisableCommandHint = buildRemoveSncStartupShortcutCommandHint();
    lifecycle.startWithWindowsShortcutPath =
        config.startWithWindowsShortcutPath.empty() ? buildDefaultSncStartupShortcutPath()
                                                    : config.startWithWindowsShortcutPath;

    if (config.useConfiguredStartWithWindowsState) {
        lifecycle.startWithWindowsEnabled = config.startWithWindowsEnabled;
        lifecycle.startWithWindowsSource = "config_override";
        lifecycle.startWithWindowsShortcutState =
            lifecycle.startWithWindowsEnabled ? "override_enabled" : "override_disabled";
        lifecycle.startWithWindowsCommandHint =
            lifecycle.startWithWindowsEnabled ? lifecycle.startWithWindowsDisableCommandHint
                                              : lifecycle.startWithWindowsEnableCommandHint;
        lifecycle.startWithWindowsCommandHintSource =
            lifecycle.startWithWindowsEnabled ? "startup_shortcut_remove_command"
                                              : "startup_shortcut_install_command";
        return lifecycle;
    }

    if (lifecycle.startWithWindowsShortcutPath.empty()) {
        lifecycle.startWithWindowsEnabled = false;
        lifecycle.startWithWindowsShortcutState = "shortcut_path_unavailable";
        lifecycle.startWithWindowsCommandHintSource = "startup_shortcut_unavailable";
        return lifecycle;
    }

    std::error_code error;
    const bool shortcutExists = std::filesystem::exists(lifecycle.startWithWindowsShortcutPath, error);
    if (error) {
        lifecycle.startWithWindowsEnabled = false;
        lifecycle.startWithWindowsShortcutState = "shortcut_probe_failed";
        lifecycle.startWithWindowsCommandHint = lifecycle.startWithWindowsEnableCommandHint;
        lifecycle.startWithWindowsCommandHintSource = "startup_shortcut_install_command";
        return lifecycle;
    }

    if (!shortcutExists) {
        lifecycle.startWithWindowsEnabled = false;
        lifecycle.startWithWindowsShortcutState = "shortcut_not_installed";
        lifecycle.startWithWindowsCommandHint = lifecycle.startWithWindowsEnableCommandHint;
        lifecycle.startWithWindowsCommandHintSource = "startup_shortcut_install_command";
        return lifecycle;
    }

    if (!std::filesystem::is_regular_file(lifecycle.startWithWindowsShortcutPath, error) || error) {
        lifecycle.startWithWindowsEnabled = false;
        lifecycle.startWithWindowsShortcutState = "shortcut_path_not_file";
        lifecycle.startWithWindowsCommandHint = lifecycle.startWithWindowsEnableCommandHint;
        lifecycle.startWithWindowsCommandHintSource = "startup_shortcut_install_command";
        return lifecycle;
    }

    lifecycle.startWithWindowsEnabled = true;
    lifecycle.startWithWindowsShortcutState = "shortcut_installed";
    lifecycle.startWithWindowsCommandHint = lifecycle.startWithWindowsDisableCommandHint;
    lifecycle.startWithWindowsCommandHintSource = "startup_shortcut_remove_command";
    return lifecycle;
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

std::string buildOpenSncSupportReportCommandHint(const std::filesystem::path& previewPath)
{
    if (previewPath.empty()) {
        return std::string();
    }
    return "cmd /c start \"\" " + quoteCliPathArg(previewPath);
}

std::optional<bool> extractJsonBool(const std::string& json, const char* key);
std::optional<std::size_t> extractJsonSize(const std::string& json, const char* key);

CompanionSupportReportStatus buildSupportReportStatus(const CompanionStatusConfig& config)
{
    CompanionSupportReportStatus status;
    status.previewPath = config.supportReportPreviewPath;
    status.dataCategories = {
        "status_center_state_and_reason",
        "next_action_guidance",
        "post_play_pipeline_artifact_refs",
        "generated_overlay_publish_gate_state",
        "mp_overlay_package_metadata",
        "local_llm_model_state",
        "startup_lifecycle_state"
    };
    status.prepareCommandHint = buildPrepareSncSupportReportCommandHint();

    if (status.previewPath.empty()) {
        status.state = "preview_path_unavailable";
        status.reason = "support report preview path unavailable";
        return status;
    }

    std::error_code error;
    const bool previewExists = std::filesystem::exists(status.previewPath, error);
    if (error) {
        status.state = "preview_probe_failed";
        status.reason = "support report preview probe failed";
        return status;
    }

    if (!previewExists) {
        status.state = "not_prepared";
        status.reason = "prepare the local support report preview before manual review or sending it";
        return status;
    }

    if (!std::filesystem::is_regular_file(status.previewPath, error) || error) {
        status.state = "preview_path_not_file";
        status.reason = "support report preview path is not a file";
        return status;
    }

    status.state = "ready_for_review";
    status.reason = "local support report preview is prepared; explicit owner approval is required before sending";
    status.openCommandHint = buildOpenSncSupportReportCommandHint(status.previewPath);
    return status;
}

bool supportReportNeedsAttention(const CompanionSupportReportStatus& status)
{
    return status.state == "not_prepared" ||
           status.state == "preview_probe_failed" ||
           status.state == "preview_path_unavailable" ||
           status.state == "preview_path_not_file";
}

std::string defaultCrashRecoveryReasonForState(const std::string& state)
{
    if (state == "no_recent_unexpected_crash") {
        return "no crash recovery record present";
    }
    if (state == "recent_unexpected_crash_recovered") {
        return "unexpected crash recorded; SNC recovered within restart budget";
    }
    if (state == "waiting_for_bounded_restart") {
        return "unexpected crash recorded; bounded restart backoff is active";
    }
    if (state == "restart_budget_exhausted") {
        return "unexpected crashes exhausted restart budget; manual start required";
    }
    if (state == "disabled_until_manual_start") {
        return "auto-restart disabled until manual SNC start";
    }
    if (state == "explicit_exit_recorded") {
        return "latest SNC stop was explicit exit; auto-restart stayed disabled";
    }
    if (state == "os_shutdown_recorded") {
        return "latest SNC stop matched Windows shutdown or logoff";
    }
    if (state == "state_unavailable") {
        return "crash recovery state unavailable";
    }
    return "crash recovery state loaded";
}

CompanionCrashRecoveryStatus buildCrashRecoveryStatus(const CompanionStatusConfig& config)
{
    CompanionCrashRecoveryStatus status;
    status.statePath = config.crashRecoveryStatePath;

    if (status.statePath.empty()) {
        status.state = "state_unavailable";
        status.reason = "crash recovery state path unavailable";
        status.warningVisible = true;
        return status;
    }

    std::error_code error;
    const bool stateExists = std::filesystem::exists(status.statePath, error);
    if (error) {
        status.state = "state_unavailable";
        status.reason = "crash recovery state probe failed";
        status.warningVisible = true;
        return status;
    }
    if (!stateExists) {
        status.state = "no_recent_unexpected_crash";
        status.reason = "no crash recovery record present";
        return status;
    }
    if (!std::filesystem::is_regular_file(status.statePath, error) || error) {
        status.state = "needs_attention";
        status.reason = "crash recovery state path is not a file";
        status.warningVisible = true;
        return status;
    }

    std::string json;
    if (!common::tryReadTextFile(status.statePath, json)) {
        status.state = "needs_attention";
        status.reason = "crash recovery state unreadable";
        status.warningVisible = true;
        return status;
    }

    const auto schemaVersion = extractJsonSize(json, "schema_version");
    if (!schemaVersion.has_value() || *schemaVersion != 1) {
        status.state = "needs_attention";
        status.reason = "crash recovery state schema unsupported";
        status.warningVisible = true;
        return status;
    }

    const auto stateValue = common::extractJsonString(json, "state");
    if (!stateValue.has_value() || stateValue->empty()) {
        status.state = "needs_attention";
        status.reason = "crash recovery state malformed";
        status.warningVisible = true;
        return status;
    }

    status.state = *stateValue;
    status.reason =
        common::extractJsonString(json, "reason").value_or(defaultCrashRecoveryReasonForState(status.state));
    status.lastCrashAtLocal = common::extractJsonString(json, "last_crash_at_local").value_or("");
    status.lastRecoveryAction = common::extractJsonString(json, "last_recovery_action").value_or("");
    status.lastOperation = common::extractJsonString(json, "last_operation").value_or("");
    status.appVersion = common::extractJsonString(json, "app_version").value_or("");
    status.recentUnexpectedRestartCount = extractJsonSize(json, "recent_unexpected_restart_count").value_or(0);
    status.restartWindowMinutes = extractJsonSize(json, "restart_window_minutes").value_or(10);
    status.nextBackoffSeconds = extractJsonSize(json, "next_backoff_seconds").value_or(0);
    status.restartBudgetExhausted = extractJsonBool(json, "restart_budget_exhausted").value_or(false);
    status.warningVisible = extractJsonBool(json, "warning_visible").value_or(false);
    status.supportReportRecommended = extractJsonBool(json, "support_report_recommended").value_or(false);

    if (status.state == "restart_budget_exhausted" || status.state == "disabled_until_manual_start") {
        status.restartBudgetExhausted = true;
    }
    if (status.restartBudgetExhausted) {
        status.warningVisible = true;
        status.supportReportRecommended = true;
    }
    if (status.state == "needs_attention" || status.state == "state_unavailable") {
        status.warningVisible = true;
    }

    return status;
}

bool crashRecoveryNeedsAttention(const CompanionCrashRecoveryStatus& status)
{
    return status.warningVisible || status.restartBudgetExhausted || status.state == "needs_attention" ||
           status.state == "state_unavailable";
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

std::string parsePostPlayPlayerCountryId(const std::string& json)
{
    const auto entriesBody = extractArrayBody(json, "entries");
    if (!entriesBody.has_value()) {
        return {};
    }

    for (const auto& object : extractObjectBodies(*entriesBody)) {
        const auto playerCountryId = common::extractJsonString(object, "player_country_id").value_or("");
        if (!playerCountryId.empty()) {
            return playerCountryId;
        }
    }

    return {};
}

struct MemoryRecoveryEntry {
    std::string id;
    std::string campaignKey;
    std::string relativePath;
    std::string saveName;
    std::string saveDate;
    std::string sourceKind;
    std::string analysisState;
    std::filesystem::path anchorPath;
    std::size_t compatibleArchivedEvidenceCount = 0;
    std::size_t laterArchivedEvidenceCount = 0;
};

bool isBetterMemoryRecoveryEntry(
    const MemoryRecoveryEntry& candidate,
    const std::optional<MemoryRecoveryEntry>& best)
{
    if (!best.has_value()) {
        return true;
    }

    if (candidate.saveDate != best->saveDate) {
        if (candidate.saveDate.empty()) {
            return false;
        }
        if (best->saveDate.empty()) {
            return true;
        }
        return candidate.saveDate > best->saveDate;
    }

    if (candidate.compatibleArchivedEvidenceCount != best->compatibleArchivedEvidenceCount) {
        return candidate.compatibleArchivedEvidenceCount > best->compatibleArchivedEvidenceCount;
    }

    if (candidate.laterArchivedEvidenceCount != best->laterArchivedEvidenceCount) {
        return candidate.laterArchivedEvidenceCount < best->laterArchivedEvidenceCount;
    }

    if (candidate.analysisState != best->analysisState) {
        if (candidate.analysisState == "ready" && best->analysisState != "ready") {
            return true;
        }
        if (candidate.analysisState != "ready" && best->analysisState == "ready") {
            return false;
        }
        if (candidate.analysisState == "ready_conservative" && best->analysisState == "ambiguous") {
            return true;
        }
        if (candidate.analysisState == "ambiguous" && best->analysisState == "ready_conservative") {
            return false;
        }
    }

    return candidate.anchorPath.generic_string() < best->anchorPath.generic_string();
}

CompanionMemoryRecoveryStatus buildMemoryRecoveryStatus(const std::string& entryPointAnalysisJson)
{
    CompanionMemoryRecoveryStatus recovery;
    if (entryPointAnalysisJson.empty()) {
        recovery.reason = "entry point analysis unavailable";
        return recovery;
    }

    const auto readiness = common::extractJsonString(entryPointAnalysisJson, "readiness").value_or("");
    const bool branchAmbiguityDetected = extractJsonBool(entryPointAnalysisJson, "branch_ambiguity_detected").value_or(false);
    const auto entriesBody = extractArrayBody(entryPointAnalysisJson, "entry_points");
    if (!entriesBody.has_value()) {
        recovery.reason = readiness.empty() ? "entry point analysis missing entry points"
                                            : "entry point analysis " + readiness;
        return recovery;
    }

    std::optional<MemoryRecoveryEntry> bestEntry;
    for (const auto& object : extractObjectBodies(*entriesBody)) {
        MemoryRecoveryEntry entry;
        entry.id = common::extractJsonString(object, "id").value_or("");
        entry.campaignKey = common::extractJsonString(object, "campaign_key").value_or("");
        entry.relativePath = common::extractJsonString(object, "relative_path").value_or("");
        entry.saveName = common::extractJsonString(object, "save_name").value_or("");
        entry.saveDate = common::extractJsonString(object, "save_date").value_or("");
        entry.sourceKind = common::extractJsonString(object, "source_kind").value_or("");
        entry.analysisState = common::extractJsonString(object, "analysis_state").value_or("");
        const auto sourceRoot = common::extractJsonString(object, "source_root").value_or("");
        if (!sourceRoot.empty() && !entry.relativePath.empty()) {
            entry.anchorPath = std::filesystem::path(sourceRoot) / entry.relativePath;
        } else if (!entry.relativePath.empty()) {
            entry.anchorPath = entry.relativePath;
        }
        entry.compatibleArchivedEvidenceCount = extractJsonSize(object, "compatible_archived_evidence_count").value_or(0);
        entry.laterArchivedEvidenceCount = extractJsonSize(object, "later_archived_evidence_count").value_or(0);

        if (entry.id.empty() && entry.saveName.empty() && entry.saveDate.empty() && entry.relativePath.empty() &&
            entry.compatibleArchivedEvidenceCount == 0 && entry.laterArchivedEvidenceCount == 0) {
            continue;
        }

        if (!bestEntry.has_value() || isBetterMemoryRecoveryEntry(entry, bestEntry)) {
            bestEntry = entry;
        }
    }

    if (!bestEntry.has_value()) {
        recovery.reason = "no loadable save entry points found";
        return recovery;
    }

    recovery.anchorEntryPointId = bestEntry->id;
    recovery.anchorCampaignKey = bestEntry->campaignKey;
    recovery.anchorPath = bestEntry->anchorPath;
    recovery.anchorSaveName = bestEntry->saveName;
    recovery.anchorSaveDate = bestEntry->saveDate;
    recovery.anchorSourceKind = bestEntry->sourceKind;
    recovery.compatibleArchivedEvidenceCount = bestEntry->compatibleArchivedEvidenceCount;
    recovery.laterArchivedEvidenceCount = bestEntry->laterArchivedEvidenceCount;

    if (readiness == "blocked" || readiness == "needs_attention") {
        recovery.state = "needs_attention";
        recovery.reason = readiness == "blocked" ? "entry point analysis blocked"
                                                 : "entry point analysis needs attention";
        recovery.confidence = "low";
        recovery.warningVisible = true;
        return recovery;
    }

    if (bestEntry->compatibleArchivedEvidenceCount == 0) {
        recovery.state = "needs_attention";
        recovery.reason = "no compatible archived evidence for latest loadable save";
        recovery.confidence = "low";
        recovery.warningVisible = true;
        return recovery;
    }

    const bool conservative =
        branchAmbiguityDetected || readiness == "ambiguous" || bestEntry->laterArchivedEvidenceCount > 0 ||
        bestEntry->analysisState == "ready_conservative" || bestEntry->analysisState == "ambiguous";
    if (conservative) {
        recovery.state = "degraded";
        recovery.reason = branchAmbiguityDetected ? "branch ambiguity requires conservative recovery anchor"
                                                  : "later archived evidence excluded from recovery anchor";
        recovery.confidence = "reduced";
        recovery.warningVisible = true;
        return recovery;
    }

    recovery.state = "ready";
    recovery.reason = "latest loadable save selected as recovery anchor";
    recovery.confidence = "high";
    recovery.warningVisible = false;
    return recovery;
}

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.rfind(prefix, 0) == 0;
}

bool containsExactString(const std::vector<std::string>& values, const std::string& needle)
{
    return std::find(values.begin(), values.end(), needle) != values.end();
}

bool isPublishedMonthlyReactiveOwnerTestReady(const CompanionStatusSnapshot& snapshot)
{
    return snapshot.generatedOverlayPublishGate.state == "published" &&
           snapshot.generatedOverlay.reactivePolicyPackCapability == "event_family_dispatch" &&
           containsExactString(snapshot.generatedOverlay.eventFamilies, "monthly_strategy_tick") &&
           snapshot.gameplayAcceptance.state == "ready";
}

std::filesystem::path buildCompanionOwnerTestPlaybookPath(const CompanionStatusSnapshot& snapshot)
{
    if (!isPublishedMonthlyReactiveOwnerTestReady(snapshot)) {
        return {};
    }
    return std::filesystem::path("docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md");
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

bool isReadyLikeReadiness(const std::string& readiness)
{
    return startsWith(readiness, "ready");
}

bool isStagedOverlayReadyLike(const std::string& readiness)
{
    return readiness == "staged_verified";
}

std::filesystem::path postPlayPipelineFocusPath(const CompanionPostPlayPipelineStatus& status)
{
    const auto hasPrefix = [](const std::string& value, const std::string& prefix) {
        return value.rfind(prefix, 0) == 0;
    };

    if (status.branchAmbiguityDetected) {
        return status.entryPointAnalysisPath;
    }
    if (hasPrefix(status.reason, "generated overlay staging ")) {
        return status.generatedOverlayStagingStatusPath;
    }
    if (hasPrefix(status.reason, "dsl draft ")) {
        return status.dslDraftAuditPath;
    }
    if (hasPrefix(status.reason, "candidate decision package ")) {
        return status.candidateDecisionPackagePath;
    }
    if (hasPrefix(status.reason, "decision input package ")) {
        return status.decisionInputPackagePath;
    }
    if (hasPrefix(status.reason, "post-play package ")) {
        return status.postPlayPackagePath;
    }
    if (!status.generatedOverlayStagingReadiness.empty() || !status.generatedOverlayStagingReason.empty()) {
        return status.generatedOverlayStagingStatusPath;
    }
    if (!status.dslDraftReadiness.empty() || !status.dslDraftReason.empty()) {
        return status.dslDraftAuditPath;
    }
    if (!status.candidateDecisionPackageReadiness.empty() || !status.candidateDecisionPackageReason.empty()) {
        return status.candidateDecisionPackagePath;
    }
    if (!status.decisionInputPackageReadiness.empty() || !status.decisionInputPackageReason.empty()) {
        return status.decisionInputPackagePath;
    }
    if (!status.postPlayPackageReadiness.empty() || !status.postPlayPackageReason.empty()) {
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
        populateGeneratedOverlayManifestSignals(status, overlayDirectory);
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
    status.previousHostAvailableKnown =
        tryParseStatusTextBoolField(verification.statusText, "previous_host_available", status.previousHostAvailable);
    status.readiness = verification.readiness;
    status.hostReadiness = trimWhitespace(readStatusTextField(verification.statusText, "host_readiness"));
    status.clientReadinessGate = trimWhitespace(readStatusTextField(verification.statusText, "client_readiness_gate"));
    status.hostNextStep = trimWhitespace(readStatusTextField(verification.statusText, "host_next_step"));
    status.clientNextStep = trimWhitespace(readStatusTextField(verification.statusText, "client_next_step"));
    if (status.handoffStatus == "degraded_previous_host_unavailable") {
        status.handoffRecoveryHint =
            "nacti nejnovnejsi handoff balicek pokud je k dispozici; jinak pouzij starsi overeny archiv nebo klientsky save a nech confidence snizenou";
    }
    status.packageManifestHash = verification.packageManifestHash;
    status.provenanceState = verification.provenanceState;
    status.humanControlGuardState =
        verification.humanControlGuardState.empty() ? "unknown" : verification.humanControlGuardState;
    status.sourceQualities = verification.sourceQualities;
    status.bootstrapCampaignCount = verification.bootstrapCampaignCount;
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
        status.statusText += "human_control_guard: " + status.humanControlGuardState + "\n";
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
        status.packageZipState = "not_exported";
        status.packageZipReason = "mp overlay package zip not exported by current status source";
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

std::filesystem::path resolveMpOverlayPackageZipPath(
    const CompanionMpOverlayPackageStatus& status,
    const std::filesystem::path& configuredZipPath)
{
    if (!configuredZipPath.empty()) {
        return configuredZipPath;
    }
    if (status.path.empty()) {
        return std::filesystem::path();
    }
    return status.path.parent_path() / "snc_mp_overlay_package.zip";
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

void populateCampaignLibraryPlanStatus(
    CompanionPostPlayPipelineStatus& status,
    const CompanionStatusConfig& config)
{
    const auto inspectPlanDirectory = [&](const std::filesystem::path& sourcePath, const std::string& source) {
        if (sourcePath.empty()) {
            return false;
        }

        const auto planPath = sourcePath.parent_path() / "strategic_nexus_campaign_library_plan.json";
        std::error_code error;
        if (!std::filesystem::exists(planPath, error) || error) {
            return false;
        }
        if (!std::filesystem::is_regular_file(planPath, error) || error) {
            return false;
        }

        status.campaignLibraryPlanPath = planPath;
        status.campaignLibraryPlanPresent = true;
        status.campaignLibraryPlanSource = source;

        std::string json;
        if (!common::tryReadTextFile(planPath, json)) {
            status.campaignLibraryPlanReadiness = "needs_attention";
            status.campaignLibraryPlanReason = "campaign library plan unreadable";
            return false;
        }

        const auto schemaVersion = extractJsonSize(json, "schema_version").value_or(0);
        if (schemaVersion == 1) {
            status.campaignLibraryPlanReadiness = "ready";
            status.campaignLibraryPlanReason = "campaign library plan loaded";
        } else if (schemaVersion == 0) {
            status.campaignLibraryPlanReadiness = "degraded";
            status.campaignLibraryPlanReason =
                "campaign library plan loaded from legacy schema_version 0";
        } else {
            status.campaignLibraryPlanReadiness = "needs_attention";
            status.campaignLibraryPlanReason = "campaign library plan schema unsupported";
            return false;
        }

        const auto limitReached = extractJsonBool(json, "limit_reached");
        if (!limitReached.has_value()) {
            status.campaignLibraryPlanReadiness = "needs_attention";
            status.campaignLibraryPlanReason = "campaign library plan missing limit_reached";
            return false;
        }

        status.campaignLibraryLimitReached = *limitReached;
        status.campaignLibrarySkippedDueToLimitCount =
            extractJsonSize(json, "skipped_due_to_limit_count").value_or(0);

        const auto pinPath = planPath.parent_path() / "strategic_nexus_campaign_library_pins.json";
        status.campaignLibraryPinPath = pinPath;
        status.campaignLibraryPinCommandTemplate = buildCampaignLibraryPinCommandTemplate(pinPath);

        const auto pinSet = loadCampaignLibraryPins(pinPath);
        const bool legacyPinManifest =
            pinSet.present && pinSet.schemaSupported &&
            pinSet.reason == "pinned campaign exception manifest loaded from legacy schema_version 0";
        if (pinSet.present) {
            status.campaignLibraryPinPresent = true;
            status.campaignLibraryPinnedCount = pinSet.pinnedCount;
            status.campaignLibraryPinnedMissingLocalSaveCount =
                extractJsonSize(json, "pinned_missing_local_save_count").value_or(0);
            status.campaignLibraryPinState = pinSet.state;
            status.campaignLibraryPinReason = pinSet.reason;
            status.campaignLibraryPinNextStep = pinSet.nextStep;

            if (pinSet.schemaSupported && !legacyPinManifest && status.campaignLibraryPinnedMissingLocalSaveCount > 0) {
                status.campaignLibraryPinState = "degraded";
                status.campaignLibraryPinReason =
                    "pinned campaign exceptions are available, but some pinned campaigns are not locally playable";
                status.campaignLibraryPinNextStep =
                    "Use the pin toggle command to trim unreachable pinned campaigns or restore the missing local save root.";
            } else if (pinSet.schemaSupported && !legacyPinManifest && status.campaignLibraryLimitReached && status.campaignLibraryPinnedCount > 0) {
                status.campaignLibraryPinState = "degraded";
                status.campaignLibraryPinReason =
                    "pinned campaign exceptions are available, but the active library is still truncated by the configured limit";
                status.campaignLibraryPinNextStep =
                    "Raise the max-campaign cap or remove unused local campaigns before adding more pinned exceptions.";
            }
        }
        return true;
    };

    if (inspectPlanDirectory(config.postPlayPackagePath, "post_play_status_directory")) {
        return;
    }
    if (inspectPlanDirectory(config.postPlayGeneratedOverlayStagingStatusPath, "staging_status_directory")) {
        return;
    }
    inspectPlanDirectory(config.generatedOverlayPublishStatusPath, "publish_status_directory");
}

CompanionPostPlayPipelineStatus buildPostPlayPipelineStatus(
    const CompanionStatusConfig& config)
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
    std::string entryPointAnalysisJson;
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
        status.postPlayPackageCampaignIdentityStateSummary =
            common::extractJsonString(json, "campaign_identity_state_summary").value_or("");
        status.postPlayPackagePersonalityProfilePromptOutputNote =
            common::extractJsonString(json, "personality_profile_prompt_output_note").value_or("");
        status.playerCountryId = parsePostPlayPlayerCountryId(json);
        status.postPlayDecisionReadyEntryCount = extractJsonSize(json, "decision_ready_entry_count").value_or(0);
        parsePostPlayCampaignSummaries(json, status);
    } else if (!status.postPlayPackagePath.empty() && fileError != "missing") {
        status.state = "needs_attention";
        status.reason = "post-play package " + fileError;
        return status;
    }

    if (inspectJsonFile(status.entryPointAnalysisPath, entryPointAnalysisJson, fileError)) {
        entryPointAvailable = true;
        status.entryPointReadiness = common::extractJsonString(entryPointAnalysisJson, "readiness").value_or("");
        status.entryPointReason = common::extractJsonString(entryPointAnalysisJson, "reason").value_or("");
        status.entryPointCount = extractJsonSize(entryPointAnalysisJson, "entry_point_count").value_or(0);
        status.branchAmbiguityDetected =
            extractJsonBool(entryPointAnalysisJson, "branch_ambiguity_detected").value_or(false);
    } else if (!status.entryPointAnalysisPath.empty() && fileError != "missing") {
        status.state = "needs_attention";
        status.reason = "entry point analysis " + fileError;
        return status;
    }

    status.memoryRecovery = buildMemoryRecoveryStatus(entryPointAnalysisJson);

    populateCampaignLibraryPlanStatus(status, config);
    if (status.campaignLibraryPlanPresent && status.campaignLibraryPlanReadiness != "ready") {
        status.state = "needs_attention";
        status.reason = status.campaignLibraryPlanReason.empty()
            ? "campaign library plan needs attention"
            : status.campaignLibraryPlanReason;
        return status;
    }

    const bool postPlayReadyLike = postPlayAvailable && isReadyLikeReadiness(status.postPlayPackageReadiness);
    const bool decisionInputReadyLike =
        decisionInputAvailable && isReadyLikeReadiness(status.decisionInputPackageReadiness);
    const bool candidateReadyLike =
        candidateAvailable && isReadyLikeReadiness(status.candidateDecisionPackageReadiness);
    const bool dslDraftReadyLike = dslDraftAvailable && isReadyLikeReadiness(status.dslDraftReadiness);

    if (status.branchAmbiguityDetected) {
        status.state = "needs_attention";
        status.reason = "entry point analysis branch ambiguity detected";
        return status;
    }
    if (generatedOverlayStagingAvailable && dslDraftReadyLike && candidateReadyLike && decisionInputReadyLike &&
        postPlayReadyLike && isStagedOverlayReadyLike(status.generatedOverlayStagingReadiness)) {
        status.state = stateFromGeneratedOverlayStagingReadiness(status.generatedOverlayStagingReadiness);
        status.reason = reasonFromGeneratedOverlayStagingReadiness(status.generatedOverlayStagingReadiness);
        return status;
    }
    if (dslDraftAvailable && candidateReadyLike && decisionInputReadyLike && postPlayReadyLike) {
        status.state = stateFromReadiness(status.dslDraftReadiness);
        status.reason = reasonFromReadiness("dsl draft", status.dslDraftReadiness);
        return status;
    }
    if (candidateAvailable && decisionInputReadyLike && postPlayReadyLike) {
        status.state = stateFromReadiness(status.candidateDecisionPackageReadiness);
        status.reason = reasonFromReadiness("candidate decision package", status.candidateDecisionPackageReadiness);
        return status;
    }
    if (decisionInputAvailable && postPlayReadyLike) {
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

CompanionLocalLlmStatus buildLocalLlmStatus(const CompanionStatusConfig& config)
{
    const auto catalog = defaultLocalLlmCatalog();
    const auto state = loadLocalLlmModelState(config.localLlmModelStatePath);
    const LocalLlmHardwareProfile hardware;
    const auto readiness = evaluateLocalLlmReadiness(catalog, state, hardware);

    CompanionLocalLlmStatus status;
    status.state = readiness.state;
    status.reason = readiness.reason;
    status.selectedModelId = readiness.selectedModelId;
    status.selectedDisplayName = readiness.selectedDisplayName;
    status.runtime = readiness.runtime;
    status.catalogStatus = readiness.catalogStatus;
    status.supportedModelCatalogCount = catalog.size();
    status.supportedModelCatalogSummary = buildLocalLlmCatalogSummary(catalog);
    status.localPath = readiness.localPath;
    status.hardwareFit = readiness.hardwareFit;
    status.recommendedModelId = readiness.recommendedModelId;
    status.recommendedDisplayName = readiness.recommendedDisplayName;
    status.recommendedRuntime = readiness.recommendedRuntime;
    status.modelStatePath = config.localLlmModelStatePath;
    status.canRunInference = readiness.canRunInference;
    status.reducedMode = readiness.reducedMode;
    status.userActionRequired = readiness.userActionRequired;
    status.downloadAllowed = readiness.downloadAllowed;
    const std::string prepareModelId =
        !readiness.selectedModelId.empty() && readiness.state == "model_ready"
            ? readiness.selectedModelId
            : readiness.recommendedModelId;
    status.prepareCommandHint = buildLocalLlmPrepareCommandHint(
        prepareModelId,
        config.localLlmModelStatePath);
    status.installGuidance = strategic_nexus::buildLocalLlmInstallGuidance(readiness, status.prepareCommandHint);
    {
        std::ostringstream summary;
        if (status.selectedModelId.empty()) {
            summary << "Není vybraný žádný podporovaný lokální model";
        } else if (!status.selectedDisplayName.empty()) {
            summary << "Vybraný model: " << status.selectedDisplayName << " (" << status.selectedModelId << ")";
        } else {
            summary << "Vybraný model ID: " << status.selectedModelId;
        }
        summary << "; stav modelu: " << status.state;
        summary << "; redukovaný režim: " << (status.reducedMode ? "true" : "false");
        if (status.canRunInference) {
            summary << "; další krok: žádný";
        } else if (!status.installGuidance.empty()) {
            summary << "; další krok: " << status.installGuidance;
        } else if (!status.reason.empty()) {
            summary << "; další krok: " << status.reason;
        }
        status.summary = summary.str();
    }
    return status;
}

CompanionSubsystemStatus buildStatusCenterStatus(
    const CompanionSubsystemStatus& saveDiscovery,
    const CompanionSubsystemStatus& archive,
    const CompanionSubsystemStatus& generatedOverlay,
    const CompanionGeneratedOverlayPublishGateStatus& generatedOverlayPublishGate,
    const CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const CompanionPostPlayPipelineStatus& postPlayPipeline,
    const CompanionSubsystemStatus& gameplayAcceptance,
    const CompanionSupportReportStatus& supportReport,
    const CompanionCrashRecoveryStatus& crashRecovery,
    const CompanionLocalLlmStatus& localLlm,
    const CompanionFriendTrustStoreStatus& friendTrustStore)
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
    const bool memoryRecoveryNeedsAttention = postPlayPipeline.memoryRecovery.warningVisible;
    const bool gameplayAcceptanceNeedsAttention = gameplayAcceptance.state == "needs_attention";
    const bool friendTrustStoreNeedsAttention = friendTrustStore.state == "needs_attention";
    const bool recoveryNeedsAttention = crashRecoveryNeedsAttention(crashRecovery);
    const bool localLlmNeedsAttention =
        localLlm.state == "model_incompatible_with_hardware"
        || localLlm.state == "model_license_not_supported"
        || localLlm.state == "model_license_requires_user_action"
        || localLlm.state == "model_runtime_failed"
        || localLlm.state == "model_changed_revalidation_needed";

    if (archiveNeedsAttention || overlayNeedsAttention || publishGateNeedsAttention || mpNeedsAttention ||
        postPlayNeedsAttention || memoryRecoveryNeedsAttention || gameplayAcceptanceNeedsAttention ||
        friendTrustStoreNeedsAttention || recoveryNeedsAttention || localLlmNeedsAttention) {
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
        if (postPlayNeedsAttention && memoryRecoveryNeedsAttention) {
            status.reason = "post-play pipeline and memory recovery need attention";
            if (postPlayPipeline.branchAmbiguityDetected) {
                status.path = postPlayPipeline.entryPointAnalysisPath;
            } else {
                status.path = postPlayPipeline.memoryRecovery.anchorPath.empty()
                    ? postPlayPipelineFocusPath(postPlayPipeline)
                    : postPlayPipeline.memoryRecovery.anchorPath;
            }
            if (status.path.empty()) {
                status.path = postPlayPipeline.entryPointAnalysisPath;
            }
            return status;
        }
        if (postPlayNeedsAttention && recoveryNeedsAttention) {
            status.reason = "post-play pipeline and crash recovery need attention";
            status.path = postPlayPipelineFocusPath(postPlayPipeline);
            if (status.path.empty()) {
                status.path = crashRecovery.statePath;
            }
            return status;
        }
        if (postPlayNeedsAttention) {
            status.reason = "post-play pipeline needs attention";
            status.path = postPlayPipelineFocusPath(postPlayPipeline);
            return status;
        }
        if (memoryRecoveryNeedsAttention) {
            status.reason = "memory recovery needs attention";
            status.path = postPlayPipeline.memoryRecovery.anchorPath.empty()
                ? postPlayPipeline.entryPointAnalysisPath
                : postPlayPipeline.memoryRecovery.anchorPath;
            return status;
        }
        if (gameplayAcceptanceNeedsAttention) {
            status.reason = "gameplay acceptance needs attention";
            status.path = gameplayAcceptance.path;
            return status;
        }
        if (friendTrustStoreNeedsAttention) {
            status.reason = "friend trust store needs attention";
            status.path = friendTrustStore.path;
            return status;
        }
        if (recoveryNeedsAttention) {
            status.reason = "crash recovery needs attention";
            status.path = crashRecovery.statePath;
            return status;
        }
        if (localLlmNeedsAttention) {
            status.reason = "sprava lokalniho LLM modelu vyzaduje pozornost";
            status.path = localLlm.localPath;
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
    const bool memoryRecoveryStarting = postPlayPipeline.memoryRecovery.state == "needs_attention";
    const bool gameplayAcceptanceStarting = gameplayAcceptance.state == "starting";

    if (generatedOverlayPublishGate.state == "blocked") {
        status.state = "waiting";
        status.reason = generatedOverlayPublishGate.reason;
        status.path = generatedOverlayPublishGate.path;
        return status;
    }

    if (archiveStarting || overlayStarting || publishGateStarting || postPlayStarting || memoryRecoveryStarting ||
        gameplayAcceptanceStarting) {
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
        if (postPlayStarting && memoryRecoveryStarting) {
            status.reason = "waiting for post-play pipeline and memory recovery";
            status.path = postPlayPipeline.memoryRecovery.anchorPath.empty()
                ? postPlayPipeline.entryPointAnalysisPath
                : postPlayPipeline.memoryRecovery.anchorPath;
            return status;
        }
        if (postPlayStarting) {
            status.reason = "waiting for post-play pipeline";
            status.path = postPlayPipelineFocusPath(postPlayPipeline);
            return status;
        }
        if (memoryRecoveryStarting) {
            status.reason = "waiting for memory recovery";
            status.path = postPlayPipeline.memoryRecovery.anchorPath.empty()
                ? postPlayPipeline.entryPointAnalysisPath
                : postPlayPipeline.memoryRecovery.anchorPath;
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

CompanionFriendTrustStoreStatus buildFriendTrustStoreStatus(const CompanionStatusConfig& config)
{
    CompanionFriendTrustStoreStatus status;
    status.path = config.friendTrustStorePath;
    status.mpSyncTransportAdapterPath = config.mpOverlayPackageDirectory;
    status.pairingCommandTemplate =
        "Strategic Nexus.exe --create-snc-friend-request "
        "\"dist/private_reports/snc_friend_request.snc-friend-request\" "
        "<local_node_id> \"<local_display_name>\" "
        "<signing_public_key> <encryption_public_key> <fingerprint> "
        "<created_at_utc> [expires_at_utc]\n"
        "Strategic Nexus.exe --create-snc-friend-acceptance "
        "\"<friend_request_path>\" "
        "\"dist/private_reports/snc_friend_acceptance.snc-friend-acceptance\" "
        "<local_node_id> \"<local_display_name>\" "
        "<signing_public_key> <encryption_public_key> <fingerprint> "
        "<created_at_utc>\n"
        "Strategic Nexus.exe --import-snc-friend-acceptance "
        "\"<original_friend_request_path>\" "
        "\"<friend_acceptance_path>\" "
        "\"dist/private_reports/snc_friend_trust_store.json\" "
        "<accepted_at_utc> [local_alias]";
    status.controlsCommandTemplate =
        "Strategic Nexus.exe --update-snc-friend-trust-store-entry "
        "\"dist/private_reports/snc_friend_trust_store.json\" "
        "<friend_node_id> <trusted|revoked|blocked> "
        "<auto_sync_enabled:true|false> <updated_at_utc> [local_alias]";
    status.controlsState = "not_configured";
    status.controlsReason =
        "friend trust store update controls are available once the trust store exists";
    status.controlsNextStep =
        "Import a friend acceptance first, then use the update command to revoke, block, or disable auto-sync for a trusted friend.";
    status.mpSyncEnvelopeCommandTemplate =
        "Strategic Nexus.exe --create-snc-friend-mp-sync-envelope "
        "\"dist/private_reports/snc_friend_mp_sync_envelope.json\" "
        "<sender_node_id> \"<sender_display_name>\" "
        "<sender_signing_public_key> <sender_encryption_public_key> <sender_fingerprint> "
        "<recipient_node_id> \"<recipient_display_name>\" "
        "<recipient_signing_public_key> <recipient_encryption_public_key> <recipient_fingerprint> "
        "<campaign_id> <overlay_version> <package_manifest_hash> <package_zip_hash> "
        "<encrypted_payload_hash> <encrypted_payload_bytes> ed25519 x25519-xsalsa20-poly1305 "
        "<signature> <created_at_utc>\n"
        "Strategic Nexus.exe --verify-snc-friend-mp-sync-envelope "
        "\"<friend_mp_sync_envelope_path>\" [stellaris_running:true|false]";
    status.mpSyncInboxPlanCommandTemplate =
        "Strategic Nexus.exe --plan-snc-friend-mp-sync-inbox "
        "\"<friend_mp_sync_envelope_path>\" "
        "\"<encrypted_payload_path>\" "
        "[stellaris_running:true|false] [friend_auto_sync_enabled:true|false]";
    status.mpSyncOutboxPlanCommandTemplate =
        "Strategic Nexus.exe --plan-snc-friend-mp-sync-outbox "
        "\"<friend_mp_sync_envelope_path>\" "
        "\"<encrypted_payload_path>\" "
        "[stellaris_running:true|false] [friend_auto_sync_enabled:true|false]";
    status.mpSyncInboxPlanState = "disabled_not_implemented";
    status.mpSyncInboxPlanReason =
        "signed/encrypted friend MP sync transport adapter is not implemented; automatic download and package staging disabled";
    status.mpSyncInboxAutomaticDownloadEnabled = false;
    status.mpSyncInboxPackageStagingAllowed = false;
    if (status.path.empty()) {
        status.reason = "friend trust store path not configured; automatic friend sync disabled";
        return status;
    }

    std::string json;
    std::error_code error;
    if (!std::filesystem::exists(status.path, error) || error) {
        status.reason = "friend trust store not present; automatic friend sync disabled";
        return status;
    }
    if (!std::filesystem::is_regular_file(status.path, error) || error) {
        status.state = "needs_attention";
        status.reason = "friend trust store path is not a file";
        status.controlsState = "needs_attention";
        status.controlsReason = "friend trust store controls need a valid trust store file";
        status.controlsNextStep =
            "Create or point SNC at a valid friend trust store before using the update command.";
        return status;
    }
    if (!common::tryReadTextFile(status.path, json)) {
        status.state = "needs_attention";
        status.reason = "friend trust store path is not readable";
        status.controlsState = "needs_attention";
        status.controlsReason = "friend trust store controls need a readable trust store file";
        status.controlsNextStep =
            "Fix read access to the trust store, then use the update command to change revoke/block/auto-sync state.";
        return status;
    }

    const auto store = parseSncFriendTrustStoreJson(json);
    if (!store.ok) {
        status.state = "needs_attention";
        status.reason = store.reason.empty()
            ? "friend trust store failed validation"
            : store.reason;
        status.controlsState = "needs_attention";
        status.controlsReason = "friend trust store controls are blocked until the trust store validates";
        status.controlsNextStep =
            "Repair or recreate the trust store before relying on revoke/block/auto-sync controls.";
        return status;
    }

    status.sourceSchemaVersion = store.sourceSchemaVersion;
    status.schemaCompatibilityState = store.schemaCompatibilityState;
    status.schemaCompatibilityNote = store.schemaCompatibilityNote;
    status.schemaCompatibilityKnown = true;
    for (const auto& friendEntry : store.friends) {
        if (friendEntry.trustState == "trusted") {
            ++status.trustedFriendCount;
            if (friendEntry.autoSyncEnabled) {
                ++status.autoSyncEnabledCount;
            }
        } else if (friendEntry.trustState == "revoked") {
            ++status.revokedFriendCount;
        } else if (friendEntry.trustState == "blocked") {
            ++status.blockedFriendCount;
        }
    }
    status.autoSyncAvailable = status.autoSyncEnabledCount > 0;
    status.state = status.autoSyncAvailable ? "ready" : "ready_disabled";
    status.reason = status.autoSyncAvailable
        ? "trusted friends available for future explicit package sync"
        : "friend trust store parsed; no trusted friend has auto-sync enabled";
    status.controlsState = "ready";
    status.controlsReason =
        "trusted friends can be revoked, blocked, or have auto-sync disabled through the trust-store update command";
    status.controlsNextStep =
        "Use the trust-store update command with the target node_id, trust_state, auto_sync_enabled flag, and updated_at timestamp.";
    if (status.schemaCompatibilityState != "current" && !status.schemaCompatibilityNote.empty()) {
        status.controlsState = "degraded";
        status.controlsReason = status.schemaCompatibilityNote;
        status.controlsNextStep =
            "Regenerate the friend trust store before relying on friend mesh sync.";
    }
    return status;
}

CompanionFriendMpSyncTransportStatus buildFriendMpSyncTransportStatusImpl(
    const CompanionFriendTrustStoreStatus& friendTrustStore)
{
    CompanionFriendMpSyncTransportStatus status;
    if (!friendTrustStore.mpSyncTransportState.empty()) {
        status.state = friendTrustStore.mpSyncTransportState;
    }
    if (!friendTrustStore.mpSyncTransportReason.empty()) {
        status.reason = friendTrustStore.mpSyncTransportReason;
    }
    if (!friendTrustStore.mpSyncTransportNextStep.empty()) {
        status.nextStep = friendTrustStore.mpSyncTransportNextStep;
    }
    return status;
}

CompanionFriendMpSyncTransportAdapterStatus buildFriendMpSyncTransportAdapterStatusImpl(
    const CompanionFriendTrustStoreStatus& friendTrustStore)
{
    CompanionFriendMpSyncTransportAdapterStatus status;
    if (!friendTrustStore.mpSyncTransportAdapterKind.empty()) {
        status.kind = friendTrustStore.mpSyncTransportAdapterKind;
    }
    status.path = friendTrustStore.mpSyncTransportAdapterPath;

    if (friendTrustStore.state == "needs_attention") {
        status.state = "needs_attention";
        status.reason = friendTrustStore.reason.empty()
            ? "friend trust store needs attention"
            : friendTrustStore.reason;
        status.nextStep = friendTrustStore.controlsNextStep.empty()
            ? "Repair or recreate the friend trust store before relying on transport adapters."
            : friendTrustStore.controlsNextStep;
        return status;
    }

    if (status.path.empty()) {
        status.state = "not_configured";
        status.reason =
            "shared-folder/cloud-folder transport adapter path not configured; manual MP package export/import remains the fallback";
        status.nextStep =
            "Choose a synced folder path in a future adapter settings flow, then keep manual export/import and strict verify until signed/encrypted friend transport is implemented.";
        return status;
    }

    std::error_code error;
    if (!std::filesystem::exists(status.path, error) || error) {
        status.state = "needs_attention";
        status.reason = "selected shared-folder/cloud-folder transport adapter path is missing";
        status.nextStep =
            "Point SNC at a readable synced folder or use manual MP package export/import and strict verify.";
        return status;
    }

    if (!std::filesystem::is_directory(status.path, error) || error) {
        status.state = "needs_attention";
        status.reason = "selected shared-folder/cloud-folder transport adapter path is not a folder";
        status.nextStep =
            "Point SNC at a readable synced folder or use manual MP package export/import and strict verify.";
        return status;
    }

    if (friendTrustStore.state == "not_configured") {
        status.state = "not_configured";
        status.reason = friendTrustStore.reason.empty()
            ? "friend trust store not present; automatic friend sync disabled"
            : friendTrustStore.reason;
        status.nextStep = friendTrustStore.controlsNextStep.empty()
            ? "Import a friend acceptance first, then revisit the transport adapter boundary."
            : friendTrustStore.controlsNextStep;
        return status;
    }

    if (friendTrustStore.state == "ready" || friendTrustStore.state == "ready_disabled") {
        status.state = "ready_disabled";
        status.reason =
            "selected shared-folder/cloud-folder transport adapter path is configured and readable; signed/encrypted friend MP sync transport adapter is not implemented yet";
        status.nextStep =
            "Use manual MP package export/import and strict verify until signed/encrypted friend transport is implemented.";
        return status;
    }

    return status;
}

CompanionFriendMeshUpdateStatus buildFriendMeshUpdateStatusImpl(
    const CompanionFriendTrustStoreStatus& friendTrustStore,
    const CompanionMpOverlayPackageStatus& mpOverlayPackage)
{
    CompanionFriendMeshUpdateStatus status;

    if (friendTrustStore.state == "needs_attention") {
        status.state = "blocked";
        status.reason = friendTrustStore.reason.empty()
            ? "friend trust store needs attention"
            : friendTrustStore.reason;
        status.nextStep = "Fix or recreate the friend trust store before relying on friend mesh sync.";
        return status;
    }

    if (mpOverlayPackage.identityMismatchWarning || mpOverlayPackage.mismatchWarningState != "no_mismatch") {
        status.state = "mismatch";
        status.reason = mpOverlayPackage.mismatchWarningReason.empty()
            ? "package identity mismatch detected"
            : mpOverlayPackage.mismatchWarningReason;
        status.nextStep = "Run strict verify/import before any friend mesh update.";
        return status;
    }

    if (mpOverlayPackage.handoffStatus == "degraded_previous_host_unavailable") {
        status.state = "degraded_handoff";
        status.reason = "previous host unavailable; handoff continuity is degraded";
        status.nextStep = mpOverlayPackage.handoffRecoveryHint.empty()
            ? "Use a verified archive or manual host rotation handoff until continuity is restored."
            : mpOverlayPackage.handoffRecoveryHint;
        return status;
    }

    if (friendTrustStore.autoSyncAvailable) {
        status.state = "ready";
        status.reason = "trusted friends available for future explicit package sync";
        status.nextStep = friendTrustStore.mpSyncTransportNextStep.empty()
            ? "Use manual MP package export/import and strict verify until signed/encrypted friend transport is implemented."
            : friendTrustStore.mpSyncTransportNextStep;
        return status;
    }

    status.state = "waiting";
    status.reason = friendTrustStore.reason.empty()
        ? "friend trust store parsed; waiting for trusted friend mesh prerequisites"
        : friendTrustStore.reason;
    status.nextStep = friendTrustStore.mpSyncTransportNextStep.empty()
        ? "Use manual MP package export/import and strict verify until signed/encrypted friend transport is implemented."
        : friendTrustStore.mpSyncTransportNextStep;
    return status;
}

std::string buildStatusCenterSummaryText(
    const std::string& generatedAtLocal,
    const CompanionLifecycleStatus& lifecycle,
    const CompanionSupportReportStatus& supportReport,
    const CompanionCrashRecoveryStatus& crashRecovery,
    const CompanionSubsystemStatus& saveDiscovery,
    const CompanionSubsystemStatus& archive,
    const CompanionSubsystemStatus& generatedOverlay,
    const CompanionGeneratedOverlayPublishGateStatus& generatedOverlayPublishGate,
    const CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const CompanionPostPlayPipelineStatus& postPlayPipeline,
    const CompanionSubsystemStatus& gameplayAcceptance,
    const CompanionLocalLlmStatus& localLlm,
    const CompanionFriendTrustStoreStatus& friendTrustStore,
    const CompanionSubsystemStatus& statusCenter,
    const std::string& nextAction,
    const std::string& nextActionReason,
    const std::string& nextActionCommandHint,
    const std::string& nextActionCommandHintSource,
    const std::filesystem::path& nextActionPath,
    const std::filesystem::path& statusUiStatePath)
{
    std::ostringstream text;
    CompanionStatusSnapshot ownerTestSnapshot;
    ownerTestSnapshot.generatedOverlay = generatedOverlay;
    ownerTestSnapshot.generatedOverlayPublishGate = generatedOverlayPublishGate;
    ownerTestSnapshot.gameplayAcceptance = gameplayAcceptance;
    const bool monthlyReactiveOwnerTestReady = isPublishedMonthlyReactiveOwnerTestReady(ownerTestSnapshot);
    const auto ownerTestPlaybookPath = buildCompanionOwnerTestPlaybookPath(ownerTestSnapshot);
    text << "Strategic Nexus Status Center\n";
    if (!generatedAtLocal.empty()) {
        text << "generated_at_local: " << generatedAtLocal << "\n";
    }
    text << "startup_lifecycle_state: "
         << (lifecycle.startWithWindowsEnabled ? "owner_enabled_start_with_windows" : "manual_start_only") << "\n";
    text << "startup_start_with_windows_enabled: "
         << (lifecycle.startWithWindowsEnabled ? "true" : "false") << "\n";
    text << "startup_start_with_windows_source: " << lifecycle.startWithWindowsSource << "\n";
    text << "startup_start_with_windows_shortcut_state: "
         << lifecycle.startWithWindowsShortcutState << "\n";
    if (!lifecycle.startWithWindowsShortcutPath.empty()) {
        text << "startup_start_with_windows_shortcut_path: "
             << pathString(lifecycle.startWithWindowsShortcutPath) << "\n";
    }
    text << "startup_start_with_windows_command_hint_source: "
         << lifecycle.startWithWindowsCommandHintSource << "\n";
    if (!lifecycle.startWithWindowsCommandHint.empty()) {
        text << "startup_start_with_windows_command_hint: "
             << lifecycle.startWithWindowsCommandHint << "\n";
    }
    if (!lifecycle.startWithWindowsEnableCommandHint.empty()) {
        text << "startup_start_with_windows_enable_command_hint: "
             << lifecycle.startWithWindowsEnableCommandHint << "\n";
    }
    if (!lifecycle.startWithWindowsDisableCommandHint.empty()) {
        text << "startup_start_with_windows_disable_command_hint: "
             << lifecycle.startWithWindowsDisableCommandHint << "\n";
    }
    text << "support_report_state: " << supportReport.state << "\n";
    text << "support_report_reason: " << supportReport.reason << "\n";
    if (!supportReport.previewPath.empty()) {
        text << "support_report_preview_path: " << pathString(supportReport.previewPath) << "\n";
    }
    text << "support_report_contact_email: " << supportReport.supportContactEmail << "\n";
    text << "support_report_send_requires_approval: "
         << (supportReport.sendRequiresApproval ? "true" : "false") << "\n";
    text << "support_report_raw_saves_included: "
         << (supportReport.rawSavesIncluded ? "true" : "false") << "\n";
    if (!supportReport.dataCategories.empty()) {
        text << "support_report_data_categories: "
             << joinStrings(supportReport.dataCategories, ",") << "\n";
    }
    if (!supportReport.prepareCommandHint.empty()) {
        text << "support_report_prepare_command_hint: "
             << supportReport.prepareCommandHint << "\n";
    }
    if (!supportReport.openCommandHint.empty()) {
        text << "support_report_open_command_hint: "
             << supportReport.openCommandHint << "\n";
    }
    text << "crash_recovery_state: " << crashRecovery.state << "\n";
    text << "crash_recovery_reason: " << crashRecovery.reason << "\n";
    if (!crashRecovery.statePath.empty()) {
        text << "crash_recovery_state_path: " << pathString(crashRecovery.statePath) << "\n";
    }
    if (!crashRecovery.lastCrashAtLocal.empty()) {
        text << "crash_recovery_last_crash_at_local: " << crashRecovery.lastCrashAtLocal << "\n";
    }
    if (!crashRecovery.lastRecoveryAction.empty()) {
        text << "crash_recovery_last_recovery_action: " << crashRecovery.lastRecoveryAction << "\n";
    }
    if (!crashRecovery.lastOperation.empty()) {
        text << "crash_recovery_last_operation: " << crashRecovery.lastOperation << "\n";
    }
    if (!crashRecovery.appVersion.empty()) {
        text << "crash_recovery_app_version: " << crashRecovery.appVersion << "\n";
    }
    text << "crash_recovery_recent_unexpected_restart_count: "
         << crashRecovery.recentUnexpectedRestartCount << "\n";
    text << "crash_recovery_restart_window_minutes: " << crashRecovery.restartWindowMinutes << "\n";
    text << "crash_recovery_next_backoff_seconds: " << crashRecovery.nextBackoffSeconds << "\n";
    text << "crash_recovery_restart_budget_exhausted: "
         << (crashRecovery.restartBudgetExhausted ? "true" : "false") << "\n";
    text << "crash_recovery_warning_visible: "
         << (crashRecovery.warningVisible ? "true" : "false") << "\n";
    text << "crash_recovery_support_report_recommended: "
         << (crashRecovery.supportReportRecommended ? "true" : "false") << "\n";
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
    if (!mpOverlayPackage.packageZipHash.empty()) {
        text << "mp_package_zip_hash: " << mpOverlayPackage.packageZipHash << "\n";
    }
    if (!mpOverlayPackage.packageZipState.empty()) {
        text << "mp_package_zip_bytes: " << mpOverlayPackage.packageZipBytes << "\n";
    }
    if (!mpOverlayPackage.packageZipPath.empty() || !mpOverlayPackage.packageManifestHash.empty()) {
        text << "mp_sdileni_tip: zkopiruj mp_package_zip_path a mp_package_manifest_hash; host/client kroky jsou nize.\n";
    }
    if (!mpOverlayPackage.hostReadiness.empty()) {
        text << "mp_host_readiness: " << mpOverlayPackage.hostReadiness << "\n";
    }
    if (!mpOverlayPackage.clientReadinessGate.empty()) {
        text << "mp_client_gate: " << mpOverlayPackage.clientReadinessGate << "\n";
    }
    if (!mpOverlayPackage.hostNextStep.empty()) {
        text << "mp_host_krok: " << mpOverlayPackage.hostNextStep << "\n";
    }
    if (!mpOverlayPackage.clientNextStep.empty()) {
        text << "mp_client_krok: " << mpOverlayPackage.clientNextStep << "\n";
    }
    if (!mpOverlayPackage.handoffRecoveryHint.empty()) {
        text << "mp_handoff_recovery_hint: " << mpOverlayPackage.handoffRecoveryHint << "\n";
    }
    text << "mp_host_rotation_sync_state: " << mpOverlayPackage.hostRotationSyncState << "\n";
    text << "mp_host_rotation_sync_reason: " << mpOverlayPackage.hostRotationSyncReason << "\n";
    text << "mp_host_rotation_sync_next_step: " << mpOverlayPackage.hostRotationSyncNextStep << "\n";
    const auto friendMeshUpdate = buildFriendMeshUpdateStatus(friendTrustStore, mpOverlayPackage);
    text << "friend_mesh_update_state: " << friendMeshUpdate.state << "\n";
    text << "friend_mesh_update_reason: " << friendMeshUpdate.reason << "\n";
    text << "friend_mesh_update_next_step: " << friendMeshUpdate.nextStep << "\n";
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
    text << "memory_recovery: " << postPlayPipeline.memoryRecovery.state << " - "
         << postPlayPipeline.memoryRecovery.reason << "\n";
    text << "memory_recovery_confidence: " << postPlayPipeline.memoryRecovery.confidence << "\n";
    text << "memory_recovery_warning_visible: "
         << (postPlayPipeline.memoryRecovery.warningVisible ? "true" : "false") << "\n";
    if (!postPlayPipeline.memoryRecovery.anchorEntryPointId.empty()) {
        text << "memory_recovery_anchor_entry_point_id: "
             << postPlayPipeline.memoryRecovery.anchorEntryPointId << "\n";
    }
    if (!postPlayPipeline.memoryRecovery.anchorCampaignKey.empty()) {
        text << "memory_recovery_anchor_campaign_key: "
             << postPlayPipeline.memoryRecovery.anchorCampaignKey << "\n";
    }
    if (!postPlayPipeline.memoryRecovery.anchorSaveName.empty()) {
        text << "memory_recovery_anchor_save_name: " << postPlayPipeline.memoryRecovery.anchorSaveName << "\n";
    }
    if (!postPlayPipeline.memoryRecovery.anchorSaveDate.empty()) {
        text << "memory_recovery_anchor_save_date: " << postPlayPipeline.memoryRecovery.anchorSaveDate << "\n";
    }
    if (!postPlayPipeline.memoryRecovery.anchorSourceKind.empty()) {
        text << "memory_recovery_anchor_source_kind: "
             << postPlayPipeline.memoryRecovery.anchorSourceKind << "\n";
    }
    if (!postPlayPipeline.memoryRecovery.anchorPath.empty()) {
        text << "memory_recovery_anchor_path: "
             << pathString(postPlayPipeline.memoryRecovery.anchorPath) << "\n";
    }
    text << "memory_recovery_compatible_archived_evidence_count: "
         << postPlayPipeline.memoryRecovery.compatibleArchivedEvidenceCount << "\n";
    text << "memory_recovery_later_archived_evidence_count: "
         << postPlayPipeline.memoryRecovery.laterArchivedEvidenceCount << "\n";
    if (postPlayPipeline.memoryRecovery.warningVisible) {
        text << "memory_recovery_owner_note: latest loadable save anchor is degraded or needs attention\n";
    }
    if (!postPlayPipeline.postPlayPackagePath.empty()) {
        text << "post_play_package_path: " << pathString(postPlayPipeline.postPlayPackagePath) << "\n";
    }
    if (!postPlayPipeline.postPlayPackageReadiness.empty()) {
        text << "post_play_package_readiness: " << postPlayPipeline.postPlayPackageReadiness << "\n";
    }
    if (!postPlayPipeline.postPlayPackageReason.empty()) {
        text << "post_play_package_reason: " << postPlayPipeline.postPlayPackageReason << "\n";
    }
    if (!postPlayPipeline.postPlayPackageCampaignIdentityStateSummary.empty()) {
        text << "post_play_package_campaign_identity_state_summary: "
             << postPlayPipeline.postPlayPackageCampaignIdentityStateSummary << "\n";
        if (postPlayPipeline.postPlayPackageCampaignIdentityStateSummary == "ambiguous_save_identity") {
            text << "post_play_package_campaign_identity_owner_note: save-content identity is ambiguous; keep generated rules conservative until identity resolves\n";
        } else if (postPlayPipeline.postPlayPackageCampaignIdentityStateSummary == "mixed_save_identity_resolution") {
            text << "post_play_package_campaign_identity_owner_note: save-content identity is mixed between resolved and fallback entries; keep generated rules conservative until identity fully resolves\n";
        } else if (postPlayPipeline.postPlayPackageCampaignIdentityStateSummary == "folder_alias_fallback") {
            text << "post_play_package_campaign_identity_owner_note: save-content identity fell back to folder alias; keep generated rules conservative until identity resolves from save contents\n";
        }
    }
    if (!postPlayPipeline.postPlayPackagePersonalityProfilePromptOutputNote.empty()) {
        text << "post_play_package_personality_profile_prompt_output_note: "
             << postPlayPipeline.postPlayPackagePersonalityProfilePromptOutputNote << "\n";
    }
    if (!postPlayPipeline.playerCountryId.empty()) {
        text << "post_play_player_country_id: " << postPlayPipeline.playerCountryId << "\n";
    }
    text << "post_play_decision_ready_entry_count: " << postPlayPipeline.postPlayDecisionReadyEntryCount << "\n";
    text << "post_play_campaign_count: " << postPlayPipeline.postPlayCampaignCount << "\n";
    text << "post_play_ready_campaign_count: " << postPlayPipeline.postPlayReadyCampaignCount << "\n";
    text << "post_play_partial_campaign_count: " << postPlayPipeline.postPlayPartialCampaignCount << "\n";
    text << "post_play_blocked_campaign_count: " << postPlayPipeline.postPlayBlockedCampaignCount << "\n";
    for (const auto& summary : postPlayPipeline.postPlayCampaignReadinessSummaries) {
        text << "post_play_campaign_summary: " << summary << "\n";
    }
    if (postPlayPipeline.campaignLibraryPlanPresent) {
        text << "campaign_library_plan_path: " << pathString(postPlayPipeline.campaignLibraryPlanPath) << "\n";
        text << "campaign_library_plan_source: " << postPlayPipeline.campaignLibraryPlanSource << "\n";
        text << "campaign_library_plan_readiness: " << postPlayPipeline.campaignLibraryPlanReadiness << "\n";
        text << "campaign_library_plan_reason: " << postPlayPipeline.campaignLibraryPlanReason << "\n";
        text << "campaign_library_limit_reached: "
             << (postPlayPipeline.campaignLibraryLimitReached ? "true" : "false") << "\n";
        text << "campaign_library_skipped_due_to_limit_count: "
             << postPlayPipeline.campaignLibrarySkippedDueToLimitCount << "\n";
        text << "campaign_library_pinned_count: " << postPlayPipeline.campaignLibraryPinnedCount << "\n";
        text << "campaign_library_pinned_missing_local_save_count: "
             << postPlayPipeline.campaignLibraryPinnedMissingLocalSaveCount << "\n";
        if (!postPlayPipeline.campaignLibraryPinPath.empty()) {
            text << "campaign_library_pin_path: " << pathString(postPlayPipeline.campaignLibraryPinPath) << "\n";
        }
        text << "campaign_library_pin_state: " << postPlayPipeline.campaignLibraryPinState << "\n";
        text << "campaign_library_pin_reason: " << postPlayPipeline.campaignLibraryPinReason << "\n";
        text << "campaign_library_pin_next_step: " << postPlayPipeline.campaignLibraryPinNextStep << "\n";
        if (!postPlayPipeline.campaignLibraryPinCommandTemplate.empty()) {
            text << "campaign_library_pin_command_template: "
                 << postPlayPipeline.campaignLibraryPinCommandTemplate << "\n";
        }
        if (postPlayPipeline.campaignLibraryPlanReadiness == "degraded") {
            text << "campaign_library_owner_note: campaign library plan is loaded from legacy schema_version 0; regenerate it before SNC trusts active campaign coverage\n";
        } else if (postPlayPipeline.campaignLibraryPlanReadiness == "needs_attention") {
            text << "campaign_library_owner_note: campaign library plan needs attention before SNC should trust active campaign coverage\n";
        } else if (postPlayPipeline.campaignLibraryPinState == "degraded"
            && postPlayPipeline.campaignLibraryPinReason ==
                "pinned campaign exception manifest loaded from legacy schema_version 0") {
            text << "campaign_library_owner_note: pinned campaign exception manifest is loaded from legacy schema_version 0; regenerate it before SNC trusts pinned coverage\n";
        } else if (postPlayPipeline.campaignLibraryPinState == "needs_attention") {
            text << "campaign_library_owner_note: pinned campaign exception manifest needs attention before SNC should trust pinned coverage\n";
        } else if (postPlayPipeline.campaignLibraryPinState == "degraded") {
            text << "campaign_library_owner_note: pinned campaign exceptions are available, but some pinned campaigns are not locally playable\n";
        } else if (postPlayPipeline.campaignLibraryLimitReached) {
            text << "campaign_library_owner_note: active generated campaign library is truncated by the configured limit; raise the cap or clean local campaigns before broader coverage tests\n";
            if (postPlayPipeline.campaignLibraryPinState == "unavailable") {
                text << "campaign_library_owner_note: user-pinned campaign exceptions are not yet available; keep the local save root present or restore it before broader coverage tests\n";
            }
        } else {
            text << "campaign_library_owner_note: active generated campaign library fits within the configured limit\n";
        }
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
    text << "generated_overlay_reactive_capability: "
         << (generatedOverlay.reactivePolicyPackCapability.empty() ? "unknown"
                                                                   : generatedOverlay.reactivePolicyPackCapability)
         << "\n";
    text << "generated_overlay_event_family_count: " << generatedOverlay.eventFamilies.size() << "\n";
    if (!generatedOverlay.eventFamilies.empty()) {
        text << "generated_overlay_event_families: "
             << joinStrings(generatedOverlay.eventFamilies, ",") << "\n";
    }
    text << "generated_overlay_source_quality_count: " << generatedOverlay.sourceQualities.size() << "\n";
    if (!generatedOverlay.sourceQualities.empty()) {
        text << "generated_overlay_source_qualities: "
             << joinStrings(generatedOverlay.sourceQualities, ",") << "\n";
    }
    text << "generated_overlay_bootstrap_campaign_count: "
         << generatedOverlay.bootstrapCampaignCount << "\n";
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
    if (mpOverlayPackage.previousHostAvailableKnown) {
        text << "mp_previous_host_available: "
             << (mpOverlayPackage.previousHostAvailable ? "true" : "false") << "\n";
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
    if (!mpOverlayPackage.handoffRecoveryHint.empty()) {
        text << "mp_handoff_recovery_hint: " << mpOverlayPackage.handoffRecoveryHint << "\n";
    }
    text << "mp_host_rotation_sync_state: " << mpOverlayPackage.hostRotationSyncState << "\n";
    text << "mp_host_rotation_sync_reason: " << mpOverlayPackage.hostRotationSyncReason << "\n";
    text << "mp_host_rotation_sync_next_step: " << mpOverlayPackage.hostRotationSyncNextStep << "\n";
    if (!mpOverlayPackage.packageManifestHash.empty()) {
        text << "package_manifest_hash: " << mpOverlayPackage.packageManifestHash << "\n";
    }
    if (!mpOverlayPackage.provenanceState.empty()) {
        text << "mp_provenance_state: " << mpOverlayPackage.provenanceState << "\n";
    }
    text << "human_control_guard_state: " << mpOverlayPackage.humanControlGuardState << "\n";
    text << "mp_source_quality_count: " << mpOverlayPackage.sourceQualities.size() << "\n";
    for (const auto& sourceQuality : mpOverlayPackage.sourceQualities) {
        text << "mp_source_quality: " << sourceQuality << "\n";
    }
    text << "mp_bootstrap_campaign_count: " << mpOverlayPackage.bootstrapCampaignCount << "\n";
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
    text << "local_llm_model: " << localLlm.state << " - " << localLlm.reason << "\n";
    text << "local_llm_reduced_mode: " << (localLlm.reducedMode ? "true" : "false") << "\n";
    text << "local_llm_can_run_inference: " << (localLlm.canRunInference ? "true" : "false") << "\n";
    text << "local_llm_user_action_required: " << (localLlm.userActionRequired ? "true" : "false") << "\n";
    text << "local_llm_download_allowed: " << (localLlm.downloadAllowed ? "true" : "false") << "\n";
    if (!localLlm.selectedModelId.empty()) {
        text << "local_llm_selected_model_id: " << localLlm.selectedModelId << "\n";
    }
    if (!localLlm.selectedDisplayName.empty()) {
        text << "local_llm_selected_display_name: " << localLlm.selectedDisplayName << "\n";
    }
    if (!localLlm.runtime.empty()) {
        text << "local_llm_runtime: " << localLlm.runtime << "\n";
    }
    if (!localLlm.catalogStatus.empty()) {
        text << "local_llm_catalog_status: " << localLlm.catalogStatus << "\n";
    }
    text << "local_llm_supported_model_catalog_count: "
         << localLlm.supportedModelCatalogCount << "\n";
    if (!localLlm.supportedModelCatalogSummary.empty()) {
        text << "local_llm_supported_model_catalog_summary: "
             << localLlm.supportedModelCatalogSummary << "\n";
    }
    if (!localLlm.localPath.empty()) {
        text << "local_llm_local_path: " << pathString(localLlm.localPath) << "\n";
    }
    if (!localLlm.hardwareFit.empty()) {
        text << "local_llm_hardware_fit: " << localLlm.hardwareFit << "\n";
    }
    if (!localLlm.recommendedModelId.empty()) {
        text << "local_llm_recommended_model_id: " << localLlm.recommendedModelId << "\n";
    }
    if (!localLlm.recommendedDisplayName.empty()) {
        text << "local_llm_recommended_display_name: " << localLlm.recommendedDisplayName << "\n";
    }
    if (!localLlm.recommendedRuntime.empty()) {
        text << "local_llm_recommended_runtime: " << localLlm.recommendedRuntime << "\n";
    }
    if (!localLlm.modelStatePath.empty()) {
        text << "local_llm_model_state_path: " << pathString(localLlm.modelStatePath) << "\n";
    }
    if (!localLlm.prepareCommandHint.empty()) {
        text << "local_llm_prepare_command_hint: " << localLlm.prepareCommandHint << "\n";
    }
    if (!localLlm.installGuidance.empty()) {
        text << "local_llm_install_guidance: " << localLlm.installGuidance << "\n";
    }
    if (!localLlm.summary.empty()) {
        text << "local_llm_model_manager_summary: " << localLlm.summary << "\n";
    }
    text << "friend_trust_store: " << friendTrustStore.state << " - "
         << friendTrustStore.reason << "\n";
    if (!friendTrustStore.path.empty()) {
        text << "friend_trust_store_path: " << pathString(friendTrustStore.path) << "\n";
    }
    if (friendTrustStore.schemaCompatibilityKnown) {
        text << "friend_trust_store_source_schema_version: " << friendTrustStore.sourceSchemaVersion << "\n";
        text << "friend_trust_store_schema_compatibility_state: "
             << friendTrustStore.schemaCompatibilityState << "\n";
        if (!friendTrustStore.schemaCompatibilityNote.empty()) {
            text << "friend_trust_store_schema_compatibility_note: "
                 << friendTrustStore.schemaCompatibilityNote << "\n";
            if (friendTrustStore.schemaCompatibilityState != "current") {
                text << "friend_trust_store_owner_note: "
                     << friendTrustStore.schemaCompatibilityNote << "\n";
            }
        }
    }
    text << "friend_trust_store_trusted_count: "
         << friendTrustStore.trustedFriendCount << "\n";
    text << "friend_trust_store_revoked_count: "
         << friendTrustStore.revokedFriendCount << "\n";
    text << "friend_trust_store_blocked_count: "
         << friendTrustStore.blockedFriendCount << "\n";
    text << "friend_trust_store_auto_sync_enabled_count: "
         << friendTrustStore.autoSyncEnabledCount << "\n";
    text << "friend_trust_store_auto_sync_available: "
         << (friendTrustStore.autoSyncAvailable ? "true" : "false") << "\n";
    if (!friendTrustStore.controlsState.empty()) {
        text << "friend_trust_store_controls_state: " << friendTrustStore.controlsState << "\n";
    }
    if (!friendTrustStore.controlsReason.empty()) {
        text << "friend_trust_store_controls_reason: " << friendTrustStore.controlsReason << "\n";
    }
    if (!friendTrustStore.controlsNextStep.empty()) {
        text << "friend_trust_store_controls_next_step: " << friendTrustStore.controlsNextStep << "\n";
    }
    if (!friendTrustStore.controlsCommandTemplate.empty()) {
        text << "friend_trust_store_update_command_template: "
             << friendTrustStore.controlsCommandTemplate << "\n";
    }
    if (!friendTrustStore.pairingCommandTemplate.empty()) {
        text << "friend_pairing_command_template: "
             << friendTrustStore.pairingCommandTemplate << "\n";
    }
    if (!friendTrustStore.mpSyncEnvelopeCommandTemplate.empty()) {
        text << "friend_mp_sync_envelope_command_template: "
             << friendTrustStore.mpSyncEnvelopeCommandTemplate << "\n";
    }
    if (!friendTrustStore.mpSyncInboxPlanCommandTemplate.empty()) {
        text << "friend_mp_sync_inbox_plan_command_template: "
             << friendTrustStore.mpSyncInboxPlanCommandTemplate << "\n";
    }
    if (!friendTrustStore.mpSyncOutboxPlanCommandTemplate.empty()) {
        text << "friend_mp_sync_outbox_plan_command_template: "
             << friendTrustStore.mpSyncOutboxPlanCommandTemplate << "\n";
    }
    if (!friendTrustStore.mpSyncInboxPlanState.empty()) {
        text << "friend_mp_sync_inbox_plan_state: "
             << friendTrustStore.mpSyncInboxPlanState << "\n";
    }
    if (!friendTrustStore.mpSyncInboxPlanReason.empty()) {
        text << "friend_mp_sync_inbox_plan_reason: "
             << friendTrustStore.mpSyncInboxPlanReason << "\n";
    }
    text << "friend_mp_sync_inbox_plan_automatic_download_enabled: "
         << (friendTrustStore.mpSyncInboxAutomaticDownloadEnabled ? "true" : "false") << "\n";
    text << "friend_mp_sync_inbox_plan_package_staging_allowed: "
         << (friendTrustStore.mpSyncInboxPackageStagingAllowed ? "true" : "false") << "\n";
    const auto friendMpSyncTransport = buildFriendMpSyncTransportStatus(friendTrustStore);
    text << "friend_mp_sync_transport_state: "
         << friendMpSyncTransport.state << "\n";
    text << "friend_mp_sync_transport_reason: "
         << friendMpSyncTransport.reason << "\n";
    text << "friend_mp_sync_transport_next_step: "
         << friendMpSyncTransport.nextStep << "\n";
    text << "friend_mp_sync_transport_adapter_kind: "
         << friendTrustStore.mpSyncTransportAdapterKind << "\n";
    text << "friend_mp_sync_transport_adapter_path: "
         << pathString(friendTrustStore.mpSyncTransportAdapterPath) << "\n";
    const auto friendMpSyncTransportAdapter = buildFriendMpSyncTransportAdapterStatus(friendTrustStore);
    text << "friend_mp_sync_transport_adapter_state: "
         << friendMpSyncTransportAdapter.state << "\n";
    text << "friend_mp_sync_transport_adapter_reason: "
         << friendMpSyncTransportAdapter.reason << "\n";
    text << "friend_mp_sync_transport_adapter_next_step: "
         << friendMpSyncTransportAdapter.nextStep << "\n";
    if (!friendTrustStore.mpSyncPreflightChecklist.empty()) {
        text << "friend_mp_sync_preflight_checklist: "
             << friendTrustStore.mpSyncPreflightChecklist << "\n";
    }
    if (monthlyReactiveOwnerTestReady) {
        text << "owner_test_contract_state: ready_for_monthly_reactive_session_test\n";
        text << "owner_test_scope: load_or_resume_a_real_non_ironman_session_with_the_current_published_overlay_and_wait_for_the_next_monthly_pulse\n";
        text << "owner_test_playbook_path: " << pathString(ownerTestPlaybookPath) << "\n";
        text << "owner_test_visible_markers: Strategic Nexus v0: Defensive military posture|Strategic Nexus v0: Aggressive military posture|Strategic Nexus v0: Economy research bias|Strategic Nexus v0: Military industry research bias\n";
        text << "owner_test_expected_log_prefixes: Strategic Nexus: generated overlay projected military_posture|Strategic Nexus: generated overlay projected research_bias\n";
        text << "owner_test_codex_artifacts: generated_overlay_publish_status.json|generated_overlay_gameplay_acceptance_v0.json|Stellaris logs/error.log\n";
        text << "owner_test_known_limitations: marker proves published monthly reactive branch visibility only; it is gameplay-neutral and not full strategic quality validation\n";
    }
    if (!nextAction.empty()) {
        text << "next_action: " << nextAction << "\n";
    }
    if (!nextActionReason.empty()) {
        text << "next_action_reason: " << nextActionReason << "\n";
    }
    if (!nextActionCommandHint.empty()) {
        text << "next_action_command_hint: " << nextActionCommandHint << "\n";
    }
    if (!nextActionCommandHintSource.empty()) {
        text << "next_action_command_hint_source: " << nextActionCommandHintSource << "\n";
    }
    if (!nextActionPath.empty()) {
        text << "next_action_path: " << pathString(nextActionPath) << "\n";
    }
    if (!statusUiStatePath.empty()) {
        text << "status_ui_state_path: " << pathString(statusUiStatePath) << "\n";
        text << "status_ui_state_note: active page and normal window placement persist between launches\n";
    }
    return text.str();
}

std::string buildCompanionNextAction(const CompanionStatusSnapshot& snapshot)
{
    if (snapshot.mpOverlayPackage.identityMismatchWarning) {
        return "review_mp_package_mismatch_warning";
    }
    if (crashRecoveryNeedsAttention(snapshot.crashRecovery)) {
        return "review_crash_recovery_status";
    }
    if (snapshot.postPlayPipeline.branchAmbiguityDetected) {
        return "review_entry_point_ambiguity";
    }
    if (memoryRecoveryNeedsAttention(snapshot)) {
        return "review_memory_recovery_status";
    }
    if (archiveNeedsAttention(snapshot)) {
        return "review_archive_status";
    }
    if (hasDegradedMpHandoffContinuity(snapshot)) {
        return "review_mp_handoff_continuity";
    }
    if (snapshot.mpOverlayPackage.state == "needs_attention") {
        return "review_mp_overlay_package_warning";
    }
    if (snapshot.generatedOverlayPublishGate.canPublish) {
        return "review_staged_overlay_and_publish_if_desired";
    }
    if (isPublishedMonthlyReactiveOwnerTestReady(snapshot)) {
        return "run_monthly_reactive_owner_test";
    }
    if (snapshot.generatedOverlayPublishGate.state == "published") {
        return "review_published_overlay_status";
    }
    if (snapshot.localLlm.state == "model_incompatible_with_hardware" ||
        snapshot.localLlm.state == "no_model_installed" ||
        snapshot.localLlm.state == "model_missing" ||
        snapshot.localLlm.state == "model_license_not_supported" ||
        snapshot.localLlm.state == "model_license_requires_user_action" ||
        snapshot.localLlm.state == "model_runtime_failed" ||
        snapshot.localLlm.state == "model_changed_revalidation_needed") {
        return "review_local_llm_model_manager";
    }
    if (supportReportNeedsAttention(snapshot.supportReport)) {
        return "review_support_report_status";
    }
    if (snapshot.postPlayPipeline.generatedOverlayStagingReadiness == "staged_verified") {
        return "review_staged_overlay_status";
    }
    if (startsWith(snapshot.postPlayPipeline.dslDraftReadiness, "ready")) {
        return "review_dsl_draft";
    }
    if (startsWith(snapshot.postPlayPipeline.candidateDecisionPackageReadiness, "ready")) {
        return "review_candidate_decision_package";
    }
    if (snapshot.statusCenter.state == "attention_required") {
        return "review_status_center_issue";
    }
    if (snapshot.statusCenter.state == "starting" || snapshot.statusCenter.state == "waiting") {
        return "wait_for_next_pipeline_step";
    }
    return "review_status_center_summary";
}

std::string buildCompanionNextActionReason(const CompanionStatusSnapshot& snapshot)
{
    if (snapshot.mpOverlayPackage.identityMismatchWarning) {
        return "package_identity_mismatch_detected";
    }
    if (crashRecoveryNeedsAttention(snapshot.crashRecovery)) {
        return snapshot.crashRecovery.reason.empty() ? snapshot.crashRecovery.state : snapshot.crashRecovery.reason;
    }
    if (snapshot.postPlayPipeline.branchAmbiguityDetected) {
        return "entry_point_branch_ambiguity_detected";
    }
    if (memoryRecoveryNeedsAttention(snapshot)) {
        return "entry_point_analysis_unavailable";
    }
    if (archiveNeedsAttention(snapshot)) {
        return snapshot.archive.reason.empty() ? snapshot.archive.state : snapshot.archive.reason;
    }
    if (hasDegradedMpHandoffContinuity(snapshot)) {
        return "mp_handoff_degraded_previous_host_unavailable";
    }
    if (snapshot.mpOverlayPackage.state == "needs_attention") {
        return snapshot.mpOverlayPackage.mismatchWarningReason.empty() ? snapshot.mpOverlayPackage.reason
                                                                       : snapshot.mpOverlayPackage.mismatchWarningReason;
    }
    if (snapshot.generatedOverlayPublishGate.canPublish) {
        return "staged_overlay_ready_owner_gate_available";
    }
    if (isPublishedMonthlyReactiveOwnerTestReady(snapshot)) {
        return "published_monthly_reactive_overlay_ready_for_owner_test";
    }
    if (snapshot.generatedOverlayPublishGate.state == "published") {
        return "current_staged_overlay_already_published";
    }
    if (snapshot.localLlm.state == "model_incompatible_with_hardware" ||
        snapshot.localLlm.state == "no_model_installed" ||
        snapshot.localLlm.state == "model_missing" ||
        snapshot.localLlm.state == "model_license_not_supported" ||
        snapshot.localLlm.state == "model_license_requires_user_action" ||
        snapshot.localLlm.state == "model_runtime_failed" ||
        snapshot.localLlm.state == "model_changed_revalidation_needed") {
        return snapshot.localLlm.reason.empty() ? snapshot.localLlm.state : snapshot.localLlm.reason;
    }
    if (supportReportNeedsAttention(snapshot.supportReport)) {
        return snapshot.supportReport.reason.empty() ? "support report needs attention"
                                                     : snapshot.supportReport.reason;
    }
    if (snapshot.postPlayPipeline.generatedOverlayStagingReadiness == "staged_verified") {
        return "staged_overlay_written_but_publish_gate_not_ready";
    }
    if (startsWith(snapshot.postPlayPipeline.dslDraftReadiness, "ready")) {
        return "dsl_draft_ready_before_overlay_stage";
    }
    if (startsWith(snapshot.postPlayPipeline.candidateDecisionPackageReadiness, "ready")) {
        return "candidate_decisions_ready_before_dsl_draft";
    }
    if (!snapshot.statusCenter.reason.empty()) {
        return snapshot.statusCenter.reason;
    }
    return "see_status_center_summary";
}

std::string buildCompanionNextActionCommandHint(const CompanionStatusSnapshot& snapshot)
{
    if (snapshot.mpOverlayPackage.identityMismatchWarning) {
        if (!snapshot.mpOverlayPackage.strictVerifyCommand.empty()) {
            return snapshot.mpOverlayPackage.strictVerifyCommand;
        }
        if (!snapshot.mpOverlayPackage.verifyCommand.empty()) {
            return snapshot.mpOverlayPackage.verifyCommand;
        }
    }
    if (crashRecoveryNeedsAttention(snapshot.crashRecovery)) {
        if (snapshot.crashRecovery.supportReportRecommended && !snapshot.supportReport.openCommandHint.empty()) {
            return snapshot.supportReport.openCommandHint;
        }
        if (snapshot.crashRecovery.supportReportRecommended && !snapshot.supportReport.prepareCommandHint.empty()) {
            return snapshot.supportReport.prepareCommandHint;
        }
    }
    if (hasDegradedMpHandoffContinuity(snapshot)) {
        if (!snapshot.mpOverlayPackage.strictVerifyCommand.empty()) {
            return snapshot.mpOverlayPackage.strictVerifyCommand;
        }
        if (!snapshot.mpOverlayPackage.verifyCommand.empty()) {
            return snapshot.mpOverlayPackage.verifyCommand;
        }
    }
    if (buildCompanionNextAction(snapshot) == "review_support_report_status" &&
        supportReportNeedsAttention(snapshot.supportReport)) {
        if (!snapshot.supportReport.openCommandHint.empty()) {
            return snapshot.supportReport.openCommandHint;
        }
        if (!snapshot.supportReport.prepareCommandHint.empty()) {
            return snapshot.supportReport.prepareCommandHint;
        }
    }
    if (buildCompanionNextAction(snapshot) == "review_archive_status" && !archiveAttentionPath(snapshot).empty()) {
        return "open " + pathString(archiveAttentionPath(snapshot));
    }
    if (snapshot.mpOverlayPackage.state == "needs_attention" && !snapshot.mpOverlayPackage.verifyCommand.empty()) {
        return snapshot.mpOverlayPackage.verifyCommand;
    }
    if (snapshot.generatedOverlayPublishGate.canPublish && !snapshot.generatedOverlayPublishGate.publishCommand.empty()) {
        return snapshot.generatedOverlayPublishGate.publishCommand;
    }
    return std::string();
}

std::string buildCompanionNextActionCommandHintSource(const CompanionStatusSnapshot& snapshot)
{
    if (snapshot.mpOverlayPackage.identityMismatchWarning) {
        if (!snapshot.mpOverlayPackage.strictVerifyCommand.empty()) {
            return "mp_overlay_package_strict_verify_command";
        }
        if (!snapshot.mpOverlayPackage.verifyCommand.empty()) {
            return "mp_overlay_package_verify_command";
        }
    }
    if (crashRecoveryNeedsAttention(snapshot.crashRecovery)) {
        if (snapshot.crashRecovery.supportReportRecommended && !snapshot.supportReport.openCommandHint.empty()) {
            return "support_report_open_command";
        }
        if (snapshot.crashRecovery.supportReportRecommended && !snapshot.supportReport.prepareCommandHint.empty()) {
            return "support_report_prepare_command";
        }
    }
    if (hasDegradedMpHandoffContinuity(snapshot)) {
        if (!snapshot.mpOverlayPackage.strictVerifyCommand.empty()) {
            return "mp_overlay_package_strict_verify_command";
        }
        if (!snapshot.mpOverlayPackage.verifyCommand.empty()) {
            return "mp_overlay_package_verify_command";
        }
    }
    if (buildCompanionNextAction(snapshot) == "review_support_report_status" &&
        supportReportNeedsAttention(snapshot.supportReport)) {
        if (!snapshot.supportReport.openCommandHint.empty()) {
            return "support_report_open_command";
        }
        if (!snapshot.supportReport.prepareCommandHint.empty()) {
            return "support_report_prepare_command";
        }
    }
    if (buildCompanionNextAction(snapshot) == "review_archive_status") {
        return "archive_root_path";
    }
    if (snapshot.mpOverlayPackage.state == "needs_attention" && !snapshot.mpOverlayPackage.verifyCommand.empty()) {
        return "mp_overlay_package_verify_command";
    }
    if (snapshot.generatedOverlayPublishGate.canPublish && !snapshot.generatedOverlayPublishGate.publishCommand.empty()) {
        return "generated_overlay_publish_gate_publish_command";
    }
    return "none";
}

std::filesystem::path buildCompanionNextActionPath(const CompanionStatusSnapshot& snapshot)
{
    if (snapshot.mpOverlayPackage.identityMismatchWarning) {
        return snapshot.mpOverlayPackage.path;
    }
    if (crashRecoveryNeedsAttention(snapshot.crashRecovery)) {
        if (snapshot.crashRecovery.supportReportRecommended && !snapshot.supportReport.previewPath.empty()) {
            return snapshot.supportReport.previewPath;
        }
        return snapshot.crashRecovery.statePath;
    }
    if (snapshot.postPlayPipeline.branchAmbiguityDetected) {
        return snapshot.postPlayPipeline.entryPointAnalysisPath;
    }
    if (memoryRecoveryNeedsAttention(snapshot) && !snapshot.postPlayPipeline.entryPointAnalysisPath.empty()) {
        return snapshot.postPlayPipeline.entryPointAnalysisPath;
    }
    if (archiveNeedsAttention(snapshot) && !archiveAttentionPath(snapshot).empty()) {
        return archiveAttentionPath(snapshot);
    }
    if (hasDegradedMpHandoffContinuity(snapshot) || snapshot.mpOverlayPackage.state == "needs_attention") {
        return snapshot.mpOverlayPackage.path;
    }
    if (snapshot.generatedOverlayPublishGate.canPublish) {
        return snapshot.generatedOverlayPublishGate.stagingStatusPath.empty()
            ? snapshot.generatedOverlayPublishGate.path
            : snapshot.generatedOverlayPublishGate.stagingStatusPath;
    }
    if (snapshot.generatedOverlayPublishGate.state == "published") {
        if (isPublishedMonthlyReactiveOwnerTestReady(snapshot) && !snapshot.gameplayAcceptance.path.empty()) {
            return snapshot.gameplayAcceptance.path;
        }
        return snapshot.generatedOverlayPublishGate.publishStatusPath.empty()
            ? snapshot.generatedOverlayPublishGate.path
            : snapshot.generatedOverlayPublishGate.publishStatusPath;
    }
    if (snapshot.localLlm.state == "model_incompatible_with_hardware" ||
        snapshot.localLlm.state == "no_model_installed" ||
        snapshot.localLlm.state == "model_missing" ||
        snapshot.localLlm.state == "model_license_not_supported" ||
        snapshot.localLlm.state == "model_license_requires_user_action" ||
        snapshot.localLlm.state == "model_runtime_failed" ||
        snapshot.localLlm.state == "model_changed_revalidation_needed") {
        if (!snapshot.localLlm.modelStatePath.empty()) {
            return snapshot.localLlm.modelStatePath;
        }
        if (!snapshot.localLlm.localPath.empty()) {
            return snapshot.localLlm.localPath;
        }
    }
    if (buildCompanionNextAction(snapshot) == "review_support_report_status" &&
        supportReportNeedsAttention(snapshot.supportReport)) {
        if (!snapshot.supportReport.previewPath.empty()) {
            return snapshot.supportReport.previewPath;
        }
    }
    if (snapshot.postPlayPipeline.generatedOverlayStagingReadiness == "staged_verified") {
        return snapshot.postPlayPipeline.generatedOverlayStagingStatusPath;
    }
    if (startsWith(snapshot.postPlayPipeline.dslDraftReadiness, "ready")) {
        return snapshot.postPlayPipeline.dslDraftAuditPath.empty() ? snapshot.postPlayPipeline.dslDraftPath
                                                                   : snapshot.postPlayPipeline.dslDraftAuditPath;
    }
    if (startsWith(snapshot.postPlayPipeline.candidateDecisionPackageReadiness, "ready")) {
        return snapshot.postPlayPipeline.candidateDecisionPackagePath;
    }
    return snapshot.statusCenter.path;
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

void writeGeneratedOverlayJson(
    std::ostringstream& output,
    const CompanionSubsystemStatus& status,
    const std::string& indent)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"path\": " << jsonString(pathString(status.path)) << ",\n";
    output << indent << "  \"manifest_hash\": " << jsonString(status.manifestHash) << ",\n";
    output << indent << "  \"reactive_policy_pack_capability\": "
           << jsonString(status.reactivePolicyPackCapability) << ",\n";
    output << indent << "  \"event_families\": [";
    for (std::size_t index = 0; index < status.eventFamilies.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << jsonString(status.eventFamilies[index]);
    }
    output << "],\n";
    output << indent << "  \"source_qualities\": [";
    for (std::size_t index = 0; index < status.sourceQualities.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << jsonString(status.sourceQualities[index]);
    }
    output << "],\n";
    output << indent << "  \"bootstrap_campaign_count\": " << status.bootstrapCampaignCount << "\n";
    output << indent << "}";
}

void writeLocalLlmJson(
    std::ostringstream& output,
    const CompanionLocalLlmStatus& status,
    const std::string& indent)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"selected_model_id\": " << jsonString(status.selectedModelId) << ",\n";
    output << indent << "  \"selected_display_name\": " << jsonString(status.selectedDisplayName) << ",\n";
    output << indent << "  \"runtime\": " << jsonString(status.runtime) << ",\n";
    output << indent << "  \"catalog_status\": " << jsonString(status.catalogStatus) << ",\n";
    output << indent << "  \"supported_model_catalog_count\": "
           << status.supportedModelCatalogCount << ",\n";
    output << indent << "  \"supported_model_catalog_summary\": "
           << jsonString(status.supportedModelCatalogSummary) << ",\n";
    output << indent << "  \"local_path\": " << jsonString(pathString(status.localPath)) << ",\n";
    output << indent << "  \"hardware_fit\": " << jsonString(status.hardwareFit) << ",\n";
    output << indent << "  \"recommended_model_id\": " << jsonString(status.recommendedModelId) << ",\n";
    output << indent << "  \"recommended_display_name\": " << jsonString(status.recommendedDisplayName) << ",\n";
    output << indent << "  \"recommended_runtime\": " << jsonString(status.recommendedRuntime) << ",\n";
    output << indent << "  \"model_state_path\": " << jsonString(pathString(status.modelStatePath)) << ",\n";
    output << indent << "  \"prepare_command_hint\": " << jsonString(status.prepareCommandHint) << ",\n";
    output << indent << "  \"install_guidance\": " << jsonString(status.installGuidance) << ",\n";
    output << indent << "  \"summary\": " << jsonString(status.summary) << ",\n";
    output << indent << "  \"can_run_inference\": " << (status.canRunInference ? "true" : "false") << ",\n";
    output << indent << "  \"reduced_mode\": " << (status.reducedMode ? "true" : "false") << ",\n";
    output << indent << "  \"user_action_required\": "
           << (status.userActionRequired ? "true" : "false") << ",\n";
    output << indent << "  \"download_allowed\": " << (status.downloadAllowed ? "true" : "false") << "\n";
    output << indent << "}";
}

void writeFriendTrustStoreJson(
    std::ostringstream& output,
    const CompanionFriendTrustStoreStatus& status,
    const std::string& indent)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"path\": " << jsonString(pathString(status.path)) << ",\n";
    if (status.schemaCompatibilityKnown) {
        output << indent << "  \"source_schema_version\": " << status.sourceSchemaVersion << ",\n";
        output << indent << "  \"schema_compatibility_state\": "
               << jsonString(status.schemaCompatibilityState) << ",\n";
        output << indent << "  \"schema_compatibility_note\": "
               << jsonString(status.schemaCompatibilityNote) << ",\n";
    }
    output << indent << "  \"trusted_friend_count\": " << status.trustedFriendCount << ",\n";
    output << indent << "  \"revoked_friend_count\": " << status.revokedFriendCount << ",\n";
    output << indent << "  \"blocked_friend_count\": " << status.blockedFriendCount << ",\n";
    output << indent << "  \"auto_sync_enabled_count\": " << status.autoSyncEnabledCount << ",\n";
    output << indent << "  \"controls_state\": " << jsonString(status.controlsState) << ",\n";
    output << indent << "  \"controls_reason\": " << jsonString(status.controlsReason) << ",\n";
    output << indent << "  \"controls_next_step\": " << jsonString(status.controlsNextStep) << ",\n";
    output << indent << "  \"update_command_template\": "
           << jsonString(status.controlsCommandTemplate) << ",\n";
    output << indent << "  \"pairing_command_template\": "
           << jsonString(status.pairingCommandTemplate) << ",\n";
    output << indent << "  \"mp_sync_transport_adapter_kind\": "
           << jsonString(status.mpSyncTransportAdapterKind) << ",\n";
    output << indent << "  \"mp_sync_transport_adapter_path\": "
           << jsonString(pathString(status.mpSyncTransportAdapterPath)) << ",\n";
    output << indent << "  \"mp_sync_envelope_command_template\": "
           << jsonString(status.mpSyncEnvelopeCommandTemplate) << ",\n";
    output << indent << "  \"mp_sync_inbox_plan_command_template\": "
           << jsonString(status.mpSyncInboxPlanCommandTemplate) << ",\n";
    output << indent << "  \"mp_sync_outbox_plan_command_template\": "
           << jsonString(status.mpSyncOutboxPlanCommandTemplate) << ",\n";
    output << indent << "  \"mp_sync_inbox_plan_state\": "
           << jsonString(status.mpSyncInboxPlanState) << ",\n";
    output << indent << "  \"mp_sync_inbox_plan_reason\": "
           << jsonString(status.mpSyncInboxPlanReason) << ",\n";
    output << indent << "  \"mp_sync_inbox_plan_automatic_download_enabled\": "
           << (status.mpSyncInboxAutomaticDownloadEnabled ? "true" : "false") << ",\n";
    output << indent << "  \"mp_sync_inbox_plan_package_staging_allowed\": "
           << (status.mpSyncInboxPackageStagingAllowed ? "true" : "false") << ",\n";
    const auto friendMpSyncTransport = buildFriendMpSyncTransportStatus(status);
    output << indent << "  \"mp_sync_transport_state\": "
           << jsonString(friendMpSyncTransport.state) << ",\n";
    output << indent << "  \"mp_sync_transport_reason\": "
           << jsonString(friendMpSyncTransport.reason) << ",\n";
    output << indent << "  \"mp_sync_transport_next_step\": "
           << jsonString(friendMpSyncTransport.nextStep) << ",\n";
    const auto friendMpSyncTransportAdapter = buildFriendMpSyncTransportAdapterStatus(status);
    output << indent << "  \"mp_sync_transport_adapter_state\": "
           << jsonString(friendMpSyncTransportAdapter.state) << ",\n";
    output << indent << "  \"mp_sync_transport_adapter_reason\": "
           << jsonString(friendMpSyncTransportAdapter.reason) << ",\n";
    output << indent << "  \"mp_sync_transport_adapter_next_step\": "
           << jsonString(friendMpSyncTransportAdapter.nextStep) << ",\n";
    output << indent << "  \"mp_sync_preflight_checklist\": "
           << jsonString(status.mpSyncPreflightChecklist) << ",\n";
    output << indent << "  \"auto_sync_available\": "
           << (status.autoSyncAvailable ? "true" : "false") << "\n";
    output << indent << "}";
}

void writeCrashRecoveryJson(
    std::ostringstream& output,
    const CompanionCrashRecoveryStatus& status,
    const std::string& indent)
{
    output << indent << "{\n";
    output << indent << "  \"state\": " << jsonString(status.state) << ",\n";
    output << indent << "  \"reason\": " << jsonString(status.reason) << ",\n";
    output << indent << "  \"state_path\": " << jsonString(pathString(status.statePath)) << ",\n";
    output << indent << "  \"last_crash_at_local\": " << jsonString(status.lastCrashAtLocal) << ",\n";
    output << indent << "  \"last_recovery_action\": " << jsonString(status.lastRecoveryAction) << ",\n";
    output << indent << "  \"last_operation\": " << jsonString(status.lastOperation) << ",\n";
    output << indent << "  \"app_version\": " << jsonString(status.appVersion) << ",\n";
    output << indent << "  \"recent_unexpected_restart_count\": "
           << status.recentUnexpectedRestartCount << ",\n";
    output << indent << "  \"restart_window_minutes\": " << status.restartWindowMinutes << ",\n";
    output << indent << "  \"next_backoff_seconds\": " << status.nextBackoffSeconds << ",\n";
    output << indent << "  \"restart_budget_exhausted\": "
           << (status.restartBudgetExhausted ? "true" : "false") << ",\n";
    output << indent << "  \"warning_visible\": " << (status.warningVisible ? "true" : "false") << ",\n";
    output << indent << "  \"support_report_recommended\": "
           << (status.supportReportRecommended ? "true" : "false") << "\n";
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
    output << indent << "  \"package_zip_hash\": " << jsonString(status.packageZipHash) << ",\n";
    output << indent << "  \"package_zip_bytes\": " << status.packageZipBytes << ",\n";
    output << indent << "  \"campaign_id\": " << jsonString(status.campaignId) << ",\n";
    output << indent << "  \"overlay_version\": " << jsonString(status.overlayVersion) << ",\n";
    output << indent << "  \"game_version\": " << jsonString(status.gameVersion) << ",\n";
    output << indent << "  \"strategic_nexus_mod_version\": " << jsonString(status.strategicNexusModVersion) << ",\n";
    output << indent << "  \"handoff_status\": " << jsonString(status.handoffStatus) << ",\n";
    output << indent << "  \"previous_host_available\": "
           << (status.previousHostAvailableKnown ? (status.previousHostAvailable ? "true" : "false") : "null") << ",\n";
    output << indent << "  \"previous_host_available_known\": "
           << (status.previousHostAvailableKnown ? "true" : "false") << ",\n";
    output << indent << "  \"readiness\": " << jsonString(status.readiness) << ",\n";
    output << indent << "  \"host_readiness\": " << jsonString(status.hostReadiness) << ",\n";
    output << indent << "  \"client_readiness_gate\": " << jsonString(status.clientReadinessGate) << ",\n";
    output << indent << "  \"host_next_step\": " << jsonString(status.hostNextStep) << ",\n";
    output << indent << "  \"client_next_step\": " << jsonString(status.clientNextStep) << ",\n";
    output << indent << "  \"handoff_recovery_hint\": " << jsonString(status.handoffRecoveryHint) << ",\n";
    output << indent << "  \"host_rotation_sync_state\": " << jsonString(status.hostRotationSyncState) << ",\n";
    output << indent << "  \"host_rotation_sync_reason\": " << jsonString(status.hostRotationSyncReason) << ",\n";
    output << indent << "  \"host_rotation_sync_next_step\": " << jsonString(status.hostRotationSyncNextStep) << ",\n";
    output << indent << "  \"package_manifest_hash\": " << jsonString(status.packageManifestHash) << ",\n";
    output << indent << "  \"provenance_state\": " << jsonString(status.provenanceState) << ",\n";
    output << indent << "  \"human_control_guard_state\": " << jsonString(status.humanControlGuardState) << ",\n";
    output << indent << "  \"source_qualities\": [";
    for (std::size_t index = 0; index < status.sourceQualities.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << jsonString(status.sourceQualities[index]);
    }
    output << "],\n";
    output << indent << "  \"bootstrap_campaign_count\": " << status.bootstrapCampaignCount << ",\n";
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
    output << indent << "  \"memory_recovery\": {\n";
    output << indent << "    \"state\": " << jsonString(status.memoryRecovery.state) << ",\n";
    output << indent << "    \"reason\": " << jsonString(status.memoryRecovery.reason) << ",\n";
    output << indent << "    \"confidence\": " << jsonString(status.memoryRecovery.confidence) << ",\n";
    output << indent << "    \"warning_visible\": "
           << (status.memoryRecovery.warningVisible ? "true" : "false") << ",\n";
    output << indent << "    \"anchor_entry_point_id\": "
           << jsonString(status.memoryRecovery.anchorEntryPointId) << ",\n";
    output << indent << "    \"anchor_campaign_key\": "
           << jsonString(status.memoryRecovery.anchorCampaignKey) << ",\n";
    output << indent << "    \"anchor_path\": " << jsonString(pathString(status.memoryRecovery.anchorPath)) << ",\n";
    output << indent << "    \"anchor_save_name\": " << jsonString(status.memoryRecovery.anchorSaveName) << ",\n";
    output << indent << "    \"anchor_save_date\": " << jsonString(status.memoryRecovery.anchorSaveDate) << ",\n";
    output << indent << "    \"anchor_source_kind\": "
           << jsonString(status.memoryRecovery.anchorSourceKind) << ",\n";
    output << indent << "    \"compatible_archived_evidence_count\": "
           << status.memoryRecovery.compatibleArchivedEvidenceCount << ",\n";
    output << indent << "    \"later_archived_evidence_count\": "
           << status.memoryRecovery.laterArchivedEvidenceCount << "\n";
    output << indent << "  },\n";
    output << indent << "  \"post_play_package_path\": " << jsonString(pathString(status.postPlayPackagePath)) << ",\n";
    output << indent << "  \"post_play_package_readiness\": " << jsonString(status.postPlayPackageReadiness) << ",\n";
    output << indent << "  \"post_play_package_reason\": " << jsonString(status.postPlayPackageReason) << ",\n";
    output << indent << "  \"post_play_package_campaign_identity_state_summary\": "
           << jsonString(status.postPlayPackageCampaignIdentityStateSummary) << ",\n";
    output << indent << "  \"post_play_package_personality_profile_prompt_output_note\": "
           << jsonString(status.postPlayPackagePersonalityProfilePromptOutputNote) << ",\n";
    output << indent << "  \"post_play_player_country_id\": " << jsonString(status.playerCountryId) << ",\n";
    output << indent << "  \"post_play_decision_ready_entry_count\": " << status.postPlayDecisionReadyEntryCount << ",\n";
    output << indent << "  \"post_play_campaign_count\": " << status.postPlayCampaignCount << ",\n";
    output << indent << "  \"post_play_ready_campaign_count\": " << status.postPlayReadyCampaignCount << ",\n";
    output << indent << "  \"post_play_partial_campaign_count\": " << status.postPlayPartialCampaignCount << ",\n";
    output << indent << "  \"post_play_blocked_campaign_count\": " << status.postPlayBlockedCampaignCount << ",\n";
    output << indent << "  \"campaign_library_plan_path\": "
           << jsonString(pathString(status.campaignLibraryPlanPath)) << ",\n";
    output << indent << "  \"campaign_library_plan_present\": "
           << (status.campaignLibraryPlanPresent ? "true" : "false") << ",\n";
    output << indent << "  \"campaign_library_limit_reached\": "
           << (status.campaignLibraryLimitReached ? "true" : "false") << ",\n";
    output << indent << "  \"campaign_library_skipped_due_to_limit_count\": "
           << status.campaignLibrarySkippedDueToLimitCount << ",\n";
    output << indent << "  \"campaign_library_pinned_count\": " << status.campaignLibraryPinnedCount << ",\n";
    output << indent << "  \"campaign_library_pinned_missing_local_save_count\": "
           << status.campaignLibraryPinnedMissingLocalSaveCount << ",\n";
    output << indent << "  \"campaign_library_pin_path\": "
           << jsonString(pathString(status.campaignLibraryPinPath)) << ",\n";
    output << indent << "  \"campaign_library_pin_present\": "
           << (status.campaignLibraryPinPresent ? "true" : "false") << ",\n";
    output << indent << "  \"campaign_library_pin_state\": "
           << jsonString(status.campaignLibraryPinState) << ",\n";
    output << indent << "  \"campaign_library_pin_reason\": "
           << jsonString(status.campaignLibraryPinReason) << ",\n";
    output << indent << "  \"campaign_library_pin_next_step\": "
           << jsonString(status.campaignLibraryPinNextStep) << ",\n";
    output << indent << "  \"campaign_library_pin_command_template\": "
           << jsonString(status.campaignLibraryPinCommandTemplate) << ",\n";
    output << indent << "  \"campaign_library_plan_source\": "
           << jsonString(status.campaignLibraryPlanSource) << ",\n";
    output << indent << "  \"campaign_library_plan_readiness\": "
           << jsonString(status.campaignLibraryPlanReadiness) << ",\n";
    output << indent << "  \"campaign_library_plan_reason\": "
           << jsonString(status.campaignLibraryPlanReason) << ",\n";
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

CompanionFriendMpSyncTransportStatus buildFriendMpSyncTransportStatus(
    const CompanionFriendTrustStoreStatus& friendTrustStore)
{
    return buildFriendMpSyncTransportStatusImpl(friendTrustStore);
}

CompanionFriendMpSyncTransportAdapterStatus buildFriendMpSyncTransportAdapterStatus(
    const CompanionFriendTrustStoreStatus& friendTrustStore)
{
    return buildFriendMpSyncTransportAdapterStatusImpl(friendTrustStore);
}

CompanionFriendMeshUpdateStatus buildFriendMeshUpdateStatus(
    const CompanionFriendTrustStoreStatus& friendTrustStore,
    const CompanionMpOverlayPackageStatus& mpOverlayPackage)
{
    return buildFriendMeshUpdateStatusImpl(friendTrustStore, mpOverlayPackage);
}

std::string buildCampaignLibraryPinCommandTemplate(const std::filesystem::path& pinPath)
{
    if (pinPath.empty()) {
        return "Strategic Nexus.exe --toggle-campaign-library-pin <manifest> <campaign_key> pin|unpin";
    }

    return "Strategic Nexus.exe --toggle-campaign-library-pin \"" + pathString(pinPath) +
        "\" <campaign_key> pin|unpin";
}

CompanionStatusSnapshot StrategicNexusCompanion::buildStatusSnapshot(const CompanionStatusConfig& config) const
{
    CompanionStatusSnapshot snapshot;
    snapshot.lifecycle = buildLifecycleStatus(config);
    snapshot.supportReport = buildSupportReportStatus(config);
    snapshot.crashRecovery = buildCrashRecoveryStatus(config);
    snapshot.generatedAtLocal = formatLocalTimestamp(std::chrono::system_clock::now());
    snapshot.saveDiscovery = buildSaveDiscoveryStatus();
    snapshot.archive = buildArchiveStatus(config.archiveRoot);
    snapshot.generatedOverlay = buildGeneratedOverlayStatus(config.generatedOverlayDirectory);
    snapshot.generatedOverlayPublishGate = buildGeneratedOverlayPublishGateStatus(snapshot.generatedOverlay, config);
    snapshot.mpOverlayPackage = buildMpOverlayPackageStatus(config.mpOverlayPackageDirectory);
    const auto effectiveMpPackageZipPath =
        resolveMpOverlayPackageZipPath(snapshot.mpOverlayPackage, config.mpOverlayPackageZipPath);
    if (snapshot.mpOverlayPackage.state == "ready" && !effectiveMpPackageZipPath.empty()) {
        const generated_overlay::MpOverlayPackageExporter exporter;
        const auto zipExportResult = exporter.exportPackageZip(snapshot.mpOverlayPackage.path, effectiveMpPackageZipPath);
        if (zipExportResult.ok) {
            snapshot.mpOverlayPackage.packageZipPath = zipExportResult.packageZipPath;
            snapshot.mpOverlayPackage.packageZipHash = zipExportResult.packageZipHash;
            snapshot.mpOverlayPackage.packageZipBytes = zipExportResult.packageZipBytes;
            snapshot.mpOverlayPackage.packageZipState = "ready";
            snapshot.mpOverlayPackage.packageZipReason = "mp overlay package zip ready for handoff";
        } else {
            snapshot.mpOverlayPackage.packageZipPath = zipExportResult.packageZipPath;
            snapshot.mpOverlayPackage.packageZipState = "needs_attention";
            snapshot.mpOverlayPackage.packageZipReason = zipExportResult.reason.empty()
                ? "mp overlay package zip export failed"
                : zipExportResult.reason;
            snapshot.mpOverlayPackage.state = "needs_attention";
            snapshot.mpOverlayPackage.reason = snapshot.mpOverlayPackage.packageZipReason;
        }
    } else {
        populateMpOverlayPackageZipStatus(snapshot.mpOverlayPackage, effectiveMpPackageZipPath);
    }
    snapshot.postPlayPipeline = buildPostPlayPipelineStatus(config);
    snapshot.campaignLibraryPinnedCount = snapshot.postPlayPipeline.campaignLibraryPinnedCount;
    snapshot.campaignLibraryPinnedMissingLocalSaveCount =
        snapshot.postPlayPipeline.campaignLibraryPinnedMissingLocalSaveCount;
    snapshot.campaignLibraryPinPath = snapshot.postPlayPipeline.campaignLibraryPinPath;
    snapshot.campaignLibraryPinPresent = snapshot.postPlayPipeline.campaignLibraryPinPresent;
    snapshot.campaignLibraryPinState = snapshot.postPlayPipeline.campaignLibraryPinState;
    snapshot.campaignLibraryPinReason = snapshot.postPlayPipeline.campaignLibraryPinReason;
    snapshot.campaignLibraryPinNextStep = snapshot.postPlayPipeline.campaignLibraryPinNextStep;
    snapshot.campaignLibraryPinCommandTemplate = snapshot.postPlayPipeline.campaignLibraryPinCommandTemplate;
    snapshot.gameplayAcceptance = buildGameplayAcceptanceStatus(config.gameplayAcceptanceReportPath);
    snapshot.localLlm = buildLocalLlmStatus(config);
    snapshot.friendTrustStore = buildFriendTrustStoreStatus(config);
    snapshot.statusCenter =
        buildStatusCenterStatus(
            snapshot.saveDiscovery,
            snapshot.archive,
            snapshot.generatedOverlay,
            snapshot.generatedOverlayPublishGate,
            snapshot.mpOverlayPackage,
            snapshot.postPlayPipeline,
            snapshot.gameplayAcceptance,
            snapshot.supportReport,
            snapshot.crashRecovery,
            snapshot.localLlm,
            snapshot.friendTrustStore);
    snapshot.nextAction = buildCompanionNextAction(snapshot);
    snapshot.nextActionReason = buildCompanionNextActionReason(snapshot);
    snapshot.nextActionCommandHint = buildCompanionNextActionCommandHint(snapshot);
    snapshot.nextActionCommandHintSource = buildCompanionNextActionCommandHintSource(snapshot);
    snapshot.nextActionPath = buildCompanionNextActionPath(snapshot);
    snapshot.ownerTestPlaybookPath = buildCompanionOwnerTestPlaybookPath(snapshot);
    snapshot.statusUiStatePath = config.statusUiStatePath;
    snapshot.statusCenterSummaryText = buildStatusCenterSummaryText(
        snapshot.generatedAtLocal,
        snapshot.lifecycle,
        snapshot.supportReport,
        snapshot.crashRecovery,
        snapshot.saveDiscovery,
        snapshot.archive,
        snapshot.generatedOverlay,
        snapshot.generatedOverlayPublishGate,
        snapshot.mpOverlayPackage,
        snapshot.postPlayPipeline,
        snapshot.gameplayAcceptance,
        snapshot.localLlm,
        snapshot.friendTrustStore,
        snapshot.statusCenter,
        snapshot.nextAction,
        snapshot.nextActionReason,
        snapshot.nextActionCommandHint,
        snapshot.nextActionCommandHintSource,
        snapshot.nextActionPath,
        snapshot.statusUiStatePath);
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
    output << "    \"startup_rationale\": "
           << jsonString(snapshot.lifecycle.startupRationale) << ",\n";
    output << "    \"start_with_windows_default_state\": "
           << jsonString(snapshot.lifecycle.startWithWindowsDefaultState) << ",\n";
    output << "    \"start_with_windows_source\": "
           << jsonString(snapshot.lifecycle.startWithWindowsSource) << ",\n";
    output << "    \"start_with_windows_shortcut_state\": "
           << jsonString(snapshot.lifecycle.startWithWindowsShortcutState) << ",\n";
    output << "    \"start_with_windows_shortcut_path\": "
           << jsonString(pathString(snapshot.lifecycle.startWithWindowsShortcutPath)) << ",\n";
    output << "    \"start_with_windows_command_hint\": "
           << jsonString(snapshot.lifecycle.startWithWindowsCommandHint) << ",\n";
    output << "    \"start_with_windows_command_hint_source\": "
           << jsonString(snapshot.lifecycle.startWithWindowsCommandHintSource) << ",\n";
    output << "    \"start_with_windows_enable_command_hint\": "
           << jsonString(snapshot.lifecycle.startWithWindowsEnableCommandHint) << ",\n";
    output << "    \"start_with_windows_disable_command_hint\": "
           << jsonString(snapshot.lifecycle.startWithWindowsDisableCommandHint) << ",\n";
    output << "    \"window_close_behavior\": "
           << jsonString(snapshot.lifecycle.windowCloseBehavior) << ",\n";
    output << "    \"explicit_exit_behavior\": "
           << jsonString(snapshot.lifecycle.explicitExitBehavior) << ",\n";
    output << "    \"crash_restart_policy\": "
           << jsonString(snapshot.lifecycle.crashRestartPolicy) << "\n";
    output << "  },\n";
    output << "  \"startup_lifecycle_state\": "
           << jsonString(
                  snapshot.lifecycle.startWithWindowsEnabled
                      ? "owner_enabled_start_with_windows"
                      : "manual_start_only")
           << ",\n";
    output << "  \"startup_rationale\": "
           << jsonString(snapshot.lifecycle.startupRationale) << ",\n";
    output << "  \"start_with_windows_default_state\": "
           << jsonString(snapshot.lifecycle.startWithWindowsDefaultState) << ",\n";
    output << "  \"support_report_state\": "
           << jsonString(snapshot.supportReport.state) << ",\n";
    output << "  \"support_report_reason\": "
           << jsonString(snapshot.supportReport.reason) << ",\n";
    output << "  \"support_report_preview_path\": "
           << jsonString(pathString(snapshot.supportReport.previewPath)) << ",\n";
    output << "  \"support_report_contact_email\": "
           << jsonString(snapshot.supportReport.supportContactEmail) << ",\n";
    output << "  \"support_report_send_requires_approval\": "
           << (snapshot.supportReport.sendRequiresApproval ? "true" : "false") << ",\n";
    output << "  \"support_report_raw_saves_included\": "
           << (snapshot.supportReport.rawSavesIncluded ? "true" : "false") << ",\n";
    output << "  \"support_report_data_categories\": [";
    for (std::size_t index = 0; index < snapshot.supportReport.dataCategories.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << jsonString(snapshot.supportReport.dataCategories[index]);
    }
    output << "],\n";
    output << "  \"support_report_prepare_command_hint\": "
           << jsonString(snapshot.supportReport.prepareCommandHint) << ",\n";
    output << "  \"support_report_open_command_hint\": "
           << jsonString(snapshot.supportReport.openCommandHint) << ",\n";
    output << "  \"crash_recovery_state\": "
           << jsonString(snapshot.crashRecovery.state) << ",\n";
    output << "  \"crash_recovery_reason\": "
           << jsonString(snapshot.crashRecovery.reason) << ",\n";
    output << "  \"crash_recovery_state_path\": "
           << jsonString(pathString(snapshot.crashRecovery.statePath)) << ",\n";
    output << "  \"crash_recovery_last_crash_at_local\": "
           << jsonString(snapshot.crashRecovery.lastCrashAtLocal) << ",\n";
    output << "  \"crash_recovery_last_recovery_action\": "
           << jsonString(snapshot.crashRecovery.lastRecoveryAction) << ",\n";
    output << "  \"crash_recovery_last_operation\": "
           << jsonString(snapshot.crashRecovery.lastOperation) << ",\n";
    output << "  \"crash_recovery_app_version\": "
           << jsonString(snapshot.crashRecovery.appVersion) << ",\n";
    output << "  \"crash_recovery_recent_unexpected_restart_count\": "
           << snapshot.crashRecovery.recentUnexpectedRestartCount << ",\n";
    output << "  \"crash_recovery_restart_window_minutes\": "
           << snapshot.crashRecovery.restartWindowMinutes << ",\n";
    output << "  \"crash_recovery_next_backoff_seconds\": "
           << snapshot.crashRecovery.nextBackoffSeconds << ",\n";
    output << "  \"crash_recovery_restart_budget_exhausted\": "
           << (snapshot.crashRecovery.restartBudgetExhausted ? "true" : "false") << ",\n";
    output << "  \"crash_recovery_warning_visible\": "
           << (snapshot.crashRecovery.warningVisible ? "true" : "false") << ",\n";
    output << "  \"crash_recovery_support_report_recommended\": "
           << (snapshot.crashRecovery.supportReportRecommended ? "true" : "false") << ",\n";
    output << "  \"start_with_windows_source\": "
           << jsonString(snapshot.lifecycle.startWithWindowsSource) << ",\n";
    output << "  \"start_with_windows_shortcut_state\": "
           << jsonString(snapshot.lifecycle.startWithWindowsShortcutState) << ",\n";
    output << "  \"start_with_windows_shortcut_path\": "
           << jsonString(pathString(snapshot.lifecycle.startWithWindowsShortcutPath)) << ",\n";
    output << "  \"start_with_windows_command_hint\": "
           << jsonString(snapshot.lifecycle.startWithWindowsCommandHint) << ",\n";
    output << "  \"start_with_windows_command_hint_source\": "
           << jsonString(snapshot.lifecycle.startWithWindowsCommandHintSource) << ",\n";
    output << "  \"start_with_windows_enable_command_hint\": "
           << jsonString(snapshot.lifecycle.startWithWindowsEnableCommandHint) << ",\n";
    output << "  \"start_with_windows_disable_command_hint\": "
           << jsonString(snapshot.lifecycle.startWithWindowsDisableCommandHint) << ",\n";
    output << "  \"archive_status\": ";
    writeSubsystemJson(output, snapshot.archive, "  ");
    output << ",\n";
    output << "  \"save_discovery_status\": ";
    writeSubsystemJson(output, snapshot.saveDiscovery, "  ");
    output << ",\n";
    output << "  \"generated_overlay_status\": ";
    writeGeneratedOverlayJson(output, snapshot.generatedOverlay, "  ");
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
    output << "  \"campaign_library_pinned_count\": " << snapshot.campaignLibraryPinnedCount << ",\n";
    output << "  \"campaign_library_pinned_missing_local_save_count\": "
           << snapshot.campaignLibraryPinnedMissingLocalSaveCount << ",\n";
    output << "  \"campaign_library_pin_path\": " << jsonString(pathString(snapshot.campaignLibraryPinPath)) << ",\n";
    output << "  \"campaign_library_pin_present\": "
           << (snapshot.campaignLibraryPinPresent ? "true" : "false") << ",\n";
    output << "  \"campaign_library_pin_state\": " << jsonString(snapshot.campaignLibraryPinState) << ",\n";
    output << "  \"campaign_library_pin_reason\": " << jsonString(snapshot.campaignLibraryPinReason) << ",\n";
    output << "  \"campaign_library_pin_next_step\": " << jsonString(snapshot.campaignLibraryPinNextStep) << ",\n";
    output << "  \"campaign_library_pin_command_template\": "
           << jsonString(snapshot.campaignLibraryPinCommandTemplate) << ",\n";
    output << "  \"gameplay_acceptance_status\": ";
    writeSubsystemJson(output, snapshot.gameplayAcceptance, "  ");
    output << ",\n";
    output << "  \"local_llm_model_status\": ";
    writeLocalLlmJson(output, snapshot.localLlm, "  ");
    output << ",\n";
    output << "  \"friend_trust_store_status\": ";
    writeFriendTrustStoreJson(output, snapshot.friendTrustStore, "  ");
    output << ",\n";
    if (snapshot.friendTrustStore.schemaCompatibilityKnown) {
        output << "  \"friend_trust_store_source_schema_version\": "
               << snapshot.friendTrustStore.sourceSchemaVersion << ",\n";
        output << "  \"friend_trust_store_schema_compatibility_state\": "
               << jsonString(snapshot.friendTrustStore.schemaCompatibilityState) << ",\n";
        output << "  \"friend_trust_store_schema_compatibility_note\": "
               << jsonString(snapshot.friendTrustStore.schemaCompatibilityNote) << ",\n";
    }
    output << "  \"friend_trust_store_controls_state\": "
           << jsonString(snapshot.friendTrustStore.controlsState) << ",\n";
    output << "  \"friend_trust_store_controls_reason\": "
           << jsonString(snapshot.friendTrustStore.controlsReason) << ",\n";
    output << "  \"friend_trust_store_controls_next_step\": "
           << jsonString(snapshot.friendTrustStore.controlsNextStep) << ",\n";
    output << "  \"friend_trust_store_update_command_template\": "
           << jsonString(snapshot.friendTrustStore.controlsCommandTemplate) << ",\n";
    const auto friendMeshUpdate = buildFriendMeshUpdateStatus(snapshot.friendTrustStore, snapshot.mpOverlayPackage);
    output << "  \"friend_mesh_update_state\": " << jsonString(friendMeshUpdate.state) << ",\n";
    output << "  \"friend_mesh_update_reason\": " << jsonString(friendMeshUpdate.reason) << ",\n";
    output << "  \"friend_mesh_update_next_step\": " << jsonString(friendMeshUpdate.nextStep) << ",\n";
    output << "  \"crash_recovery\": ";
    writeCrashRecoveryJson(output, snapshot.crashRecovery, "  ");
    output << ",\n";
    output << "  \"status_center\": ";
    writeSubsystemJson(output, snapshot.statusCenter, "  ");
    output << ",\n";
    output << "  \"next_action\": " << jsonString(snapshot.nextAction) << ",\n";
    output << "  \"next_action_reason\": " << jsonString(snapshot.nextActionReason) << ",\n";
    output << "  \"next_action_command_hint\": " << jsonString(snapshot.nextActionCommandHint) << ",\n";
    output << "  \"next_action_command_hint_source\": " << jsonString(snapshot.nextActionCommandHintSource) << ",\n";
    output << "  \"next_action_path\": " << jsonString(pathString(snapshot.nextActionPath)) << ",\n";
    output << "  \"owner_test_playbook_path\": " << jsonString(pathString(snapshot.ownerTestPlaybookPath)) << ",\n";
    output << "  \"status_ui_state_path\": " << jsonString(pathString(snapshot.statusUiStatePath)) << ",\n";
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
