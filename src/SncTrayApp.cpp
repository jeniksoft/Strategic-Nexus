// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "AutosaveArchiveSummarizer.h"
#include "AutosaveArchiveVerifier.h"
#include "AutosaveArchiver.h"
#include "PostPlayPackageBuilder.h"
#include "SaveEntryPointAnalyzer.h"
#include "SncCandidateDecisionPackageBuilder.h"
#include "SncDecisionInputPackageBuilder.h"
#include "SncDslDraftPackageBuilder.h"
#include "SncGeneratedOverlayPublishGate.h"
#include "SncGeneratedOverlayStager.h"
#include "StrategicNexusCompanion.h"
#include "StellarisProcessDetector.h"
#include "StellarisSavePathResolver.h"
#include "common/FileUtil.h"
#include "common/JsonExtract.h"
#include "generated_overlay/MpOverlayPackage.h"

#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

constexpr UINT WM_SNC_TRAY = WM_APP + 10;
constexpr UINT WM_SNC_TASKBAR_CREATED = WM_APP + 11;
constexpr UINT ID_TRAY_STATUS = 101;
constexpr UINT ID_TRAY_OPEN_ARCHIVE = 102;
constexpr UINT ID_TRAY_EXIT = 103;
constexpr UINT ID_TRAY_PUBLISH_GENERATED_OVERLAY = 104;

std::atomic_bool g_stopRequested{false};
std::thread g_worker;
std::mutex g_statusMutex;
std::wstring g_statusTitle = L"Strategic Nexus Companion";
std::wstring g_statusText = L"SNC startuje.";
std::filesystem::path g_repoRoot;
std::filesystem::path g_archiveRoot;
std::filesystem::path g_trayStatusPath;
std::filesystem::path g_liveStatusPath;
std::filesystem::path g_lastSummaryPath;
std::filesystem::path g_entryPointAnalysisPath;
std::filesystem::path g_postPlayPackagePath;
std::filesystem::path g_decisionInputPackagePath;
std::filesystem::path g_candidateDecisionPackagePath;
std::filesystem::path g_dslDraftPath;
std::filesystem::path g_dslDraftAuditPath;
std::filesystem::path g_nextStepsBriefPath;
std::filesystem::path g_generatedOverlayStagingDirectory;
std::filesystem::path g_generatedOverlayStagingStatusPath;
std::filesystem::path g_generatedOverlayActiveDirectory;
std::filesystem::path g_generatedOverlayPublishStatusPath;
std::filesystem::path g_generatedOverlayPublishBackupRootDirectory;
std::filesystem::path g_mpOverlayPackageDirectory;
std::filesystem::path g_mpOverlayPackageZipPath;
constexpr const char* kTrayMpOverlayVersion = "overlay_v0_session";
constexpr const char* kTrayMpGameVersion = "stellaris_4.x";
constexpr const char* kTrayMpModVersion = "strategic_nexus_v0";
NOTIFYICONDATAW g_trayIcon{};
UINT g_taskbarCreatedMessage = 0;

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const char ch : value) {
        switch (ch) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
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

std::string pathString(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::string formatLocalTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (localtime_s(&localTime, &timeValue) != 0) {
        return "unknown-time";
    }

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%d.%m.%Y  %H:%M:%S", &localTime);
    return buffer;
}

std::string formatSessionToken()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (localtime_s(&localTime, &timeValue) != 0) {
        return "unknown-time";
    }

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%S", &localTime);
    return buffer;
}

std::string safeIdentifier(const std::string& value, const std::string& fallback)
{
    std::string output;
    output.reserve(value.size());
    bool lastWasSeparator = false;

    for (const unsigned char raw : value) {
        if (std::isalnum(raw) != 0) {
            output.push_back(static_cast<char>(std::tolower(raw)));
            lastWasSeparator = false;
            continue;
        }
        if (!lastWasSeparator && !output.empty()) {
            output.push_back('_');
            lastWasSeparator = true;
        }
    }

    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }
    if (output.empty()) {
        output = fallback;
    }
    if (std::isdigit(static_cast<unsigned char>(output.front())) != 0) {
        output = fallback + "_" + output;
    }
    return output;
}

std::optional<std::string> extractArrayBody(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*\\[");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }

    const std::size_t start = static_cast<std::size_t>(match.position()) + match.length();
    int depth = 1;
    bool inString = false;
    bool escaped = false;

    for (std::size_t index = start; index < json.size(); ++index) {
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
                return json.substr(start, index - start);
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string> extractObjectBodies(const std::string& arrayBody)
{
    std::vector<std::string> objects;
    bool inString = false;
    bool escaped = false;
    int depth = 0;
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
        } else if (ch == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
        } else if (ch == '}') {
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
    } catch (...) {
        return std::nullopt;
    }
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

void parsePostPlayCampaignSummaries(
    const std::string& json,
    std::size_t& campaignCount,
    std::size_t& readyCampaignCount,
    std::size_t& partialCampaignCount,
    std::size_t& blockedCampaignCount,
    std::vector<std::string>& campaignSummaries)
{
    campaignCount = 0;
    readyCampaignCount = 0;
    partialCampaignCount = 0;
    blockedCampaignCount = 0;
    campaignSummaries.clear();

    const auto campaignsBody = extractArrayBody(json, "campaigns");
    if (!campaignsBody.has_value()) {
        return;
    }

    for (const auto& object : extractObjectBodies(*campaignsBody)) {
        const auto campaignKey = strategic_nexus::common::extractJsonString(object, "campaign_key").value_or("");
        const auto readiness = strategic_nexus::common::extractJsonString(object, "readiness").value_or("");
        const auto entryPointCount = extractJsonSize(object, "entry_point_count").value_or(0);
        const auto decisionReadyEntryCount = extractJsonSize(object, "decision_ready_entry_count").value_or(0);
        const auto branchAmbiguityDetected = extractJsonBool(object, "branch_ambiguity_detected").value_or(false);

        if (campaignKey.empty() && readiness.empty() && entryPointCount == 0 && decisionReadyEntryCount == 0 &&
            !branchAmbiguityDetected) {
            continue;
        }

        ++campaignCount;
        if (readiness.rfind("ready_partial", 0) == 0) {
            ++partialCampaignCount;
        } else if (readiness.rfind("ready", 0) == 0) {
            ++readyCampaignCount;
        } else {
            ++blockedCampaignCount;
        }

        std::ostringstream summary;
        summary << (campaignKey.empty() ? "unknown_campaign" : campaignKey)
                << ": " << (readiness.empty() ? "unknown" : readiness)
                << " (" << decisionReadyEntryCount << "/" << entryPointCount << " ready";
        if (branchAmbiguityDetected) {
            summary << ", ambiguous";
        }
        summary << ")";
        campaignSummaries.push_back(summary.str());
    }
}

void writeStringArrayJson(std::ostringstream& json, const std::vector<std::string>& values)
{
    json << "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            json << ", ";
        }
        json << "\"" << jsonEscape(values[index]) << "\"";
    }
    json << "]";
}

void appendMpPackageSummaryLines(
    std::ostringstream& output,
    const strategic_nexus::CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const std::string& mpPackageRefreshState,
    const std::string& mpPackageRefreshReason)
{
    output << "mp_package_refresh_state: " << mpPackageRefreshState << "\n";
    output << "mp_package_refresh_reason: " << mpPackageRefreshReason << "\n";
    output << "mp_overlay_package_directory: " << pathString(g_mpOverlayPackageDirectory) << "\n";
    output << "mp_overlay_package_zip_path: " << pathString(g_mpOverlayPackageZipPath) << "\n";
    output << "mp_overlay_package_state: " << mpOverlayPackage.state << "\n";
    output << "mp_overlay_package_reason: " << mpOverlayPackage.reason << "\n";
    if (!mpOverlayPackage.path.empty()) {
        output << "mp_overlay_package_path: " << pathString(mpOverlayPackage.path) << "\n";
    }
    if (!mpOverlayPackage.campaignId.empty()) {
        output << "mp_campaign_id: " << mpOverlayPackage.campaignId << "\n";
    }
    if (!mpOverlayPackage.overlayVersion.empty()) {
        output << "mp_overlay_version: " << mpOverlayPackage.overlayVersion << "\n";
    }
    if (!mpOverlayPackage.gameVersion.empty()) {
        output << "mp_game_version: " << mpOverlayPackage.gameVersion << "\n";
    }
    if (!mpOverlayPackage.strategicNexusModVersion.empty()) {
        output << "mp_mod_version: " << mpOverlayPackage.strategicNexusModVersion << "\n";
    }
    if (!mpOverlayPackage.handoffStatus.empty()) {
        output << "mp_handoff_status: " << mpOverlayPackage.handoffStatus << "\n";
    }
    if (!mpOverlayPackage.readiness.empty()) {
        output << "mp_readiness: " << mpOverlayPackage.readiness << "\n";
    }
    if (!mpOverlayPackage.hostReadiness.empty()) {
        output << "mp_host_readiness: " << mpOverlayPackage.hostReadiness << "\n";
    }
    if (!mpOverlayPackage.clientReadinessGate.empty()) {
        output << "mp_client_readiness_gate: " << mpOverlayPackage.clientReadinessGate << "\n";
    }
    if (!mpOverlayPackage.hostNextStep.empty()) {
        output << "mp_host_next_step: " << mpOverlayPackage.hostNextStep << "\n";
    }
    if (!mpOverlayPackage.clientNextStep.empty()) {
        output << "mp_client_next_step: " << mpOverlayPackage.clientNextStep << "\n";
    }
    if (!mpOverlayPackage.packageManifestHash.empty()) {
        output << "mp_package_manifest_hash: " << mpOverlayPackage.packageManifestHash << "\n";
    }
    if (!mpOverlayPackage.verifyCommand.empty()) {
        output << "mp_verify_command: " << mpOverlayPackage.verifyCommand << "\n";
    }
    if (!mpOverlayPackage.importCommand.empty()) {
        output << "mp_import_command: " << mpOverlayPackage.importCommand << "\n";
    }
    if (!mpOverlayPackage.strictVerifyCommand.empty()) {
        output << "mp_strict_verify_command: " << mpOverlayPackage.strictVerifyCommand << "\n";
    }
    if (!mpOverlayPackage.strictImportCommand.empty()) {
        output << "mp_strict_import_command: " << mpOverlayPackage.strictImportCommand << "\n";
    }
    output << "mp_warning_count: " << mpOverlayPackage.warningCount << "\n";
    for (const auto& warningCode : mpOverlayPackage.warningCodes) {
        output << "mp_warning_code: " << warningCode << "\n";
    }
    output << "mp_identity_mismatch_warning: "
           << (mpOverlayPackage.identityMismatchWarning ? "true" : "false") << "\n";
    for (const auto& warningCode : mpOverlayPackage.identityMismatchWarningCodes) {
        output << "mp_identity_mismatch_warning_code: " << warningCode << "\n";
    }
    output << "mp_mismatch_warning_state: " << mpOverlayPackage.mismatchWarningState << "\n";
    output << "mp_mismatch_warning_reason: " << mpOverlayPackage.mismatchWarningReason << "\n";
    for (const auto& warningCode : mpOverlayPackage.mismatchWarningCodes) {
        output << "mp_mismatch_warning_code: " << warningCode << "\n";
    }
    if (!mpOverlayPackage.identityMismatchAlert.empty()) {
        output << "mp_identity_mismatch_alert: " << mpOverlayPackage.identityMismatchAlert << "\n";
    }
    if (!mpOverlayPackage.packageZipState.empty()) {
        output << "mp_package_zip_state: " << mpOverlayPackage.packageZipState << "\n";
        output << "mp_package_zip_bytes: " << mpOverlayPackage.packageZipBytes << "\n";
    }
    if (!mpOverlayPackage.packageZipReason.empty()) {
        output << "mp_package_zip_reason: " << mpOverlayPackage.packageZipReason << "\n";
    }
    if (!mpOverlayPackage.packageZipPath.empty()) {
        output << "mp_package_zip_runtime_path: " << pathString(mpOverlayPackage.packageZipPath) << "\n";
    }
}

std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty()) {
        return std::wstring();
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 1) {
        return std::wstring();
    }

    std::wstring output(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, output.data(), required);
    if (!output.empty() && output.back() == L'\0') {
        output.pop_back();
    }
    return output;
}

std::filesystem::path modulePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    buffer.resize(length);
    return std::filesystem::path(buffer);
}

bool looksLikeRepoRoot(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_directory(path / "src", error) &&
        std::filesystem::is_directory(path / "docs", error);
}

std::filesystem::path findRepoRoot()
{
    auto current = modulePath().parent_path();
    for (int index = 0; index < 8 && !current.empty(); ++index) {
        if (looksLikeRepoRoot(current)) {
            return current;
        }
        current = current.parent_path();
    }
    return std::filesystem::current_path();
}

bool existingDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error) && std::filesystem::is_directory(path, error);
}

void sleepResponsive(const std::chrono::milliseconds total)
{
    constexpr auto step = std::chrono::milliseconds(100);
    auto remaining = total;
    while (!g_stopRequested.load() && remaining.count() > 0) {
        const auto chunk = remaining < step ? remaining : step;
        std::this_thread::sleep_for(chunk);
        remaining -= chunk;
    }
}

void updateTrayTip(HWND hwnd, const std::wstring& text)
{
    if (g_trayIcon.cbSize == 0) {
        return;
    }

    std::wstring tip = L"SNC - " + text;
    if (tip.size() >= sizeof(g_trayIcon.szTip) / sizeof(g_trayIcon.szTip[0])) {
        tip.resize((sizeof(g_trayIcon.szTip) / sizeof(g_trayIcon.szTip[0])) - 1);
    }
    wcscpy_s(g_trayIcon.szTip, tip.c_str());
    g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayIcon.hWnd = hwnd;
    Shell_NotifyIconW(NIM_MODIFY, &g_trayIcon);
}

std::string buildNextAction(
    const std::string& state,
    const std::string& postPlayState,
    const bool generatedOverlayPublishAllowed,
    const std::string& generatedOverlayStagingReadiness,
    const std::string& dslDraftReadiness,
    const std::string& candidateDecisionPackageReadiness)
{
    if (state == "capturing") {
        return "keep_playing_capture_active";
    }
    if (state == "waiting_for_stellaris") {
        return "wait_for_stellaris_launch";
    }
    if (generatedOverlayPublishAllowed) {
        return "review_staged_overlay_and_publish_if_desired";
    }
    if (generatedOverlayStagingReadiness.rfind("ready", 0) == 0) {
        return "review_staged_overlay_status";
    }
    if (dslDraftReadiness.rfind("ready", 0) == 0) {
        return "review_dsl_draft";
    }
    if (candidateDecisionPackageReadiness.rfind("ready", 0) == 0) {
        return "review_candidate_decision_package";
    }
    if (postPlayState == "entry_points_ambiguous") {
        return "review_entry_point_ambiguity";
    }
    if (postPlayState == "entry_point_analysis_blocked") {
        return "review_entry_point_analysis_failure";
    }
    return "review_tray_status";
}

std::string buildNextActionReason(
    const std::string& state,
    const std::string& postPlayState,
    const bool generatedOverlayPublishAllowed,
    const std::string& generatedOverlayStagingReadiness,
    const std::string& dslDraftReadiness,
    const std::string& candidateDecisionPackageReadiness)
{
    if (state == "capturing") {
        return "stellaris_running_capture_window_active";
    }
    if (state == "waiting_for_stellaris") {
        return "stellaris_not_running";
    }
    if (generatedOverlayPublishAllowed) {
        return "staged_overlay_ready_owner_gate_available";
    }
    if (generatedOverlayStagingReadiness.rfind("ready", 0) == 0) {
        return "staged_overlay_written_but_publish_gate_not_ready";
    }
    if (dslDraftReadiness.rfind("ready", 0) == 0) {
        return "dsl_draft_ready_before_overlay_stage";
    }
    if (candidateDecisionPackageReadiness.rfind("ready", 0) == 0) {
        return "candidate_decisions_ready_before_dsl_draft";
    }
    if (postPlayState == "entry_points_ambiguous") {
        return "entry_point_branch_ambiguity_detected";
    }
    if (postPlayState == "entry_point_analysis_blocked") {
        return "entry_point_analysis_not_ready";
    }
    return "see_status_summary";
}

std::string buildNextActionCommandHint(
    const std::string& nextAction,
    const std::filesystem::path& nextStepsBriefPath)
{
    if (nextAction == "review_staged_overlay_and_publish_if_desired" ||
        nextAction == "review_staged_overlay_status" ||
        nextAction == "review_dsl_draft" ||
        nextAction == "review_candidate_decision_package" ||
        nextAction == "review_entry_point_ambiguity" ||
        nextAction == "review_entry_point_analysis_failure" ||
        nextAction == "review_tray_status") {
        return "open " + pathString(nextStepsBriefPath);
    }
    return std::string();
}

std::string buildStatusCenterSummaryText(
    const std::string& state,
    const std::string& reason,
    const bool stellarisRunning,
    const std::string& sessionId,
    const std::filesystem::path& sessionDirectory,
    const std::size_t copiedTotal,
    const std::size_t skippedTotal,
    const std::string& postPlayState,
    const bool archiveVerified,
    const std::filesystem::path& entryPointAnalysisPath,
    const std::size_t entryPointCount,
    const bool branchAmbiguityDetected,
    const std::string& entryPointReadiness,
    const std::filesystem::path& postPlayPackagePath,
    const std::string& postPlayPackageReadiness,
    const std::size_t postPlayDecisionReadyEntryCount,
    const std::size_t postPlayCampaignCount,
    const std::size_t postPlayReadyCampaignCount,
    const std::size_t postPlayPartialCampaignCount,
    const std::size_t postPlayBlockedCampaignCount,
    const std::vector<std::string>& postPlayCampaignSummaries,
    const std::filesystem::path& decisionInputPackagePath,
    const std::string& decisionInputPackageReadiness,
    const std::size_t decisionInputCount,
    const std::filesystem::path& candidateDecisionPackagePath,
    const std::string& candidateDecisionPackageReadiness,
    const std::size_t candidateDecisionCount,
    const std::filesystem::path& dslDraftPath,
    const std::string& dslDraftReadiness,
    const std::size_t dslDraftRuleCount,
    const std::filesystem::path& generatedOverlayStagingDirectory,
    const std::filesystem::path& generatedOverlayStagingStatusPath,
    const std::string& generatedOverlayStagingReadiness,
    const std::size_t generatedOverlayStagingRuleCount,
    const bool generatedOverlayManifestVerified,
    const bool generatedOverlayPublishAllowed,
    const std::string& generatedOverlayManifestHash,
    const strategic_nexus::CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const std::string& mpPackageRefreshState,
    const std::string& mpPackageRefreshReason)
{
    std::ostringstream summary;
    summary << "Strategic Nexus Status Center\n";
    summary << "updated_at_local: " << formatLocalTimestamp() << "\n";
    summary << "stav: " << state << " - " << reason << "\n";
    summary << "stellaris_running: " << (stellarisRunning ? "true" : "false") << "\n";
    if (!sessionId.empty()) {
        summary << "session_id: " << sessionId << "\n";
    }
    if (!sessionDirectory.empty()) {
        summary << "capture_session_directory: " << pathString(sessionDirectory) << "\n";
    }
    summary << "archive_root: " << pathString(g_archiveRoot) << "\n";
    summary << "archive_verified: " << (archiveVerified ? "true" : "false") << "\n";
    summary << "copied_count_total: " << copiedTotal << "\n";
    summary << "skipped_count_total: " << skippedTotal << "\n";
    summary << "post_play_state: " << postPlayState << "\n";
    if (!entryPointAnalysisPath.empty()) {
        summary << "entry_point_analysis_path: " << pathString(entryPointAnalysisPath) << "\n";
    }
    summary << "entry_point_count: " << entryPointCount << "\n";
    summary << "branch_ambiguity_detected: " << (branchAmbiguityDetected ? "true" : "false") << "\n";
    if (!entryPointReadiness.empty()) {
        summary << "entry_point_readiness: " << entryPointReadiness << "\n";
    }
    if (!postPlayPackagePath.empty()) {
        summary << "post_play_package_path: " << pathString(postPlayPackagePath) << "\n";
    }
    if (!postPlayPackageReadiness.empty()) {
        summary << "post_play_package_readiness: " << postPlayPackageReadiness << "\n";
    }
    summary << "post_play_decision_ready_entry_count: " << postPlayDecisionReadyEntryCount << "\n";
    summary << "post_play_campaign_count: " << postPlayCampaignCount << "\n";
    summary << "post_play_ready_campaign_count: " << postPlayReadyCampaignCount << "\n";
    summary << "post_play_partial_campaign_count: " << postPlayPartialCampaignCount << "\n";
    summary << "post_play_blocked_campaign_count: " << postPlayBlockedCampaignCount << "\n";
    for (const auto& campaignSummary : postPlayCampaignSummaries) {
        summary << "post_play_campaign_summary: " << campaignSummary << "\n";
    }
    if (!decisionInputPackagePath.empty()) {
        summary << "decision_input_package_path: " << pathString(decisionInputPackagePath) << "\n";
    }
    if (!decisionInputPackageReadiness.empty()) {
        summary << "decision_input_package_readiness: " << decisionInputPackageReadiness << "\n";
    }
    summary << "decision_input_count: " << decisionInputCount << "\n";
    if (!candidateDecisionPackagePath.empty()) {
        summary << "candidate_decision_package_path: " << pathString(candidateDecisionPackagePath) << "\n";
    }
    if (!candidateDecisionPackageReadiness.empty()) {
        summary << "candidate_decision_package_readiness: " << candidateDecisionPackageReadiness << "\n";
    }
    summary << "candidate_decision_count: " << candidateDecisionCount << "\n";
    if (!dslDraftPath.empty()) {
        summary << "dsl_draft_path: " << pathString(dslDraftPath) << "\n";
    }
    if (!dslDraftReadiness.empty()) {
        summary << "dsl_draft_readiness: " << dslDraftReadiness << "\n";
    }
    summary << "dsl_draft_rule_count: " << dslDraftRuleCount << "\n";
    if (!generatedOverlayStagingDirectory.empty()) {
        summary << "generated_overlay_staging_directory: " << pathString(generatedOverlayStagingDirectory) << "\n";
    }
    if (!generatedOverlayStagingStatusPath.empty()) {
        summary << "generated_overlay_staging_status_path: " << pathString(generatedOverlayStagingStatusPath) << "\n";
    }
    if (!generatedOverlayStagingReadiness.empty()) {
        summary << "generated_overlay_staging_readiness: " << generatedOverlayStagingReadiness << "\n";
    }
    summary << "generated_overlay_staging_rule_count: " << generatedOverlayStagingRuleCount << "\n";
    summary << "generated_overlay_manifest_verified: " << (generatedOverlayManifestVerified ? "true" : "false") << "\n";
    summary << "generated_overlay_publish_allowed: " << (generatedOverlayPublishAllowed ? "true" : "false") << "\n";
    if (!generatedOverlayManifestHash.empty()) {
        summary << "generated_overlay_manifest_hash: " << generatedOverlayManifestHash << "\n";
    }
    summary << "generated_overlay_active_directory: " << pathString(g_generatedOverlayActiveDirectory) << "\n";
    summary << "generated_overlay_publish_status_path: " << pathString(g_generatedOverlayPublishStatusPath) << "\n";
    summary << "generated_overlay_publish_backup_root_directory: "
            << pathString(g_generatedOverlayPublishBackupRootDirectory) << "\n";
    appendMpPackageSummaryLines(summary, mpOverlayPackage, mpPackageRefreshState, mpPackageRefreshReason);
    return summary.str();
}

void writeNextStepsBrief(
    const std::string& state,
    const std::string& postPlayState,
    const std::filesystem::path& entryPointAnalysisPath,
    const std::string& entryPointReadiness,
    const std::filesystem::path& postPlayPackagePath,
    const std::string& postPlayPackageReadiness,
    const std::filesystem::path& decisionInputPackagePath,
    const std::string& decisionInputPackageReadiness,
    const std::filesystem::path& candidateDecisionPackagePath,
    const std::string& candidateDecisionPackageReadiness,
    const std::filesystem::path& dslDraftPath,
    const std::string& dslDraftReadiness,
    const std::filesystem::path& generatedOverlayStagingStatusPath,
    const std::string& generatedOverlayStagingReadiness,
    const bool generatedOverlayPublishAllowed,
    const strategic_nexus::CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const std::string& mpPackageRefreshState,
    const std::string& mpPackageRefreshReason,
    const std::string& nextAction,
    const std::string& nextActionReason)
{
    std::ostringstream brief;
    brief << "Strategic Nexus Companion - dalsi kroky\n";
    brief << "Aktualizovano: " << formatLocalTimestamp() << "\n";
    brief << "Stav: " << state << "\n";
    brief << "Post-play stav: " << postPlayState << "\n";
    brief << "Dalsi akce: " << nextAction << "\n";
    brief << "Duvod: " << nextActionReason << "\n\n";
    brief << "- Entry point analysis: " << pathString(entryPointAnalysisPath);
    if (!entryPointReadiness.empty()) {
        brief << " (" << entryPointReadiness << ")";
    }
    brief << "\n";
    brief << "- Post-play package: " << pathString(postPlayPackagePath);
    if (!postPlayPackageReadiness.empty()) {
        brief << " (" << postPlayPackageReadiness << ")";
    }
    brief << "\n";
    brief << "- Decision input package: " << pathString(decisionInputPackagePath);
    if (!decisionInputPackageReadiness.empty()) {
        brief << " (" << decisionInputPackageReadiness << ")";
    }
    brief << "\n";
    brief << "- Candidate decision package: " << pathString(candidateDecisionPackagePath);
    if (!candidateDecisionPackageReadiness.empty()) {
        brief << " (" << candidateDecisionPackageReadiness << ")";
    }
    brief << "\n";
    brief << "- DSL draft: " << pathString(dslDraftPath);
    if (!dslDraftReadiness.empty()) {
        brief << " (" << dslDraftReadiness << ")";
    }
    brief << "\n";
    brief << "- SNC staged overlay status: " << pathString(generatedOverlayStagingStatusPath);
    if (!generatedOverlayStagingReadiness.empty()) {
        brief << " (" << generatedOverlayStagingReadiness << ")";
    }
    brief << "\n";
    brief << "- Publish gate available: " << (generatedOverlayPublishAllowed ? "ano" : "ne") << "\n";
    brief << "- Active overlay snapshot: " << pathString(g_generatedOverlayActiveDirectory) << "\n";
    brief << "- Publish status output: " << pathString(g_generatedOverlayPublishStatusPath) << "\n";
    brief << "- MP package refresh: " << mpPackageRefreshState << " (" << mpPackageRefreshReason << ")\n";
    brief << "- MP overlay package directory: " << pathString(g_mpOverlayPackageDirectory) << "\n";
    brief << "- MP package zip path: " << pathString(g_mpOverlayPackageZipPath) << "\n";
    brief << "- MP overlay package state: " << mpOverlayPackage.state;
    if (!mpOverlayPackage.reason.empty()) {
        brief << " (" << mpOverlayPackage.reason << ")";
    }
    brief << "\n";
    if (!mpOverlayPackage.readiness.empty()) {
        brief << "- MP readiness: " << mpOverlayPackage.readiness << "\n";
    }
    if (!mpOverlayPackage.hostReadiness.empty()) {
        brief << "- MP host readiness: " << mpOverlayPackage.hostReadiness << "\n";
    }
    if (!mpOverlayPackage.clientReadinessGate.empty()) {
        brief << "- MP client readiness gate: " << mpOverlayPackage.clientReadinessGate << "\n";
    }
    if (!mpOverlayPackage.verifyCommand.empty()) {
        brief << "- MP verify command: " << mpOverlayPackage.verifyCommand << "\n";
    }
    if (!mpOverlayPackage.importCommand.empty()) {
        brief << "- MP import command: " << mpOverlayPackage.importCommand << "\n";
    }
    if (!mpOverlayPackage.strictVerifyCommand.empty()) {
        brief << "- MP strict verify command: " << mpOverlayPackage.strictVerifyCommand << "\n";
    }
    if (!mpOverlayPackage.strictImportCommand.empty()) {
        brief << "- MP strict import command: " << mpOverlayPackage.strictImportCommand << "\n";
    }
    if (!mpOverlayPackage.hostNextStep.empty()) {
        brief << "- MP host next step: " << mpOverlayPackage.hostNextStep << "\n";
    }
    if (!mpOverlayPackage.clientNextStep.empty()) {
        brief << "- MP client next step: " << mpOverlayPackage.clientNextStep << "\n";
    }
    if (!mpOverlayPackage.identityMismatchAlert.empty()) {
        brief << "- MP mismatch alert: " << mpOverlayPackage.identityMismatchAlert << "\n";
    }

    strategic_nexus::common::writeTextFileAtomically(g_nextStepsBriefPath, brief.str());
}

void writeStatus(
    HWND hwnd,
    const std::string& state,
    const std::string& reason,
    const bool stellarisRunning,
    const std::string& sessionId,
    const std::filesystem::path& sessionDirectory,
    const std::size_t candidateRootCount,
    const std::size_t existingRootCount,
    const std::size_t copiedTotal,
    const std::size_t skippedTotal,
    const std::string& postPlayState,
    const bool archiveVerified,
    const std::filesystem::path& entryPointAnalysisPath = std::filesystem::path(),
    const std::size_t entryPointCount = 0,
    const bool branchAmbiguityDetected = false,
    const std::string& entryPointReadiness = std::string(),
    const std::filesystem::path& postPlayPackagePath = std::filesystem::path(),
    const std::string& postPlayPackageReadiness = std::string(),
    const std::size_t postPlayDecisionReadyEntryCount = 0,
    const std::size_t postPlayCampaignCount = 0,
    const std::size_t postPlayReadyCampaignCount = 0,
    const std::size_t postPlayPartialCampaignCount = 0,
    const std::size_t postPlayBlockedCampaignCount = 0,
    const std::vector<std::string>& postPlayCampaignSummaries = {},
    const std::filesystem::path& decisionInputPackagePath = std::filesystem::path(),
    const std::string& decisionInputPackageReadiness = std::string(),
    const std::size_t decisionInputCount = 0,
    const std::filesystem::path& candidateDecisionPackagePath = std::filesystem::path(),
    const std::string& candidateDecisionPackageReadiness = std::string(),
    const std::size_t candidateDecisionCount = 0,
    const bool candidateDecisionValidatorPassed = false,
    const std::filesystem::path& dslDraftPath = std::filesystem::path(),
    const std::filesystem::path& dslDraftAuditPath = std::filesystem::path(),
    const std::string& dslDraftReadiness = std::string(),
    const std::size_t dslDraftRuleCount = 0,
    const bool dslDraftValidatorPassed = false,
    const std::filesystem::path& generatedOverlayStagingDirectory = std::filesystem::path(),
    const std::filesystem::path& generatedOverlayStagingStatusPath = std::filesystem::path(),
    const std::string& generatedOverlayStagingReadiness = std::string(),
    const std::size_t generatedOverlayStagingRuleCount = 0,
    const bool generatedOverlayManifestVerified = false,
    const bool generatedOverlayPublishAllowed = false,
    const std::string& generatedOverlayManifestHash = std::string(),
    const std::string& mpPackageRefreshState = "not_attempted",
    const std::string& mpPackageRefreshReason = "waiting_for_staged_overlay")
{
    const strategic_nexus::StrategicNexusCompanion companion;
    strategic_nexus::CompanionStatusConfig companionConfig;
    companionConfig.archiveRoot = sessionDirectory.empty() ? g_archiveRoot : sessionDirectory;
    companionConfig.generatedOverlayDirectory = g_generatedOverlayActiveDirectory;
    companionConfig.mpOverlayPackageDirectory = g_mpOverlayPackageDirectory;
    companionConfig.useDetectedStellarisState = false;
    companionConfig.stellarisRunningOverride = stellarisRunning;
    companionConfig.generatedOverlayStagingStatusPath = g_generatedOverlayStagingStatusPath;
    companionConfig.generatedOverlayActiveDirectory = g_generatedOverlayActiveDirectory;
    companionConfig.generatedOverlayPublishStatusPath = g_generatedOverlayPublishStatusPath;
    companionConfig.generatedOverlayPublishBackupRootDirectory = g_generatedOverlayPublishBackupRootDirectory;
    companionConfig.entryPointAnalysisPath = g_entryPointAnalysisPath;
    companionConfig.postPlayPackagePath = g_postPlayPackagePath;
    companionConfig.decisionInputPackagePath = g_decisionInputPackagePath;
    companionConfig.candidateDecisionPackagePath = g_candidateDecisionPackagePath;
    companionConfig.dslDraftPath = g_dslDraftPath;
    companionConfig.dslDraftAuditPath = g_dslDraftAuditPath;
    companionConfig.postPlayGeneratedOverlayStagingStatusPath = g_generatedOverlayStagingStatusPath;
    companionConfig.mpOverlayPackageZipPath = g_mpOverlayPackageZipPath;
    const auto companionSnapshot = companion.buildStatusSnapshot(companionConfig);

    const auto nextAction = buildNextAction(
        state,
        postPlayState,
        generatedOverlayPublishAllowed,
        generatedOverlayStagingReadiness,
        dslDraftReadiness,
        candidateDecisionPackageReadiness);
    const auto nextActionReason = buildNextActionReason(
        state,
        postPlayState,
        generatedOverlayPublishAllowed,
        generatedOverlayStagingReadiness,
        dslDraftReadiness,
        candidateDecisionPackageReadiness);
    const auto nextActionCommandHint = buildNextActionCommandHint(nextAction, g_nextStepsBriefPath);
    const auto statusCenterSummaryText = buildStatusCenterSummaryText(
        state,
        reason,
        stellarisRunning,
        sessionId,
        sessionDirectory,
        copiedTotal,
        skippedTotal,
        postPlayState,
        archiveVerified,
        entryPointAnalysisPath,
        entryPointCount,
        branchAmbiguityDetected,
        entryPointReadiness,
        postPlayPackagePath,
        postPlayPackageReadiness,
        postPlayDecisionReadyEntryCount,
        postPlayCampaignCount,
        postPlayReadyCampaignCount,
        postPlayPartialCampaignCount,
        postPlayBlockedCampaignCount,
        postPlayCampaignSummaries,
        decisionInputPackagePath,
        decisionInputPackageReadiness,
        decisionInputCount,
        candidateDecisionPackagePath,
        candidateDecisionPackageReadiness,
        candidateDecisionCount,
        dslDraftPath,
        dslDraftReadiness,
        dslDraftRuleCount,
        generatedOverlayStagingDirectory,
        generatedOverlayStagingStatusPath,
        generatedOverlayStagingReadiness,
        generatedOverlayStagingRuleCount,
        generatedOverlayManifestVerified,
        generatedOverlayPublishAllowed,
        generatedOverlayManifestHash,
        companionSnapshot.mpOverlayPackage,
        mpPackageRefreshState,
        mpPackageRefreshReason);
    writeNextStepsBrief(
        state,
        postPlayState,
        entryPointAnalysisPath,
        entryPointReadiness,
        postPlayPackagePath,
        postPlayPackageReadiness,
        decisionInputPackagePath,
        decisionInputPackageReadiness,
        candidateDecisionPackagePath,
        candidateDecisionPackageReadiness,
        dslDraftPath,
        dslDraftReadiness,
        generatedOverlayStagingStatusPath,
        generatedOverlayStagingReadiness,
        generatedOverlayPublishAllowed,
        companionSnapshot.mpOverlayPackage,
        mpPackageRefreshState,
        mpPackageRefreshReason,
        nextAction,
        nextActionReason);

    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"app_name\": \"Strategic Nexus Companion\",\n";
    json << "  \"mode\": \"tray\",\n";
    json << "  \"state\": \"" << jsonEscape(state) << "\",\n";
    json << "  \"reason\": \"" << jsonEscape(reason) << "\",\n";
    json << "  \"updated_at_local\": \"" << jsonEscape(formatLocalTimestamp()) << "\",\n";
    json << "  \"stellaris_running\": " << (stellarisRunning ? "true" : "false") << ",\n";
    json << "  \"session_id\": \"" << jsonEscape(sessionId) << "\",\n";
    json << "  \"capture_session_directory\": \"" << jsonEscape(pathString(sessionDirectory)) << "\",\n";
    json << "  \"archive_root\": \"" << jsonEscape(pathString(g_archiveRoot)) << "\",\n";
    json << "  \"candidate_root_count\": " << candidateRootCount << ",\n";
    json << "  \"existing_root_count\": " << existingRootCount << ",\n";
    json << "  \"copied_count_total\": " << copiedTotal << ",\n";
    json << "  \"skipped_count_total\": " << skippedTotal << ",\n";
    json << "  \"post_play_state\": \"" << jsonEscape(postPlayState) << "\",\n";
    json << "  \"archive_verified\": " << (archiveVerified ? "true" : "false") << ",\n";
    json << "  \"entry_point_analysis_path\": \"" << jsonEscape(pathString(entryPointAnalysisPath)) << "\",\n";
    json << "  \"entry_point_count\": " << entryPointCount << ",\n";
    json << "  \"branch_ambiguity_detected\": " << (branchAmbiguityDetected ? "true" : "false") << ",\n";
    json << "  \"entry_point_readiness\": \"" << jsonEscape(entryPointReadiness) << "\",\n";
    json << "  \"post_play_package_path\": \"" << jsonEscape(pathString(postPlayPackagePath)) << "\",\n";
    json << "  \"post_play_package_readiness\": \"" << jsonEscape(postPlayPackageReadiness) << "\",\n";
    json << "  \"post_play_decision_ready_entry_count\": " << postPlayDecisionReadyEntryCount << ",\n";
    json << "  \"post_play_campaign_count\": " << postPlayCampaignCount << ",\n";
    json << "  \"post_play_ready_campaign_count\": " << postPlayReadyCampaignCount << ",\n";
    json << "  \"post_play_partial_campaign_count\": " << postPlayPartialCampaignCount << ",\n";
    json << "  \"post_play_blocked_campaign_count\": " << postPlayBlockedCampaignCount << ",\n";
    json << "  \"post_play_campaign_summaries\": [";
    for (std::size_t index = 0; index < postPlayCampaignSummaries.size(); ++index) {
        if (index > 0) {
            json << ", ";
        }
        json << "\"" << jsonEscape(postPlayCampaignSummaries[index]) << "\"";
    }
    json << "],\n";
    json << "  \"decision_input_package_path\": \"" << jsonEscape(pathString(decisionInputPackagePath)) << "\",\n";
    json << "  \"decision_input_package_readiness\": \"" << jsonEscape(decisionInputPackageReadiness) << "\",\n";
    json << "  \"decision_input_count\": " << decisionInputCount << ",\n";
    json << "  \"candidate_decision_package_path\": \"" << jsonEscape(pathString(candidateDecisionPackagePath)) << "\",\n";
    json << "  \"candidate_decision_package_readiness\": \"" << jsonEscape(candidateDecisionPackageReadiness) << "\",\n";
    json << "  \"candidate_decision_count\": " << candidateDecisionCount << ",\n";
    json << "  \"candidate_decision_validator_passed\": "
         << (candidateDecisionValidatorPassed ? "true" : "false") << ",\n";
    json << "  \"dsl_draft_path\": \"" << jsonEscape(pathString(dslDraftPath)) << "\",\n";
    json << "  \"dsl_draft_audit_path\": \"" << jsonEscape(pathString(dslDraftAuditPath)) << "\",\n";
    json << "  \"dsl_draft_readiness\": \"" << jsonEscape(dslDraftReadiness) << "\",\n";
    json << "  \"dsl_draft_rule_count\": " << dslDraftRuleCount << ",\n";
    json << "  \"dsl_draft_validator_passed\": "
         << (dslDraftValidatorPassed ? "true" : "false") << ",\n";
    json << "  \"generated_overlay_staging_directory\": \""
         << jsonEscape(pathString(generatedOverlayStagingDirectory)) << "\",\n";
    json << "  \"generated_overlay_staging_status_path\": \""
         << jsonEscape(pathString(generatedOverlayStagingStatusPath)) << "\",\n";
    json << "  \"generated_overlay_active_directory\": \""
         << jsonEscape(pathString(g_generatedOverlayActiveDirectory)) << "\",\n";
    json << "  \"generated_overlay_publish_status_path\": \""
         << jsonEscape(pathString(g_generatedOverlayPublishStatusPath)) << "\",\n";
    json << "  \"generated_overlay_publish_backup_root_directory\": \""
         << jsonEscape(pathString(g_generatedOverlayPublishBackupRootDirectory)) << "\",\n";
    json << "  \"generated_overlay_staging_readiness\": \""
         << jsonEscape(generatedOverlayStagingReadiness) << "\",\n";
    json << "  \"generated_overlay_staging_rule_count\": " << generatedOverlayStagingRuleCount << ",\n";
    json << "  \"generated_overlay_manifest_verified\": "
         << (generatedOverlayManifestVerified ? "true" : "false") << ",\n";
    json << "  \"generated_overlay_publish_allowed\": "
         << (generatedOverlayPublishAllowed ? "true" : "false") << ",\n";
    json << "  \"generated_overlay_manifest_hash\": \""
         << jsonEscape(generatedOverlayManifestHash) << "\",\n";
    json << "  \"mp_package_refresh_state\": \"" << jsonEscape(mpPackageRefreshState) << "\",\n";
    json << "  \"mp_package_refresh_reason\": \"" << jsonEscape(mpPackageRefreshReason) << "\",\n";
    json << "  \"mp_overlay_package_directory\": \"" << jsonEscape(pathString(g_mpOverlayPackageDirectory)) << "\",\n";
    json << "  \"mp_overlay_package_zip_path\": \"" << jsonEscape(pathString(g_mpOverlayPackageZipPath)) << "\",\n";
    json << "  \"mp_overlay_package_state\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.state) << "\",\n";
    json << "  \"mp_overlay_package_reason\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.reason) << "\",\n";
    json << "  \"mp_overlay_package_path\": \"" << jsonEscape(pathString(companionSnapshot.mpOverlayPackage.path)) << "\",\n";
    json << "  \"mp_overlay_package_campaign_id\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.campaignId) << "\",\n";
    json << "  \"mp_overlay_package_overlay_version\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.overlayVersion) << "\",\n";
    json << "  \"mp_overlay_package_game_version\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.gameVersion) << "\",\n";
    json << "  \"mp_overlay_package_mod_version\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.strategicNexusModVersion) << "\",\n";
    json << "  \"mp_overlay_package_handoff_status\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.handoffStatus) << "\",\n";
    json << "  \"mp_overlay_package_readiness\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.readiness) << "\",\n";
    json << "  \"mp_overlay_package_host_readiness\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.hostReadiness) << "\",\n";
    json << "  \"mp_overlay_package_client_readiness_gate\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.clientReadinessGate) << "\",\n";
    json << "  \"mp_overlay_package_host_next_step\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.hostNextStep) << "\",\n";
    json << "  \"mp_overlay_package_client_next_step\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.clientNextStep) << "\",\n";
    json << "  \"mp_overlay_package_manifest_hash\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.packageManifestHash) << "\",\n";
    json << "  \"mp_overlay_package_verify_command\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.verifyCommand) << "\",\n";
    json << "  \"mp_overlay_package_import_command\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.importCommand) << "\",\n";
    json << "  \"mp_overlay_package_strict_verify_command\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.strictVerifyCommand) << "\",\n";
    json << "  \"mp_overlay_package_strict_import_command\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.strictImportCommand) << "\",\n";
    json << "  \"mp_overlay_package_warning_count\": " << companionSnapshot.mpOverlayPackage.warningCount << ",\n";
    json << "  \"mp_overlay_package_warning_codes\": ";
    writeStringArrayJson(json, companionSnapshot.mpOverlayPackage.warningCodes);
    json << ",\n";
    json << "  \"mp_overlay_package_identity_mismatch_warning\": "
         << (companionSnapshot.mpOverlayPackage.identityMismatchWarning ? "true" : "false") << ",\n";
    json << "  \"mp_overlay_package_identity_mismatch_warning_codes\": ";
    writeStringArrayJson(json, companionSnapshot.mpOverlayPackage.identityMismatchWarningCodes);
    json << ",\n";
    json << "  \"mp_overlay_package_mismatch_warning_state\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.mismatchWarningState) << "\",\n";
    json << "  \"mp_overlay_package_mismatch_warning_reason\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.mismatchWarningReason) << "\",\n";
    json << "  \"mp_overlay_package_mismatch_warning_codes\": ";
    writeStringArrayJson(json, companionSnapshot.mpOverlayPackage.mismatchWarningCodes);
    json << ",\n";
    json << "  \"mp_overlay_package_identity_mismatch_alert\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.identityMismatchAlert) << "\",\n";
    json << "  \"mp_overlay_package_zip_state\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.packageZipState) << "\",\n";
    json << "  \"mp_overlay_package_zip_reason\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.packageZipReason) << "\",\n";
    json << "  \"mp_overlay_package_zip_runtime_path\": \"" << jsonEscape(pathString(companionSnapshot.mpOverlayPackage.packageZipPath)) << "\",\n";
    json << "  \"mp_overlay_package_zip_bytes\": " << companionSnapshot.mpOverlayPackage.packageZipBytes << ",\n";
    json << "  \"status_center_state\": \"" << jsonEscape(state) << "\",\n";
    json << "  \"status_center_reason\": \"" << jsonEscape(reason) << "\",\n";
    json << "  \"status_center_summary_text\": \"" << jsonEscape(statusCenterSummaryText) << "\",\n";
    json << "  \"next_action\": \"" << jsonEscape(nextAction) << "\",\n";
    json << "  \"next_action_reason\": \"" << jsonEscape(nextActionReason) << "\",\n";
    json << "  \"next_action_command_hint\": \"" << jsonEscape(nextActionCommandHint) << "\",\n";
    json << "  \"next_steps_brief_path\": \"" << jsonEscape(pathString(g_nextStepsBriefPath)) << "\"\n";
    json << "}\n";

    const auto text = json.str();
    strategic_nexus::common::writeTextFileAtomically(g_trayStatusPath, text);
    strategic_nexus::common::writeTextFileAtomically(g_liveStatusPath, text);

    const auto wideReason = utf8ToWide(reason);
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_statusText = wideReason.empty() ? L"SNC bezi." : wideReason;
    }
    updateTrayTip(hwnd, wideReason.empty() ? L"bezi" : wideReason);
}

std::vector<std::filesystem::path> discoverExistingSaveRoots(std::size_t& candidateRootCount)
{
    std::vector<std::filesystem::path> roots;
    const strategic_nexus::StellarisSavePathResolver resolver;
    const auto discovery = resolver.discoverFromEnvironment();
    candidateRootCount = discovery.candidates.size();
    for (const auto& candidate : discovery.candidates) {
        if (candidate.exists) {
            roots.push_back(candidate.path);
        }
    }
    return roots;
}

void writeLastSummary(const strategic_nexus::AutosaveArchiveSummary& summary)
{
    strategic_nexus::common::writeTextFileAtomically(
        g_lastSummaryPath,
        strategic_nexus::serializeAutosaveArchiveSummary(summary));
}

void workerLoop(HWND hwnd)
{
    const strategic_nexus::StellarisProcessDetector detector;
    const strategic_nexus::AutosaveArchiver archiver;
    const strategic_nexus::AutosaveArchiveVerifier verifier;
    const strategic_nexus::AutosaveArchiveSummarizer summarizer;
    const strategic_nexus::SaveEntryPointAnalyzer entryPointAnalyzer;
    const strategic_nexus::PostPlayPackageBuilder postPlayPackageBuilder;
    const strategic_nexus::SncDecisionInputPackageBuilder decisionInputPackageBuilder;
    const strategic_nexus::SncCandidateDecisionPackageBuilder candidateDecisionPackageBuilder;
    const strategic_nexus::SncDslDraftPackageBuilder dslDraftPackageBuilder;
    const strategic_nexus::SncGeneratedOverlayStager generatedOverlayStager;

    bool wasRunning = false;
    std::string sessionId;
    std::filesystem::path sessionDirectory;
    std::size_t copiedTotal = 0;
    std::size_t skippedTotal = 0;
    bool archiveVerified = false;
    std::string postPlayState = "not_started";
    std::filesystem::path lastEntryPointAnalysisPath;
    std::size_t lastEntryPointCount = 0;
    bool lastBranchAmbiguityDetected = false;
    std::string lastEntryPointReadiness;
    std::filesystem::path lastPostPlayPackagePath;
    std::string lastPostPlayPackageReadiness;
    std::size_t lastPostPlayDecisionReadyEntryCount = 0;
    std::size_t lastPostPlayCampaignCount = 0;
    std::size_t lastPostPlayReadyCampaignCount = 0;
    std::size_t lastPostPlayPartialCampaignCount = 0;
    std::size_t lastPostPlayBlockedCampaignCount = 0;
    std::vector<std::string> lastPostPlayCampaignSummaries;
    std::filesystem::path lastDecisionInputPackagePath;
    std::string lastDecisionInputPackageReadiness;
    std::size_t lastDecisionInputCount = 0;
    std::filesystem::path lastCandidateDecisionPackagePath;
    std::string lastCandidateDecisionPackageReadiness;
    std::size_t lastCandidateDecisionCount = 0;
    bool lastCandidateDecisionValidatorPassed = false;
    std::filesystem::path lastDslDraftPath;
    std::filesystem::path lastDslDraftAuditPath;
    std::string lastDslDraftReadiness;
    std::size_t lastDslDraftRuleCount = 0;
    bool lastDslDraftValidatorPassed = false;
    std::filesystem::path lastGeneratedOverlayStagingDirectory;
    std::filesystem::path lastGeneratedOverlayStagingStatusPath;
    std::string lastGeneratedOverlayStagingReadiness;
    std::size_t lastGeneratedOverlayStagingRuleCount = 0;
    bool lastGeneratedOverlayManifestVerified = false;
    bool lastGeneratedOverlayPublishAllowed = false;
    std::string lastGeneratedOverlayManifestHash;
    std::string lastMpPackageRefreshState = "not_attempted";
    std::string lastMpPackageRefreshReason = "waiting_for_stellaris_session";

    writeStatus(
        hwnd,
        "waiting_for_stellaris",
        "SNC tray bezi; ceka na stellaris.exe.",
        false,
        sessionId,
        sessionDirectory,
        0,
        0,
        copiedTotal,
        skippedTotal,
        "not_started",
        archiveVerified);

    while (!g_stopRequested.load()) {
        const auto processStatus = detector.detectFromSystem();
        const bool running = processStatus.running;

        if (running && !wasRunning) {
            sessionId = "snc-stellaris-" + formatSessionToken();
            sessionDirectory = g_archiveRoot / sessionId;
            copiedTotal = 0;
            skippedTotal = 0;
            archiveVerified = false;
            postPlayState = "not_started";
            lastEntryPointAnalysisPath.clear();
            lastEntryPointCount = 0;
            lastBranchAmbiguityDetected = false;
            lastEntryPointReadiness.clear();
            lastPostPlayPackagePath.clear();
            lastPostPlayPackageReadiness.clear();
            lastPostPlayDecisionReadyEntryCount = 0;
            lastPostPlayCampaignCount = 0;
            lastPostPlayReadyCampaignCount = 0;
            lastPostPlayPartialCampaignCount = 0;
            lastPostPlayBlockedCampaignCount = 0;
            lastPostPlayCampaignSummaries.clear();
            lastDecisionInputPackagePath.clear();
            lastDecisionInputPackageReadiness.clear();
            lastDecisionInputCount = 0;
            lastCandidateDecisionPackagePath.clear();
            lastCandidateDecisionPackageReadiness.clear();
            lastCandidateDecisionCount = 0;
            lastCandidateDecisionValidatorPassed = false;
            lastDslDraftPath.clear();
            lastDslDraftAuditPath.clear();
            lastDslDraftReadiness.clear();
            lastDslDraftRuleCount = 0;
            lastDslDraftValidatorPassed = false;
            lastGeneratedOverlayStagingDirectory.clear();
            lastGeneratedOverlayStagingStatusPath.clear();
            lastGeneratedOverlayStagingReadiness.clear();
            lastGeneratedOverlayStagingRuleCount = 0;
            lastGeneratedOverlayManifestVerified = false;
            lastGeneratedOverlayPublishAllowed = false;
            lastGeneratedOverlayManifestHash.clear();
            lastMpPackageRefreshState = "not_attempted";
            lastMpPackageRefreshReason = "waiting_for_staged_overlay";
            writeStatus(
                hwnd,
                "capturing",
                "Stellaris bezi; SNC zacalo capture okno.",
                true,
                sessionId,
                sessionDirectory,
                0,
                0,
                copiedTotal,
                skippedTotal,
                postPlayState,
                archiveVerified);
        }

        if (running) {
            std::size_t candidateRootCount = 0;
            const auto roots = discoverExistingSaveRoots(candidateRootCount);
            std::size_t existingRootCount = 0;
            for (const auto& root : roots) {
                if (!existingDirectory(root)) {
                    continue;
                }

                ++existingRootCount;
                const auto archiveResult = archiver.archiveLiveSaves(root, g_archiveRoot, sessionId, 250);
                copiedTotal += archiveResult.copiedCount;
                skippedTotal += archiveResult.skippedCount;
            }
            if (existingRootCount > 1) {
                archiver.writeLiveArchiveManifest(
                    g_archiveRoot,
                    sessionId,
                    std::filesystem::path("multiple_stellaris_save_roots"),
                    true);
            }

            writeStatus(
                hwnd,
                "capturing",
                "Stellaris bezi; SNC zachytava stabilni autosavy.",
                true,
                sessionId,
                sessionDirectory,
                candidateRootCount,
                existingRootCount,
                copiedTotal,
                skippedTotal,
                postPlayState,
                archiveVerified);
        } else if (wasRunning) {
            const auto verification = verifier.verify(sessionDirectory);
            const auto summary = summarizer.summarize(sessionDirectory);
            writeLastSummary(summary);
            archiveVerified = verification.ok;
            std::size_t entryPointCandidateRootCount = 0;
            const auto entryPointRoots = discoverExistingSaveRoots(entryPointCandidateRootCount);
            const auto entryPointAnalysis = archiveVerified
                ? entryPointAnalyzer.analyze(sessionDirectory, entryPointRoots)
                : strategic_nexus::SaveEntryPointAnalysis{};
            const bool entryPointAnalysisWritten = archiveVerified && strategic_nexus::common::writeTextFileAtomically(
                g_entryPointAnalysisPath,
                strategic_nexus::serializeSaveEntryPointAnalysis(entryPointAnalysis));
            const auto postPlayPackage = archiveVerified
                ? postPlayPackageBuilder.build(summary, entryPointAnalysis)
                : strategic_nexus::PostPlayPackage{};
            const bool postPlayPackageWritten = entryPointAnalysisWritten && strategic_nexus::common::writeTextFileAtomically(
                g_postPlayPackagePath,
                strategic_nexus::serializePostPlayPackage(postPlayPackage));
            const auto decisionInputPackage = postPlayPackageWritten
                ? decisionInputPackageBuilder.build(postPlayPackage, g_postPlayPackagePath)
                : strategic_nexus::SncDecisionInputPackage{};
            const bool decisionInputPackageWritten = postPlayPackageWritten && strategic_nexus::common::writeTextFileAtomically(
                g_decisionInputPackagePath,
                strategic_nexus::serializeSncDecisionInputPackage(decisionInputPackage));
            const auto candidateDecisionPackage = decisionInputPackageWritten
                ? candidateDecisionPackageBuilder.build(decisionInputPackage, g_decisionInputPackagePath)
                : strategic_nexus::SncCandidateDecisionPackage{};
            const bool candidateDecisionPackageWritten = decisionInputPackageWritten && strategic_nexus::common::writeTextFileAtomically(
                g_candidateDecisionPackagePath,
                strategic_nexus::serializeSncCandidateDecisionPackage(candidateDecisionPackage));
            const auto dslDraftPackage = candidateDecisionPackageWritten
                ? dslDraftPackageBuilder.build(candidateDecisionPackage, g_candidateDecisionPackagePath)
                : strategic_nexus::SncDslDraftPackage{};
            const bool dslDraftAuditWritten = candidateDecisionPackageWritten && strategic_nexus::common::writeTextFileAtomically(
                g_dslDraftAuditPath,
                strategic_nexus::serializeSncDslDraftPackage(dslDraftPackage));
            const bool dslDraftWritten = dslDraftPackage.ok && strategic_nexus::common::writeTextFileAtomically(
                g_dslDraftPath,
                dslDraftPackage.dslText);
            const auto generatedOverlayStagingStatus = dslDraftWritten
                ? generatedOverlayStager.stage(g_dslDraftPath, g_generatedOverlayStagingDirectory)
                : strategic_nexus::SncGeneratedOverlayStagingStatus{};
            const bool generatedOverlayStagingStatusWritten = dslDraftWritten &&
                strategic_nexus::common::writeTextFileAtomically(
                    g_generatedOverlayStagingStatusPath,
                    strategic_nexus::serializeSncGeneratedOverlayStagingStatus(generatedOverlayStagingStatus));
            lastMpPackageRefreshState = "not_attempted";
            lastMpPackageRefreshReason = "waiting_for_staged_overlay";
            if (generatedOverlayStagingStatus.ok) {
                const std::string effectiveCampaignId =
                    (!candidateDecisionPackage.candidateDecisions.empty())
                        ? safeIdentifier(candidateDecisionPackage.candidateDecisions.front().campaignKey, "campaign")
                        : std::string();
                if (effectiveCampaignId.empty()) {
                    lastMpPackageRefreshState = "blocked";
                    lastMpPackageRefreshReason = "missing_campaign_identity_for_mp_package_export";
                } else {
                    const strategic_nexus::generated_overlay::MpOverlayPackageExporter mpExporter;
                    const auto mpExportResult = mpExporter.exportPackage(
                        g_generatedOverlayStagingDirectory,
                        effectiveCampaignId,
                        kTrayMpOverlayVersion,
                        kTrayMpGameVersion,
                        kTrayMpModVersion,
                        g_mpOverlayPackageDirectory,
                        false);
                    if (!mpExportResult.ok) {
                        lastMpPackageRefreshState = "failed";
                        lastMpPackageRefreshReason = "mp_package_export_failed: " + mpExportResult.reason;
                    } else {
                        std::error_code zipError;
                        if (std::filesystem::exists(g_mpOverlayPackageZipPath, zipError)) {
                            std::filesystem::remove(g_mpOverlayPackageZipPath, zipError);
                        }
                        lastMpPackageRefreshState = "ready";
                        lastMpPackageRefreshReason = "mp_package_exported_from_staged_overlay";
                    }
                }
            } else if (dslDraftWritten) {
                lastMpPackageRefreshState = "blocked";
                lastMpPackageRefreshReason = "generated_overlay_staging_not_verified";
            }
            postPlayState = "blocked_by_archive_verification";
            lastEntryPointAnalysisPath.clear();
            lastEntryPointCount = entryPointAnalysis.entryPointCount;
            lastBranchAmbiguityDetected = entryPointAnalysis.branchAmbiguityDetected;
            lastEntryPointReadiness = entryPointAnalysis.readiness;
            lastPostPlayPackagePath.clear();
            lastPostPlayPackageReadiness = postPlayPackage.readiness;
            lastPostPlayDecisionReadyEntryCount = postPlayPackage.decisionReadyEntryCount;
            lastPostPlayCampaignCount = 0;
            lastPostPlayReadyCampaignCount = 0;
            lastPostPlayPartialCampaignCount = 0;
            lastPostPlayBlockedCampaignCount = 0;
            lastPostPlayCampaignSummaries.clear();
            parsePostPlayCampaignSummaries(
                strategic_nexus::serializePostPlayPackage(postPlayPackage),
                lastPostPlayCampaignCount,
                lastPostPlayReadyCampaignCount,
                lastPostPlayPartialCampaignCount,
                lastPostPlayBlockedCampaignCount,
                lastPostPlayCampaignSummaries);
            lastDecisionInputPackagePath.clear();
            lastDecisionInputPackageReadiness = decisionInputPackage.readiness;
            lastDecisionInputCount = decisionInputPackage.decisionInputCount;
            lastCandidateDecisionPackagePath.clear();
            lastCandidateDecisionPackageReadiness = candidateDecisionPackage.readiness;
            lastCandidateDecisionCount = candidateDecisionPackage.candidateDecisionCount;
            lastCandidateDecisionValidatorPassed = candidateDecisionPackage.validatorPassed;
            lastDslDraftPath.clear();
            lastDslDraftAuditPath.clear();
            lastDslDraftReadiness = dslDraftPackage.readiness;
            lastDslDraftRuleCount = dslDraftPackage.dslRuleCount;
            lastDslDraftValidatorPassed = dslDraftPackage.validatorPassed;
            lastGeneratedOverlayStagingDirectory.clear();
            lastGeneratedOverlayStagingStatusPath.clear();
            lastGeneratedOverlayStagingReadiness = generatedOverlayStagingStatus.readiness;
            lastGeneratedOverlayStagingRuleCount = generatedOverlayStagingStatus.dslRuleCount;
            lastGeneratedOverlayManifestVerified = generatedOverlayStagingStatus.manifestVerified;
            lastGeneratedOverlayPublishAllowed = generatedOverlayStagingStatus.publishAllowed;
            lastGeneratedOverlayManifestHash = generatedOverlayStagingStatus.manifestHash;
            if (archiveVerified) {
                if (!entryPointAnalysisWritten) {
                    postPlayState = "entry_point_analysis_write_failed";
                } else if (!postPlayPackageWritten) {
                    postPlayState = "post_play_package_write_failed";
                } else if (!decisionInputPackageWritten) {
                    postPlayState = "decision_input_package_write_failed";
                } else if (!candidateDecisionPackageWritten) {
                    postPlayState = "candidate_decision_package_write_failed";
                } else if (!dslDraftAuditWritten) {
                    postPlayState = "dsl_draft_audit_write_failed";
                } else if (dslDraftPackage.ok && !dslDraftWritten) {
                    postPlayState = "dsl_draft_write_failed";
                } else if (dslDraftWritten && !generatedOverlayStagingStatusWritten) {
                    postPlayState = "generated_overlay_staging_status_write_failed";
                } else if (dslDraftWritten && generatedOverlayStagingStatus.ok) {
                    postPlayState = "generated_overlay_staged";
                } else if (dslDraftWritten) {
                    postPlayState = "generated_overlay_staging_failed";
                } else if (dslDraftPackage.ok && dslDraftPackage.readiness.rfind("ready", 0) == 0) {
                    postPlayState = "dsl_draft_ready";
                } else if (dslDraftPackage.readiness == "needs_identity") {
                    postPlayState = "dsl_draft_needs_identity";
                } else if (candidateDecisionPackage.ok && candidateDecisionPackage.readiness.rfind("ready", 0) == 0) {
                    postPlayState = "candidate_decision_package_ready";
                } else if (candidateDecisionPackage.ok) {
                    postPlayState = "candidate_decision_package_attention";
                } else if (decisionInputPackage.ok && decisionInputPackage.readiness.rfind("ready", 0) == 0) {
                    postPlayState = "decision_input_package_ready";
                } else if (decisionInputPackage.ok) {
                    postPlayState = "decision_input_package_attention";
                } else if (postPlayPackage.ok && postPlayPackage.readiness.rfind("ready", 0) == 0) {
                    postPlayState = "post_play_package_ready";
                } else if (postPlayPackage.ok) {
                    postPlayState = "post_play_package_attention";
                } else if (entryPointAnalysis.branchAmbiguityDetected) {
                    postPlayState = "entry_points_ambiguous";
                } else if (entryPointAnalysis.ok) {
                    postPlayState = "entry_points_ready";
                } else {
                    postPlayState = "entry_point_analysis_blocked";
                }
                if (entryPointAnalysisWritten) {
                    lastEntryPointAnalysisPath = g_entryPointAnalysisPath;
                }
                if (postPlayPackageWritten) {
                    lastPostPlayPackagePath = g_postPlayPackagePath;
                }
                if (decisionInputPackageWritten) {
                    lastDecisionInputPackagePath = g_decisionInputPackagePath;
                }
                if (candidateDecisionPackageWritten) {
                    lastCandidateDecisionPackagePath = g_candidateDecisionPackagePath;
                }
                if (dslDraftAuditWritten) {
                    lastDslDraftAuditPath = g_dslDraftAuditPath;
                }
                if (dslDraftWritten) {
                    lastDslDraftPath = g_dslDraftPath;
                }
                if (generatedOverlayStagingStatus.ok) {
                    lastGeneratedOverlayStagingDirectory = g_generatedOverlayStagingDirectory;
                }
                if (generatedOverlayStagingStatusWritten) {
                    lastGeneratedOverlayStagingStatusPath = g_generatedOverlayStagingStatusPath;
                }
            }
            const std::string reason = verification.ok
                ? "Stellaris skoncil; archiv je overeny a SNC post-play balicky jsou aktualizovane."
                : "Stellaris skoncil; archiv vyzaduje pozornost: " + verification.reason;

            writeStatus(
                hwnd,
                verification.ok ? "post_play_ready" : "post_play_attention",
                reason,
                false,
                sessionId,
                sessionDirectory,
                entryPointCandidateRootCount,
                entryPointRoots.size(),
                copiedTotal,
                skippedTotal,
                postPlayState,
                archiveVerified,
                lastEntryPointAnalysisPath,
                lastEntryPointCount,
                lastBranchAmbiguityDetected,
                lastEntryPointReadiness,
                lastPostPlayPackagePath,
                lastPostPlayPackageReadiness,
                lastPostPlayDecisionReadyEntryCount,
                lastPostPlayCampaignCount,
                lastPostPlayReadyCampaignCount,
                lastPostPlayPartialCampaignCount,
                lastPostPlayBlockedCampaignCount,
                lastPostPlayCampaignSummaries,
                lastDecisionInputPackagePath,
                lastDecisionInputPackageReadiness,
                lastDecisionInputCount,
                lastCandidateDecisionPackagePath,
                lastCandidateDecisionPackageReadiness,
                lastCandidateDecisionCount,
                lastCandidateDecisionValidatorPassed,
                lastDslDraftPath,
                lastDslDraftAuditPath,
                lastDslDraftReadiness,
                lastDslDraftRuleCount,
                lastDslDraftValidatorPassed,
                lastGeneratedOverlayStagingDirectory,
                lastGeneratedOverlayStagingStatusPath,
                lastGeneratedOverlayStagingReadiness,
                lastGeneratedOverlayStagingRuleCount,
                lastGeneratedOverlayManifestVerified,
                lastGeneratedOverlayPublishAllowed,
                lastGeneratedOverlayManifestHash,
                lastMpPackageRefreshState,
                lastMpPackageRefreshReason);
        } else {
            writeStatus(
                hwnd,
                "waiting_for_stellaris",
                "SNC tray bezi; ceka na stellaris.exe.",
                false,
                sessionId,
                sessionDirectory,
                0,
                0,
                copiedTotal,
                skippedTotal,
                postPlayState,
                archiveVerified,
                lastEntryPointAnalysisPath,
                lastEntryPointCount,
                lastBranchAmbiguityDetected,
                lastEntryPointReadiness,
                lastPostPlayPackagePath,
                lastPostPlayPackageReadiness,
                lastPostPlayDecisionReadyEntryCount,
                lastPostPlayCampaignCount,
                lastPostPlayReadyCampaignCount,
                lastPostPlayPartialCampaignCount,
                lastPostPlayBlockedCampaignCount,
                lastPostPlayCampaignSummaries,
                lastDecisionInputPackagePath,
                lastDecisionInputPackageReadiness,
                lastDecisionInputCount,
                lastCandidateDecisionPackagePath,
                lastCandidateDecisionPackageReadiness,
                lastCandidateDecisionCount,
                lastCandidateDecisionValidatorPassed,
                lastDslDraftPath,
                lastDslDraftAuditPath,
                lastDslDraftReadiness,
                lastDslDraftRuleCount,
                lastDslDraftValidatorPassed,
                lastGeneratedOverlayStagingDirectory,
                lastGeneratedOverlayStagingStatusPath,
                lastGeneratedOverlayStagingReadiness,
                lastGeneratedOverlayStagingRuleCount,
                lastGeneratedOverlayManifestVerified,
                lastGeneratedOverlayPublishAllowed,
                lastGeneratedOverlayManifestHash,
                lastMpPackageRefreshState,
                lastMpPackageRefreshReason);
        }

        wasRunning = running;
        sleepResponsive(std::chrono::milliseconds(1000));
    }

    writeStatus(
        hwnd,
        "stopping",
        "SNC tray se ukoncuje.",
        false,
        sessionId,
        sessionDirectory,
        0,
        0,
        copiedTotal,
        skippedTotal,
        postPlayState,
        archiveVerified,
        lastEntryPointAnalysisPath,
        lastEntryPointCount,
        lastBranchAmbiguityDetected,
        lastEntryPointReadiness,
        lastPostPlayPackagePath,
        lastPostPlayPackageReadiness,
        lastPostPlayDecisionReadyEntryCount,
        lastPostPlayCampaignCount,
        lastPostPlayReadyCampaignCount,
        lastPostPlayPartialCampaignCount,
        lastPostPlayBlockedCampaignCount,
        lastPostPlayCampaignSummaries,
        lastDecisionInputPackagePath,
        lastDecisionInputPackageReadiness,
        lastDecisionInputCount,
        lastCandidateDecisionPackagePath,
        lastCandidateDecisionPackageReadiness,
        lastCandidateDecisionCount,
        lastCandidateDecisionValidatorPassed,
        lastDslDraftPath,
        lastDslDraftAuditPath,
        lastDslDraftReadiness,
        lastDslDraftRuleCount,
        lastDslDraftValidatorPassed,
        lastGeneratedOverlayStagingDirectory,
        lastGeneratedOverlayStagingStatusPath,
        lastGeneratedOverlayStagingReadiness,
        lastGeneratedOverlayStagingRuleCount,
        lastGeneratedOverlayManifestVerified,
        lastGeneratedOverlayPublishAllowed,
        lastGeneratedOverlayManifestHash,
        lastMpPackageRefreshState,
        lastMpPackageRefreshReason);
}

bool addTrayIcon(HWND hwnd)
{
    g_trayIcon = {};
    g_trayIcon.cbSize = sizeof(g_trayIcon);
    g_trayIcon.hWnd = hwnd;
    g_trayIcon.uID = 1;
    g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayIcon.uCallbackMessage = WM_SNC_TRAY;
    g_trayIcon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_trayIcon.szTip, L"SNC - ceka na Stellaris");

    if (!Shell_NotifyIconW(NIM_ADD, &g_trayIcon)) {
        return false;
    }

    g_trayIcon.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_trayIcon);
    return true;
}

void removeTrayIcon()
{
    if (g_trayIcon.cbSize != 0) {
        Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
    }
}

void showStatusDialog(HWND hwnd)
{
    std::wstring text;
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        text = g_statusText;
    }

    text += L"\n\nStatus JSON:\n";
    text += g_trayStatusPath.wstring();
    text += L"\n\nArchiv:\n";
    text += g_archiveRoot.wstring();
    text += L"\n\nStaged overlay status:\n";
    text += g_generatedOverlayStagingStatusPath.wstring();
    text += L"\n\nActive overlay snapshot:\n";
    text += g_generatedOverlayActiveDirectory.wstring();
    text += L"\n\nDalsi kroky:\n";
    text += g_nextStepsBriefPath.wstring();

    MessageBoxW(hwnd, text.c_str(), g_statusTitle.c_str(), MB_OK | MB_ICONINFORMATION);
}

void openArchiveDirectory()
{
    std::error_code error;
    std::filesystem::create_directories(g_archiveRoot, error);
    ShellExecuteW(nullptr, L"open", g_archiveRoot.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void publishStagedGeneratedOverlay(HWND hwnd)
{
    if (!std::filesystem::exists(g_generatedOverlayStagingStatusPath)) {
        MessageBoxW(
            hwnd,
            L"Staged generated overlay status zatim neexistuje.\n\nNejdriv nech SNC dokoncit post-play pipeline po ukonceni Stellaris.",
            L"Strategic Nexus Companion",
            MB_OK | MB_ICONWARNING);
        return;
    }

    const int confirmation = MessageBoxW(
        hwnd,
        L"Publikovat staged generated overlay do active snapshotu?\n\nSNC zkontroluje, ze Stellaris nebezi, vytvori zalohu existujiciho active snapshotu a zapise audit status.",
        L"Strategic Nexus Companion",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (confirmation != IDYES) {
        return;
    }

    bool stellarisRunning = true;
    std::string processReason = "Stellaris process detection unavailable";
    {
        const strategic_nexus::StellarisProcessDetector detector;
        const auto processStatus = detector.detectFromSystem();
        processReason = processStatus.reason;
        if (processStatus.detectionAvailable) {
            stellarisRunning = processStatus.running;
        }
    }

    strategic_nexus::SncGeneratedOverlayPublishGateRequest request;
    request.stagingStatusPath = g_generatedOverlayStagingStatusPath;
    request.activeOverlayDirectory = g_generatedOverlayActiveDirectory;
    request.backupRootDirectory = g_generatedOverlayPublishBackupRootDirectory;
    request.ownerApprovalToken = "owner-approved";
    request.stellarisRunning = stellarisRunning;

    const strategic_nexus::SncGeneratedOverlayPublishGate gate;
    const auto result = gate.publish(request);
    const bool statusWritten = strategic_nexus::common::writeTextFileAtomically(
        g_generatedOverlayPublishStatusPath,
        strategic_nexus::serializeSncGeneratedOverlayPublishGateResult(result));

    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_statusText = utf8ToWide(result.reason);
    }
    updateTrayTip(hwnd, utf8ToWide(result.reason));

    std::wstring message = L"Vysledek: ";
    message += utf8ToWide(result.reason);
    message += L"\n\nStellaris: ";
    message += stellarisRunning ? L"bezi nebo nebylo mozne bezpecne overit" : L"nebezi";
    if (!processReason.empty()) {
        message += L"\nDetekce: ";
        message += utf8ToWide(processReason);
    }
    message += L"\n\nActive overlay:\n";
    message += g_generatedOverlayActiveDirectory.wstring();
    message += L"\n\nStatus:\n";
    message += g_generatedOverlayPublishStatusPath.wstring();
    message += statusWritten ? L"\n\nAudit status zapsan." : L"\n\nAudit status se nepodarilo zapsat.";

    MessageBoxW(
        hwnd,
        message.c_str(),
        L"Strategic Nexus Companion",
        MB_OK | (result.ok && statusWritten ? MB_ICONINFORMATION : MB_ICONWARNING));
}

void showTrayMenu(HWND hwnd)
{
    POINT point{};
    GetCursorPos(&point);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_STATUS, L"Stav");
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_ARCHIVE, L"Otevrit archiv");
    AppendMenuW(menu, MF_STRING, ID_TRAY_PUBLISH_GENERATED_OVERLAY, L"Publikovat staged overlay");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Konec");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, point.x, point.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == g_taskbarCreatedMessage) {
        addTrayIcon(hwnd);
        return 0;
    }

    switch (message) {
    case WM_CREATE:
        addTrayIcon(hwnd);
        g_worker = std::thread(workerLoop, hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_STATUS:
            showStatusDialog(hwnd);
            return 0;
        case ID_TRAY_OPEN_ARCHIVE:
            openArchiveDirectory();
            return 0;
        case ID_TRAY_PUBLISH_GENERATED_OVERLAY:
            publishStagedGeneratedOverlay(hwnd);
            return 0;
        case ID_TRAY_EXIT:
            DestroyWindow(hwnd);
            return 0;
        default:
            return 0;
        }

    case WM_SNC_TRAY:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            showTrayMenu(hwnd);
        } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            showStatusDialog(hwnd);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_stopRequested.store(true);
        if (g_worker.joinable()) {
            g_worker.join();
        }
        removeTrayIcon();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void initializePaths()
{
    g_repoRoot = findRepoRoot();
    g_archiveRoot = g_repoRoot / "dist" / "snc_live_autosave_archive";
    g_trayStatusPath = g_repoRoot / "dist" / "private_reports" / "snc_tray_status.json";
    g_liveStatusPath = g_repoRoot / "dist" / "private_reports" / "snc_live_autosave_monitor_status.json";
    g_lastSummaryPath = g_repoRoot / "dist" / "private_reports" / "snc_last_capture_summary.json";
    g_entryPointAnalysisPath = g_repoRoot / "dist" / "private_reports" / "snc_entry_point_analysis.json";
    g_postPlayPackagePath = g_repoRoot / "dist" / "private_reports" / "snc_post_play_package.json";
    g_decisionInputPackagePath = g_repoRoot / "dist" / "private_reports" / "snc_decision_input_package.json";
    g_candidateDecisionPackagePath = g_repoRoot / "dist" / "private_reports" / "snc_candidate_decision_package.json";
    g_dslDraftPath = g_repoRoot / "dist" / "private_reports" / "snc_validated_dsl_draft.dsl";
    g_dslDraftAuditPath = g_repoRoot / "dist" / "private_reports" / "snc_dsl_draft_package.json";
    g_nextStepsBriefPath = g_repoRoot / "dist" / "private_reports" / "snc_next_steps_brief.txt";
    g_generatedOverlayStagingDirectory = g_repoRoot / "dist" / "private_reports" / "snc_generated_overlay_staged";
    g_generatedOverlayStagingStatusPath = g_repoRoot / "dist" / "private_reports" / "snc_generated_overlay_staging_status.json";
    g_generatedOverlayActiveDirectory = g_repoRoot / "dist" / "private_reports" / "snc_generated_overlay_active";
    g_generatedOverlayPublishStatusPath = g_repoRoot / "dist" / "private_reports" / "snc_generated_overlay_publish_status.json";
    g_generatedOverlayPublishBackupRootDirectory = g_repoRoot / "dist" / "private_reports" / "snc_generated_overlay_backups";
    g_mpOverlayPackageDirectory = g_repoRoot / "dist" / "private_reports" / "snc_mp_overlay_package";
    g_mpOverlayPackageZipPath = g_repoRoot / "dist" / "private_reports" / "snc_mp_overlay_package.zip";
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    initializePaths();

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"StrategicNexusCompanionTraySingleInstance");
    if (mutex == nullptr) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"SNC uz bezi.", L"Strategic Nexus Companion", MB_OK | MB_ICONINFORMATION);
        CloseHandle(mutex);
        return 0;
    }

    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"StrategicNexusCompanionTrayWindow";
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    if (RegisterClassW(&windowClass) == 0) {
        CloseHandle(mutex);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"Strategic Nexus Companion",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr) {
        CloseHandle(mutex);
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
