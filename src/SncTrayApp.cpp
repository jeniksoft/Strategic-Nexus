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
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace {

constexpr UINT WM_SNC_TRAY = WM_APP + 10;
constexpr UINT WM_SNC_TASKBAR_CREATED = WM_APP + 11;
constexpr UINT WM_SNC_STATUS_REFRESH = WM_APP + 12;
constexpr UINT ID_TRAY_STATUS = 101;
constexpr UINT ID_TRAY_OPEN_ARCHIVE = 102;
constexpr UINT ID_TRAY_EXIT = 103;
constexpr UINT ID_TRAY_PUBLISH_GENERATED_OVERLAY = 104;
constexpr UINT ID_STATUS_REFRESH = 201;
constexpr UINT ID_STATUS_COPY = 202;
constexpr UINT ID_STATUS_OPEN_ARCHIVE = 203;
constexpr UINT ID_STATUS_OPEN_BRIEF = 204;
constexpr UINT ID_STATUS_CLOSE = 205;

enum class StatusFieldId : std::size_t {
    LiveState = 0,
    LiveStellaris,
    LiveSession,
    LiveStartup,
    ArchivePath,
    ArchiveVerified,
    ArchiveCopied,
    ArchiveSkipped,
    OverlayStaging,
    OverlayGate,
    OverlayAllowed,
    OverlayActive,
    ModelState,
    ModelRuntime,
    ModelRecommendation,
    ModelMode,
    Count
};

struct StatusDashboardData {
    std::wstring subtitle;
    std::wstring liveState;
    std::wstring liveStellaris;
    std::wstring liveSession;
    std::wstring liveStartup;
    std::wstring archivePath;
    std::wstring archiveVerified;
    std::wstring archiveCopied;
    std::wstring archiveSkipped;
    std::wstring overlayStaging;
    std::wstring overlayGate;
    std::wstring overlayAllowed;
    std::wstring overlayActive;
    std::wstring modelState;
    std::wstring modelRuntime;
    std::wstring modelRecommendation;
    std::wstring modelMode;
    std::wstring nextAction;
    std::wstring nextReason;
    std::wstring nextHint;
    std::wstring nextPlaybook;
    std::wstring nextActionPath;
};

struct DashboardSectionSpec {
    const wchar_t* title;
    std::array<StatusFieldId, 4> fields;
};

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
std::filesystem::path g_gameplayAcceptanceReportPath;
std::filesystem::path g_mpOverlayPackageDirectory;
std::filesystem::path g_mpOverlayPackageZipPath;
std::filesystem::path g_trayIconPath;
std::array<HWND, static_cast<std::size_t>(StatusFieldId::Count)> g_statusFieldLabels{};
std::array<HWND, static_cast<std::size_t>(StatusFieldId::Count)> g_statusFieldValues{};
std::array<HWND, 4> g_statusSectionTitles{};
HWND g_statusBottomTitle = nullptr;
HWND g_statusRefreshButton = nullptr;
HWND g_statusCopyButton = nullptr;
HWND g_statusOpenArchiveButton = nullptr;
HWND g_statusOpenBriefButton = nullptr;
HWND g_statusCloseButton = nullptr;
HWND g_statusWindow = nullptr;
HWND g_statusHeader = nullptr;
HWND g_statusSubtitle = nullptr;
HWND g_statusDetails = nullptr;
HFONT g_statusHeaderFont = nullptr;
HFONT g_statusSectionFont = nullptr;
HFONT g_statusFieldFont = nullptr;
HFONT g_statusSubtitleFont = nullptr;
HFONT g_statusDetailsFont = nullptr;
HBRUSH g_statusBackgroundBrush = nullptr;
HBRUSH g_statusPanelBrush = nullptr;
HBRUSH g_statusAccentBrush = nullptr;
HBRUSH g_statusBorderBrush = nullptr;
HBRUSH g_statusValueBrush = nullptr;
const std::array<DashboardSectionSpec, 4> kStatusSections = {
    DashboardSectionSpec{L"\u017Div\u00E9 hran\u00ED", {StatusFieldId::LiveState, StatusFieldId::LiveStellaris, StatusFieldId::LiveSession, StatusFieldId::LiveStartup}},
    DashboardSectionSpec{L"Archiv", {StatusFieldId::ArchivePath, StatusFieldId::ArchiveVerified, StatusFieldId::ArchiveCopied, StatusFieldId::ArchiveSkipped}},
    DashboardSectionSpec{L"Overlay", {StatusFieldId::OverlayStaging, StatusFieldId::OverlayGate, StatusFieldId::OverlayAllowed, StatusFieldId::OverlayActive}},
    DashboardSectionSpec{L"Model", {StatusFieldId::ModelState, StatusFieldId::ModelRuntime, StatusFieldId::ModelRecommendation, StatusFieldId::ModelMode}},
};
constexpr const char* kTrayMpOverlayVersion = "overlay_v0_session";
constexpr const char* kTrayMpGameVersion = "stellaris_4.x";
constexpr const char* kTrayMpModVersion = "strategic_nexus_v0";
constexpr COLORREF kStatusBackgroundColor = RGB(8, 15, 17);
constexpr COLORREF kStatusPanelColor = RGB(12, 29, 33);
constexpr COLORREF kStatusTextColor = RGB(232, 239, 240);
constexpr COLORREF kStatusMutedColor = RGB(146, 162, 164);
constexpr COLORREF kStatusAccentColor = RGB(214, 170, 76);
constexpr COLORREF kStatusBorderColor = RGB(26, 71, 76);
constexpr COLORREF kStatusValueBackgroundColor = RGB(7, 18, 20);
constexpr COLORREF kStatusValueBorderColor = RGB(39, 72, 74);
constexpr const wchar_t* kStatusEmptyValue = L"\u2014";
constexpr wchar_t kStatusWindowClassName[] = L"StrategicNexusCompanionStatusWindow";
constexpr int kSncTrayIconResourceId = 1;
NOTIFYICONDATAW g_trayIcon{};
HICON g_sncTrayIcon = nullptr;
UINT g_taskbarCreatedMessage = 0;

std::wstring buildStatusDialogText();
std::wstring buildStatusSubtitleText();
std::wstring buildDashboardCopyText(const StatusDashboardData& data);
std::wstring buildDashboardBottomText(const StatusDashboardData& data);
std::wstring utf8ToWide(const std::string& value);
StatusDashboardData loadStatusDashboardData();
std::optional<std::string> extractSummaryLineValue(const std::string& summary, const char* key);
std::wstring makeRelativeDisplay(const std::filesystem::path& path);
std::wstring compactBoolText(const std::string& value, const wchar_t* yesText = L"ano", const wchar_t* noText = L"ne");
std::wstring combineValues(const std::vector<std::wstring>& values, const wchar_t* separator = L" | ");
const wchar_t* statusFieldLabel(StatusFieldId id);
void setWindowFont(HWND hwnd, HFONT font);
HWND createStatusStatic(HWND parent, const wchar_t* text, DWORD style = WS_CHILD | WS_VISIBLE);
HWND createStatusValue(HWND parent, const wchar_t* text = L"");
HWND createStatusButton(HWND parent, UINT id, const wchar_t* text);
void drawStatusButton(const DRAWITEMSTRUCT& drawItem);
void openPathWithShell(HWND hwnd, const std::filesystem::path& path);
bool copyTextToClipboard(HWND hwnd, const std::wstring& text);
void ensureStatusWindowResources();
void destroyStatusWindowResources();
void refreshStatusWindowContent();
void requestStatusWindowRefresh();
void layoutStatusWindow(HWND hwnd);
void registerStatusWindowClass();
void applyWindowIcons(HWND hwnd);
LRESULT CALLBACK statusWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

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

std::string pathString(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::wstring buildStatusDialogText()
{
    return buildDashboardBottomText(loadStatusDashboardData());
}

std::wstring buildStatusSubtitleText()
{
    std::wstring subtitle;
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        subtitle = g_statusText;
    }
    if (subtitle.empty()) {
        subtitle = L"SNC bezi.";
    }
    return L"Stav: " + subtitle;
}

std::optional<std::string> extractSummaryLineValue(const std::string& summary, const char* key)
{
    const std::string prefix = std::string(key) + ": ";
    std::istringstream stream(summary);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind(prefix, 0) == 0) {
            return line.substr(prefix.size());
        }
    }
    return std::nullopt;
}

std::wstring makeRelativeDisplay(const std::filesystem::path& path)
{
    if (path.empty()) {
        return kStatusEmptyValue;
    }

    std::error_code error;
    const auto relative = std::filesystem::relative(path, g_repoRoot, error);
    if (!error && !relative.empty()) {
        return utf8ToWide(pathString(relative));
    }
    return utf8ToWide(pathString(path));
}

std::wstring compactBoolText(const std::string& value, const wchar_t* yesText, const wchar_t* noText)
{
    if (value == "true") {
        return yesText;
    }
    if (value == "false") {
        return noText;
    }
    return value.empty() ? kStatusEmptyValue : utf8ToWide(value);
}

std::wstring combineValues(const std::vector<std::wstring>& values, const wchar_t* separator)
{
    std::wstring output;
    for (const auto& value : values) {
        if (value.empty() || value == kStatusEmptyValue) {
            continue;
        }
        if (!output.empty()) {
            output += separator;
        }
        output += value;
    }
    return output.empty() ? kStatusEmptyValue : output;
}

const wchar_t* statusFieldLabel(const StatusFieldId id)
{
    switch (id) {
    case StatusFieldId::LiveState: return L"Stav";
    case StatusFieldId::LiveStellaris: return L"Stellaris";
    case StatusFieldId::LiveSession: return L"Session";
    case StatusFieldId::LiveStartup: return L"Start";
    case StatusFieldId::ArchivePath: return L"Cesta";
    case StatusFieldId::ArchiveVerified: return L"Ověřeno";
    case StatusFieldId::ArchiveCopied: return L"Kopie";
    case StatusFieldId::ArchiveSkipped: return L"Přeskočeno";
    case StatusFieldId::OverlayStaging: return L"Staging";
    case StatusFieldId::OverlayGate: return L"Gate";
    case StatusFieldId::OverlayAllowed: return L"Povoleno";
    case StatusFieldId::OverlayActive: return L"Aktivní";
    case StatusFieldId::ModelState: return L"Stav";
    case StatusFieldId::ModelRuntime: return L"Runtime";
    case StatusFieldId::ModelRecommendation: return L"Doporučení";
    case StatusFieldId::ModelMode: return L"Režim";
    case StatusFieldId::Count: break;
    }
    return L"";
}

void setWindowFont(HWND hwnd, HFONT font)
{
    if (hwnd != nullptr && font != nullptr) {
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

HWND createStatusStatic(HWND parent, const wchar_t* text, DWORD style)
{
    return CreateWindowExW(
        0,
        L"STATIC",
        text,
        style,
        0,
        0,
        0,
        0,
        parent,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
}

HWND createStatusValue(HWND parent, const wchar_t* text)
{
    return CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        text,
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY | ES_AUTOHSCROLL | ES_NOHIDESEL,
        0,
        0,
        0,
        0,
        parent,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
}

HWND createStatusButton(HWND parent, UINT id, const wchar_t* text)
{
    return CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
}

void drawStatusButton(const DRAWITEMSTRUCT& drawItem)
{
    if (drawItem.hDC == nullptr) {
        return;
    }

    const bool disabled = (drawItem.itemState & ODS_DISABLED) != 0;
    const bool selected = (drawItem.itemState & ODS_SELECTED) != 0;
    const bool focused = (drawItem.itemState & ODS_FOCUS) != 0;

    RECT rc = drawItem.rcItem;
    const COLORREF fillColor = disabled
        ? RGB(6, 11, 13)
        : (selected ? RGB(16, 39, 42) : kStatusPanelColor);

    const HBRUSH fillBrush = CreateSolidBrush(fillColor);
    FillRect(drawItem.hDC, &rc, fillBrush);
    DeleteObject(fillBrush);

    const HBRUSH borderBrush = disabled ? g_statusBorderBrush : (selected ? g_statusAccentBrush : g_statusBorderBrush);
    FrameRect(drawItem.hDC, &rc, borderBrush);

    if (focused) {
        RECT focusRc = rc;
        InflateRect(&focusRc, -3, -3);
        DrawFocusRect(drawItem.hDC, &focusRc);
    }

    wchar_t label[128]{};
    GetWindowTextW(drawItem.hwndItem, label, static_cast<int>(std::size(label)));

    SetBkMode(drawItem.hDC, TRANSPARENT);
    SetTextColor(drawItem.hDC, disabled ? kStatusMutedColor : kStatusTextColor);
    const HGDIOBJ oldFont = SelectObject(drawItem.hDC, g_statusSectionFont != nullptr ? g_statusSectionFont : GetStockObject(DEFAULT_GUI_FONT));
    rc.left += 10;
    rc.right -= 10;
    DrawTextW(drawItem.hDC, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (oldFont != nullptr) {
        SelectObject(drawItem.hDC, oldFont);
    }
}

void openPathWithShell(HWND hwnd, const std::filesystem::path& path)
{
    if (path.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    const HINSTANCE result = ShellExecuteW(hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        MessageBeep(MB_ICONWARNING);
    }
}

StatusDashboardData loadStatusDashboardData()
{
    StatusDashboardData data;
    data.subtitle = L"Stav: SNC bezi.";
    data.liveState = L"SNC bezi.";
    data.liveStellaris = L"—";
    data.liveSession = L"—";
    data.liveStartup = L"—";
    data.archivePath = makeRelativeDisplay(g_archiveRoot);
    data.archiveVerified = L"—";
    data.archiveCopied = L"0";
    data.archiveSkipped = L"0";
    data.overlayStaging = L"—";
    data.overlayGate = L"—";
    data.overlayAllowed = L"—";
    data.overlayActive = makeRelativeDisplay(g_generatedOverlayActiveDirectory);
    data.modelState = L"—";
    data.modelRuntime = L"—";
    data.modelRecommendation = L"—";
    data.modelMode = L"—";
    data.nextAction = L"—";
    data.nextReason = L"—";
    data.nextHint = L"—";
    data.nextPlaybook = g_nextStepsBriefPath.empty() ? L"—" : utf8ToWide(pathString(g_nextStepsBriefPath));
    data.nextActionPath = L"—";

    std::string json;
    if (!strategic_nexus::common::tryReadTextFile(g_trayStatusPath, json)) {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        const std::wstring fallback = g_statusText.empty() ? L"SNC bezi." : g_statusText;
        data.subtitle = L"Stav: " + fallback;
        data.liveState = fallback;
        return data;
    }

    const auto summaryText = strategic_nexus::common::extractJsonString(json, "status_center_summary_text").value_or("");
    const auto summaryValue = [&](const char* key) -> std::string {
        return extractSummaryLineValue(summaryText, key).value_or("");
    };

    const std::string stateLine = summaryValue("stav");
    if (!stateLine.empty()) {
        data.subtitle = L"Stav: " + utf8ToWide(stateLine);
        data.liveState = utf8ToWide(stateLine);
    }

    data.liveStellaris = compactBoolText(summaryValue("stellaris_running"));
    data.liveSession = utf8ToWide(summaryValue("session_id"));
    data.liveStartup = utf8ToWide(summaryValue("startup_lifecycle_state"));
    if (data.liveStartup == L"owner_enabled_start_with_windows") {
        data.liveStartup = L"spustit pri startu Windows";
    } else if (data.liveStartup == L"manual_start_only") {
        data.liveStartup = L"rucni start";
    }

    const std::string archiveRoot = summaryValue("archive_root");
    if (!archiveRoot.empty()) {
        data.archivePath = utf8ToWide(archiveRoot);
    }
    data.archiveVerified = compactBoolText(summaryValue("archive_verified"));
    data.archiveCopied = utf8ToWide(summaryValue("copied_count_total"));
    data.archiveSkipped = utf8ToWide(summaryValue("skipped_count_total"));

    data.overlayStaging = utf8ToWide(summaryValue("generated_overlay_staging_readiness"));
    const std::string publishGateState = summaryValue("generated_overlay_publish_gate_state");
    const std::string publishGateReason = summaryValue("generated_overlay_publish_gate_reason");
    if (!publishGateState.empty() && !publishGateReason.empty()) {
        data.overlayGate = utf8ToWide(publishGateState) + L" - " + utf8ToWide(publishGateReason);
    } else if (!publishGateState.empty()) {
        data.overlayGate = utf8ToWide(publishGateState);
    }
    data.overlayAllowed = compactBoolText(summaryValue("generated_overlay_publish_allowed"));
    const std::string activeDirectory = summaryValue("generated_overlay_active_directory");
    if (!activeDirectory.empty()) {
        data.overlayActive = utf8ToWide(activeDirectory);
    }

    const std::string selectedDisplayName = summaryValue("local_llm_selected_display_name");
    const std::string selectedModelId = summaryValue("local_llm_selected_model_id");
    const std::string localLlmState = summaryValue("local_llm_model");
    if (!selectedDisplayName.empty()) {
        data.modelState = utf8ToWide(selectedDisplayName);
    } else if (!selectedModelId.empty()) {
        data.modelState = utf8ToWide(selectedModelId);
    } else if (!localLlmState.empty()) {
        data.modelState = utf8ToWide(localLlmState);
    }
    const std::string runtime = summaryValue("local_llm_runtime");
    const std::string canRunInference = summaryValue("local_llm_can_run_inference");
    const std::string reducedMode = summaryValue("local_llm_reduced_mode");
    const std::string userActionRequired = summaryValue("local_llm_user_action_required");
    std::vector<std::wstring> runtimeParts;
    if (!runtime.empty()) {
        runtimeParts.push_back(utf8ToWide(runtime));
    }
    if (!canRunInference.empty()) {
        runtimeParts.push_back(L"inference " + compactBoolText(canRunInference));
    }
    data.modelRuntime = combineValues(runtimeParts);

    const std::string recommendedDisplayName = summaryValue("local_llm_recommended_display_name");
    const std::string recommendedModelId = summaryValue("local_llm_recommended_model_id");
    const std::string recommendedRuntime = summaryValue("local_llm_recommended_runtime");
    std::vector<std::wstring> recommendationParts;
    if (!recommendedDisplayName.empty()) {
        recommendationParts.push_back(utf8ToWide(recommendedDisplayName));
    }
    if (!recommendedModelId.empty()) {
        recommendationParts.push_back(utf8ToWide(recommendedModelId));
    }
    if (!recommendedRuntime.empty()) {
        recommendationParts.push_back(utf8ToWide(recommendedRuntime));
    }
    data.modelRecommendation = combineValues(recommendationParts);

    std::vector<std::wstring> modeParts;
    if (!reducedMode.empty()) {
        modeParts.push_back(L"reduced " + compactBoolText(reducedMode));
    }
    if (!userActionRequired.empty()) {
        modeParts.push_back(L"user action " + compactBoolText(userActionRequired));
    }
    if (modeParts.empty()) {
        const std::string downloadAllowed = summaryValue("local_llm_download_allowed");
        if (!downloadAllowed.empty()) {
            modeParts.push_back(L"download " + compactBoolText(downloadAllowed));
        }
    }
    data.modelMode = combineValues(modeParts);

    data.nextAction = utf8ToWide(strategic_nexus::common::extractJsonString(json, "next_action").value_or(""));
    data.nextReason = utf8ToWide(strategic_nexus::common::extractJsonString(json, "next_action_reason").value_or(""));
    data.nextHint = utf8ToWide(strategic_nexus::common::extractJsonString(json, "next_action_command_hint").value_or(""));
    data.nextPlaybook = utf8ToWide(strategic_nexus::common::extractJsonString(json, "owner_test_playbook_path").value_or(""));
    data.nextActionPath = utf8ToWide(strategic_nexus::common::extractJsonString(json, "next_action_path").value_or(""));
    return data;
}

std::wstring buildDashboardBottomText(const StatusDashboardData& data)
{
    std::wstring text = L"Další krok: ";
    text += data.nextAction.empty() ? kStatusEmptyValue : data.nextAction;
    text += L"\r\nDůvod: ";
    text += data.nextReason.empty() ? kStatusEmptyValue : data.nextReason;
    text += L"\r\nHint: ";
    text += data.nextHint.empty() ? kStatusEmptyValue : data.nextHint;
    text += L"\r\nPlaybook: ";
    text += data.nextPlaybook.empty() ? kStatusEmptyValue : data.nextPlaybook;
    text += L"\r\nCíl: ";
    text += data.nextActionPath.empty() ? kStatusEmptyValue : data.nextActionPath;
    return text;
}

std::wstring buildDashboardCopyText(const StatusDashboardData& data)
{
    std::wstring text;
    text += L"Strategic Nexus Companion\n";
    text += L"Stav: " + (data.liveState.empty() ? kStatusEmptyValue : data.liveState) + L"\n";
    text += L"Stellaris: " + data.liveStellaris + L"\n";
    text += L"Session: " + data.liveSession + L"\n";
    text += L"Start: " + data.liveStartup + L"\n";
    text += L"Archiv: " + data.archivePath + L"\n";
    text += L"Publikace: " + data.overlayGate + L"\n";
    text += L"LLM: " + data.modelState + L"\n";
    text += L"Další krok: " + data.nextAction + L"\n";
    text += L"Hint: " + data.nextHint;
    return text;
}

bool copyTextToClipboard(HWND hwnd, const std::wstring& text)
{
    if (!OpenClipboard(hwnd)) {
        return false;
    }

    EmptyClipboard();
    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
        CloseClipboard();
        return false;
    }

    void* locked = GlobalLock(memory);
    if (locked == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    memcpy(locked, text.c_str(), bytes);
    GlobalUnlock(memory);
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

HFONT createUiFont(const int pointSize, const int weight, const wchar_t* faceName)
{
    const HDC screen = GetDC(nullptr);
    const int dpiY = screen != nullptr ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
    if (screen != nullptr) {
        ReleaseDC(nullptr, screen);
    }
    return CreateFontW(
        -MulDiv(pointSize, dpiY, 72),
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        faceName);
}

void ensureStatusWindowResources()
{
    if (g_statusBackgroundBrush == nullptr) {
        g_statusBackgroundBrush = CreateSolidBrush(kStatusBackgroundColor);
    }
    if (g_statusPanelBrush == nullptr) {
        g_statusPanelBrush = CreateSolidBrush(kStatusPanelColor);
    }
    if (g_statusValueBrush == nullptr) {
        g_statusValueBrush = CreateSolidBrush(kStatusValueBackgroundColor);
    }
    if (g_statusAccentBrush == nullptr) {
        g_statusAccentBrush = CreateSolidBrush(kStatusAccentColor);
    }
    if (g_statusBorderBrush == nullptr) {
        g_statusBorderBrush = CreateSolidBrush(kStatusBorderColor);
    }
    if (g_statusHeaderFont == nullptr) {
        g_statusHeaderFont = createUiFont(18, FW_SEMIBOLD, L"Bahnschrift");
    }
    if (g_statusSectionFont == nullptr) {
        g_statusSectionFont = createUiFont(11, FW_SEMIBOLD, L"Bahnschrift");
    }
    if (g_statusFieldFont == nullptr) {
        g_statusFieldFont = createUiFont(9, FW_NORMAL, L"Bahnschrift");
    }
    if (g_statusSubtitleFont == nullptr) {
        g_statusSubtitleFont = createUiFont(9, FW_NORMAL, L"Bahnschrift");
    }
    if (g_statusDetailsFont == nullptr) {
        g_statusDetailsFont = createUiFont(9, FW_NORMAL, L"Cascadia Mono");
    }
}

void destroyStatusWindowResources()
{
    if (g_statusHeaderFont != nullptr) {
        DeleteObject(g_statusHeaderFont);
        g_statusHeaderFont = nullptr;
    }
    if (g_statusSectionFont != nullptr) {
        DeleteObject(g_statusSectionFont);
        g_statusSectionFont = nullptr;
    }
    if (g_statusFieldFont != nullptr) {
        DeleteObject(g_statusFieldFont);
        g_statusFieldFont = nullptr;
    }
    if (g_statusSubtitleFont != nullptr) {
        DeleteObject(g_statusSubtitleFont);
        g_statusSubtitleFont = nullptr;
    }
    if (g_statusDetailsFont != nullptr) {
        DeleteObject(g_statusDetailsFont);
        g_statusDetailsFont = nullptr;
    }
    if (g_statusPanelBrush != nullptr) {
        DeleteObject(g_statusPanelBrush);
        g_statusPanelBrush = nullptr;
    }
    if (g_statusAccentBrush != nullptr) {
        DeleteObject(g_statusAccentBrush);
        g_statusAccentBrush = nullptr;
    }
    if (g_statusBorderBrush != nullptr) {
        DeleteObject(g_statusBorderBrush);
        g_statusBorderBrush = nullptr;
    }
    if (g_statusValueBrush != nullptr) {
        DeleteObject(g_statusValueBrush);
        g_statusValueBrush = nullptr;
    }
    if (g_statusBackgroundBrush != nullptr) {
        DeleteObject(g_statusBackgroundBrush);
        g_statusBackgroundBrush = nullptr;
    }
}

void refreshStatusWindowContent()
{
    if (g_statusWindow == nullptr) {
        return;
    }

    const auto data = loadStatusDashboardData();
    const auto setField = [](HWND hwnd, const std::wstring& value) {
        if (hwnd != nullptr) {
            SetWindowTextW(hwnd, value.c_str());
        }
    };

    if (g_statusHeader != nullptr) {
        SetWindowTextW(g_statusHeader, g_statusTitle.c_str());
    }
    if (g_statusSubtitle != nullptr) {
        SetWindowTextW(g_statusSubtitle, data.subtitle.c_str());
    }

    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::LiveState)], data.liveState);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::LiveStellaris)], data.liveStellaris);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::LiveSession)], data.liveSession);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::LiveStartup)], data.liveStartup);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::ArchivePath)], data.archivePath);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::ArchiveVerified)], data.archiveVerified);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::ArchiveCopied)], data.archiveCopied);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::ArchiveSkipped)], data.archiveSkipped);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::OverlayStaging)], data.overlayStaging);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::OverlayGate)], data.overlayGate);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::OverlayAllowed)], data.overlayAllowed);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::OverlayActive)], data.overlayActive);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::ModelState)], data.modelState);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::ModelRuntime)], data.modelRuntime);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::ModelRecommendation)], data.modelRecommendation);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::ModelMode)], data.modelMode);

    if (g_statusBottomTitle != nullptr) {
        SetWindowTextW(g_statusBottomTitle, L"Další krok");
    }
    if (g_statusDetails != nullptr) {
        const auto details = buildDashboardBottomText(data);
        SetWindowTextW(g_statusDetails, details.c_str());
    }

    const bool briefAvailable = !g_nextStepsBriefPath.empty() && std::filesystem::exists(g_nextStepsBriefPath);
    if (g_statusRefreshButton != nullptr) {
        EnableWindow(g_statusRefreshButton, TRUE);
    }
    if (g_statusCopyButton != nullptr) {
        EnableWindow(g_statusCopyButton, TRUE);
    }
    if (g_statusOpenArchiveButton != nullptr) {
        EnableWindow(g_statusOpenArchiveButton, TRUE);
    }
    if (g_statusOpenBriefButton != nullptr) {
        EnableWindow(g_statusOpenBriefButton, briefAvailable);
    }
    if (g_statusCloseButton != nullptr) {
        EnableWindow(g_statusCloseButton, TRUE);
    }

    RedrawWindow(g_statusWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
}

void requestStatusWindowRefresh()
{
    if (g_statusWindow != nullptr && IsWindow(g_statusWindow)) {
        PostMessageW(g_statusWindow, WM_SNC_STATUS_REFRESH, 0, 0);
    }
}

void applyWindowIcons(HWND hwnd)
{
    if (hwnd == nullptr || g_sncTrayIcon == nullptr) {
        return;
    }

    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_sncTrayIcon));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_sncTrayIcon));
    SetClassLongPtrW(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(g_sncTrayIcon));
    SetClassLongPtrW(hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(g_sncTrayIcon));
}

void layoutStatusWindow(HWND hwnd)
{
    RECT client{};
    GetClientRect(hwnd, &client);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = 18;
    const int top = 14;
    const int headerHeight = 30;
    const int subtitleHeight = 20;
    const int buttonHeight = 29;
    const int buttonGap = 8;
    int buttonWidth = 116;
    const int availableWidth = width - (margin * 2);
    const int requiredButtonWidth = (buttonWidth * 5) + (buttonGap * 4);
    if (requiredButtonWidth > availableWidth) {
        buttonWidth = (availableWidth - (buttonGap * 4)) / 5;
        if (buttonWidth < 88) {
            buttonWidth = 88;
        }
    }

    const int headerWidth = availableWidth;
    const int subtitleTop = top + headerHeight + 2;
    const int buttonTop = subtitleTop + subtitleHeight + 12;
    const int gridTop = buttonTop + buttonHeight + 14;

    int bottomHeight = (height / 4) + 96;
    if (bottomHeight < 180) {
        bottomHeight = 180;
    }
    if (bottomHeight > 240) {
        bottomHeight = 240;
    }

    const int bottomTop = height - margin - bottomHeight;
    int gridBottom = bottomTop - 14;
    if (gridBottom <= gridTop) {
        gridBottom = gridTop + 220;
    }

    const int cardGap = 14;
    int cardWidth = (availableWidth - cardGap) / 2;
    if (cardWidth < 300) {
        cardWidth = 300;
    }
    int cardHeight = (gridBottom - gridTop - cardGap) / 2;
    if (cardHeight < 120) {
        cardHeight = 120;
    }

    int labelWidth = (cardWidth * 30) / 100;
    if (labelWidth < 84) {
        labelWidth = 84;
    }
    if (labelWidth > 120) {
        labelWidth = 120;
    }
    int valueWidth = cardWidth - labelWidth - 12;
    if (valueWidth < 130) {
        valueWidth = 130;
    }

    const int fieldGap = 6;
    int rowHeight = (cardHeight - 26 - (fieldGap * 3)) / 4;
    if (rowHeight < 20) {
        rowHeight = 20;
    }

    if (g_statusHeader != nullptr) {
        MoveWindow(g_statusHeader, margin, top, headerWidth, headerHeight, TRUE);
    }
    if (g_statusSubtitle != nullptr) {
        MoveWindow(g_statusSubtitle, margin, subtitleTop, headerWidth, subtitleHeight, TRUE);
    }

    const HWND buttons[] = { g_statusRefreshButton, g_statusCopyButton, g_statusOpenArchiveButton, g_statusOpenBriefButton, g_statusCloseButton };
    const wchar_t* buttonTexts[] = { L"Obnovit", L"Kopírovat", L"Archiv", L"Souhrn", L"Konec" };
    for (std::size_t index = 0; index < std::size(buttons); ++index) {
        if (buttons[index] != nullptr) {
            SetWindowTextW(buttons[index], buttonTexts[index]);
            MoveWindow(buttons[index], margin + static_cast<int>(index) * (buttonWidth + buttonGap), buttonTop, buttonWidth, buttonHeight, TRUE);
        }
    }

    for (std::size_t sectionIndex = 0; sectionIndex < kStatusSections.size(); ++sectionIndex) {
        const int column = static_cast<int>(sectionIndex % 2);
        const int row = static_cast<int>(sectionIndex / 2);
        const int cardLeft = margin + column * (cardWidth + cardGap);
        const int cardTop = gridTop + row * (cardHeight + cardGap);

        if (g_statusSectionTitles[sectionIndex] != nullptr) {
            MoveWindow(g_statusSectionTitles[sectionIndex], cardLeft, cardTop, cardWidth, 20, TRUE);
        }

        const int titleOffset = 24;
        const int fieldBaseTop = cardTop + titleOffset;
        for (std::size_t fieldIndex = 0; fieldIndex < kStatusSections[sectionIndex].fields.size(); ++fieldIndex) {
            const auto fieldId = kStatusSections[sectionIndex].fields[fieldIndex];
            const std::size_t fieldSlot = static_cast<std::size_t>(fieldId);
            const int fieldTop = fieldBaseTop + static_cast<int>(fieldIndex) * (rowHeight + fieldGap);
            if (g_statusFieldLabels[fieldSlot] != nullptr) {
                MoveWindow(g_statusFieldLabels[fieldSlot], cardLeft, fieldTop + 4, labelWidth - 4, rowHeight, TRUE);
            }
            if (g_statusFieldValues[fieldSlot] != nullptr) {
                MoveWindow(g_statusFieldValues[fieldSlot], cardLeft + labelWidth, fieldTop, valueWidth, rowHeight + 4, TRUE);
            }
        }
    }

    if (g_statusBottomTitle != nullptr) {
        MoveWindow(g_statusBottomTitle, margin, bottomTop, availableWidth, 20, TRUE);
    }
    if (g_statusDetails != nullptr) {
        const int detailsTop = bottomTop + 24;
        const int detailsHeight = height - detailsTop - margin;
        MoveWindow(g_statusDetails, margin, detailsTop, availableWidth, detailsHeight, TRUE);
    }
}

void registerStatusWindowClass()
{
    static bool registered = false;
    if (registered) {
        return;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = statusWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = kStatusWindowClassName;
    windowClass.hIcon = g_sncTrayIcon != nullptr ? g_sncTrayIcon : LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = g_sncTrayIcon != nullptr ? g_sncTrayIcon : LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;

    const ATOM atom = RegisterClassExW(&windowClass);
    if (atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        registered = true;
    }
}

LRESULT CALLBACK statusWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE: {
        ensureStatusWindowResources();

        g_statusWindow = hwnd;

        g_statusHeader = createStatusStatic(hwnd, g_statusTitle.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
        g_statusSubtitle = createStatusStatic(hwnd, L"", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
        g_statusRefreshButton = createStatusButton(hwnd, ID_STATUS_REFRESH, L"Obnovit");
        g_statusCopyButton = createStatusButton(hwnd, ID_STATUS_COPY, L"Kopírovat");
        g_statusOpenArchiveButton = createStatusButton(hwnd, ID_STATUS_OPEN_ARCHIVE, L"Archiv");
        g_statusOpenBriefButton = createStatusButton(hwnd, ID_STATUS_OPEN_BRIEF, L"Souhrn");
        g_statusCloseButton = createStatusButton(hwnd, ID_STATUS_CLOSE, L"Konec");
        g_statusBottomTitle = createStatusStatic(hwnd, L"Další krok", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
        g_statusDetails = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | ES_NOHIDESEL,
            0,
            0,
            0,
            0,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);

        for (std::size_t index = 0; index < kStatusSections.size(); ++index) {
            g_statusSectionTitles[index] = createStatusStatic(hwnd, kStatusSections[index].title, WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
            setWindowFont(g_statusSectionTitles[index], g_statusSectionFont);
            for (const auto field : kStatusSections[index].fields) {
                const std::size_t slot = static_cast<std::size_t>(field);
                g_statusFieldLabels[slot] = createStatusStatic(hwnd, statusFieldLabel(field), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
                g_statusFieldValues[slot] = createStatusValue(hwnd, kStatusEmptyValue);
                setWindowFont(g_statusFieldLabels[slot], g_statusFieldFont);
                setWindowFont(g_statusFieldValues[slot], g_statusFieldFont);
            }
        }

        setWindowFont(g_statusHeader, g_statusHeaderFont);
        setWindowFont(g_statusSubtitle, g_statusSubtitleFont);
        setWindowFont(g_statusRefreshButton, g_statusFieldFont);
        setWindowFont(g_statusCopyButton, g_statusFieldFont);
        setWindowFont(g_statusOpenArchiveButton, g_statusFieldFont);
        setWindowFont(g_statusOpenBriefButton, g_statusFieldFont);
        setWindowFont(g_statusCloseButton, g_statusFieldFont);
        setWindowFont(g_statusBottomTitle, g_statusSectionFont);
        setWindowFont(g_statusDetails, g_statusDetailsFont);

        applyWindowIcons(hwnd);
        layoutStatusWindow(hwnd);
        refreshStatusWindowContent();
        return 0;
    }
    case WM_SIZE:
        layoutStatusWindow(hwnd);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_STATUS_REFRESH:
            refreshStatusWindowContent();
            return 0;
        case ID_STATUS_COPY:
            if (!copyTextToClipboard(hwnd, buildDashboardCopyText(loadStatusDashboardData()))) {
                MessageBeep(MB_ICONWARNING);
            }
            return 0;
        case ID_STATUS_OPEN_ARCHIVE:
            openPathWithShell(hwnd, g_archiveRoot);
            return 0;
        case ID_STATUS_OPEN_BRIEF:
            openPathWithShell(hwnd, g_nextStepsBriefPath);
            return 0;
        case ID_STATUS_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_DRAWITEM: {
        const auto* drawItem = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (drawItem != nullptr && drawItem->CtlType == ODT_BUTTON) {
            drawStatusButton(*drawItem);
            return TRUE;
        }
        break;
    }
    case WM_SNC_STATUS_REFRESH:
        refreshStatusWindowContent();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 900;
        info->ptMinTrackSize.y = 640;
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        const HDC hdc = reinterpret_cast<HDC>(wParam);
        const HWND control = reinterpret_cast<HWND>(lParam);
        COLORREF color = kStatusMutedColor;
        bool accent = false;

        if (control == g_statusHeader || control == g_statusBottomTitle) {
            accent = true;
        } else if (control == g_statusSubtitle) {
            color = kStatusMutedColor;
        } else {
            for (const auto sectionTitle : g_statusSectionTitles) {
                if (control == sectionTitle) {
                    accent = true;
                    break;
                }
            }
            if (!accent) {
                for (const auto label : g_statusFieldLabels) {
                    if (control == label) {
                        color = kStatusMutedColor;
                        break;
                    }
                }
            }
        }

        SetTextColor(hdc, accent ? kStatusAccentColor : color);
        SetBkMode(hdc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(g_statusBackgroundBrush);
    }
    case WM_CTLCOLOREDIT: {
        const HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, kStatusTextColor);
        SetBkColor(hdc, kStatusValueBackgroundColor);
        return reinterpret_cast<LRESULT>(g_statusValueBrush != nullptr ? g_statusValueBrush : g_statusPanelBrush);
    }
    case WM_ERASEBKGND: {
        const HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(hdc, &client, g_statusBackgroundBrush);

        RECT topBand = client;
        topBand.bottom = topBand.top + 4;
        FillRect(hdc, &topBand, g_statusAccentBrush);

        RECT border = client;
        border.top = border.bottom - 2;
        FillRect(hdc, &border, g_statusBorderBrush);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_statusWindow == hwnd) {
            g_statusWindow = nullptr;
        }
        g_statusHeader = nullptr;
        g_statusSubtitle = nullptr;
        g_statusDetails = nullptr;
        g_statusBottomTitle = nullptr;
        g_statusRefreshButton = nullptr;
        g_statusCopyButton = nullptr;
        g_statusOpenArchiveButton = nullptr;
        g_statusOpenBriefButton = nullptr;
        g_statusCloseButton = nullptr;
        for (auto& label : g_statusFieldLabels) {
            label = nullptr;
        }
        for (auto& value : g_statusFieldValues) {
            value = nullptr;
        }
        for (auto& sectionTitle : g_statusSectionTitles) {
            sectionTitle = nullptr;
        }
        destroyStatusWindowResources();
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

HICON loadSncTrayIcon()
{
    const HICON embeddedIcon = static_cast<HICON>(
        LoadImageW(
            GetModuleHandleW(nullptr),
            MAKEINTRESOURCEW(kSncTrayIconResourceId),
            IMAGE_ICON,
            0,
            0,
            LR_DEFAULTSIZE));
    if (embeddedIcon != nullptr) {
        return embeddedIcon;
    }

    if (g_trayIconPath.empty()) {
        return nullptr;
    }

    const auto iconHandle = static_cast<HICON>(
        LoadImageW(
            nullptr,
            g_trayIconPath.c_str(),
            IMAGE_ICON,
            0,
            0,
            LR_LOADFROMFILE | LR_DEFAULTSIZE));
    return iconHandle;
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
    output << "mp_previous_host_available: "
           << (mpOverlayPackage.previousHostAvailable ? "true" : "false") << "\n";
    output << "mp_previous_host_available_known: "
           << (mpOverlayPackage.previousHostAvailableKnown ? "true" : "false") << "\n";
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
    if (!mpOverlayPackage.provenanceState.empty()) {
        output << "mp_provenance_state: " << mpOverlayPackage.provenanceState << "\n";
    }
    output << "mp_source_quality_count: " << mpOverlayPackage.sourceQualities.size() << "\n";
    for (const auto& sourceQuality : mpOverlayPackage.sourceQualities) {
        output << "mp_source_quality: " << sourceQuality << "\n";
    }
    output << "mp_bootstrap_campaign_count: " << mpOverlayPackage.bootstrapCampaignCount << "\n";
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
    if (!mpOverlayPackage.packageZipHash.empty()) {
        output << "mp_package_zip_hash: " << mpOverlayPackage.packageZipHash << "\n";
    }
}

std::string buildStartupLifecycleState(const bool startWithWindowsEnabled)
{
    return startWithWindowsEnabled ? "owner_enabled_start_with_windows" : "manual_start_only";
}

std::string buildStartupLifecycleBrief(const bool startWithWindowsEnabled)
{
    return startWithWindowsEnabled
        ? "owner enabled start with Windows"
        : "manual start only (start with Windows disabled)";
}

void appendStartupRationaleLines(std::ostringstream& output, const bool startWithWindowsEnabled)
{
    output << "startup_rationale: start SNC before Stellaris to preserve more autosave history before the game rotates older saves away\n";
    output << "startup_start_with_windows: optional_owner_setting_default_disabled\n";
    output << "startup_lifecycle_state: " << buildStartupLifecycleState(startWithWindowsEnabled) << "\n";
    output << "startup_start_with_windows_enabled: " << (startWithWindowsEnabled ? "true" : "false") << "\n";
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

bool isReadyReadiness(const std::string& readiness)
{
    return readiness.rfind("ready", 0) == 0;
}

bool isGeneratedOverlayStagingReviewable(const bool publishAllowed, const std::string& readiness)
{
    return publishAllowed || readiness == "staged_verified" || isReadyReadiness(readiness);
}

std::string buildNextAction(
    const std::string& state,
    const std::string& postPlayState,
    const std::string& generatedOverlayPublishGateState,
    const bool generatedOverlayPublishGateCanPublish,
    const bool generatedOverlayPublishAllowed,
    const std::string& generatedOverlayStagingReadiness,
    const std::string& dslDraftReadiness,
    const std::string& candidateDecisionPackageReadiness,
    const std::string& companionNextAction)
{
    if (state == "capturing") {
        return "keep_playing_capture_active";
    }
    if (generatedOverlayPublishAllowed || generatedOverlayPublishGateCanPublish) {
        return "review_staged_overlay_and_publish_if_desired";
    }
    if (companionNextAction == "run_monthly_reactive_owner_test") {
        return companionNextAction;
    }
    if (generatedOverlayPublishGateState == "published") {
        return "review_tray_status";
    }
    if (isGeneratedOverlayStagingReviewable(generatedOverlayPublishAllowed, generatedOverlayStagingReadiness)) {
        return "review_staged_overlay_status";
    }
    if (isReadyReadiness(dslDraftReadiness)) {
        return "review_dsl_draft";
    }
    if (isReadyReadiness(candidateDecisionPackageReadiness)) {
        return "review_candidate_decision_package";
    }
    if (postPlayState == "entry_points_ambiguous") {
        return "review_entry_point_ambiguity";
    }
    if (postPlayState == "entry_point_analysis_blocked") {
        return "review_entry_point_analysis_failure";
    }
    if (state == "waiting_for_stellaris") {
        return "wait_for_stellaris_launch";
    }
    return "review_tray_status";
}

std::string buildNextActionReason(
    const std::string& state,
    const std::string& postPlayState,
    const std::string& generatedOverlayPublishGateState,
    const bool generatedOverlayPublishGateCanPublish,
    const bool generatedOverlayPublishAllowed,
    const std::string& generatedOverlayStagingReadiness,
    const std::string& dslDraftReadiness,
    const std::string& candidateDecisionPackageReadiness,
    const std::string& companionNextAction,
    const std::string& companionNextActionReason)
{
    if (state == "capturing") {
        return "stellaris_running_capture_window_active";
    }
    if (generatedOverlayPublishAllowed || generatedOverlayPublishGateCanPublish) {
        return "staged_overlay_ready_owner_gate_available";
    }
    if (companionNextAction == "run_monthly_reactive_owner_test") {
        return companionNextActionReason;
    }
    if (generatedOverlayPublishGateState == "published") {
        return "current_staged_overlay_already_published";
    }
    if (isGeneratedOverlayStagingReviewable(generatedOverlayPublishAllowed, generatedOverlayStagingReadiness)) {
        return "staged_overlay_written_but_publish_gate_not_ready";
    }
    if (isReadyReadiness(dslDraftReadiness)) {
        return "dsl_draft_ready_before_overlay_stage";
    }
    if (isReadyReadiness(candidateDecisionPackageReadiness)) {
        return "candidate_decisions_ready_before_dsl_draft";
    }
    if (postPlayState == "entry_points_ambiguous") {
        return "entry_point_branch_ambiguity_detected";
    }
    if (postPlayState == "entry_point_analysis_blocked") {
        return "entry_point_analysis_not_ready";
    }
    if (state == "waiting_for_stellaris") {
        return "stellaris_not_running";
    }
    return "see_status_summary";
}

std::string buildNextActionCommandHint(
    const std::string& nextAction,
    const std::filesystem::path& nextStepsBriefPath,
    const std::filesystem::path& ownerTestPlaybookPath)
{
    if (nextAction == "run_monthly_reactive_owner_test" && !ownerTestPlaybookPath.empty()) {
        return "open " + pathString(ownerTestPlaybookPath);
    }
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

std::string buildNextActionCommandHintSource(const std::string& nextActionCommandHint)
{
    if (!nextActionCommandHint.empty()) {
        if (nextActionCommandHint.rfind("open docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md", 0) == 0) {
            return "owner_test_playbook_path";
        }
        return "snc_next_steps_brief";
    }
    return "none";
}

std::filesystem::path buildNextActionPath(
    const std::string& nextAction,
    const strategic_nexus::CompanionGeneratedOverlayPublishGateStatus& generatedOverlayPublishGate,
    const strategic_nexus::CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const std::filesystem::path& entryPointAnalysisPath,
    const std::filesystem::path& candidateDecisionPackagePath,
    const std::filesystem::path& dslDraftAuditPath,
    const std::filesystem::path& dslDraftPath,
    const std::filesystem::path& generatedOverlayStagingStatusPath,
    const std::filesystem::path& statusPath,
    const std::filesystem::path& companionNextActionPath)
{
    if (nextAction == "run_monthly_reactive_owner_test" && !companionNextActionPath.empty()) {
        return companionNextActionPath;
    }
    if (nextAction == "review_staged_overlay_and_publish_if_desired") {
        return generatedOverlayPublishGate.stagingStatusPath.empty()
            ? generatedOverlayPublishGate.path
            : generatedOverlayPublishGate.stagingStatusPath;
    }
    if (nextAction == "review_staged_overlay_status") {
        return generatedOverlayStagingStatusPath;
    }
    if (nextAction == "review_dsl_draft") {
        return dslDraftAuditPath.empty() ? dslDraftPath : dslDraftAuditPath;
    }
    if (nextAction == "review_candidate_decision_package") {
        return candidateDecisionPackagePath;
    }
    if (nextAction == "review_entry_point_ambiguity" || nextAction == "review_entry_point_analysis_failure") {
        return entryPointAnalysisPath;
    }
    if (nextAction == "review_tray_status") {
        if (mpOverlayPackage.identityMismatchWarning || mpOverlayPackage.state == "needs_attention") {
            return mpOverlayPackage.path;
        }
        if (generatedOverlayPublishGate.state == "published") {
            return generatedOverlayPublishGate.publishStatusPath.empty()
                ? generatedOverlayPublishGate.path
                : generatedOverlayPublishGate.publishStatusPath;
        }
    }
    return statusPath;
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
    const std::filesystem::path& campaignLibraryPlanPath,
    const bool campaignLibraryPlanPresent,
    const bool campaignLibraryLimitReached,
    const std::size_t campaignLibrarySkippedDueToLimitCount,
    const std::string& campaignLibraryPlanSource,
    const std::string& campaignLibraryPlanReadiness,
    const std::string& campaignLibraryPlanReason,
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
    const strategic_nexus::CompanionSubsystemStatus& generatedOverlayStatus,
    const strategic_nexus::CompanionGeneratedOverlayPublishGateStatus& generatedOverlayPublishGate,
    const strategic_nexus::CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const strategic_nexus::CompanionLocalLlmStatus& localLlm,
    const bool startWithWindowsEnabled,
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
    if (campaignLibraryPlanPresent) {
        summary << "campaign_library_plan_path: " << pathString(campaignLibraryPlanPath) << "\n";
        summary << "campaign_library_plan_source: " << campaignLibraryPlanSource << "\n";
        summary << "campaign_library_plan_readiness: " << campaignLibraryPlanReadiness << "\n";
        summary << "campaign_library_plan_reason: " << campaignLibraryPlanReason << "\n";
        summary << "campaign_library_limit_reached: " << (campaignLibraryLimitReached ? "true" : "false") << "\n";
        summary << "campaign_library_skipped_due_to_limit_count: " << campaignLibrarySkippedDueToLimitCount << "\n";
        if (campaignLibraryPlanReadiness == "needs_attention") {
            summary << "campaign_library_owner_note: campaign library plan needs attention before SNC should trust active campaign coverage\n";
        } else if (campaignLibraryLimitReached) {
            summary << "campaign_library_owner_note: active generated campaign library is truncated by the configured limit; raise the cap or clean local campaigns before broader coverage tests\n";
        } else {
            summary << "campaign_library_owner_note: active generated campaign library fits within the configured limit\n";
        }
    }
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
    summary << "generated_overlay_reactive_capability: "
            << (generatedOverlayStatus.reactivePolicyPackCapability.empty()
                    ? "unknown"
                    : generatedOverlayStatus.reactivePolicyPackCapability)
            << "\n";
    summary << "generated_overlay_event_family_count: "
            << generatedOverlayStatus.eventFamilies.size() << "\n";
    if (!generatedOverlayStatus.eventFamilies.empty()) {
        summary << "generated_overlay_event_families: "
                << joinStrings(generatedOverlayStatus.eventFamilies, ",") << "\n";
    }
    summary << "generated_overlay_source_quality_count: "
            << generatedOverlayStatus.sourceQualities.size() << "\n";
    if (!generatedOverlayStatus.sourceQualities.empty()) {
        summary << "generated_overlay_source_qualities: "
                << joinStrings(generatedOverlayStatus.sourceQualities, ",") << "\n";
    }
    summary << "generated_overlay_bootstrap_campaign_count: "
            << generatedOverlayStatus.bootstrapCampaignCount << "\n";
    summary << "generated_overlay_publish_gate_state: " << generatedOverlayPublishGate.state << "\n";
    summary << "generated_overlay_publish_gate_reason: " << generatedOverlayPublishGate.reason << "\n";
    summary << "generated_overlay_publish_gate_published: "
            << (generatedOverlayPublishGate.published ? "true" : "false") << "\n";
    summary << "generated_overlay_publish_gate_can_publish: "
            << (generatedOverlayPublishGate.canPublish ? "true" : "false") << "\n";
    summary << "generated_overlay_publish_gate_owner_approval_required: "
            << (generatedOverlayPublishGate.ownerApprovalRequired ? "true" : "false") << "\n";
    if (!generatedOverlayPublishGate.manifestHash.empty()) {
        summary << "generated_overlay_publish_gate_manifest_hash: " << generatedOverlayPublishGate.manifestHash << "\n";
    }
    if (!generatedOverlayPublishGate.publishedManifestHash.empty()) {
        summary << "generated_overlay_publish_gate_published_manifest_hash: "
                << generatedOverlayPublishGate.publishedManifestHash << "\n";
    }
    if (!generatedOverlayPublishGate.backupDirectory.empty()) {
        summary << "generated_overlay_publish_gate_backup_directory: "
                << pathString(generatedOverlayPublishGate.backupDirectory) << "\n";
    }
    summary << "generated_overlay_active_directory: " << pathString(g_generatedOverlayActiveDirectory) << "\n";
    summary << "generated_overlay_publish_status_path: " << pathString(g_generatedOverlayPublishStatusPath) << "\n";
    summary << "generated_overlay_publish_backup_root_directory: "
            << pathString(g_generatedOverlayPublishBackupRootDirectory) << "\n";
    summary << "local_llm_model: " << localLlm.state << " - " << localLlm.reason << "\n";
    summary << "local_llm_reduced_mode: " << (localLlm.reducedMode ? "true" : "false") << "\n";
    summary << "local_llm_can_run_inference: " << (localLlm.canRunInference ? "true" : "false") << "\n";
    summary << "local_llm_user_action_required: " << (localLlm.userActionRequired ? "true" : "false") << "\n";
    summary << "local_llm_download_allowed: " << (localLlm.downloadAllowed ? "true" : "false") << "\n";
    if (!localLlm.selectedModelId.empty()) {
        summary << "local_llm_selected_model_id: " << localLlm.selectedModelId << "\n";
    }
    if (!localLlm.selectedDisplayName.empty()) {
        summary << "local_llm_selected_display_name: " << localLlm.selectedDisplayName << "\n";
    }
    if (!localLlm.runtime.empty()) {
        summary << "local_llm_runtime: " << localLlm.runtime << "\n";
    }
    if (!localLlm.catalogStatus.empty()) {
        summary << "local_llm_catalog_status: " << localLlm.catalogStatus << "\n";
    }
    if (!localLlm.recommendedModelId.empty()) {
        summary << "local_llm_recommended_model_id: " << localLlm.recommendedModelId << "\n";
    }
    if (!localLlm.recommendedDisplayName.empty()) {
        summary << "local_llm_recommended_display_name: " << localLlm.recommendedDisplayName << "\n";
    }
    if (!localLlm.recommendedRuntime.empty()) {
        summary << "local_llm_recommended_runtime: " << localLlm.recommendedRuntime << "\n";
    }
    if (!localLlm.modelStatePath.empty()) {
        summary << "local_llm_model_state_path: " << pathString(localLlm.modelStatePath) << "\n";
    }
    if (!localLlm.prepareCommandHint.empty()) {
        summary << "local_llm_prepare_command_hint: " << localLlm.prepareCommandHint << "\n";
    }
    appendStartupRationaleLines(summary, startWithWindowsEnabled);
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
    const std::filesystem::path& campaignLibraryPlanPath,
    const bool campaignLibraryPlanPresent,
    const bool campaignLibraryLimitReached,
    const std::size_t campaignLibrarySkippedDueToLimitCount,
    const std::string& campaignLibraryPlanReadiness,
    const std::string& campaignLibraryPlanReason,
    const std::filesystem::path& decisionInputPackagePath,
    const std::string& decisionInputPackageReadiness,
    const std::filesystem::path& candidateDecisionPackagePath,
    const std::string& candidateDecisionPackageReadiness,
    const std::filesystem::path& dslDraftPath,
    const std::string& dslDraftReadiness,
    const std::filesystem::path& generatedOverlayStagingStatusPath,
    const std::string& generatedOverlayStagingReadiness,
    const bool generatedOverlayPublishAllowed,
    const strategic_nexus::CompanionGeneratedOverlayPublishGateStatus& generatedOverlayPublishGate,
    const strategic_nexus::CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const bool startWithWindowsEnabled,
    const std::string& mpPackageRefreshState,
    const std::string& mpPackageRefreshReason,
    const std::string& nextAction,
    const std::string& nextActionReason,
    const std::string& nextActionCommandHint,
    const std::string& nextActionCommandHintSource,
    const std::filesystem::path& nextActionPath,
    const std::filesystem::path& ownerTestPlaybookPath)
{
    std::ostringstream brief;
    brief << "Strategic Nexus Companion - dalsi kroky\n";
    brief << "Aktualizovano: " << formatLocalTimestamp() << "\n";
    brief << "Stav: " << state << "\n";
    brief << "Post-play stav: " << postPlayState << "\n";
    brief << "Dalsi akce: " << nextAction << "\n";
    brief << "Duvod: " << nextActionReason << "\n\n";
    if (!nextActionCommandHint.empty()) {
        brief << "Command hint: " << nextActionCommandHint << "\n";
        brief << "Command hint source: " << nextActionCommandHintSource << "\n\n";
    }
    if (!nextActionPath.empty()) {
        brief << "Action path: " << pathString(nextActionPath) << "\n\n";
    }
    if (!ownerTestPlaybookPath.empty()) {
        brief << "Owner test playbook: " << pathString(ownerTestPlaybookPath) << "\n\n";
    }
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
    if (campaignLibraryPlanPresent) {
        brief << "- Campaign library plan: " << pathString(campaignLibraryPlanPath) << "\n";
        brief << "- Campaign library readiness: " << campaignLibraryPlanReadiness;
        if (!campaignLibraryPlanReason.empty()) {
            brief << " (" << campaignLibraryPlanReason << ")";
        }
        brief << "\n";
        brief << "- Campaign library saturation: "
              << (campaignLibraryLimitReached ? "truncated_by_limit" : "within_limit");
        if (campaignLibraryPlanReadiness == "needs_attention") {
            brief << "\n";
            brief << "- Poznamka: sidecar plan knihovny kampani potrebuje pozornost; SNC nema duverovat aktivnimu coverage, dokud se plan neopraví.\n";
        } else if (campaignLibraryLimitReached) {
            brief << " (" << campaignLibrarySkippedDueToLimitCount << " campaign(s) skipped by limit)";
            brief << "\n";
            brief << "- Poznamka: aktivni generovana knihovna kampani je zamerne omezena; pokud v pristim testu chybi kampan, zvedni limit nebo uklid lokalni save root.\n";
        } else {
            brief << "\n";
            brief << "- Poznamka: aktivni generovana knihovna kampani se vejde do nastaveneho limitu.\n";
        }
    }
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
    brief << "- Publish gate state: " << generatedOverlayPublishGate.state;
    if (!generatedOverlayPublishGate.reason.empty()) {
        brief << " (" << generatedOverlayPublishGate.reason << ")";
    }
    brief << "\n";
    brief << "- Publish gate available: " << (generatedOverlayPublishGate.canPublish ? "ano" : "ne") << "\n";
    brief << "- Publish allowed now: " << (generatedOverlayPublishAllowed ? "ano" : "ne") << "\n";
    brief << "- Owner approval required: "
          << (generatedOverlayPublishGate.ownerApprovalRequired ? "ano" : "ne") << "\n";
    brief << "- Current staged overlay already published: " << (generatedOverlayPublishGate.published ? "ano" : "ne") << "\n";
    brief << "- Startup note: Spusteni SNC pred Stellaris pomaha uchovat vic autosave historie driv, nez hra prepise starsi autosavy.\n";
    brief << "- Start with Windows: volitelne nastaveni, vychozi stav ma zustat vypnuty.\n";
    brief << "- Startup lifecycle state: " << buildStartupLifecycleBrief(startWithWindowsEnabled) << ".\n";
    brief << "- Active overlay snapshot: " << pathString(g_generatedOverlayActiveDirectory) << "\n";
    brief << "- Publish status output: " << pathString(g_generatedOverlayPublishStatusPath) << "\n";
    if (!generatedOverlayPublishGate.manifestHash.empty()) {
        brief << "- Staged overlay manifest hash: " << generatedOverlayPublishGate.manifestHash << "\n";
    }
    if (!generatedOverlayPublishGate.publishedManifestHash.empty()) {
        brief << "- Published overlay manifest hash: " << generatedOverlayPublishGate.publishedManifestHash << "\n";
    }
    if (!generatedOverlayPublishGate.backupDirectory.empty()) {
        brief << "- Publish backup directory: " << pathString(generatedOverlayPublishGate.backupDirectory) << "\n";
    }
    brief << "- MP package refresh: " << mpPackageRefreshState << " (" << mpPackageRefreshReason << ")\n";
    brief << "- MP overlay package directory: " << pathString(g_mpOverlayPackageDirectory) << "\n";
    brief << "- MP package zip path: " << pathString(g_mpOverlayPackageZipPath) << "\n";
    if (!mpOverlayPackage.packageZipHash.empty()) {
        brief << "- MP package zip hash: " << mpOverlayPackage.packageZipHash << "\n";
    }
    if (!mpOverlayPackage.packageZipState.empty()) {
        brief << "- MP package zip state: " << mpOverlayPackage.packageZipState << "\n";
    }
    if (!mpOverlayPackage.packageZipReason.empty()) {
        brief << "- MP package zip reason: " << mpOverlayPackage.packageZipReason << "\n";
    }
    if (!mpOverlayPackage.packageZipState.empty()) {
        brief << "- MP package zip bytes: " << mpOverlayPackage.packageZipBytes << "\n";
    }
    brief << "- MP overlay package state: " << mpOverlayPackage.state;
    if (!mpOverlayPackage.reason.empty()) {
        brief << " (" << mpOverlayPackage.reason << ")";
    }
    brief << "\n";
    brief << "- MP previous host available: "
          << (mpOverlayPackage.previousHostAvailable ? "ano" : "ne") << "\n";
    brief << "- MP previous host availability known: "
          << (mpOverlayPackage.previousHostAvailableKnown ? "ano" : "ne") << "\n";
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
    companionConfig.gameplayAcceptanceReportPath = g_gameplayAcceptanceReportPath;
    companionConfig.entryPointAnalysisPath = g_entryPointAnalysisPath;
    companionConfig.postPlayPackagePath = g_postPlayPackagePath;
    companionConfig.decisionInputPackagePath = g_decisionInputPackagePath;
    companionConfig.candidateDecisionPackagePath = g_candidateDecisionPackagePath;
    companionConfig.dslDraftPath = g_dslDraftPath;
    companionConfig.dslDraftAuditPath = g_dslDraftAuditPath;
    companionConfig.postPlayGeneratedOverlayStagingStatusPath = g_generatedOverlayStagingStatusPath;
    companionConfig.mpOverlayPackageZipPath = g_mpOverlayPackageZipPath;
    const auto companionSnapshot = companion.buildStatusSnapshot(companionConfig);
    const auto& diskPipeline = companionSnapshot.postPlayPipeline;

    auto choosePath = [](const std::filesystem::path& liveValue, const std::filesystem::path& diskValue) {
        return liveValue.empty() ? diskValue : liveValue;
    };
    auto chooseString = [](const std::string& liveValue, const std::string& diskValue) {
        return liveValue.empty() ? diskValue : liveValue;
    };
    auto chooseSize = [](const std::size_t liveValue, const std::size_t diskValue) {
        return liveValue == 0 ? diskValue : liveValue;
    };

    std::string effectivePostPlayState = postPlayState;
    if (effectivePostPlayState == "not_started" && diskPipeline.state != "starting") {
        if (!diskPipeline.generatedOverlayStagingReadiness.empty()) {
            effectivePostPlayState = diskPipeline.generatedOverlayStagingReadiness.rfind("staged", 0) == 0
                ? "generated_overlay_staged"
                : "generated_overlay_staging_failed";
        } else if (!diskPipeline.dslDraftReadiness.empty()) {
            effectivePostPlayState = diskPipeline.dslDraftReadiness == "needs_identity"
                ? "dsl_draft_needs_identity"
                : "dsl_draft_ready";
        } else if (!diskPipeline.candidateDecisionPackageReadiness.empty()) {
            effectivePostPlayState = "candidate_decision_package_ready";
        } else if (!diskPipeline.decisionInputPackageReadiness.empty()) {
            effectivePostPlayState = "decision_input_package_ready";
        } else if (!diskPipeline.postPlayPackageReadiness.empty()) {
            effectivePostPlayState = "post_play_package_ready";
        } else if (!diskPipeline.entryPointReadiness.empty()) {
            effectivePostPlayState = diskPipeline.branchAmbiguityDetected
                ? "entry_points_ambiguous"
                : "entry_points_ready";
        }
    }

    const auto effectiveEntryPointAnalysisPath =
        choosePath(entryPointAnalysisPath, diskPipeline.entryPointAnalysisPath);
    const auto effectiveEntryPointCount = chooseSize(entryPointCount, diskPipeline.entryPointCount);
    const bool effectiveBranchAmbiguityDetected =
        branchAmbiguityDetected || diskPipeline.branchAmbiguityDetected;
    const auto effectiveEntryPointReadiness =
        chooseString(entryPointReadiness, diskPipeline.entryPointReadiness);
    const auto effectivePostPlayPackagePath =
        choosePath(postPlayPackagePath, diskPipeline.postPlayPackagePath);
    const auto effectivePostPlayPackageReadiness =
        chooseString(postPlayPackageReadiness, diskPipeline.postPlayPackageReadiness);
    const auto effectivePostPlayDecisionReadyEntryCount =
        chooseSize(postPlayDecisionReadyEntryCount, diskPipeline.postPlayDecisionReadyEntryCount);
    const auto effectivePostPlayCampaignCount =
        chooseSize(postPlayCampaignCount, diskPipeline.postPlayCampaignCount);
    const auto effectivePostPlayReadyCampaignCount =
        chooseSize(postPlayReadyCampaignCount, diskPipeline.postPlayReadyCampaignCount);
    const auto effectivePostPlayPartialCampaignCount =
        chooseSize(postPlayPartialCampaignCount, diskPipeline.postPlayPartialCampaignCount);
    const auto effectivePostPlayBlockedCampaignCount =
        chooseSize(postPlayBlockedCampaignCount, diskPipeline.postPlayBlockedCampaignCount);
    const auto effectiveCampaignLibraryPlanPath = diskPipeline.campaignLibraryPlanPath;
    const bool effectiveCampaignLibraryPlanPresent = diskPipeline.campaignLibraryPlanPresent;
    const bool effectiveCampaignLibraryLimitReached = diskPipeline.campaignLibraryLimitReached;
    const auto effectiveCampaignLibrarySkippedDueToLimitCount =
        diskPipeline.campaignLibrarySkippedDueToLimitCount;
    const auto effectiveCampaignLibraryPlanSource = diskPipeline.campaignLibraryPlanSource;
    const auto effectiveCampaignLibraryPlanReadiness = diskPipeline.campaignLibraryPlanReadiness;
    const auto effectiveCampaignLibraryPlanReason = diskPipeline.campaignLibraryPlanReason;
    const auto effectivePostPlayCampaignSummaries = postPlayCampaignSummaries.empty()
        ? diskPipeline.postPlayCampaignReadinessSummaries
        : postPlayCampaignSummaries;
    const auto effectiveDecisionInputPackagePath =
        choosePath(decisionInputPackagePath, diskPipeline.decisionInputPackagePath);
    const auto effectiveDecisionInputPackageReadiness =
        chooseString(decisionInputPackageReadiness, diskPipeline.decisionInputPackageReadiness);
    const auto effectiveDecisionInputCount = chooseSize(decisionInputCount, diskPipeline.decisionInputCount);
    const auto effectiveCandidateDecisionPackagePath =
        choosePath(candidateDecisionPackagePath, diskPipeline.candidateDecisionPackagePath);
    const auto effectiveCandidateDecisionPackageReadiness =
        chooseString(candidateDecisionPackageReadiness, diskPipeline.candidateDecisionPackageReadiness);
    const auto effectiveCandidateDecisionCount =
        chooseSize(candidateDecisionCount, diskPipeline.candidateDecisionCount);
    const bool effectiveCandidateDecisionValidatorPassed =
        candidateDecisionValidatorPassed || diskPipeline.candidateDecisionValidatorPassed;
    const auto effectiveDslDraftPath = choosePath(dslDraftPath, diskPipeline.dslDraftPath);
    const auto effectiveDslDraftAuditPath = choosePath(dslDraftAuditPath, diskPipeline.dslDraftAuditPath);
    const auto effectiveDslDraftReadiness = chooseString(dslDraftReadiness, diskPipeline.dslDraftReadiness);
    const auto effectiveDslDraftRuleCount = chooseSize(dslDraftRuleCount, diskPipeline.dslDraftRuleCount);
    const bool effectiveDslDraftValidatorPassed =
        dslDraftValidatorPassed || diskPipeline.dslDraftValidatorPassed;
    const auto effectiveGeneratedOverlayStagingDirectory = generatedOverlayStagingDirectory.empty()
        ? g_generatedOverlayStagingDirectory
        : generatedOverlayStagingDirectory;
    const auto effectiveGeneratedOverlayStagingStatusPath =
        choosePath(generatedOverlayStagingStatusPath, diskPipeline.generatedOverlayStagingStatusPath);
    const auto effectiveGeneratedOverlayStagingReadiness =
        chooseString(generatedOverlayStagingReadiness, diskPipeline.generatedOverlayStagingReadiness);
    const auto effectiveGeneratedOverlayStagingRuleCount =
        chooseSize(generatedOverlayStagingRuleCount, diskPipeline.generatedOverlayStagingRuleCount);
    const bool effectiveGeneratedOverlayManifestVerified =
        generatedOverlayManifestVerified || diskPipeline.generatedOverlayManifestVerified;
    const bool effectiveGeneratedOverlayPublishAllowed =
        generatedOverlayPublishAllowed || diskPipeline.generatedOverlayPublishAllowed;
    const auto effectiveGeneratedOverlayManifestHash = chooseString(
        generatedOverlayManifestHash,
        companionSnapshot.generatedOverlayPublishGate.manifestHash);

    const auto nextAction = buildNextAction(
        state,
        effectivePostPlayState,
        companionSnapshot.generatedOverlayPublishGate.state,
        companionSnapshot.generatedOverlayPublishGate.canPublish,
        effectiveGeneratedOverlayPublishAllowed,
        effectiveGeneratedOverlayStagingReadiness,
        effectiveDslDraftReadiness,
        effectiveCandidateDecisionPackageReadiness,
        companionSnapshot.nextAction);
    const auto nextActionReason = buildNextActionReason(
        state,
        effectivePostPlayState,
        companionSnapshot.generatedOverlayPublishGate.state,
        companionSnapshot.generatedOverlayPublishGate.canPublish,
        effectiveGeneratedOverlayPublishAllowed,
        effectiveGeneratedOverlayStagingReadiness,
        effectiveDslDraftReadiness,
        effectiveCandidateDecisionPackageReadiness,
        companionSnapshot.nextAction,
        companionSnapshot.nextActionReason);
    const auto nextActionCommandHint =
        buildNextActionCommandHint(nextAction, g_nextStepsBriefPath, companionSnapshot.ownerTestPlaybookPath);
    const auto nextActionCommandHintSource = buildNextActionCommandHintSource(nextActionCommandHint);
    const auto nextActionPath = buildNextActionPath(
        nextAction,
        companionSnapshot.generatedOverlayPublishGate,
        companionSnapshot.mpOverlayPackage,
        effectiveEntryPointAnalysisPath,
        effectiveCandidateDecisionPackagePath,
        effectiveDslDraftAuditPath,
        effectiveDslDraftPath,
        effectiveGeneratedOverlayStagingStatusPath,
        g_trayStatusPath,
        companionSnapshot.nextActionPath);
    const auto statusCenterSummaryText = buildStatusCenterSummaryText(
        state,
        reason,
        stellarisRunning,
        sessionId,
        sessionDirectory,
        copiedTotal,
        skippedTotal,
        effectivePostPlayState,
        archiveVerified,
        effectiveEntryPointAnalysisPath,
        effectiveEntryPointCount,
        effectiveBranchAmbiguityDetected,
        effectiveEntryPointReadiness,
        effectivePostPlayPackagePath,
        effectivePostPlayPackageReadiness,
        effectivePostPlayDecisionReadyEntryCount,
        effectivePostPlayCampaignCount,
        effectivePostPlayReadyCampaignCount,
        effectivePostPlayPartialCampaignCount,
        effectivePostPlayBlockedCampaignCount,
        effectiveCampaignLibraryPlanPath,
        effectiveCampaignLibraryPlanPresent,
        effectiveCampaignLibraryLimitReached,
        effectiveCampaignLibrarySkippedDueToLimitCount,
        effectiveCampaignLibraryPlanSource,
        effectiveCampaignLibraryPlanReadiness,
        effectiveCampaignLibraryPlanReason,
        effectivePostPlayCampaignSummaries,
        effectiveDecisionInputPackagePath,
        effectiveDecisionInputPackageReadiness,
        effectiveDecisionInputCount,
        effectiveCandidateDecisionPackagePath,
        effectiveCandidateDecisionPackageReadiness,
        effectiveCandidateDecisionCount,
        effectiveDslDraftPath,
        effectiveDslDraftReadiness,
        effectiveDslDraftRuleCount,
        effectiveGeneratedOverlayStagingDirectory,
        effectiveGeneratedOverlayStagingStatusPath,
        effectiveGeneratedOverlayStagingReadiness,
        effectiveGeneratedOverlayStagingRuleCount,
        effectiveGeneratedOverlayManifestVerified,
        effectiveGeneratedOverlayPublishAllowed,
        effectiveGeneratedOverlayManifestHash,
        companionSnapshot.generatedOverlay,
        companionSnapshot.generatedOverlayPublishGate,
        companionSnapshot.mpOverlayPackage,
        companionSnapshot.localLlm,
        companionSnapshot.lifecycle.startWithWindowsEnabled,
        mpPackageRefreshState,
        mpPackageRefreshReason);
    writeNextStepsBrief(
        state,
        effectivePostPlayState,
        effectiveEntryPointAnalysisPath,
        effectiveEntryPointReadiness,
        effectivePostPlayPackagePath,
        effectivePostPlayPackageReadiness,
        effectiveCampaignLibraryPlanPath,
        effectiveCampaignLibraryPlanPresent,
        effectiveCampaignLibraryLimitReached,
        effectiveCampaignLibrarySkippedDueToLimitCount,
        effectiveCampaignLibraryPlanReadiness,
        effectiveCampaignLibraryPlanReason,
        effectiveDecisionInputPackagePath,
        effectiveDecisionInputPackageReadiness,
        effectiveCandidateDecisionPackagePath,
        effectiveCandidateDecisionPackageReadiness,
        effectiveDslDraftPath,
        effectiveDslDraftReadiness,
        effectiveGeneratedOverlayStagingStatusPath,
        effectiveGeneratedOverlayStagingReadiness,
        effectiveGeneratedOverlayPublishAllowed,
        companionSnapshot.generatedOverlayPublishGate,
        companionSnapshot.mpOverlayPackage,
        companionSnapshot.lifecycle.startWithWindowsEnabled,
        mpPackageRefreshState,
        mpPackageRefreshReason,
        nextAction,
        nextActionReason,
        nextActionCommandHint,
        nextActionCommandHintSource,
        nextActionPath,
        companionSnapshot.ownerTestPlaybookPath);

    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"app_name\": \"Strategic Nexus Companion\",\n";
    json << "  \"mode\": \"tray\",\n";
    json << "  \"state\": \"" << jsonEscape(state) << "\",\n";
    json << "  \"reason\": \"" << jsonEscape(reason) << "\",\n";
    json << "  \"updated_at_local\": \"" << jsonEscape(formatLocalTimestamp()) << "\",\n";
    json << "  \"start_with_windows_enabled\": "
         << (companionSnapshot.lifecycle.startWithWindowsEnabled ? "true" : "false") << ",\n";
    json << "  \"window_close_behavior\": \""
         << jsonEscape(companionSnapshot.lifecycle.windowCloseBehavior) << "\",\n";
    json << "  \"explicit_exit_behavior\": \""
         << jsonEscape(companionSnapshot.lifecycle.explicitExitBehavior) << "\",\n";
    json << "  \"crash_restart_policy\": \""
         << jsonEscape(companionSnapshot.lifecycle.crashRestartPolicy) << "\",\n";
    json << "  \"startup_lifecycle_state\": \""
         << jsonEscape(buildStartupLifecycleState(companionSnapshot.lifecycle.startWithWindowsEnabled)) << "\",\n";
    json << "  \"stellaris_running\": " << (stellarisRunning ? "true" : "false") << ",\n";
    json << "  \"session_id\": \"" << jsonEscape(sessionId) << "\",\n";
    json << "  \"capture_session_directory\": \"" << jsonEscape(pathString(sessionDirectory)) << "\",\n";
    json << "  \"archive_root\": \"" << jsonEscape(pathString(g_archiveRoot)) << "\",\n";
    json << "  \"candidate_root_count\": " << candidateRootCount << ",\n";
    json << "  \"existing_root_count\": " << existingRootCount << ",\n";
    json << "  \"copied_count_total\": " << copiedTotal << ",\n";
    json << "  \"skipped_count_total\": " << skippedTotal << ",\n";
    json << "  \"post_play_state\": \"" << jsonEscape(effectivePostPlayState) << "\",\n";
    json << "  \"archive_verified\": " << (archiveVerified ? "true" : "false") << ",\n";
    json << "  \"entry_point_analysis_path\": \"" << jsonEscape(pathString(effectiveEntryPointAnalysisPath)) << "\",\n";
    json << "  \"entry_point_count\": " << effectiveEntryPointCount << ",\n";
    json << "  \"branch_ambiguity_detected\": " << (effectiveBranchAmbiguityDetected ? "true" : "false") << ",\n";
    json << "  \"entry_point_readiness\": \"" << jsonEscape(effectiveEntryPointReadiness) << "\",\n";
    json << "  \"post_play_package_path\": \"" << jsonEscape(pathString(effectivePostPlayPackagePath)) << "\",\n";
    json << "  \"post_play_package_readiness\": \"" << jsonEscape(effectivePostPlayPackageReadiness) << "\",\n";
    json << "  \"post_play_decision_ready_entry_count\": " << effectivePostPlayDecisionReadyEntryCount << ",\n";
    json << "  \"post_play_campaign_count\": " << effectivePostPlayCampaignCount << ",\n";
    json << "  \"post_play_ready_campaign_count\": " << effectivePostPlayReadyCampaignCount << ",\n";
    json << "  \"post_play_partial_campaign_count\": " << effectivePostPlayPartialCampaignCount << ",\n";
    json << "  \"post_play_blocked_campaign_count\": " << effectivePostPlayBlockedCampaignCount << ",\n";
    json << "  \"campaign_library_plan_path\": \"" << jsonEscape(pathString(effectiveCampaignLibraryPlanPath)) << "\",\n";
    json << "  \"campaign_library_plan_present\": " << (effectiveCampaignLibraryPlanPresent ? "true" : "false") << ",\n";
    json << "  \"campaign_library_limit_reached\": " << (effectiveCampaignLibraryLimitReached ? "true" : "false") << ",\n";
    json << "  \"campaign_library_skipped_due_to_limit_count\": " << effectiveCampaignLibrarySkippedDueToLimitCount << ",\n";
    json << "  \"campaign_library_plan_source\": \"" << jsonEscape(effectiveCampaignLibraryPlanSource) << "\",\n";
    json << "  \"campaign_library_plan_readiness\": \"" << jsonEscape(effectiveCampaignLibraryPlanReadiness) << "\",\n";
    json << "  \"campaign_library_plan_reason\": \"" << jsonEscape(effectiveCampaignLibraryPlanReason) << "\",\n";
    json << "  \"post_play_campaign_summaries\": [";
    for (std::size_t index = 0; index < effectivePostPlayCampaignSummaries.size(); ++index) {
        if (index > 0) {
            json << ", ";
        }
        json << "\"" << jsonEscape(effectivePostPlayCampaignSummaries[index]) << "\"";
    }
    json << "],\n";
    json << "  \"decision_input_package_path\": \"" << jsonEscape(pathString(effectiveDecisionInputPackagePath)) << "\",\n";
    json << "  \"decision_input_package_readiness\": \"" << jsonEscape(effectiveDecisionInputPackageReadiness) << "\",\n";
    json << "  \"decision_input_count\": " << effectiveDecisionInputCount << ",\n";
    json << "  \"candidate_decision_package_path\": \"" << jsonEscape(pathString(effectiveCandidateDecisionPackagePath)) << "\",\n";
    json << "  \"candidate_decision_package_readiness\": \"" << jsonEscape(effectiveCandidateDecisionPackageReadiness) << "\",\n";
    json << "  \"candidate_decision_count\": " << effectiveCandidateDecisionCount << ",\n";
    json << "  \"candidate_decision_validator_passed\": "
         << (effectiveCandidateDecisionValidatorPassed ? "true" : "false") << ",\n";
    json << "  \"dsl_draft_path\": \"" << jsonEscape(pathString(effectiveDslDraftPath)) << "\",\n";
    json << "  \"dsl_draft_audit_path\": \"" << jsonEscape(pathString(effectiveDslDraftAuditPath)) << "\",\n";
    json << "  \"dsl_draft_readiness\": \"" << jsonEscape(effectiveDslDraftReadiness) << "\",\n";
    json << "  \"dsl_draft_rule_count\": " << effectiveDslDraftRuleCount << ",\n";
    json << "  \"dsl_draft_validator_passed\": "
         << (effectiveDslDraftValidatorPassed ? "true" : "false") << ",\n";
    json << "  \"generated_overlay_staging_directory\": \""
         << jsonEscape(pathString(effectiveGeneratedOverlayStagingDirectory)) << "\",\n";
    json << "  \"generated_overlay_staging_status_path\": \""
         << jsonEscape(pathString(effectiveGeneratedOverlayStagingStatusPath)) << "\",\n";
    json << "  \"generated_overlay_active_directory\": \""
         << jsonEscape(pathString(g_generatedOverlayActiveDirectory)) << "\",\n";
    json << "  \"generated_overlay_publish_status_path\": \""
         << jsonEscape(pathString(g_generatedOverlayPublishStatusPath)) << "\",\n";
    json << "  \"generated_overlay_publish_backup_root_directory\": \""
         << jsonEscape(pathString(g_generatedOverlayPublishBackupRootDirectory)) << "\",\n";
    json << "  \"generated_overlay_staging_readiness\": \""
         << jsonEscape(effectiveGeneratedOverlayStagingReadiness) << "\",\n";
    json << "  \"generated_overlay_staging_rule_count\": " << effectiveGeneratedOverlayStagingRuleCount << ",\n";
    json << "  \"generated_overlay_manifest_verified\": "
         << (effectiveGeneratedOverlayManifestVerified ? "true" : "false") << ",\n";
    json << "  \"generated_overlay_publish_allowed\": "
         << (effectiveGeneratedOverlayPublishAllowed ? "true" : "false") << ",\n";
    json << "  \"generated_overlay_manifest_hash\": \""
         << jsonEscape(effectiveGeneratedOverlayManifestHash) << "\",\n";
    json << "  \"generated_overlay_reactive_capability\": \""
         << jsonEscape(companionSnapshot.generatedOverlay.reactivePolicyPackCapability.empty()
                           ? "unknown"
                           : companionSnapshot.generatedOverlay.reactivePolicyPackCapability)
         << "\",\n";
    json << "  \"generated_overlay_event_family_count\": "
         << companionSnapshot.generatedOverlay.eventFamilies.size() << ",\n";
    json << "  \"generated_overlay_event_families\": [";
    for (std::size_t index = 0; index < companionSnapshot.generatedOverlay.eventFamilies.size(); ++index) {
        if (index > 0) {
            json << ", ";
        }
        json << "\"" << jsonEscape(companionSnapshot.generatedOverlay.eventFamilies[index]) << "\"";
    }
    json << "],\n";
    json << "  \"generated_overlay_source_quality_count\": "
         << companionSnapshot.generatedOverlay.sourceQualities.size() << ",\n";
    json << "  \"generated_overlay_source_qualities\": [";
    for (std::size_t index = 0; index < companionSnapshot.generatedOverlay.sourceQualities.size(); ++index) {
        if (index > 0) {
            json << ", ";
        }
        json << "\"" << jsonEscape(companionSnapshot.generatedOverlay.sourceQualities[index]) << "\"";
    }
    json << "],\n";
    json << "  \"generated_overlay_bootstrap_campaign_count\": "
         << companionSnapshot.generatedOverlay.bootstrapCampaignCount << ",\n";
    json << "  \"generated_overlay_publish_gate_state\": \""
         << jsonEscape(companionSnapshot.generatedOverlayPublishGate.state) << "\",\n";
    json << "  \"generated_overlay_publish_gate_reason\": \""
         << jsonEscape(companionSnapshot.generatedOverlayPublishGate.reason) << "\",\n";
    json << "  \"generated_overlay_publish_gate_published\": "
         << (companionSnapshot.generatedOverlayPublishGate.published ? "true" : "false") << ",\n";
    json << "  \"generated_overlay_publish_gate_can_publish\": "
         << (companionSnapshot.generatedOverlayPublishGate.canPublish ? "true" : "false") << ",\n";
    json << "  \"generated_overlay_publish_gate_owner_approval_required\": "
         << (companionSnapshot.generatedOverlayPublishGate.ownerApprovalRequired ? "true" : "false") << ",\n";
    json << "  \"generated_overlay_publish_gate_manifest_hash\": \""
         << jsonEscape(companionSnapshot.generatedOverlayPublishGate.manifestHash) << "\",\n";
    json << "  \"generated_overlay_publish_gate_published_manifest_hash\": \""
         << jsonEscape(companionSnapshot.generatedOverlayPublishGate.publishedManifestHash) << "\",\n";
    json << "  \"generated_overlay_publish_gate_backup_directory\": \""
         << jsonEscape(pathString(companionSnapshot.generatedOverlayPublishGate.backupDirectory)) << "\",\n";
    json << "  \"generated_overlay_publish_gate_publish_command\": \""
         << jsonEscape(companionSnapshot.generatedOverlayPublishGate.publishCommand) << "\",\n";
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
    json << "  \"mp_overlay_package_previous_host_available\": "
         << (companionSnapshot.mpOverlayPackage.previousHostAvailableKnown
                 ? (companionSnapshot.mpOverlayPackage.previousHostAvailable ? "true" : "false")
                 : "null")
         << ",\n";
    json << "  \"mp_overlay_package_previous_host_available_known\": "
         << (companionSnapshot.mpOverlayPackage.previousHostAvailableKnown ? "true" : "false") << ",\n";
    json << "  \"mp_overlay_package_readiness\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.readiness) << "\",\n";
    json << "  \"mp_overlay_package_host_readiness\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.hostReadiness) << "\",\n";
    json << "  \"mp_overlay_package_client_readiness_gate\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.clientReadinessGate) << "\",\n";
    json << "  \"mp_overlay_package_host_next_step\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.hostNextStep) << "\",\n";
    json << "  \"mp_overlay_package_client_next_step\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.clientNextStep) << "\",\n";
    json << "  \"mp_overlay_package_manifest_hash\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.packageManifestHash) << "\",\n";
    json << "  \"mp_overlay_package_provenance_state\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.provenanceState) << "\",\n";
    json << "  \"mp_overlay_package_source_quality_count\": " << companionSnapshot.mpOverlayPackage.sourceQualities.size() << ",\n";
    json << "  \"mp_overlay_package_source_qualities\": ";
    writeStringArrayJson(json, companionSnapshot.mpOverlayPackage.sourceQualities);
    json << ",\n";
    json << "  \"mp_overlay_package_bootstrap_campaign_count\": "
         << companionSnapshot.mpOverlayPackage.bootstrapCampaignCount << ",\n";
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
    json << "  \"mp_overlay_package_zip_hash\": \"" << jsonEscape(companionSnapshot.mpOverlayPackage.packageZipHash) << "\",\n";
    json << "  \"mp_overlay_package_zip_runtime_path\": \"" << jsonEscape(pathString(companionSnapshot.mpOverlayPackage.packageZipPath)) << "\",\n";
    json << "  \"mp_overlay_package_zip_bytes\": " << companionSnapshot.mpOverlayPackage.packageZipBytes << ",\n";
    json << "  \"status_center_state\": \"" << jsonEscape(state) << "\",\n";
    json << "  \"status_center_reason\": \"" << jsonEscape(reason) << "\",\n";
    json << "  \"status_center_summary_text\": \"" << jsonEscape(statusCenterSummaryText) << "\",\n";
    json << "  \"next_action\": \"" << jsonEscape(nextAction) << "\",\n";
    json << "  \"next_action_reason\": \"" << jsonEscape(nextActionReason) << "\",\n";
    json << "  \"next_action_command_hint\": \"" << jsonEscape(nextActionCommandHint) << "\",\n";
    json << "  \"next_action_command_hint_source\": \"" << jsonEscape(nextActionCommandHintSource) << "\",\n";
    json << "  \"next_action_path\": \"" << jsonEscape(pathString(nextActionPath)) << "\",\n";
    json << "  \"owner_test_playbook_path\": \"" << jsonEscape(pathString(companionSnapshot.ownerTestPlaybookPath)) << "\",\n";
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
    requestStatusWindowRefresh();
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
    g_trayIcon.hIcon = g_sncTrayIcon != nullptr ? g_sncTrayIcon : LoadIconW(nullptr, IDI_APPLICATION);
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
    registerStatusWindowClass();
    refreshStatusWindowContent();

    if (g_statusWindow != nullptr && IsWindow(g_statusWindow)) {
        ShowWindow(g_statusWindow, SW_SHOW);
        if (IsIconic(g_statusWindow)) {
            ShowWindow(g_statusWindow, SW_RESTORE);
        }
        SetForegroundWindow(g_statusWindow);
        return;
    }

    const HWND statusWindow = CreateWindowExW(
        WS_EX_APPWINDOW,
        kStatusWindowClassName,
        g_statusTitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        960,
        720,
        hwnd,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);

    if (statusWindow != nullptr) {
        applyWindowIcons(statusWindow);
        ShowWindow(statusWindow, SW_SHOWNORMAL);
        UpdateWindow(statusWindow);
        SetForegroundWindow(statusWindow);
    }
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
    requestStatusWindowRefresh();

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
    g_trayIconPath = g_repoRoot / "resources" / "snc_tray_icon.ico";
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
    g_gameplayAcceptanceReportPath =
        g_repoRoot / "dist" / "private_reports" / "generated_overlay_gameplay_acceptance_v0.json";
    g_mpOverlayPackageDirectory = g_repoRoot / "dist" / "private_reports" / "snc_mp_overlay_package";
    g_mpOverlayPackageZipPath = g_repoRoot / "dist" / "private_reports" / "snc_mp_overlay_package.zip";
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    initializePaths();
    g_sncTrayIcon = loadSncTrayIcon();

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

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"StrategicNexusCompanionTrayWindow";
    windowClass.hIcon = g_sncTrayIcon != nullptr ? g_sncTrayIcon : LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = g_sncTrayIcon != nullptr ? g_sncTrayIcon : LoadIconW(nullptr, IDI_APPLICATION);

    if (RegisterClassExW(&windowClass) == 0) {
        CloseHandle(mutex);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"Strategic Nexus Companion",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
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
    if (g_sncTrayIcon != nullptr) {
        DestroyIcon(g_sncTrayIcon);
        g_sncTrayIcon = nullptr;
    }
    return 0;
}
