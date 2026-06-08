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
#include "SncPostPlayArtifactBackfiller.h"
#include "SncTraySupportReportAction.h"
#include "SncTrayStartupShortcutAction.h"
#include "StrategicNexusCompanion.h"
#include "StellarisProcessDetector.h"
#include "StellarisSavePathResolver.h"
#include "common/FileUtil.h"
#include "common/JsonExtract.h"
#include "generated_overlay/MpOverlayPackage.h"

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>

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
constexpr UINT ID_TRAY_TOGGLE_STARTUP = 105;
constexpr UINT ID_TRAY_SUPPORT_REPORT = 106;
constexpr UINT ID_TRAY_OPEN_MP_PACKAGE = 107;
constexpr UINT ID_STATUS_REFRESH = 201;
constexpr UINT ID_STATUS_COPY = 202;
constexpr UINT ID_STATUS_OPEN_ARCHIVE = 203;
constexpr UINT ID_STATUS_OPEN_BRIEF = 204;
constexpr UINT ID_STATUS_OPEN_MP_PACKAGE = 205;
constexpr UINT ID_STATUS_CLOSE = 206;
constexpr UINT ID_STATUS_MAXIMIZE = 207;
constexpr UINT ID_STATUS_MINIMIZE = 208;
constexpr UINT ID_STATUS_TOGGLE_STARTUP = 209;
constexpr UINT ID_STATUS_SUPPORT_REPORT = 210;
constexpr UINT ID_STATUS_COPY_MP_VERIFY = 211;
constexpr UINT ID_STATUS_COPY_MP_IMPORT = 212;
constexpr UINT ID_STATUS_COPY_MP_STRICT_VERIFY = 213;
constexpr UINT ID_STATUS_COPY_MP_STRICT_IMPORT = 214;
constexpr UINT ID_STATUS_EXPORT_MP_PACKAGE = 215;
constexpr UINT ID_STATUS_COPY_LLM_PREPARE = 216;
constexpr UINT ID_STATUS_MP_IMPORT_HANDOFF = 217;
constexpr UINT ID_STATUS_PUBLISH_GENERATED_OVERLAY = 218;
constexpr UINT ID_STATUS_OPEN_NEXT_ACTION_PATH = 219;

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
    std::wstring overlayPublishCommand;
    std::wstring modelState;
    std::wstring modelRuntime;
    std::wstring modelRecommendation;
    std::wstring modelMode;
    std::wstring modelPrepareCommand;
    std::wstring nextAction;
    std::wstring nextReason;
    std::wstring nextHint;
    std::wstring nextPlaybook;
    std::wstring nextActionPath;
    std::wstring supportReportState;
    std::wstring supportReportPath;
    std::wstring crashRecoveryState;
    std::wstring crashRecoveryReason;
    std::wstring crashRecoveryPath;
    std::wstring humanControlGuardState;
    std::wstring mpPackageRefreshState;
    std::wstring mpPackageRefreshReason;
    std::wstring mpPackageDirectory;
    std::wstring mpPackageZipPath;
    std::wstring mpPackageZipState;
    std::wstring mpPackageZipReason;
    std::wstring mpPackageManifestHash;
    std::wstring mpPackageHandoffStatus;
    std::wstring mpPackageMismatchWarningState;
    std::wstring mpPackageMismatchWarningReason;
    std::wstring mpPackageIdentityMismatchAlert;
    std::wstring mpPackagePreviousHostAvailable;
    std::wstring mpPackagePreviousHostAvailableKnown;
    std::wstring mpPackageHostNextStep;
    std::wstring mpPackageClientNextStep;
    std::wstring mpPackageHandoffRecoveryHint;
    std::wstring mpPackageHostReadiness;
    std::wstring mpPackageClientReadinessGate;
    std::wstring mpPackageVerifyCommand;
    std::wstring mpPackageImportCommand;
    std::wstring mpPackageStrictVerifyCommand;
    std::wstring mpPackageStrictImportCommand;
};

struct DashboardSectionSpec {
    const wchar_t* title;
    std::array<StatusFieldId, 4> fields;
};

struct StatusScrollMetrics {
    int firstVisibleLine = 0;
    int visibleLines = 0;
    int totalLines = 0;
    int maxFirstVisibleLine = 0;
    bool canScroll = false;
};

struct StatusCustomScrollbar {
    RECT trackRect{};
    RECT thumbRect{};
    bool reserved = false;
    bool visible = false;
};

enum class StatusScrollTargetKind {
    None = 0,
    Field,
    Details
};

std::atomic_bool g_stopRequested{false};
std::atomic_bool g_mpPackageRefreshRequested{false};
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
std::filesystem::path g_supportReportPreviewPath;
std::filesystem::path g_crashRecoveryStatePath;
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
std::array<StatusCustomScrollbar, static_cast<std::size_t>(StatusFieldId::Count)> g_statusFieldScrollbars{};
std::array<HWND, 4> g_statusSectionTitles{};
StatusCustomScrollbar g_statusDetailsScrollbar{};
HWND g_statusBottomTitle = nullptr;
HWND g_statusRefreshButton = nullptr;
HWND g_statusCopyButton = nullptr;
HWND g_statusOpenArchiveButton = nullptr;
HWND g_statusOpenBriefButton = nullptr;
HWND g_statusOpenMpPackageButton = nullptr;
HWND g_statusPublishGeneratedOverlayButton = nullptr;
HWND g_statusOpenNextActionPathButton = nullptr;
HWND g_statusExportMpPackageButton = nullptr;
HWND g_statusMpImportHandoffButton = nullptr;
HWND g_statusCopyMpVerifyButton = nullptr;
HWND g_statusCopyMpImportButton = nullptr;
HWND g_statusCopyMpStrictVerifyButton = nullptr;
HWND g_statusCopyMpStrictImportButton = nullptr;
HWND g_statusCopyLlmPrepareButton = nullptr;
HWND g_statusSupportReportButton = nullptr;
HWND g_statusToggleStartupButton = nullptr;
HWND g_statusCloseButton = nullptr;
HWND g_statusMaximizeButton = nullptr;
HWND g_statusMinimizeButton = nullptr;
HWND g_statusWindow = nullptr;
HWND g_statusHeader = nullptr;
HWND g_statusSubtitle = nullptr;
HWND g_statusDetails = nullptr;
StatusScrollTargetKind g_statusScrollDragTargetKind = StatusScrollTargetKind::None;
std::size_t g_statusScrollDragFieldIndex = 0;
int g_statusScrollDragOffsetY = 0;
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
constexpr COLORREF kStatusButtonFillColor = RGB(8, 22, 28);
constexpr COLORREF kStatusButtonPressedFillColor = RGB(18, 70, 65);
constexpr COLORREF kStatusButtonDisabledFillColor = RGB(6, 11, 13);
constexpr COLORREF kStatusButtonBorderColor = RGB(164, 122, 69);
constexpr COLORREF kStatusButtonTextColor = RGB(211, 228, 221);
constexpr COLORREF kStatusButtonDisabledTextColor = RGB(92, 102, 100);
constexpr COLORREF kStatusScrollTrackColor = RGB(10, 28, 34);
constexpr COLORREF kStatusScrollThumbColor = RGB(83, 123, 116);
constexpr COLORREF kStatusScrollThumbDragColor = RGB(115, 156, 148);
constexpr const wchar_t* kStatusEmptyValue = L"\u2014";
constexpr wchar_t kStatusWindowClassName[] = L"StrategicNexusCompanionStatusWindow";
constexpr int kStatusTitleBarHeight = 34;
constexpr int kStatusTitleButtonWidth = 42;
constexpr int kStatusResizeBorder = 8;
constexpr int kStatusScrollbarTrackWidth = 12;
constexpr int kStatusScrollbarThumbWidth = 8;
constexpr UINT_PTR kStatusScrollSyncTimerId = 1;
constexpr int kSncTrayIconResourceId = 1;
constexpr wchar_t kSncAppUserModelId[] = L"StrategicNexus.Companion";
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
std::wstring compactBoolText(const std::wstring& value, const wchar_t* yesText = L"ano", const wchar_t* noText = L"ne");
std::wstring combineValues(const std::vector<std::wstring>& values, const wchar_t* separator = L" | ");
std::filesystem::path findRepoRoot();
const wchar_t* statusFieldLabel(StatusFieldId id);
bool statusFieldUsesMultiline(StatusFieldId id);
int statusFieldLineUnits(StatusFieldId id);
void setWindowFont(HWND hwnd, HFONT font);
HWND createStatusStatic(HWND parent, const wchar_t* text, DWORD style = WS_CHILD | WS_VISIBLE);
HWND createStatusValue(HWND parent, StatusFieldId fieldId, const wchar_t* text = L"");
HWND createStatusButton(HWND parent, UINT id, const wchar_t* text);
void drawStatusButton(const DRAWITEMSTRUCT& drawItem);
StatusScrollMetrics measureStatusScrollMetrics(HWND control);
StatusCustomScrollbar composeStatusScrollbar(const RECT& trackRect, const StatusScrollMetrics& metrics, bool reserveSpace);
void syncStatusCustomScrollbars(HWND hwnd, bool forceInvalidate = false);
void paintStatusCustomScrollbars(HDC hdc);
HWND statusScrollTargetWindow(StatusScrollTargetKind kind, std::size_t fieldIndex);
StatusCustomScrollbar* statusScrollTargetScrollbar(StatusScrollTargetKind kind, std::size_t fieldIndex);
bool statusPointHitsCustomScrollbar(POINT point, StatusScrollTargetKind* kind, std::size_t* fieldIndex);
void clearStatusScrollDrag();
void scrollStatusTargetByLines(StatusScrollTargetKind kind, std::size_t fieldIndex, int lineDelta);
void scrollStatusTargetByPage(StatusScrollTargetKind kind, std::size_t fieldIndex, bool forward);
void dragStatusScrollbarThumb(HWND hwnd, POINT point);
void openPathWithShell(HWND hwnd, const std::filesystem::path& path);
std::filesystem::path resolveDashboardPath(const std::wstring& path);
bool dashboardPathExists(const std::wstring& path);
void openNextActionPath(HWND hwnd);
void openMpOverlayPackageDirectory();
void publishStagedGeneratedOverlay(HWND hwnd);
bool canRefreshMpPackageExport();
void requestMpPackageExportRefresh();
bool copyTextToClipboard(HWND hwnd, const std::wstring& text);
void requestMpPackageImportHandoff(HWND hwnd);
void updateTrayTip(HWND hwnd, const std::wstring& text);
void updateStatusCaptionButtons(HWND hwnd);
void ensureStatusWindowResources();
void destroyStatusWindowResources();
void refreshStatusWindowContent();
void requestStatusWindowRefresh();
void layoutStatusWindow(HWND hwnd);
void registerStatusWindowClass();
void applyWindowIcons(HWND hwnd);
std::filesystem::path buildDefaultStartupShortcutPath();
strategic_nexus::SncTrayStartupShortcutActionStatus loadStartupShortcutActionStatus();
strategic_nexus::SncTraySupportReportActionStatus loadSupportReportActionStatus();
void toggleStartWithWindows(HWND hwnd);
void prepareOrOpenSupportReport(HWND hwnd);
LRESULT CALLBACK statusWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

struct RepoCommandExecutionResult {
    bool launched = false;
    bool completed = false;
    DWORD exitCode = ERROR_GEN_FAILURE;
    std::wstring errorMessage;
};

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

std::wstring compactBoolText(const std::wstring& value, const wchar_t* yesText, const wchar_t* noText)
{
    if (value == L"true") {
        return yesText;
    }
    if (value == L"false") {
        return noText;
    }
    return value.empty() ? kStatusEmptyValue : value;
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
    case StatusFieldId::ArchiveVerified: return L"Ov\u011B\u0159eno";
    case StatusFieldId::ArchiveCopied: return L"Kopie";
    case StatusFieldId::ArchiveSkipped: return L"P\u0159esko\u010Deno";
    case StatusFieldId::OverlayStaging: return L"Staging";
    case StatusFieldId::OverlayGate: return L"Gate";
    case StatusFieldId::OverlayAllowed: return L"Povoleno";
    case StatusFieldId::OverlayActive: return L"Aktivn\u00ED";
    case StatusFieldId::ModelState: return L"Stav";
    case StatusFieldId::ModelRuntime: return L"Runtime";
    case StatusFieldId::ModelRecommendation: return L"Doporu\u010Den\u00ED";
    case StatusFieldId::ModelMode: return L"Re\u017Eim";
    case StatusFieldId::Count: break;
    }
    return L"";
}

bool statusFieldUsesMultiline(const StatusFieldId id)
{
    switch (id) {
    case StatusFieldId::LiveState:
    case StatusFieldId::ArchivePath:
    case StatusFieldId::OverlayGate:
    case StatusFieldId::OverlayActive:
    case StatusFieldId::ModelState:
    case StatusFieldId::ModelRecommendation:
        return true;
    case StatusFieldId::LiveStellaris:
    case StatusFieldId::LiveSession:
    case StatusFieldId::LiveStartup:
    case StatusFieldId::ArchiveVerified:
    case StatusFieldId::ArchiveCopied:
    case StatusFieldId::ArchiveSkipped:
    case StatusFieldId::OverlayStaging:
    case StatusFieldId::OverlayAllowed:
    case StatusFieldId::ModelRuntime:
    case StatusFieldId::ModelMode:
    case StatusFieldId::Count:
        break;
    }
    return false;
}

int statusFieldLineUnits(const StatusFieldId id)
{
    return statusFieldUsesMultiline(id) ? 2 : 1;
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

HWND createStatusValue(HWND parent, const StatusFieldId fieldId, const wchar_t* text)
{
    DWORD style = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY | ES_NOHIDESEL;
    if (statusFieldUsesMultiline(fieldId)) {
        style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
    } else {
        style |= ES_AUTOHSCROLL;
    }

    return CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
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
        ? kStatusButtonDisabledFillColor
        : (selected ? kStatusButtonPressedFillColor : kStatusButtonFillColor);

    const HBRUSH fillBrush = CreateSolidBrush(fillColor);
    FillRect(drawItem.hDC, &rc, fillBrush);
    DeleteObject(fillBrush);

    const HBRUSH borderBrush = CreateSolidBrush(kStatusButtonBorderColor);
    FrameRect(drawItem.hDC, &rc, borderBrush);
    DeleteObject(borderBrush);

    if (focused) {
        RECT focusRc = rc;
        InflateRect(&focusRc, -4, -4);
        DrawFocusRect(drawItem.hDC, &focusRc);
    }

    wchar_t label[128]{};
    GetWindowTextW(drawItem.hwndItem, label, static_cast<int>(std::size(label)));

    SetBkMode(drawItem.hDC, TRANSPARENT);
    SetTextColor(drawItem.hDC, disabled ? kStatusButtonDisabledTextColor : kStatusButtonTextColor);
    const HGDIOBJ oldFont = SelectObject(drawItem.hDC, g_statusSectionFont != nullptr ? g_statusSectionFont : GetStockObject(DEFAULT_GUI_FONT));
    rc.left += 10;
    rc.right -= 10;
    DrawTextW(drawItem.hDC, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (oldFont != nullptr) {
        SelectObject(drawItem.hDC, oldFont);
    }
}

StatusScrollMetrics measureStatusScrollMetrics(HWND control)
{
    StatusScrollMetrics metrics{};
    if (control == nullptr || !IsWindow(control)) {
        return metrics;
    }

    RECT textRect{};
    SendMessageW(control, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&textRect));
    const int textHeight = textRect.bottom - textRect.top;

    const HDC hdc = GetDC(control);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
    const HGDIOBJ oldFont = (hdc != nullptr && font != nullptr) ? SelectObject(hdc, font) : nullptr;
    TEXTMETRICW textMetric{};
    if (hdc != nullptr) {
        GetTextMetricsW(hdc, &textMetric);
    }
    if (oldFont != nullptr) {
        SelectObject(hdc, oldFont);
    }
    if (hdc != nullptr) {
        ReleaseDC(control, hdc);
    }

    int lineHeight = textMetric.tmHeight;
    if (lineHeight <= 0) {
        lineHeight = 16;
    }

    metrics.totalLines = static_cast<int>(SendMessageW(control, EM_GETLINECOUNT, 0, 0));
    if (metrics.totalLines <= 0) {
        metrics.totalLines = 1;
    }
    metrics.visibleLines = textHeight > 0 ? (textHeight / lineHeight) : 1;
    if (metrics.visibleLines <= 0) {
        metrics.visibleLines = 1;
    }
    metrics.firstVisibleLine = static_cast<int>(SendMessageW(control, EM_GETFIRSTVISIBLELINE, 0, 0));
    metrics.maxFirstVisibleLine = metrics.totalLines - metrics.visibleLines;
    if (metrics.maxFirstVisibleLine < 0) {
        metrics.maxFirstVisibleLine = 0;
    }
    if (metrics.firstVisibleLine < 0) {
        metrics.firstVisibleLine = 0;
    }
    if (metrics.firstVisibleLine > metrics.maxFirstVisibleLine) {
        metrics.firstVisibleLine = metrics.maxFirstVisibleLine;
    }
    metrics.canScroll = metrics.totalLines > metrics.visibleLines;
    return metrics;
}

StatusCustomScrollbar composeStatusScrollbar(const RECT& trackRect, const StatusScrollMetrics& metrics, const bool reserveSpace)
{
    StatusCustomScrollbar scrollbar{};
    scrollbar.reserved = reserveSpace;
    scrollbar.trackRect = trackRect;
    scrollbar.visible = reserveSpace;
    SetRectEmpty(&scrollbar.thumbRect);

    if (!reserveSpace) {
        SetRectEmpty(&scrollbar.trackRect);
        return scrollbar;
    }

    RECT innerTrack = trackRect;
    InflateRect(&innerTrack, -2, -2);
    if ((innerTrack.right - innerTrack.left) <= 0 || (innerTrack.bottom - innerTrack.top) <= 0) {
        return scrollbar;
    }

    if (!metrics.canScroll) {
        return scrollbar;
    }

    const int trackHeight = innerTrack.bottom - innerTrack.top;
    int thumbHeight = trackHeight;
    if (metrics.totalLines > 0) {
        thumbHeight = (trackHeight * metrics.visibleLines) / metrics.totalLines;
    }
    if (thumbHeight < 22) {
        thumbHeight = 22;
    }
    if (thumbHeight > trackHeight) {
        thumbHeight = trackHeight;
    }

    const int thumbTravel = trackHeight - thumbHeight;
    int thumbTop = innerTrack.top;
    if (thumbTravel > 0 && metrics.maxFirstVisibleLine > 0) {
        thumbTop += (thumbTravel * metrics.firstVisibleLine) / metrics.maxFirstVisibleLine;
    }

    const int trackWidth = innerTrack.right - innerTrack.left;
    const int thumbWidth = (trackWidth >= kStatusScrollbarThumbWidth) ? kStatusScrollbarThumbWidth : trackWidth;
    const int thumbLeft = innerTrack.left + ((trackWidth - thumbWidth) / 2);
    scrollbar.thumbRect = { thumbLeft, thumbTop, thumbLeft + thumbWidth, thumbTop + thumbHeight };
    return scrollbar;
}

HWND statusScrollTargetWindow(const StatusScrollTargetKind kind, const std::size_t fieldIndex)
{
    switch (kind) {
    case StatusScrollTargetKind::Field:
        if (fieldIndex < g_statusFieldValues.size()) {
            return g_statusFieldValues[fieldIndex];
        }
        break;
    case StatusScrollTargetKind::Details:
        return g_statusDetails;
    case StatusScrollTargetKind::None:
        break;
    }
    return nullptr;
}

StatusCustomScrollbar* statusScrollTargetScrollbar(const StatusScrollTargetKind kind, const std::size_t fieldIndex)
{
    switch (kind) {
    case StatusScrollTargetKind::Field:
        if (fieldIndex < g_statusFieldScrollbars.size()) {
            return &g_statusFieldScrollbars[fieldIndex];
        }
        break;
    case StatusScrollTargetKind::Details:
        return &g_statusDetailsScrollbar;
    case StatusScrollTargetKind::None:
        break;
    }
    return nullptr;
}

bool statusPointHitsCustomScrollbar(const POINT point, StatusScrollTargetKind* kind, std::size_t* fieldIndex)
{
    for (std::size_t index = 0; index < g_statusFieldScrollbars.size(); ++index) {
        const auto& scrollbar = g_statusFieldScrollbars[index];
        if (scrollbar.reserved && PtInRect(&scrollbar.trackRect, point)) {
            if (kind != nullptr) {
                *kind = StatusScrollTargetKind::Field;
            }
            if (fieldIndex != nullptr) {
                *fieldIndex = index;
            }
            return true;
        }
    }

    if (g_statusDetailsScrollbar.reserved && PtInRect(&g_statusDetailsScrollbar.trackRect, point)) {
        if (kind != nullptr) {
            *kind = StatusScrollTargetKind::Details;
        }
        if (fieldIndex != nullptr) {
            *fieldIndex = 0;
        }
        return true;
    }

    return false;
}

void clearStatusScrollDrag()
{
    g_statusScrollDragTargetKind = StatusScrollTargetKind::None;
    g_statusScrollDragFieldIndex = 0;
    g_statusScrollDragOffsetY = 0;
}

void scrollStatusTargetByLines(const StatusScrollTargetKind kind, const std::size_t fieldIndex, const int lineDelta)
{
    if (lineDelta == 0) {
        return;
    }

    const HWND target = statusScrollTargetWindow(kind, fieldIndex);
    if (target == nullptr || !IsWindow(target)) {
        return;
    }

    SendMessageW(target, EM_LINESCROLL, 0, static_cast<LPARAM>(lineDelta));
}

void scrollStatusTargetByPage(const StatusScrollTargetKind kind, const std::size_t fieldIndex, const bool forward)
{
    const HWND target = statusScrollTargetWindow(kind, fieldIndex);
    if (target == nullptr || !IsWindow(target)) {
        return;
    }

    const auto metrics = measureStatusScrollMetrics(target);
    int pageDelta = metrics.visibleLines - 1;
    if (pageDelta < 1) {
        pageDelta = 1;
    }
    scrollStatusTargetByLines(kind, fieldIndex, forward ? pageDelta : -pageDelta);
}

void dragStatusScrollbarThumb(HWND hwnd, const POINT point)
{
    if (g_statusScrollDragTargetKind == StatusScrollTargetKind::None) {
        return;
    }

    const HWND target = statusScrollTargetWindow(g_statusScrollDragTargetKind, g_statusScrollDragFieldIndex);
    auto* scrollbar = statusScrollTargetScrollbar(g_statusScrollDragTargetKind, g_statusScrollDragFieldIndex);
    if (target == nullptr || scrollbar == nullptr || !scrollbar->reserved || !IsWindow(target)) {
        return;
    }

    const auto metrics = measureStatusScrollMetrics(target);
    if (!metrics.canScroll) {
        return;
    }

    RECT innerTrack = scrollbar->trackRect;
    InflateRect(&innerTrack, -2, -2);
    const int trackHeight = innerTrack.bottom - innerTrack.top;
    const int thumbHeight = scrollbar->thumbRect.bottom - scrollbar->thumbRect.top;
    const int thumbTravel = trackHeight - thumbHeight;
    if (thumbTravel <= 0 || metrics.maxFirstVisibleLine <= 0) {
        return;
    }

    int desiredTop = point.y - g_statusScrollDragOffsetY;
    if (desiredTop < innerTrack.top) {
        desiredTop = innerTrack.top;
    }
    const int maxTop = innerTrack.bottom - thumbHeight;
    if (desiredTop > maxTop) {
        desiredTop = maxTop;
    }

    const int targetFirstVisibleLine =
        ((desiredTop - innerTrack.top) * metrics.maxFirstVisibleLine + (thumbTravel / 2)) / thumbTravel;
    const int lineDelta = targetFirstVisibleLine - metrics.firstVisibleLine;
    if (lineDelta != 0) {
        scrollStatusTargetByLines(g_statusScrollDragTargetKind, g_statusScrollDragFieldIndex, lineDelta);
        syncStatusCustomScrollbars(hwnd);
    }
}

void invalidateStatusScrollbarRect(HWND hwnd, const RECT& rect)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || IsRectEmpty(&rect)) {
        return;
    }

    RECT paddedRect = rect;
    InflateRect(&paddedRect, 2, 2);
    InvalidateRect(hwnd, &paddedRect, TRUE);
}

void invalidateStatusScrollbarTransition(HWND hwnd, const StatusCustomScrollbar& previous, const StatusCustomScrollbar& current)
{
    if (IsRectEmpty(&previous.trackRect)) {
        invalidateStatusScrollbarRect(hwnd, current.trackRect);
        return;
    }
    if (IsRectEmpty(&current.trackRect)) {
        invalidateStatusScrollbarRect(hwnd, previous.trackRect);
        return;
    }

    RECT invalidateRect = previous.trackRect;
    UnionRect(&invalidateRect, &invalidateRect, &current.trackRect);
    invalidateStatusScrollbarRect(hwnd, invalidateRect);
}

void invalidateStatusCustomScrollbarAreas(HWND hwnd)
{
    for (const auto& scrollbar : g_statusFieldScrollbars) {
        invalidateStatusScrollbarRect(hwnd, scrollbar.trackRect);
    }
    invalidateStatusScrollbarRect(hwnd, g_statusDetailsScrollbar.trackRect);
}

void syncStatusCustomScrollbars(HWND hwnd, const bool forceInvalidate)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }

    for (std::size_t index = 0; index < g_statusFieldValues.size(); ++index) {
        const HWND control = g_statusFieldValues[index];
        auto& scrollbar = g_statusFieldScrollbars[index];
        if (!scrollbar.reserved || control == nullptr || !IsWindow(control)) {
            continue;
        }

        const auto previous = scrollbar;
        scrollbar = composeStatusScrollbar(scrollbar.trackRect, measureStatusScrollMetrics(control), true);
        if (forceInvalidate ||
            !EqualRect(&previous.trackRect, &scrollbar.trackRect) ||
            !EqualRect(&previous.thumbRect, &scrollbar.thumbRect) ||
            previous.visible != scrollbar.visible) {
            invalidateStatusScrollbarTransition(hwnd, previous, scrollbar);
        }
    }

    if (g_statusDetailsScrollbar.reserved && g_statusDetails != nullptr && IsWindow(g_statusDetails)) {
        const auto previous = g_statusDetailsScrollbar;
        g_statusDetailsScrollbar =
            composeStatusScrollbar(g_statusDetailsScrollbar.trackRect, measureStatusScrollMetrics(g_statusDetails), true);
        if (forceInvalidate ||
            !EqualRect(&previous.trackRect, &g_statusDetailsScrollbar.trackRect) ||
            !EqualRect(&previous.thumbRect, &g_statusDetailsScrollbar.thumbRect) ||
            previous.visible != g_statusDetailsScrollbar.visible) {
            invalidateStatusScrollbarTransition(hwnd, previous, g_statusDetailsScrollbar);
        }
    }

}

void paintStatusCustomScrollbars(HDC hdc)
{
    if (hdc == nullptr) {
        return;
    }

    const HBRUSH gutterBrush = CreateSolidBrush(kStatusValueBackgroundColor);
    const HBRUSH borderBrush = CreateSolidBrush(kStatusValueBorderColor);
    const HBRUSH trackBrush = CreateSolidBrush(kStatusScrollTrackColor);

    const auto paintScrollbar = [&](const StatusCustomScrollbar& scrollbar, const bool dragged) {
        if (!scrollbar.reserved) {
            return;
        }

        FillRect(hdc, &scrollbar.trackRect, gutterBrush);
        FrameRect(hdc, &scrollbar.trackRect, borderBrush);

        RECT innerTrack = scrollbar.trackRect;
        InflateRect(&innerTrack, -2, -2);
        if ((innerTrack.right - innerTrack.left) > 0 && (innerTrack.bottom - innerTrack.top) > 0) {
            FillRect(hdc, &innerTrack, trackBrush);
        }

        if (!IsRectEmpty(&scrollbar.thumbRect)) {
            const HBRUSH activeThumbBrush = CreateSolidBrush(dragged ? kStatusScrollThumbDragColor : kStatusScrollThumbColor);
            FillRect(hdc, &scrollbar.thumbRect, activeThumbBrush);
            DeleteObject(activeThumbBrush);
        }
    };

    for (std::size_t index = 0; index < g_statusFieldScrollbars.size(); ++index) {
        paintScrollbar(
            g_statusFieldScrollbars[index],
            g_statusScrollDragTargetKind == StatusScrollTargetKind::Field && g_statusScrollDragFieldIndex == index);
    }
    paintScrollbar(g_statusDetailsScrollbar, g_statusScrollDragTargetKind == StatusScrollTargetKind::Details);

    DeleteObject(trackBrush);
    DeleteObject(borderBrush);
    DeleteObject(gutterBrush);
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

std::filesystem::path resolveDashboardPath(const std::wstring& path)
{
    if (path.empty()) {
        return {};
    }

    std::filesystem::path resolved(path);
    if (resolved.is_relative()) {
        resolved = g_repoRoot / resolved;
    }
    return resolved.lexically_normal();
}

bool dashboardPathExists(const std::wstring& path)
{
    const auto resolved = resolveDashboardPath(path);
    if (resolved.empty()) {
        return false;
    }

    std::error_code error;
    return std::filesystem::exists(resolved, error) && !error;
}

void openNextActionPath(HWND hwnd)
{
    const auto data = loadStatusDashboardData();
    const auto path = resolveDashboardPath(data.nextActionPath);
    if (path.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    openPathWithShell(hwnd, path);
}

void updateStatusCaptionButtons(HWND hwnd)
{
    if (g_statusMaximizeButton != nullptr) {
        SetWindowTextW(g_statusMaximizeButton, IsZoomed(hwnd) ? L"[]" : L"[ ]");
    }
    if (g_statusMinimizeButton != nullptr) {
        SetWindowTextW(g_statusMinimizeButton, L"-");
    }
    if (g_statusCloseButton != nullptr) {
        SetWindowTextW(g_statusCloseButton, L"X");
    }
}

StatusDashboardData loadStatusDashboardData()
{
    StatusDashboardData data;
    data.subtitle = L"Stav: SNC bezi.";
    data.liveState = L"SNC bezi.";
    data.liveStellaris = L"\u2014";
    data.liveSession = L"\u2014";
    data.liveStartup = L"\u2014";
    data.archivePath = makeRelativeDisplay(g_archiveRoot);
    data.archiveVerified = L"\u2014";
    data.archiveCopied = L"0";
    data.archiveSkipped = L"0";
    data.overlayStaging = L"\u2014";
    data.overlayGate = L"\u2014";
    data.overlayAllowed = L"\u2014";
    data.overlayActive = makeRelativeDisplay(g_generatedOverlayActiveDirectory);
    data.modelState = L"\u2014";
    data.modelRuntime = L"\u2014";
    data.modelRecommendation = L"\u2014";
    data.modelMode = L"\u2014";
    data.modelPrepareCommand.clear();
    data.nextAction = L"\u2014";
    data.nextReason = L"\u2014";
    data.nextHint = L"\u2014";
    data.nextPlaybook = g_nextStepsBriefPath.empty() ? L"\u2014" : utf8ToWide(pathString(g_nextStepsBriefPath));
    data.nextActionPath = L"\u2014";
    data.supportReportState = L"\u2014";
    data.supportReportPath = g_supportReportPreviewPath.empty() ? L"\u2014" : utf8ToWide(pathString(g_supportReportPreviewPath));
    data.crashRecoveryState = L"\u2014";
    data.crashRecoveryReason = L"\u2014";
    data.crashRecoveryPath = g_crashRecoveryStatePath.empty() ? L"\u2014" : utf8ToWide(pathString(g_crashRecoveryStatePath));
    data.humanControlGuardState = L"\u2014";

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
    data.overlayPublishCommand = utf8ToWide(summaryValue("generated_overlay_publish_gate_publish_command"));
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
    data.modelPrepareCommand = utf8ToWide(summaryValue("local_llm_prepare_command_hint"));
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
    data.supportReportState = utf8ToWide(strategic_nexus::common::extractJsonString(json, "support_report_state").value_or(""));
    data.supportReportPath =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "support_report_preview_path").value_or(""));
    data.crashRecoveryState =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "crash_recovery_state").value_or(""));
    data.crashRecoveryReason =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "crash_recovery_reason").value_or(""));
    data.crashRecoveryPath =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "crash_recovery_state_path").value_or(""));
    const std::string humanControlGuardState = summaryValue("human_control_guard_state");
    if (!humanControlGuardState.empty()) {
        data.humanControlGuardState = utf8ToWide(humanControlGuardState);
    }
    data.mpPackageRefreshState = utf8ToWide(summaryValue("mp_package_refresh_state"));
    data.mpPackageRefreshReason = utf8ToWide(summaryValue("mp_package_refresh_reason"));
    data.mpPackageDirectory = utf8ToWide(summaryValue("mp_overlay_package_directory"));
    data.mpPackageZipPath = utf8ToWide(summaryValue("mp_package_zip_runtime_path"));
    data.mpPackageZipState = utf8ToWide(summaryValue("mp_package_zip_state"));
    data.mpPackageZipReason = utf8ToWide(summaryValue("mp_package_zip_reason"));
    data.mpPackageManifestHash = utf8ToWide(summaryValue("mp_package_manifest_hash"));
    data.mpPackageHandoffStatus = utf8ToWide(summaryValue("mp_handoff_status"));
    data.mpPackageMismatchWarningState = utf8ToWide(summaryValue("mp_mismatch_warning_state"));
    data.mpPackageMismatchWarningReason = utf8ToWide(summaryValue("mp_mismatch_warning_reason"));
    data.mpPackageIdentityMismatchAlert = utf8ToWide(summaryValue("mp_identity_mismatch_alert"));
    data.mpPackagePreviousHostAvailable = utf8ToWide(summaryValue("mp_previous_host_available"));
    data.mpPackagePreviousHostAvailableKnown = utf8ToWide(summaryValue("mp_previous_host_available_known"));
    data.mpPackageHostNextStep = utf8ToWide(summaryValue("mp_host_next_step"));
    data.mpPackageClientNextStep = utf8ToWide(summaryValue("mp_client_next_step"));
    data.mpPackageHandoffRecoveryHint = utf8ToWide(summaryValue("mp_handoff_recovery_hint"));
    data.mpPackageHostReadiness = utf8ToWide(summaryValue("mp_host_readiness"));
    data.mpPackageClientReadinessGate = utf8ToWide(summaryValue("mp_client_readiness_gate"));
    data.mpPackageVerifyCommand = utf8ToWide(summaryValue("mp_verify_command"));
    data.mpPackageImportCommand = utf8ToWide(summaryValue("mp_import_command"));
    data.mpPackageStrictVerifyCommand = utf8ToWide(summaryValue("mp_strict_verify_command"));
    data.mpPackageStrictImportCommand = utf8ToWide(summaryValue("mp_strict_import_command"));
    return data;
}

std::wstring buildDashboardBottomText(const StatusDashboardData& data)
{
    std::wstring text = L"Dal\u0161\u00ED krok: ";
    text += data.nextAction.empty() ? kStatusEmptyValue : data.nextAction;
    text += L"\r\nD\u016Fvod: ";
    text += data.nextReason.empty() ? kStatusEmptyValue : data.nextReason;
    text += L"\r\nHint: ";
    text += data.nextHint.empty() ? kStatusEmptyValue : data.nextHint;
    text += L"\r\nPlaybook: ";
    text += data.nextPlaybook.empty() ? kStatusEmptyValue : data.nextPlaybook;
    text += L"\r\nC\u00EDl: ";
    text += data.nextActionPath.empty() ? kStatusEmptyValue : data.nextActionPath;
    text += L"\r\nSupport report: ";
    text += data.supportReportState.empty() ? kStatusEmptyValue : data.supportReportState;
    text += L"\r\nReport cesta: ";
    text += data.supportReportPath.empty() ? kStatusEmptyValue : data.supportReportPath;
    text += L"\r\nRecovery: ";
    text += data.crashRecoveryState.empty() ? kStatusEmptyValue : data.crashRecoveryState;
    text += L"\r\nRecovery duvod: ";
    text += data.crashRecoveryReason.empty() ? kStatusEmptyValue : data.crashRecoveryReason;
    text += L"\r\nRecovery cesta: ";
    text += data.crashRecoveryPath.empty() ? kStatusEmptyValue : data.crashRecoveryPath;
    text += L"\r\nHuman control guard: ";
    text += data.humanControlGuardState.empty() ? kStatusEmptyValue : data.humanControlGuardState;
    if (!data.mpPackageRefreshState.empty() || !data.mpPackageZipState.empty() || !data.mpPackageManifestHash.empty()) {
        text += L"\r\nMP bal\u00ED\u010Dek: ";
        text += data.mpPackageRefreshState.empty() ? kStatusEmptyValue : data.mpPackageRefreshState;
        if (!data.mpPackageRefreshReason.empty()) {
            text += L" (" + data.mpPackageRefreshReason + L")";
        }
        text += L"\r\nMP cesta: ";
        text += data.mpPackageDirectory.empty() ? kStatusEmptyValue : data.mpPackageDirectory;
        text += L"\r\nMP zip: ";
        text += data.mpPackageZipPath.empty() ? kStatusEmptyValue : data.mpPackageZipPath;
        if (!data.mpPackageZipState.empty()) {
            text += L"\r\nMP zip stav: " + data.mpPackageZipState;
            if (!data.mpPackageZipReason.empty()) {
                text += L" (" + data.mpPackageZipReason + L")";
            }
        }
        if (!data.mpPackageManifestHash.empty()) {
            text += L"\r\nMP manifest hash: " + data.mpPackageManifestHash;
        }
        if (!data.mpPackageHandoffStatus.empty()) {
            text += L"\r\nMP handoff: " + data.mpPackageHandoffStatus;
        }
        if (!data.mpPackageMismatchWarningState.empty()) {
            text += L"\r\nMP mismatch: " + data.mpPackageMismatchWarningState;
            if (!data.mpPackageMismatchWarningReason.empty()) {
                text += L" (" + data.mpPackageMismatchWarningReason + L")";
            }
        }
        if (!data.mpPackageIdentityMismatchAlert.empty()) {
            text += L"\r\nMP mismatch alert: " + data.mpPackageIdentityMismatchAlert;
        }
        if (!data.mpPackageZipPath.empty() || !data.mpPackageManifestHash.empty()) {
            text += L"\r\nMP sdileni: zkopiruj MP zip a manifest hash; host/klient kroky jsou nize.";
        }
        if (!data.mpPackageHostReadiness.empty()) {
            text += L"\r\nMP host readiness: " + data.mpPackageHostReadiness;
        }
        if (!data.mpPackageClientReadinessGate.empty()) {
            text += L"\r\nMP client gate: " + data.mpPackageClientReadinessGate;
        }
        if (!data.mpPackageHostNextStep.empty()) {
            text += L"\r\nMP host krok: " + data.mpPackageHostNextStep;
        }
        if (!data.mpPackageClientNextStep.empty()) {
            text += L"\r\nMP client krok: " + data.mpPackageClientNextStep;
        }
        if (!data.mpPackageVerifyCommand.empty()) {
            text += L"\r\nMP verify: " + data.mpPackageVerifyCommand;
        }
        if (!data.mpPackageImportCommand.empty()) {
            text += L"\r\nMP import: " + data.mpPackageImportCommand;
        }
        if (!data.mpPackageStrictVerifyCommand.empty()) {
            text += L"\r\nMP strict verify: " + data.mpPackageStrictVerifyCommand;
        }
        if (!data.mpPackageStrictImportCommand.empty()) {
            text += L"\r\nMP strict import: " + data.mpPackageStrictImportCommand;
        }
    }
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
    text += L"Dal\u0161\u00ED krok: " + data.nextAction + L"\n";
    text += L"Hint: " + data.nextHint + L"\n";
    text += L"Support report: " + data.supportReportState + L"\n";
    text += L"Crash recovery: " + data.crashRecoveryState;
    text += L"\nHuman control guard: ";
    text += data.humanControlGuardState.empty() ? kStatusEmptyValue : data.humanControlGuardState;
    if (!data.mpPackageRefreshState.empty() || !data.mpPackageZipState.empty() || !data.mpPackageManifestHash.empty()) {
        text += L"\nMP bal\u00ED\u010Dek: ";
        text += data.mpPackageRefreshState.empty() ? kStatusEmptyValue : data.mpPackageRefreshState;
        if (!data.mpPackageRefreshReason.empty()) {
            text += L" (" + data.mpPackageRefreshReason + L")";
        }
        text += L"\nMP cesta: ";
        text += data.mpPackageDirectory.empty() ? kStatusEmptyValue : data.mpPackageDirectory;
        text += L"\nMP zip: ";
        text += data.mpPackageZipPath.empty() ? kStatusEmptyValue : data.mpPackageZipPath;
        if (!data.mpPackageZipState.empty()) {
            text += L"\nMP zip stav: " + data.mpPackageZipState;
            if (!data.mpPackageZipReason.empty()) {
                text += L" (" + data.mpPackageZipReason + L")";
            }
        }
        if (!data.mpPackageManifestHash.empty()) {
            text += L"\nMP manifest hash: " + data.mpPackageManifestHash;
        }
        if (!data.mpPackageHandoffStatus.empty()) {
            text += L"\nMP handoff: " + data.mpPackageHandoffStatus;
        }
        if (!data.mpPackageMismatchWarningState.empty()) {
            text += L"\nMP mismatch: " + data.mpPackageMismatchWarningState;
            if (!data.mpPackageMismatchWarningReason.empty()) {
                text += L" (" + data.mpPackageMismatchWarningReason + L")";
            }
        }
        if (!data.mpPackageIdentityMismatchAlert.empty()) {
            text += L"\nMP mismatch alert: " + data.mpPackageIdentityMismatchAlert;
        }
        if (!data.mpPackagePreviousHostAvailableKnown.empty()) {
            text += L"\nMP previous host known: " + data.mpPackagePreviousHostAvailableKnown;
        }
        if (!data.mpPackagePreviousHostAvailable.empty()) {
            text += L"\nMP previous host available: " + data.mpPackagePreviousHostAvailable;
        }
        if (!data.mpPackageHandoffRecoveryHint.empty()) {
            text += L"\nMP recovery: " + data.mpPackageHandoffRecoveryHint;
        }
        if (!data.mpPackageZipPath.empty() || !data.mpPackageManifestHash.empty()) {
            text += L"\nMP sdileni: zkopiruj MP zip a manifest hash; host/klient kroky jsou nize.";
        }
        if (!data.mpPackageHostReadiness.empty()) {
            text += L"\nMP host readiness: " + data.mpPackageHostReadiness;
        }
        if (!data.mpPackageClientReadinessGate.empty()) {
            text += L"\nMP client gate: " + data.mpPackageClientReadinessGate;
        }
        if (!data.mpPackageHostNextStep.empty()) {
            text += L"\nMP host krok: " + data.mpPackageHostNextStep;
        }
        if (!data.mpPackageClientNextStep.empty()) {
            text += L"\nMP client krok: " + data.mpPackageClientNextStep;
        }
        if (!data.mpPackageVerifyCommand.empty()) {
            text += L"\nMP verify: " + data.mpPackageVerifyCommand;
        }
        if (!data.mpPackageImportCommand.empty()) {
            text += L"\nMP import: " + data.mpPackageImportCommand;
        }
        if (!data.mpPackageStrictVerifyCommand.empty()) {
            text += L"\nMP strict verify: " + data.mpPackageStrictVerifyCommand;
        }
        if (!data.mpPackageStrictImportCommand.empty()) {
            text += L"\nMP strict import: " + data.mpPackageStrictImportCommand;
        }
    }
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
        g_statusHeaderFont = createUiFont(10, FW_NORMAL, L"Bahnschrift");
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
        SetWindowTextW(g_statusBottomTitle, L"Dal\u0161\u00ED krok");
    }
    if (g_statusDetails != nullptr) {
        const auto details = buildDashboardBottomText(data);
        SetWindowTextW(g_statusDetails, details.c_str());
    }

    const bool briefAvailable = !g_nextStepsBriefPath.empty() && std::filesystem::exists(g_nextStepsBriefPath);
    const auto startupAction = loadStartupShortcutActionStatus();
    const auto supportReportAction = loadSupportReportActionStatus();
    if (g_statusToggleStartupButton != nullptr) {
        const auto buttonLabel = strategic_nexus::buildSncTrayStartupShortcutToggleButtonLabel(startupAction);
        SetWindowTextW(g_statusToggleStartupButton, buttonLabel.c_str());
        EnableWindow(g_statusToggleStartupButton, startupAction.toggleAvailable ? TRUE : FALSE);
    }
    if (g_statusSupportReportButton != nullptr) {
        const auto buttonLabel =
            strategic_nexus::buildSncTraySupportReportActionButtonLabel(supportReportAction);
        SetWindowTextW(g_statusSupportReportButton, buttonLabel.c_str());
        EnableWindow(
            g_statusSupportReportButton,
            strategic_nexus::sncTraySupportReportActionAvailable(supportReportAction) ? TRUE : FALSE);
    }
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
    if (g_statusOpenMpPackageButton != nullptr) {
        EnableWindow(g_statusOpenMpPackageButton, g_mpOverlayPackageDirectory.empty() ? FALSE : TRUE);
    }
    if (g_statusPublishGeneratedOverlayButton != nullptr) {
        const bool publishReady =
            data.nextAction == L"review_staged_overlay_and_publish_if_desired" &&
            !data.overlayPublishCommand.empty();
        EnableWindow(g_statusPublishGeneratedOverlayButton, publishReady ? TRUE : FALSE);
    }
    if (g_statusOpenNextActionPathButton != nullptr) {
        EnableWindow(g_statusOpenNextActionPathButton, dashboardPathExists(data.nextActionPath) ? TRUE : FALSE);
    }
    if (g_statusExportMpPackageButton != nullptr) {
        EnableWindow(g_statusExportMpPackageButton, canRefreshMpPackageExport() ? TRUE : FALSE);
    }
    if (g_statusMpImportHandoffButton != nullptr) {
        EnableWindow(
            g_statusMpImportHandoffButton,
            (!g_mpOverlayPackageDirectory.empty() &&
             (!data.mpPackageStrictImportCommand.empty() || !data.mpPackageImportCommand.empty()))
                ? TRUE
                : FALSE);
    }
    if (g_statusCopyMpVerifyButton != nullptr) {
        EnableWindow(g_statusCopyMpVerifyButton, data.mpPackageVerifyCommand.empty() ? FALSE : TRUE);
    }
    if (g_statusCopyMpImportButton != nullptr) {
        EnableWindow(g_statusCopyMpImportButton, data.mpPackageImportCommand.empty() ? FALSE : TRUE);
    }
    if (g_statusCopyMpStrictVerifyButton != nullptr) {
        EnableWindow(g_statusCopyMpStrictVerifyButton, data.mpPackageStrictVerifyCommand.empty() ? FALSE : TRUE);
    }
    if (g_statusCopyMpStrictImportButton != nullptr) {
        EnableWindow(g_statusCopyMpStrictImportButton, data.mpPackageStrictImportCommand.empty() ? FALSE : TRUE);
    }
    if (g_statusCopyLlmPrepareButton != nullptr) {
        EnableWindow(g_statusCopyLlmPrepareButton, data.modelPrepareCommand.empty() ? FALSE : TRUE);
    }
    if (g_statusCloseButton != nullptr) {
        EnableWindow(g_statusCloseButton, TRUE);
    }
    if (g_statusMaximizeButton != nullptr) {
        EnableWindow(g_statusMaximizeButton, TRUE);
    }
    if (g_statusMinimizeButton != nullptr) {
        EnableWindow(g_statusMinimizeButton, TRUE);
    }
    updateStatusCaptionButtons(g_statusWindow);
    syncStatusCustomScrollbars(g_statusWindow, true);

    RedrawWindow(g_statusWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

void requestStatusWindowRefresh()
{
    if (g_statusWindow != nullptr && IsWindow(g_statusWindow)) {
        PostMessageW(g_statusWindow, WM_SNC_STATUS_REFRESH, 0, 0);
    }
}

bool canRefreshMpPackageExport()
{
    if (g_mpOverlayPackageDirectory.empty() ||
        g_dslDraftPath.empty() ||
        g_dslDraftAuditPath.empty() ||
        g_candidateDecisionPackagePath.empty() ||
        g_generatedOverlayStagingDirectory.empty() ||
        g_generatedOverlayStagingStatusPath.empty()) {
        return false;
    }

    std::error_code error;
    if (!std::filesystem::is_regular_file(g_dslDraftPath, error) || error) {
        return false;
    }
    error.clear();
    if (!std::filesystem::is_regular_file(g_dslDraftAuditPath, error) || error) {
        return false;
    }
    error.clear();
    if (!std::filesystem::is_regular_file(g_candidateDecisionPackagePath, error) || error) {
        return false;
    }

    // The staged overlay status may be missing; the backfiller can recreate it from
    // the validated DSL draft. The staging directory itself must still be usable.
    error.clear();
    return std::filesystem::exists(g_generatedOverlayStagingDirectory, error) && !error;
}

void requestMpPackageExportRefresh()
{
    if (!canRefreshMpPackageExport()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    g_mpPackageRefreshRequested.store(true);
    requestStatusWindowRefresh();
}

void requestMpPackageImportHandoff(HWND hwnd)
{
    const auto data = loadStatusDashboardData();
    const auto command = !data.mpPackageStrictImportCommand.empty()
        ? data.mpPackageStrictImportCommand
        : data.mpPackageImportCommand;
    if (command.empty() || g_mpOverlayPackageDirectory.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    if (!copyTextToClipboard(hwnd, command)) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    openMpOverlayPackageDirectory();
    MessageBoxW(
        hwnd,
        L"MP import prikaz je zkopirovany do schranky.\n\n"
        L"V prikazu nahraď <target_overlay_dir> cilovou generated-overlay slozkou klienta nebo dalsiho hosta. "
        L"Pak spust strict verify/import pred pripojenim do MP hry.",
        L"Strategic Nexus Companion",
        MB_OK | MB_ICONINFORMATION);
}

void applySncAppUserModelId()
{
    using SetCurrentProcessExplicitAppUserModelIdFn = HRESULT(WINAPI*)(PCWSTR);

    const HMODULE shell32 = GetModuleHandleW(L"shell32.dll");
    if (shell32 == nullptr) {
        return;
    }

    const auto setAppUserModelId = reinterpret_cast<SetCurrentProcessExplicitAppUserModelIdFn>(
        GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID"));
    if (setAppUserModelId == nullptr) {
        return;
    }

    setAppUserModelId(kSncAppUserModelId);
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
    invalidateStatusCustomScrollbarAreas(hwnd);

    RECT client{};
    GetClientRect(hwnd, &client);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = 18;
    const int top = 8;
    const int headerHeight = 22;
    const int subtitleHeight = 20;
    const int buttonHeight = 29;
    const int buttonGap = 8;
    const int titleButtonsWidth = kStatusTitleButtonWidth * 3;
    const int availableWidth = width - (margin * 2);
    constexpr int actionButtonCount = 16;
    constexpr int preferredButtonWidth = 116;
    constexpr int minimumButtonWidth = 76;
    int buttonRows = 1;
    int buttonsPerRow = actionButtonCount;
    int buttonWidth = preferredButtonWidth;
    const int singleRowRequiredWidth =
        (preferredButtonWidth * actionButtonCount) + (buttonGap * (actionButtonCount - 1));
    if (singleRowRequiredWidth > availableWidth) {
        buttonRows = 2;
        buttonsPerRow = (actionButtonCount + 1) / 2;
        buttonWidth = (availableWidth - (buttonGap * (buttonsPerRow - 1))) / buttonsPerRow;
        if (buttonWidth < minimumButtonWidth) {
            buttonWidth = minimumButtonWidth;
        }
    }

    const int headerWidth = availableWidth - titleButtonsWidth - 12;
    const int subtitleTop = top + kStatusTitleBarHeight + 12;
    const int buttonTop = subtitleTop + subtitleHeight + 10;
    const int actionRowsHeight = (buttonHeight * buttonRows) + (buttonGap * (buttonRows - 1));
    const int gridTop = buttonTop + actionRowsHeight + 14;

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
    const int scrollbarReserveWidth = kStatusScrollbarTrackWidth + 4;

    if (g_statusHeader != nullptr) {
        MoveWindow(g_statusHeader, margin, top, headerWidth, headerHeight, TRUE);
    }
    if (g_statusSubtitle != nullptr) {
        MoveWindow(g_statusSubtitle, margin, subtitleTop, headerWidth, subtitleHeight, TRUE);
    }

    const int titleButtonTop = top - 2;
    const int closeLeft = width - margin - kStatusTitleButtonWidth;
    const int maximizeLeft = closeLeft - kStatusTitleButtonWidth;
    const int minimizeLeft = maximizeLeft - kStatusTitleButtonWidth;
    updateStatusCaptionButtons(hwnd);
    if (g_statusCloseButton != nullptr) {
        MoveWindow(g_statusCloseButton, closeLeft, titleButtonTop, kStatusTitleButtonWidth, 24, TRUE);
    }
    if (g_statusMaximizeButton != nullptr) {
        MoveWindow(g_statusMaximizeButton, maximizeLeft, titleButtonTop, kStatusTitleButtonWidth, 24, TRUE);
    }
    if (g_statusMinimizeButton != nullptr) {
        MoveWindow(g_statusMinimizeButton, minimizeLeft, titleButtonTop, kStatusTitleButtonWidth, 24, TRUE);
    }

    const auto startupAction = loadStartupShortcutActionStatus();
    const auto startupButtonLabel = strategic_nexus::buildSncTrayStartupShortcutToggleButtonLabel(startupAction);
    const auto supportReportAction = loadSupportReportActionStatus();
    const auto supportReportButtonLabel =
        strategic_nexus::buildSncTraySupportReportActionButtonLabel(supportReportAction);
    const HWND buttons[] = {
        g_statusRefreshButton,
        g_statusCopyButton,
        g_statusOpenArchiveButton,
        g_statusOpenBriefButton,
        g_statusOpenMpPackageButton,
        g_statusPublishGeneratedOverlayButton,
        g_statusOpenNextActionPathButton,
        g_statusExportMpPackageButton,
        g_statusMpImportHandoffButton,
        g_statusCopyMpVerifyButton,
        g_statusCopyMpImportButton,
        g_statusCopyMpStrictVerifyButton,
        g_statusCopyMpStrictImportButton,
        g_statusCopyLlmPrepareButton,
        g_statusSupportReportButton,
        g_statusToggleStartupButton
    };
    const std::wstring buttonTexts[] = {
        L"Obnovit",
        L"Kop\u00EDrovat",
        L"Archiv",
        L"Souhrn",
        L"MP balicek",
        L"Publikovat",
        L"Akce cesta",
        L"MP export",
        L"MP import navod",
        L"MP verify",
        L"MP import",
        L"MP strict verify",
        L"MP strict import",
        L"LLM p\u0159\u00EDprava",
        supportReportButtonLabel,
        startupButtonLabel
    };
    for (std::size_t index = 0; index < std::size(buttons); ++index) {
        if (buttons[index] != nullptr) {
            SetWindowTextW(buttons[index], buttonTexts[index].c_str());
            const int row = static_cast<int>(index) / buttonsPerRow;
            const int column = static_cast<int>(index) % buttonsPerRow;
            MoveWindow(
                buttons[index],
                margin + column * (buttonWidth + buttonGap),
                buttonTop + row * (buttonHeight + buttonGap),
                buttonWidth,
                buttonHeight,
                TRUE);
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
        int totalLineUnits = 0;
        for (const auto field : kStatusSections[sectionIndex].fields) {
            totalLineUnits += statusFieldLineUnits(field);
        }
        int unitHeight =
            (cardHeight - titleOffset - (fieldGap * static_cast<int>(kStatusSections[sectionIndex].fields.size() - 1))) /
            (totalLineUnits > 0 ? totalLineUnits : 1);
        if (unitHeight < 14) {
            unitHeight = 14;
        }

        int currentFieldTop = fieldBaseTop;
        for (std::size_t fieldIndex = 0; fieldIndex < kStatusSections[sectionIndex].fields.size(); ++fieldIndex) {
            const auto fieldId = kStatusSections[sectionIndex].fields[fieldIndex];
            const std::size_t fieldSlot = static_cast<std::size_t>(fieldId);
            const int fieldHeight = unitHeight * statusFieldLineUnits(fieldId);
            if (g_statusFieldLabels[fieldSlot] != nullptr) {
                MoveWindow(
                    g_statusFieldLabels[fieldSlot],
                    cardLeft,
                    currentFieldTop + 4,
                    labelWidth - 4,
                    18,
                    TRUE);
            }
            if (g_statusFieldValues[fieldSlot] != nullptr) {
                int controlWidth = valueWidth;
                if (statusFieldUsesMultiline(fieldId) && valueWidth > (scrollbarReserveWidth + 60)) {
                    controlWidth -= scrollbarReserveWidth;
                }
                MoveWindow(
                    g_statusFieldValues[fieldSlot],
                    cardLeft + labelWidth,
                    currentFieldTop,
                    controlWidth,
                    fieldHeight + 4,
                    TRUE);
            }
            if (statusFieldUsesMultiline(fieldId) && valueWidth > (scrollbarReserveWidth + 60)) {
                const RECT trackRect = {
                    cardLeft + labelWidth + valueWidth - kStatusScrollbarTrackWidth,
                    currentFieldTop,
                    cardLeft + labelWidth + valueWidth,
                    currentFieldTop + fieldHeight + 4
                };
                g_statusFieldScrollbars[fieldSlot] =
                    composeStatusScrollbar(trackRect, measureStatusScrollMetrics(g_statusFieldValues[fieldSlot]), true);
            } else {
                g_statusFieldScrollbars[fieldSlot] = {};
            }
            currentFieldTop += fieldHeight + fieldGap;
        }
    }

    if (g_statusBottomTitle != nullptr) {
        MoveWindow(g_statusBottomTitle, margin, bottomTop, availableWidth, 20, TRUE);
    }
    if (g_statusDetails != nullptr) {
        const int detailsTop = bottomTop + 24;
        const int detailsHeight = height - detailsTop - margin;
        int detailsWidth = availableWidth;
        if (availableWidth > (scrollbarReserveWidth + 80)) {
            detailsWidth -= scrollbarReserveWidth;
        }
        MoveWindow(g_statusDetails, margin, detailsTop, detailsWidth, detailsHeight, TRUE);
        const RECT detailsTrackRect = {
            margin + availableWidth - kStatusScrollbarTrackWidth,
            detailsTop,
            margin + availableWidth,
            detailsTop + detailsHeight
        };
        g_statusDetailsScrollbar =
            composeStatusScrollbar(detailsTrackRect, measureStatusScrollMetrics(g_statusDetails), true);
    }

    syncStatusCustomScrollbars(hwnd, true);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
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
        g_statusCopyButton = createStatusButton(hwnd, ID_STATUS_COPY, L"Kop\u00EDrovat");
        g_statusOpenArchiveButton = createStatusButton(hwnd, ID_STATUS_OPEN_ARCHIVE, L"Archiv");
        g_statusOpenBriefButton = createStatusButton(hwnd, ID_STATUS_OPEN_BRIEF, L"Souhrn");
        g_statusOpenMpPackageButton = createStatusButton(hwnd, ID_STATUS_OPEN_MP_PACKAGE, L"MP bal\u00ED\u010Dek");
        g_statusPublishGeneratedOverlayButton =
            createStatusButton(hwnd, ID_STATUS_PUBLISH_GENERATED_OVERLAY, L"Publikovat");
        g_statusOpenNextActionPathButton = createStatusButton(hwnd, ID_STATUS_OPEN_NEXT_ACTION_PATH, L"Akce cesta");
        g_statusExportMpPackageButton = createStatusButton(hwnd, ID_STATUS_EXPORT_MP_PACKAGE, L"MP export");
        g_statusMpImportHandoffButton = createStatusButton(hwnd, ID_STATUS_MP_IMPORT_HANDOFF, L"MP import n\u00E1vod");
        g_statusCopyMpVerifyButton = createStatusButton(hwnd, ID_STATUS_COPY_MP_VERIFY, L"MP verify");
        g_statusCopyMpImportButton = createStatusButton(hwnd, ID_STATUS_COPY_MP_IMPORT, L"MP import");
        g_statusCopyMpStrictVerifyButton = createStatusButton(hwnd, ID_STATUS_COPY_MP_STRICT_VERIFY, L"MP strict verify");
        g_statusCopyMpStrictImportButton = createStatusButton(hwnd, ID_STATUS_COPY_MP_STRICT_IMPORT, L"MP strict import");
        g_statusCopyLlmPrepareButton = createStatusButton(hwnd, ID_STATUS_COPY_LLM_PREPARE, L"LLM p\u0159\u00EDprava");
        g_statusSupportReportButton = createStatusButton(hwnd, ID_STATUS_SUPPORT_REPORT, L"Report");
        g_statusToggleStartupButton = createStatusButton(hwnd, ID_STATUS_TOGGLE_STARTUP, L"Start");
        g_statusCloseButton = createStatusButton(hwnd, ID_STATUS_CLOSE, L"X");
        g_statusMaximizeButton = createStatusButton(hwnd, ID_STATUS_MAXIMIZE, L"[ ]");
        g_statusMinimizeButton = createStatusButton(hwnd, ID_STATUS_MINIMIZE, L"-");
        g_statusBottomTitle = createStatusStatic(hwnd, L"Dal\u0161\u00ED krok", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
        g_statusDetails = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
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
                g_statusFieldValues[slot] = createStatusValue(hwnd, field, kStatusEmptyValue);
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
        setWindowFont(g_statusOpenMpPackageButton, g_statusFieldFont);
        setWindowFont(g_statusPublishGeneratedOverlayButton, g_statusFieldFont);
        setWindowFont(g_statusOpenNextActionPathButton, g_statusFieldFont);
        setWindowFont(g_statusExportMpPackageButton, g_statusFieldFont);
        setWindowFont(g_statusMpImportHandoffButton, g_statusFieldFont);
        setWindowFont(g_statusCopyMpVerifyButton, g_statusFieldFont);
        setWindowFont(g_statusCopyMpImportButton, g_statusFieldFont);
        setWindowFont(g_statusCopyMpStrictVerifyButton, g_statusFieldFont);
        setWindowFont(g_statusCopyMpStrictImportButton, g_statusFieldFont);
        setWindowFont(g_statusCopyLlmPrepareButton, g_statusFieldFont);
        setWindowFont(g_statusSupportReportButton, g_statusFieldFont);
        setWindowFont(g_statusToggleStartupButton, g_statusFieldFont);
        setWindowFont(g_statusCloseButton, g_statusFieldFont);
        setWindowFont(g_statusMaximizeButton, g_statusFieldFont);
        setWindowFont(g_statusMinimizeButton, g_statusFieldFont);
        setWindowFont(g_statusBottomTitle, g_statusSectionFont);
        setWindowFont(g_statusDetails, g_statusDetailsFont);

        applyWindowIcons(hwnd);
        SetTimer(hwnd, kStatusScrollSyncTimerId, 120, nullptr);
        layoutStatusWindow(hwnd);
        refreshStatusWindowContent();
        return 0;
    }
    case WM_SIZE:
        layoutStatusWindow(hwnd);
        updateStatusCaptionButtons(hwnd);
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
        case ID_STATUS_OPEN_MP_PACKAGE:
            openMpOverlayPackageDirectory();
            return 0;
        case ID_STATUS_PUBLISH_GENERATED_OVERLAY:
            publishStagedGeneratedOverlay(hwnd);
            return 0;
        case ID_STATUS_OPEN_NEXT_ACTION_PATH:
            openNextActionPath(hwnd);
            return 0;
        case ID_STATUS_EXPORT_MP_PACKAGE:
            requestMpPackageExportRefresh();
            return 0;
        case ID_STATUS_MP_IMPORT_HANDOFF:
            requestMpPackageImportHandoff(hwnd);
            return 0;
        case ID_STATUS_COPY_MP_VERIFY:
        {
            const auto data = loadStatusDashboardData();
            if (data.mpPackageVerifyCommand.empty() || !copyTextToClipboard(hwnd, data.mpPackageVerifyCommand)) {
                MessageBeep(MB_ICONWARNING);
            }
            return 0;
        }
        case ID_STATUS_COPY_MP_IMPORT:
        {
            const auto data = loadStatusDashboardData();
            if (data.mpPackageImportCommand.empty() || !copyTextToClipboard(hwnd, data.mpPackageImportCommand)) {
                MessageBeep(MB_ICONWARNING);
            }
            return 0;
        }
        case ID_STATUS_COPY_MP_STRICT_VERIFY:
        {
            const auto data = loadStatusDashboardData();
            if (data.mpPackageStrictVerifyCommand.empty() ||
                !copyTextToClipboard(hwnd, data.mpPackageStrictVerifyCommand)) {
                MessageBeep(MB_ICONWARNING);
            }
            return 0;
        }
        case ID_STATUS_COPY_MP_STRICT_IMPORT:
        {
            const auto data = loadStatusDashboardData();
            if (data.mpPackageStrictImportCommand.empty() ||
                !copyTextToClipboard(hwnd, data.mpPackageStrictImportCommand)) {
                MessageBeep(MB_ICONWARNING);
            }
            return 0;
        }
        case ID_STATUS_COPY_LLM_PREPARE:
        {
            const auto data = loadStatusDashboardData();
            if (data.modelPrepareCommand.empty() || !copyTextToClipboard(hwnd, data.modelPrepareCommand)) {
                MessageBeep(MB_ICONWARNING);
            }
            return 0;
        }
        case ID_STATUS_SUPPORT_REPORT:
            prepareOrOpenSupportReport(hwnd);
            return 0;
        case ID_STATUS_TOGGLE_STARTUP:
            toggleStartWithWindows(hwnd);
            return 0;
        case ID_STATUS_MAXIMIZE:
            ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
            updateStatusCaptionButtons(hwnd);
            return 0;
        case ID_STATUS_MINIMIZE:
            ShowWindow(hwnd, SW_MINIMIZE);
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
    case WM_TIMER:
        if (wParam == kStatusScrollSyncTimerId) {
            syncStatusCustomScrollbars(hwnd);
            return 0;
        }
        break;
    case WM_SNC_STATUS_REFRESH:
        refreshStatusWindowContent();
        return 0;
    case WM_NCCALCSIZE:
        return 0;
    case WM_NCACTIVATE:
        return TRUE;
    case WM_NCHITTEST: {
        if (IsZoomed(hwnd)) {
            return HTCLIENT;
        }

        POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT windowRect{};
        GetWindowRect(hwnd, &windowRect);

        const int x = point.x - windowRect.left;
        const int y = point.y - windowRect.top;
        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;

        const bool left = x < kStatusResizeBorder;
        const bool right = x >= width - kStatusResizeBorder;
        const bool top = y < kStatusResizeBorder;
        const bool bottom = y >= height - kStatusResizeBorder;

        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;

        RECT closeRect{};
        RECT maxRect{};
        RECT minRect{};
        if ((g_statusCloseButton != nullptr && GetWindowRect(g_statusCloseButton, &closeRect) && PtInRect(&closeRect, point)) ||
            (g_statusMaximizeButton != nullptr && GetWindowRect(g_statusMaximizeButton, &maxRect) && PtInRect(&maxRect, point)) ||
            (g_statusMinimizeButton != nullptr && GetWindowRect(g_statusMinimizeButton, &minRect) && PtInRect(&minRect, point))) {
            return HTCLIENT;
        }

        if (y < kStatusTitleBarHeight) {
            return HTCAPTION;
        }

        return HTCLIENT;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 900;
        info->ptMinTrackSize.y = 640;

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (monitor != nullptr) {
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            if (GetMonitorInfoW(monitor, &monitorInfo)) {
                info->ptMaxPosition.x = monitorInfo.rcWork.left - monitorInfo.rcMonitor.left;
                info->ptMaxPosition.y = monitorInfo.rcWork.top - monitorInfo.rcMonitor.top;
                info->ptMaxSize.x = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
                info->ptMaxSize.y = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
            }
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        const HDC hdc = reinterpret_cast<HDC>(wParam);
        const HWND control = reinterpret_cast<HWND>(lParam);
        COLORREF color = kStatusMutedColor;
        bool accent = false;

        if (control == g_statusHeader) {
            color = kStatusTextColor;
        } else if (control == g_statusBottomTitle) {
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

        RECT titleBand = client;
        titleBand.bottom = titleBand.top + kStatusTitleBarHeight;
        FillRect(hdc, &titleBand, g_statusBackgroundBrush);

        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const HDC hdc = BeginPaint(hwnd, &paint);
        paintStatusCustomScrollbars(hdc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        StatusScrollTargetKind kind = StatusScrollTargetKind::None;
        std::size_t fieldIndex = 0;
        if (statusPointHitsCustomScrollbar(point, &kind, &fieldIndex)) {
            auto* scrollbar = statusScrollTargetScrollbar(kind, fieldIndex);
            if (scrollbar != nullptr && !IsRectEmpty(&scrollbar->thumbRect) && PtInRect(&scrollbar->thumbRect, point)) {
                g_statusScrollDragTargetKind = kind;
                g_statusScrollDragFieldIndex = fieldIndex;
                g_statusScrollDragOffsetY = point.y - scrollbar->thumbRect.top;
                SetCapture(hwnd);
            } else {
                const bool forward = scrollbar == nullptr || point.y >= scrollbar->thumbRect.bottom;
                scrollStatusTargetByPage(kind, fieldIndex, forward);
                syncStatusCustomScrollbars(hwnd);
            }
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE:
        if (g_statusScrollDragTargetKind != StatusScrollTargetKind::None && GetCapture() == hwnd) {
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            dragStatusScrollbarThumb(hwnd, point);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (g_statusScrollDragTargetKind != StatusScrollTargetKind::None && GetCapture() == hwnd) {
            ReleaseCapture();
            clearStatusScrollDrag();
            syncStatusCustomScrollbars(hwnd, true);
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        clearStatusScrollDrag();
        syncStatusCustomScrollbars(hwnd, true);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kStatusScrollSyncTimerId);
        clearStatusScrollDrag();
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
        g_statusOpenMpPackageButton = nullptr;
        g_statusPublishGeneratedOverlayButton = nullptr;
        g_statusOpenNextActionPathButton = nullptr;
        g_statusExportMpPackageButton = nullptr;
        g_statusMpImportHandoffButton = nullptr;
        g_statusCopyMpVerifyButton = nullptr;
        g_statusCopyMpImportButton = nullptr;
        g_statusCopyMpStrictVerifyButton = nullptr;
        g_statusCopyMpStrictImportButton = nullptr;
        g_statusCopyLlmPrepareButton = nullptr;
        g_statusSupportReportButton = nullptr;
        g_statusToggleStartupButton = nullptr;
        g_statusCloseButton = nullptr;
        g_statusMaximizeButton = nullptr;
        g_statusMinimizeButton = nullptr;
        for (auto& label : g_statusFieldLabels) {
            label = nullptr;
        }
        for (auto& value : g_statusFieldValues) {
            value = nullptr;
        }
        for (auto& scrollbar : g_statusFieldScrollbars) {
            scrollbar = {};
        }
        g_statusDetailsScrollbar = {};
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
    output << "human_control_guard_state: " << mpOverlayPackage.humanControlGuardState << "\n";
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

std::string buildStartupShortcutStateBrief(const std::string& shortcutState)
{
    if (shortcutState == "shortcut_installed" || shortcutState == "override_enabled") {
        return "startup shortcut installed";
    }
    if (shortcutState == "shortcut_not_installed" || shortcutState == "override_disabled") {
        return "startup shortcut not installed";
    }
    if (shortcutState == "shortcut_path_unavailable") {
        return "startup shortcut path unavailable";
    }
    if (shortcutState == "shortcut_path_not_file") {
        return "startup shortcut path is not a file";
    }
    if (shortcutState == "shortcut_probe_failed") {
        return "startup shortcut probe failed";
    }
    return shortcutState.empty() ? "startup shortcut status unknown" : shortcutState;
}

std::filesystem::path buildDefaultStartupShortcutPath()
{
    std::array<wchar_t, MAX_PATH> startupBuffer{};
    const HRESULT result =
        SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, SHGFP_TYPE_CURRENT, startupBuffer.data());
    if (FAILED(result) || startupBuffer[0] == L'\0') {
        return {};
    }

    std::filesystem::path startupPath(startupBuffer.data());
    if (startupPath.empty()) {
        return {};
    }

    return startupPath / "Strategic Nexus Companion.lnk";
}

strategic_nexus::SncTrayStartupShortcutActionStatus loadStartupShortcutActionStatus()
{
    strategic_nexus::SncTrayStartupShortcutActionStatus status;

    std::string json;
    if (!g_trayStatusPath.empty() && strategic_nexus::common::tryReadTextFile(g_trayStatusPath, json)) {
        status = strategic_nexus::parseSncTrayStartupShortcutActionStatus(json);
        if (status.toggleAvailable || status.stateKnown || !status.shortcutPath.empty()) {
            return status;
        }
    }

    status.stateKnown = true;
    status.enableCommandHint = R"(cmd /c tools\install_snc_tray_startup_shortcut.cmd)";
    status.disableCommandHint = R"(cmd /c tools\remove_snc_tray_startup_shortcut.cmd)";
    status.shortcutPath = buildDefaultStartupShortcutPath();
    if (status.shortcutPath.empty()) {
        status.lifecycleState = "manual_start_only";
        status.shortcutState = "shortcut_path_unavailable";
        status.toggleCommandHintSource = "startup_shortcut_unavailable";
        status.toggleAvailable = false;
        return status;
    }

    std::error_code error;
    const bool shortcutExists = std::filesystem::exists(status.shortcutPath, error);
    if (error) {
        status.startWithWindowsEnabled = false;
        status.lifecycleState = "manual_start_only";
        status.shortcutState = "shortcut_probe_failed";
        status.toggleCommandHint = status.enableCommandHint;
        status.toggleCommandHintSource = "startup_shortcut_install_command";
        status.toggleAvailable = !status.toggleCommandHint.empty();
        return status;
    }

    if (!shortcutExists) {
        status.startWithWindowsEnabled = false;
        status.lifecycleState = "manual_start_only";
        status.shortcutState = "shortcut_not_installed";
        status.toggleCommandHint = status.enableCommandHint;
        status.toggleCommandHintSource = "startup_shortcut_install_command";
        status.toggleAvailable = !status.toggleCommandHint.empty();
        return status;
    }

    error.clear();
    const bool shortcutIsFile = std::filesystem::is_regular_file(status.shortcutPath, error);
    if (error || !shortcutIsFile) {
        status.startWithWindowsEnabled = false;
        status.lifecycleState = "manual_start_only";
        status.shortcutState = "shortcut_path_not_file";
        status.toggleCommandHint = status.enableCommandHint;
        status.toggleCommandHintSource = "startup_shortcut_install_command";
        status.toggleAvailable = !status.toggleCommandHint.empty();
        return status;
    }

    status.startWithWindowsEnabled = true;
    status.lifecycleState = "owner_enabled_start_with_windows";
    status.shortcutState = "shortcut_installed";
    status.toggleCommandHint = status.disableCommandHint;
    status.toggleCommandHintSource = "startup_shortcut_remove_command";
    status.toggleAvailable = !status.toggleCommandHint.empty();
    return status;
}

std::wstring formatWindowsErrorMessage(const DWORD errorCode)
{
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length =
        FormatMessageW(flags, nullptr, errorCode, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr) {
        return L"Windows error " + std::to_wstring(errorCode);
    }

    std::wstring message(buffer, length);
    LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
        message.pop_back();
    }
    return message;
}

RepoCommandExecutionResult runRepoCommandHidden(const std::wstring& commandLine)
{
    RepoCommandExecutionResult result;
    if (commandLine.empty()) {
        result.errorMessage = L"Command line is empty.";
        return result;
    }

    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    std::wstring workingDirectory = g_repoRoot.empty() ? findRepoRoot().wstring() : g_repoRoot.wstring();
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION processInfo{};

    if (!CreateProcessW(
            nullptr,
            mutableCommandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDirectory.c_str(),
            &startupInfo,
            &processInfo)) {
        result.errorMessage = formatWindowsErrorMessage(GetLastError());
        return result;
    }

    result.launched = true;
    CloseHandle(processInfo.hThread);

    const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 30000);
    if (waitResult == WAIT_OBJECT_0) {
        result.completed = true;
        if (!GetExitCodeProcess(processInfo.hProcess, &result.exitCode)) {
            result.exitCode = ERROR_GEN_FAILURE;
            result.errorMessage = formatWindowsErrorMessage(GetLastError());
        }
    } else if (waitResult == WAIT_TIMEOUT) {
        result.errorMessage = L"Command timed out after 30 seconds.";
    } else {
        result.errorMessage = formatWindowsErrorMessage(GetLastError());
    }

    CloseHandle(processInfo.hProcess);
    return result;
}

bool waitForStartupShortcutState(
    const bool expectedEnabled,
    strategic_nexus::SncTrayStartupShortcutActionStatus& refreshedState)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        refreshedState = loadStartupShortcutActionStatus();
        if (refreshedState.stateKnown && refreshedState.startWithWindowsEnabled == expectedEnabled) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    refreshedState = loadStartupShortcutActionStatus();
    return refreshedState.stateKnown && refreshedState.startWithWindowsEnabled == expectedEnabled;
}

void toggleStartWithWindows(HWND hwnd)
{
    const auto startupAction = loadStartupShortcutActionStatus();
    if (!startupAction.toggleAvailable || startupAction.toggleCommandHint.empty()) {
        std::wstring message =
            L"Start s Windows neni v tomhle behu SNC dostupny. Zkontroluj startup shortcut path a zkus obnovit stav.";
        if (!startupAction.shortcutPath.empty()) {
            message += L"\n\nShortcut:\n";
            message += startupAction.shortcutPath.wstring();
        }

        MessageBoxW(
            hwnd,
            message.c_str(),
            L"Strategic Nexus Companion",
            MB_OK | MB_ICONWARNING);
        return;
    }

    const bool enable = strategic_nexus::sncTrayStartupShortcutToggleTargetsEnable(startupAction);
    std::wstring confirmationText = enable
        ? L"Zapnout SNC pri startu Windows?\n\nSNC vytvori startup shortcut do Windows Startup folderu."
        : L"Vypnout SNC pri startu Windows?\n\nSNC odstrani startup shortcut z Windows Startup folderu.";
    if (!startupAction.shortcutPath.empty()) {
        confirmationText += L"\n\nShortcut:\n";
        confirmationText += startupAction.shortcutPath.wstring();
    }
    confirmationText += L"\n\nPrikaz:\n";
    confirmationText += utf8ToWide(startupAction.toggleCommandHint);

    const int confirmation = MessageBoxW(
        hwnd,
        confirmationText.c_str(),
        L"Strategic Nexus Companion",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (confirmation != IDYES) {
        return;
    }

    const std::wstring pendingText =
        enable ? L"zapinam start s Windows" : L"vypinam start s Windows";
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_statusText = pendingText;
    }
    updateTrayTip(hwnd, pendingText);
    requestStatusWindowRefresh();

    const auto commandResult = runRepoCommandHidden(utf8ToWide(startupAction.toggleCommandHint));
    strategic_nexus::SncTrayStartupShortcutActionStatus refreshedState;
    const bool stateChanged = commandResult.launched && commandResult.completed && commandResult.exitCode == 0 &&
        waitForStartupShortcutState(enable, refreshedState);

    std::wstring statusText;
    std::wstring messageText;
    UINT messageFlags = MB_OK | MB_ICONWARNING;
    if (!commandResult.launched) {
        statusText = L"Zmena startu s Windows selhala.";
        messageText = L"Nepodarilo se spustit startup prikaz.\n\n";
        messageText += commandResult.errorMessage.empty() ? L"Neznamy problem." : commandResult.errorMessage;
    } else if (!commandResult.completed) {
        statusText = L"Startup prikaz stale bezi nebo vyprsel.";
        messageText = L"Startup prikaz se nedokoncil vcas.\n\n";
        messageText += commandResult.errorMessage.empty() ? L"Zkus obnovit stav za chvili." : commandResult.errorMessage;
    } else if (commandResult.exitCode != 0) {
        statusText = L"Zmena startu s Windows selhala.";
        messageText = L"Startup prikaz skoncil chybou.\n\nExit code: ";
        messageText += std::to_wstring(commandResult.exitCode);
    } else if (stateChanged) {
        statusText = enable ? L"Start s Windows zapnut." : L"Start s Windows vypnut.";
        messageText = statusText;
        messageFlags = MB_OK | MB_ICONINFORMATION;
    } else {
        statusText = L"Startup prikaz probehl, stav se jeste obnovuje.";
        messageText = L"Startup prikaz probehl, ale SNC zatim nevidi novy lifecycle stav.\n\nZkus otevrit Stav nebo pockat par sekund.";
    }

    if (!refreshedState.shortcutPath.empty()) {
        messageText += L"\n\nShortcut:\n";
        messageText += refreshedState.shortcutPath.wstring();
    } else if (!startupAction.shortcutPath.empty()) {
        messageText += L"\n\nShortcut:\n";
        messageText += startupAction.shortcutPath.wstring();
    }

    if (stateChanged || refreshedState.stateKnown) {
        messageText += L"\n\nLifecycle: ";
        messageText += utf8ToWide(refreshedState.lifecycleState);
        messageText += L"\nShortcut state: ";
        messageText += utf8ToWide(buildStartupShortcutStateBrief(refreshedState.shortcutState));
    }

    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_statusText = statusText;
    }
    updateTrayTip(hwnd, statusText);
    requestStatusWindowRefresh();

    MessageBoxW(hwnd, messageText.c_str(), L"Strategic Nexus Companion", messageFlags);
}

bool supportReportPreviewExists()
{
    if (g_supportReportPreviewPath.empty()) {
        return false;
    }

    std::error_code error;
    return std::filesystem::exists(g_supportReportPreviewPath, error) &&
        std::filesystem::is_regular_file(g_supportReportPreviewPath, error);
}

strategic_nexus::SncTraySupportReportActionStatus loadSupportReportActionStatus()
{
    strategic_nexus::SncTraySupportReportActionStatus status;
    status.previewPath = g_supportReportPreviewPath;
    status.previewPathConfigured = !g_supportReportPreviewPath.empty();
    status.previewReady = supportReportPreviewExists();
    return status;
}

std::wstring buildPrepareSupportReportCommandLine()
{
    if (g_supportReportPreviewPath.empty() || g_trayStatusPath.empty()) {
        return std::wstring();
    }

    std::wstring command = L"powershell -NoProfile -ExecutionPolicy Bypass -File ";
    command += L"\"tools\\prepare_snc_support_report.ps1\"";
    command += L" -StatusPath ";
    command += L"\"" + g_trayStatusPath.wstring() + L"\"";
    command += L" -OutputPath ";
    command += L"\"" + g_supportReportPreviewPath.wstring() + L"\"";
    return command;
}

bool waitForSupportReportPreviewReady()
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (supportReportPreviewExists()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return supportReportPreviewExists();
}

std::wstring buildSupportReportMenuLabel()
{
    return strategic_nexus::buildSncTraySupportReportActionMenuLabel(loadSupportReportActionStatus());
}

void prepareOrOpenSupportReport(HWND hwnd)
{
    if (g_supportReportPreviewPath.empty()) {
        MessageBoxW(
            hwnd,
            L"SNC nema nastavenou cestu pro local support report preview.",
            L"Strategic Nexus Companion",
            MB_OK | MB_ICONWARNING);
        return;
    }

    if (supportReportPreviewExists()) {
        openPathWithShell(hwnd, g_supportReportPreviewPath);
        return;
    }

    const auto commandLine = buildPrepareSupportReportCommandLine();
    if (commandLine.empty()) {
        MessageBoxW(
            hwnd,
            L"SNC nema pripraveny support report command.",
            L"Strategic Nexus Companion",
            MB_OK | MB_ICONWARNING);
        return;
    }

    const auto commandResult = runRepoCommandHidden(commandLine);
    if (!commandResult.launched || !commandResult.completed || commandResult.exitCode != 0) {
        std::wstring message = L"Priprava support reportu selhala.";
        if (!commandResult.errorMessage.empty()) {
            message += L"\n\n";
            message += commandResult.errorMessage;
        } else if (commandResult.completed) {
            message += L"\n\nExit code: ";
            message += std::to_wstring(commandResult.exitCode);
        }

        MessageBoxW(
            hwnd,
            message.c_str(),
            L"Strategic Nexus Companion",
            MB_OK | MB_ICONWARNING);
        return;
    }

    if (!waitForSupportReportPreviewReady()) {
        MessageBoxW(
            hwnd,
            L"Support report command probehl, ale preview soubor se jeste neukazal. Zkus obnovit stav za chvili.",
            L"Strategic Nexus Companion",
            MB_OK | MB_ICONWARNING);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        g_statusText = L"Support report pripraven.";
    }
    updateTrayTip(hwnd, L"Support report pripraven");
    requestStatusWindowRefresh();

    std::wstring info =
        L"Local support report preview je pripraven.\n\nBez tveho approvalu se nic neposila.\n\nPreview:\n";
    info += g_supportReportPreviewPath.wstring();
    MessageBoxW(
        hwnd,
        info.c_str(),
        L"Strategic Nexus Companion",
        MB_OK | MB_ICONINFORMATION);
    openPathWithShell(hwnd, g_supportReportPreviewPath);
}

void appendStartupRationaleLines(
    std::ostringstream& output,
    const strategic_nexus::CompanionLifecycleStatus& lifecycle)
{
    output << "startup_rationale: start SNC before Stellaris to preserve more autosave history before the game rotates older saves away\n";
    output << "startup_start_with_windows: optional_owner_setting_default_disabled\n";
    output << "startup_lifecycle_state: " << buildStartupLifecycleState(lifecycle.startWithWindowsEnabled) << "\n";
    output << "startup_start_with_windows_enabled: " << (lifecycle.startWithWindowsEnabled ? "true" : "false") << "\n";
    output << "startup_start_with_windows_source: " << lifecycle.startWithWindowsSource << "\n";
    output << "startup_start_with_windows_shortcut_state: "
           << lifecycle.startWithWindowsShortcutState << "\n";
    if (!lifecycle.startWithWindowsShortcutPath.empty()) {
        output << "startup_start_with_windows_shortcut_path: "
               << pathString(lifecycle.startWithWindowsShortcutPath) << "\n";
    }
    output << "startup_start_with_windows_command_hint_source: "
           << lifecycle.startWithWindowsCommandHintSource << "\n";
    if (!lifecycle.startWithWindowsCommandHint.empty()) {
        output << "startup_start_with_windows_command_hint: "
               << lifecycle.startWithWindowsCommandHint << "\n";
    }
    if (!lifecycle.startWithWindowsEnableCommandHint.empty()) {
        output << "startup_start_with_windows_enable_command_hint: "
               << lifecycle.startWithWindowsEnableCommandHint << "\n";
    }
    if (!lifecycle.startWithWindowsDisableCommandHint.empty()) {
        output << "startup_start_with_windows_disable_command_hint: "
               << lifecycle.startWithWindowsDisableCommandHint << "\n";
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
        if (g_mpPackageRefreshRequested.exchange(false)) {
            break;
        }
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

bool isCompanionRecoveryNextAction(const std::string& action)
{
    return action == "review_memory_recovery_status";
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
    if (companionNextAction == "review_crash_recovery_status") {
        return companionNextAction;
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
    if (isCompanionRecoveryNextAction(companionNextAction)) {
        return companionNextAction;
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
    if (companionNextAction == "review_crash_recovery_status") {
        return companionNextActionReason;
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
    if (isCompanionRecoveryNextAction(companionNextAction)) {
        return companionNextActionReason;
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
    const std::filesystem::path& ownerTestPlaybookPath,
    const std::string& publishCommand)
{
    if (nextAction == "run_monthly_reactive_owner_test" && !ownerTestPlaybookPath.empty()) {
        return "open " + pathString(ownerTestPlaybookPath);
    }
    if (nextAction == "review_staged_overlay_and_publish_if_desired" && !publishCommand.empty()) {
        return publishCommand;
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

std::string buildNextActionCommandHintSource(
    const std::string& nextAction,
    const std::string& nextActionCommandHint)
{
    if (!nextActionCommandHint.empty()) {
        if (nextActionCommandHint.rfind("open docs/MONTHLY_REACTIVE_OWNER_TEST_PLAYBOOK.md", 0) == 0) {
            return "owner_test_playbook_path";
        }
        if (nextAction == "review_staged_overlay_and_publish_if_desired") {
            return "generated_overlay_publish_gate_publish_command";
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
    if (isCompanionRecoveryNextAction(nextAction) && !companionNextActionPath.empty()) {
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
    const strategic_nexus::CompanionMemoryRecoveryStatus& memoryRecovery,
    const std::filesystem::path& generatedOverlayStagingDirectory,
    const std::filesystem::path& generatedOverlayStagingStatusPath,
    const std::string& generatedOverlayStagingReadiness,
    const std::size_t generatedOverlayStagingRuleCount,
    const bool generatedOverlayManifestVerified,
    const bool generatedOverlayPublishAllowed,
    const std::string& generatedOverlayManifestHash,
    const strategic_nexus::CompanionPostPlayPipelineStatus& postPlayPipeline,
    const strategic_nexus::CompanionSubsystemStatus& generatedOverlayStatus,
    const strategic_nexus::CompanionGeneratedOverlayPublishGateStatus& generatedOverlayPublishGate,
    const strategic_nexus::CompanionMpOverlayPackageStatus& mpOverlayPackage,
    const strategic_nexus::CompanionLocalLlmStatus& localLlm,
    const strategic_nexus::CompanionLifecycleStatus& lifecycle,
    const strategic_nexus::CompanionSupportReportStatus& supportReport,
    const strategic_nexus::CompanionCrashRecoveryStatus& crashRecovery,
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
    summary << "memory_recovery: " << memoryRecovery.state << " - "
            << memoryRecovery.reason << "\n";
    summary << "memory_recovery_confidence: " << memoryRecovery.confidence << "\n";
    summary << "memory_recovery_warning_visible: "
            << (memoryRecovery.warningVisible ? "true" : "false") << "\n";
    if (!memoryRecovery.anchorEntryPointId.empty()) {
        summary << "memory_recovery_anchor_entry_point_id: "
                << memoryRecovery.anchorEntryPointId << "\n";
    }
    if (!memoryRecovery.anchorCampaignKey.empty()) {
        summary << "memory_recovery_anchor_campaign_key: "
                << memoryRecovery.anchorCampaignKey << "\n";
    }
    if (!memoryRecovery.anchorSaveName.empty()) {
        summary << "memory_recovery_anchor_save_name: "
                << memoryRecovery.anchorSaveName << "\n";
    }
    if (!memoryRecovery.anchorSaveDate.empty()) {
        summary << "memory_recovery_anchor_save_date: "
                << memoryRecovery.anchorSaveDate << "\n";
    }
    if (!memoryRecovery.anchorSourceKind.empty()) {
        summary << "memory_recovery_anchor_source_kind: "
                << memoryRecovery.anchorSourceKind << "\n";
    }
    if (!memoryRecovery.anchorPath.empty()) {
        summary << "memory_recovery_anchor_path: " << pathString(memoryRecovery.anchorPath) << "\n";
    }
    summary << "memory_recovery_compatible_archived_evidence_count: "
            << memoryRecovery.compatibleArchivedEvidenceCount << "\n";
    summary << "memory_recovery_later_archived_evidence_count: "
            << memoryRecovery.laterArchivedEvidenceCount << "\n";
    if (memoryRecovery.warningVisible) {
        summary << "memory_recovery_owner_note: latest loadable save anchor is degraded or needs attention\n";
    }
    if (!entryPointReadiness.empty()) {
        summary << "entry_point_readiness: " << entryPointReadiness << "\n";
    }
    if (!postPlayPackagePath.empty()) {
        summary << "post_play_package_path: " << pathString(postPlayPackagePath) << "\n";
    }
    if (!postPlayPackageReadiness.empty()) {
        summary << "post_play_package_readiness: " << postPlayPackageReadiness << "\n";
    }
    if (!mpOverlayPackage.handoffStatus.empty()) {
        summary << "mp_handoff_status: " << mpOverlayPackage.handoffStatus << "\n";
    }
    summary << "mp_previous_host_available_known: "
            << (mpOverlayPackage.previousHostAvailableKnown ? "true" : "false") << "\n";
    if (mpOverlayPackage.previousHostAvailableKnown) {
        summary << "mp_previous_host_available: "
                << (mpOverlayPackage.previousHostAvailable ? "true" : "false") << "\n";
    }
    if (!mpOverlayPackage.packageZipPath.empty() || !mpOverlayPackage.packageManifestHash.empty()) {
        summary << "mp_sdileni_tip: zkopiruj mp_package_zip_path a mp_package_manifest_hash; host/client kroky jsou nize.\n";
    }
    if (!mpOverlayPackage.hostReadiness.empty()) {
        summary << "mp_host_readiness: " << mpOverlayPackage.hostReadiness << "\n";
    }
    if (!mpOverlayPackage.clientReadinessGate.empty()) {
        summary << "mp_client_gate: " << mpOverlayPackage.clientReadinessGate << "\n";
    }
    if (!mpOverlayPackage.hostNextStep.empty()) {
        summary << "mp_host_krok: " << mpOverlayPackage.hostNextStep << "\n";
    }
    if (!mpOverlayPackage.clientNextStep.empty()) {
        summary << "mp_client_krok: " << mpOverlayPackage.clientNextStep << "\n";
    }
    if (!mpOverlayPackage.handoffRecoveryHint.empty()) {
        summary << "mp_handoff_recovery_hint: " << mpOverlayPackage.handoffRecoveryHint << "\n";
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
    appendStartupRationaleLines(summary, lifecycle);
    summary << "support_report_state: " << supportReport.state << "\n";
    summary << "support_report_reason: " << supportReport.reason << "\n";
    if (!supportReport.previewPath.empty()) {
        summary << "support_report_preview_path: " << pathString(supportReport.previewPath) << "\n";
    }
    summary << "support_report_contact_email: " << supportReport.supportContactEmail << "\n";
    summary << "support_report_send_requires_approval: "
            << (supportReport.sendRequiresApproval ? "true" : "false") << "\n";
    summary << "support_report_raw_saves_included: "
            << (supportReport.rawSavesIncluded ? "true" : "false") << "\n";
    if (!supportReport.dataCategories.empty()) {
        summary << "support_report_data_categories: "
                << joinStrings(supportReport.dataCategories, ",") << "\n";
    }
    if (!supportReport.prepareCommandHint.empty()) {
        summary << "support_report_prepare_command_hint: " << supportReport.prepareCommandHint << "\n";
    }
    if (!supportReport.openCommandHint.empty()) {
        summary << "support_report_open_command_hint: " << supportReport.openCommandHint << "\n";
    }
    summary << "crash_recovery_state: " << crashRecovery.state << "\n";
    summary << "crash_recovery_reason: " << crashRecovery.reason << "\n";
    if (!crashRecovery.statePath.empty()) {
        summary << "crash_recovery_state_path: " << pathString(crashRecovery.statePath) << "\n";
    }
    if (!crashRecovery.lastCrashAtLocal.empty()) {
        summary << "crash_recovery_last_crash_at_local: " << crashRecovery.lastCrashAtLocal << "\n";
    }
    if (!crashRecovery.lastRecoveryAction.empty()) {
        summary << "crash_recovery_last_recovery_action: " << crashRecovery.lastRecoveryAction << "\n";
    }
    if (!crashRecovery.lastOperation.empty()) {
        summary << "crash_recovery_last_operation: " << crashRecovery.lastOperation << "\n";
    }
    if (!crashRecovery.appVersion.empty()) {
        summary << "crash_recovery_app_version: " << crashRecovery.appVersion << "\n";
    }
    summary << "crash_recovery_recent_unexpected_restart_count: "
            << crashRecovery.recentUnexpectedRestartCount << "\n";
    summary << "crash_recovery_restart_window_minutes: " << crashRecovery.restartWindowMinutes << "\n";
    summary << "crash_recovery_next_backoff_seconds: " << crashRecovery.nextBackoffSeconds << "\n";
    summary << "crash_recovery_restart_budget_exhausted: "
            << (crashRecovery.restartBudgetExhausted ? "true" : "false") << "\n";
    summary << "crash_recovery_warning_visible: "
            << (crashRecovery.warningVisible ? "true" : "false") << "\n";
    summary << "crash_recovery_support_report_recommended: "
            << (crashRecovery.supportReportRecommended ? "true" : "false") << "\n";
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
    const strategic_nexus::CompanionLifecycleStatus& lifecycle,
    const strategic_nexus::CompanionSupportReportStatus& supportReport,
    const strategic_nexus::CompanionCrashRecoveryStatus& crashRecovery,
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
            brief << "- Poznamka: sidecar plan knihovny kampani potrebuje pozornost; SNC nema duverovat aktivnimu coverage, dokud se plan neopravi.\n";
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
    brief << "- Startup lifecycle state: " << buildStartupLifecycleBrief(lifecycle.startWithWindowsEnabled) << ".\n";
    brief << "- Startup state source: " << lifecycle.startWithWindowsSource << ".\n";
    brief << "- Startup shortcut state: "
          << buildStartupShortcutStateBrief(lifecycle.startWithWindowsShortcutState) << ".\n";
    if (!lifecycle.startWithWindowsShortcutPath.empty()) {
        brief << "- Startup shortcut path: " << pathString(lifecycle.startWithWindowsShortcutPath) << "\n";
    }
    if (!lifecycle.startWithWindowsCommandHint.empty()) {
        brief << "- Startup command hint: " << lifecycle.startWithWindowsCommandHint << "\n";
        brief << "- Startup command hint source: " << lifecycle.startWithWindowsCommandHintSource << "\n";
    }
    if (!lifecycle.startWithWindowsEnableCommandHint.empty()) {
        brief << "- Startup enable command: " << lifecycle.startWithWindowsEnableCommandHint << "\n";
    }
    if (!lifecycle.startWithWindowsDisableCommandHint.empty()) {
        brief << "- Startup disable command: " << lifecycle.startWithWindowsDisableCommandHint << "\n";
    }
    brief << "- Support report state: " << supportReport.state;
    if (!supportReport.reason.empty()) {
        brief << " (" << supportReport.reason << ")";
    }
    brief << "\n";
    brief << "- Support report send approval required: "
          << (supportReport.sendRequiresApproval ? "ano" : "ne") << "\n";
    brief << "- Support report raw saves included: "
          << (supportReport.rawSavesIncluded ? "ano" : "ne") << "\n";
    brief << "- Support report contact: " << supportReport.supportContactEmail << "\n";
    if (!supportReport.previewPath.empty()) {
        brief << "- Support report preview: " << pathString(supportReport.previewPath) << "\n";
    }
    if (!supportReport.prepareCommandHint.empty()) {
        brief << "- Support report prepare command: " << supportReport.prepareCommandHint << "\n";
    }
    if (!supportReport.openCommandHint.empty()) {
        brief << "- Support report open command: " << supportReport.openCommandHint << "\n";
    }
    if (!supportReport.dataCategories.empty()) {
        brief << "- Support report data categories: " << joinStrings(supportReport.dataCategories, ",") << "\n";
    }
    brief << "- Crash recovery state: " << crashRecovery.state;
    if (!crashRecovery.reason.empty()) {
        brief << " (" << crashRecovery.reason << ")";
    }
    brief << "\n";
    brief << "- Crash recovery restart budget exhausted: "
          << (crashRecovery.restartBudgetExhausted ? "ano" : "ne") << "\n";
    brief << "- Crash recovery warning visible: " << (crashRecovery.warningVisible ? "ano" : "ne") << "\n";
    brief << "- Crash recovery support report doporuceny: "
          << (crashRecovery.supportReportRecommended ? "ano" : "ne") << "\n";
    if (!crashRecovery.statePath.empty()) {
        brief << "- Crash recovery state path: " << pathString(crashRecovery.statePath) << "\n";
    }
    if (!crashRecovery.lastCrashAtLocal.empty()) {
        brief << "- Crash recovery last crash: " << crashRecovery.lastCrashAtLocal << "\n";
    }
    if (!crashRecovery.lastRecoveryAction.empty()) {
        brief << "- Crash recovery last action: " << crashRecovery.lastRecoveryAction << "\n";
    }
    if (!crashRecovery.lastOperation.empty()) {
        brief << "- Crash recovery last operation: " << crashRecovery.lastOperation << "\n";
    }
    brief << "- Crash recovery recent restart count: " << crashRecovery.recentUnexpectedRestartCount << "\n";
    brief << "- Crash recovery restart window minutes: " << crashRecovery.restartWindowMinutes << "\n";
    brief << "- Crash recovery next backoff seconds: " << crashRecovery.nextBackoffSeconds << "\n";
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
    if (!mpOverlayPackage.handoffStatus.empty()) {
        brief << "- MP handoff status: " << mpOverlayPackage.handoffStatus << "\n";
    }
    if (!mpOverlayPackage.handoffRecoveryHint.empty()) {
        brief << "- MP recovery: " << mpOverlayPackage.handoffRecoveryHint << "\n";
    }
    brief << "- Human control guard: ";
    brief << (mpOverlayPackage.humanControlGuardState.empty() ? std::string("unknown") : mpOverlayPackage.humanControlGuardState)
          << "\n";
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
    if (!mpOverlayPackage.handoffRecoveryHint.empty()) {
        brief << "- MP recovery: " << mpOverlayPackage.handoffRecoveryHint << "\n";
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
    companionConfig.supportReportPreviewPath = g_supportReportPreviewPath;
    companionConfig.crashRecoveryStatePath = g_crashRecoveryStatePath;
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
    auto nextActionCommandHint =
        buildNextActionCommandHint(
            nextAction,
            g_nextStepsBriefPath,
            companionSnapshot.ownerTestPlaybookPath,
            companionSnapshot.generatedOverlayPublishGate.publishCommand);
    if ((nextAction == "review_crash_recovery_status" || isCompanionRecoveryNextAction(nextAction)) &&
        nextActionCommandHint.empty()) {
        nextActionCommandHint = companionSnapshot.nextActionCommandHint;
    }
    auto nextActionCommandHintSource = buildNextActionCommandHintSource(nextAction, nextActionCommandHint);
    if ((nextAction == "review_crash_recovery_status" || isCompanionRecoveryNextAction(nextAction)) &&
        !companionSnapshot.nextActionCommandHintSource.empty() &&
        companionSnapshot.nextActionCommandHintSource != "none") {
        nextActionCommandHintSource = companionSnapshot.nextActionCommandHintSource;
    }
    auto nextActionPath = buildNextActionPath(
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
    if ((nextAction == "review_crash_recovery_status" || isCompanionRecoveryNextAction(nextAction)) &&
        nextActionPath == g_trayStatusPath &&
        !companionSnapshot.nextActionPath.empty()) {
        nextActionPath = companionSnapshot.nextActionPath;
    }
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
        companionSnapshot.postPlayPipeline.memoryRecovery,
        effectiveGeneratedOverlayStagingDirectory,
        effectiveGeneratedOverlayStagingStatusPath,
        effectiveGeneratedOverlayStagingReadiness,
        effectiveGeneratedOverlayStagingRuleCount,
        effectiveGeneratedOverlayManifestVerified,
        effectiveGeneratedOverlayPublishAllowed,
        effectiveGeneratedOverlayManifestHash,
        companionSnapshot.postPlayPipeline,
        companionSnapshot.generatedOverlay,
        companionSnapshot.generatedOverlayPublishGate,
        companionSnapshot.mpOverlayPackage,
        companionSnapshot.localLlm,
        companionSnapshot.lifecycle,
        companionSnapshot.supportReport,
        companionSnapshot.crashRecovery,
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
        companionSnapshot.lifecycle,
        companionSnapshot.supportReport,
        companionSnapshot.crashRecovery,
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
    json << "  \"startup_rationale\": \""
         << jsonEscape(companionSnapshot.lifecycle.startupRationale) << "\",\n";
    json << "  \"start_with_windows_default_state\": \""
         << jsonEscape(companionSnapshot.lifecycle.startWithWindowsDefaultState) << "\",\n";
    json << "  \"start_with_windows_source\": \""
         << jsonEscape(companionSnapshot.lifecycle.startWithWindowsSource) << "\",\n";
    json << "  \"start_with_windows_shortcut_state\": \""
         << jsonEscape(companionSnapshot.lifecycle.startWithWindowsShortcutState) << "\",\n";
    json << "  \"start_with_windows_shortcut_path\": \""
         << jsonEscape(pathString(companionSnapshot.lifecycle.startWithWindowsShortcutPath)) << "\",\n";
    json << "  \"start_with_windows_command_hint\": \""
         << jsonEscape(companionSnapshot.lifecycle.startWithWindowsCommandHint) << "\",\n";
    json << "  \"start_with_windows_command_hint_source\": \""
         << jsonEscape(companionSnapshot.lifecycle.startWithWindowsCommandHintSource) << "\",\n";
    json << "  \"start_with_windows_enable_command_hint\": \""
         << jsonEscape(companionSnapshot.lifecycle.startWithWindowsEnableCommandHint) << "\",\n";
    json << "  \"start_with_windows_disable_command_hint\": \""
         << jsonEscape(companionSnapshot.lifecycle.startWithWindowsDisableCommandHint) << "\",\n";
    json << "  \"window_close_behavior\": \""
         << jsonEscape(companionSnapshot.lifecycle.windowCloseBehavior) << "\",\n";
    json << "  \"explicit_exit_behavior\": \""
         << jsonEscape(companionSnapshot.lifecycle.explicitExitBehavior) << "\",\n";
    json << "  \"crash_restart_policy\": \""
         << jsonEscape(companionSnapshot.lifecycle.crashRestartPolicy) << "\",\n";
    json << "  \"startup_lifecycle_state\": \""
         << jsonEscape(buildStartupLifecycleState(companionSnapshot.lifecycle.startWithWindowsEnabled)) << "\",\n";
    json << "  \"support_report_state\": \""
         << jsonEscape(companionSnapshot.supportReport.state) << "\",\n";
    json << "  \"support_report_reason\": \""
         << jsonEscape(companionSnapshot.supportReport.reason) << "\",\n";
    json << "  \"support_report_preview_path\": \""
         << jsonEscape(pathString(companionSnapshot.supportReport.previewPath)) << "\",\n";
    json << "  \"support_report_contact_email\": \""
         << jsonEscape(companionSnapshot.supportReport.supportContactEmail) << "\",\n";
    json << "  \"support_report_send_requires_approval\": "
         << (companionSnapshot.supportReport.sendRequiresApproval ? "true" : "false") << ",\n";
    json << "  \"support_report_raw_saves_included\": "
         << (companionSnapshot.supportReport.rawSavesIncluded ? "true" : "false") << ",\n";
    json << "  \"support_report_prepare_command_hint\": \""
         << jsonEscape(companionSnapshot.supportReport.prepareCommandHint) << "\",\n";
    json << "  \"support_report_open_command_hint\": \""
         << jsonEscape(companionSnapshot.supportReport.openCommandHint) << "\",\n";
    json << "  \"support_report_data_categories\": ";
    writeStringArrayJson(json, companionSnapshot.supportReport.dataCategories);
    json << ",\n";
    json << "  \"crash_recovery_state\": \""
         << jsonEscape(companionSnapshot.crashRecovery.state) << "\",\n";
    json << "  \"crash_recovery_reason\": \""
         << jsonEscape(companionSnapshot.crashRecovery.reason) << "\",\n";
    json << "  \"crash_recovery_state_path\": \""
         << jsonEscape(pathString(companionSnapshot.crashRecovery.statePath)) << "\",\n";
    json << "  \"crash_recovery_last_crash_at_local\": \""
         << jsonEscape(companionSnapshot.crashRecovery.lastCrashAtLocal) << "\",\n";
    json << "  \"crash_recovery_last_recovery_action\": \""
         << jsonEscape(companionSnapshot.crashRecovery.lastRecoveryAction) << "\",\n";
    json << "  \"crash_recovery_last_operation\": \""
         << jsonEscape(companionSnapshot.crashRecovery.lastOperation) << "\",\n";
    json << "  \"crash_recovery_app_version\": \""
         << jsonEscape(companionSnapshot.crashRecovery.appVersion) << "\",\n";
    json << "  \"crash_recovery_recent_unexpected_restart_count\": "
         << companionSnapshot.crashRecovery.recentUnexpectedRestartCount << ",\n";
    json << "  \"crash_recovery_restart_window_minutes\": "
         << companionSnapshot.crashRecovery.restartWindowMinutes << ",\n";
    json << "  \"crash_recovery_next_backoff_seconds\": "
         << companionSnapshot.crashRecovery.nextBackoffSeconds << ",\n";
    json << "  \"crash_recovery_restart_budget_exhausted\": "
         << (companionSnapshot.crashRecovery.restartBudgetExhausted ? "true" : "false") << ",\n";
    json << "  \"crash_recovery_warning_visible\": "
         << (companionSnapshot.crashRecovery.warningVisible ? "true" : "false") << ",\n";
    json << "  \"crash_recovery_support_report_recommended\": "
         << (companionSnapshot.crashRecovery.supportReportRecommended ? "true" : "false") << ",\n";
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
    json << "  \"memory_recovery_state\": \"" << jsonEscape(companionSnapshot.postPlayPipeline.memoryRecovery.state) << "\",\n";
    json << "  \"memory_recovery_reason\": \""
         << jsonEscape(companionSnapshot.postPlayPipeline.memoryRecovery.reason) << "\",\n";
    json << "  \"memory_recovery_confidence\": \""
         << jsonEscape(companionSnapshot.postPlayPipeline.memoryRecovery.confidence) << "\",\n";
    json << "  \"memory_recovery_warning_visible\": "
         << (companionSnapshot.postPlayPipeline.memoryRecovery.warningVisible ? "true" : "false") << ",\n";
    json << "  \"memory_recovery_anchor_entry_point_id\": \""
         << jsonEscape(companionSnapshot.postPlayPipeline.memoryRecovery.anchorEntryPointId) << "\",\n";
    json << "  \"memory_recovery_anchor_campaign_key\": \""
         << jsonEscape(companionSnapshot.postPlayPipeline.memoryRecovery.anchorCampaignKey) << "\",\n";
    json << "  \"memory_recovery_anchor_path\": \""
         << jsonEscape(pathString(companionSnapshot.postPlayPipeline.memoryRecovery.anchorPath)) << "\",\n";
    json << "  \"memory_recovery_anchor_save_name\": \""
         << jsonEscape(companionSnapshot.postPlayPipeline.memoryRecovery.anchorSaveName) << "\",\n";
    json << "  \"memory_recovery_anchor_save_date\": \""
         << jsonEscape(companionSnapshot.postPlayPipeline.memoryRecovery.anchorSaveDate) << "\",\n";
    json << "  \"memory_recovery_anchor_source_kind\": \""
         << jsonEscape(companionSnapshot.postPlayPipeline.memoryRecovery.anchorSourceKind) << "\",\n";
    json << "  \"memory_recovery_compatible_archived_evidence_count\": "
         << companionSnapshot.postPlayPipeline.memoryRecovery.compatibleArchivedEvidenceCount << ",\n";
    json << "  \"memory_recovery_later_archived_evidence_count\": "
         << companionSnapshot.postPlayPipeline.memoryRecovery.laterArchivedEvidenceCount << ",\n";
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
    json << "  \"mp_overlay_package_handoff_recovery_hint\": \""
         << jsonEscape(companionSnapshot.mpOverlayPackage.handoffRecoveryHint) << "\",\n";
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
    json << "  \"human_control_guard_state\": \""
         << jsonEscape(companionSnapshot.mpOverlayPackage.humanControlGuardState) << "\",\n";
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
    const strategic_nexus::SncPostPlayArtifactBackfiller postPlayArtifactBackfiller;

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
            strategic_nexus::SncPostPlayArtifactBackfillConfig backfillConfig;
            backfillConfig.dslDraftPath = g_dslDraftPath;
            backfillConfig.dslDraftAuditPath = g_dslDraftAuditPath;
            backfillConfig.candidateDecisionPackagePath = g_candidateDecisionPackagePath;
            backfillConfig.generatedOverlayStagingStatusPath = g_generatedOverlayStagingStatusPath;
            backfillConfig.generatedOverlayStagingDirectory = g_generatedOverlayStagingDirectory;
            backfillConfig.mpOverlayPackageDirectory = g_mpOverlayPackageDirectory;
            backfillConfig.mpOverlayPackageZipPath = g_mpOverlayPackageZipPath;
            backfillConfig.mpOverlayVersion = kTrayMpOverlayVersion;
            backfillConfig.mpGameVersion = kTrayMpGameVersion;
            backfillConfig.mpModVersion = kTrayMpModVersion;
            const auto backfillResult = postPlayArtifactBackfiller.backfill(backfillConfig);
            if (backfillResult.attempted || backfillResult.mpPackageRefreshState != "not_attempted") {
                lastMpPackageRefreshState = backfillResult.mpPackageRefreshState;
                lastMpPackageRefreshReason = backfillResult.mpPackageRefreshReason;
            }
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
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
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

void openMpOverlayPackageDirectory()
{
    if (g_mpOverlayPackageDirectory.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(g_mpOverlayPackageDirectory, error);
    const HINSTANCE result =
        ShellExecuteW(nullptr, L"open", g_mpOverlayPackageDirectory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        MessageBeep(MB_ICONWARNING);
    }
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
    const auto startupAction = loadStartupShortcutActionStatus();
    const auto startupLabel = strategic_nexus::buildSncTrayStartupShortcutToggleMenuLabel(startupAction);
    AppendMenuW(menu, MF_STRING, ID_TRAY_STATUS, L"Stav");
    AppendMenuW(
        menu,
        MF_STRING | (startupAction.toggleAvailable ? 0 : MF_GRAYED),
        ID_TRAY_TOGGLE_STARTUP,
        startupLabel.c_str());
    AppendMenuW(menu, MF_STRING, ID_TRAY_SUPPORT_REPORT, buildSupportReportMenuLabel().c_str());
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_ARCHIVE, L"Otevrit archiv");
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_MP_PACKAGE, L"Otev\u0159\u00EDt MP bal\u00ED\u010Dek");
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
        case ID_TRAY_TOGGLE_STARTUP:
            toggleStartWithWindows(hwnd);
            return 0;
        case ID_TRAY_SUPPORT_REPORT:
            prepareOrOpenSupportReport(hwnd);
            return 0;
        case ID_TRAY_OPEN_ARCHIVE:
            openArchiveDirectory();
            return 0;
        case ID_TRAY_OPEN_MP_PACKAGE:
            openMpOverlayPackageDirectory();
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
    g_supportReportPreviewPath = g_repoRoot / "dist" / "private_reports" / "snc_support_report_preview.txt";
    g_crashRecoveryStatePath = g_repoRoot / "dist" / "private_reports" / "snc_crash_recovery_state.json";
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
    applySncAppUserModelId();

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
