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

#include <wdui/DesignUi.h>

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>

#include <atomic>
#include <algorithm>
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
constexpr UINT WM_SNC_OPEN_STATUS = WM_APP + 13;
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
constexpr UINT ID_STATUS_OPEN_CAMPAIGN_LIBRARY_PLAN = 220;
constexpr UINT ID_STATUS_OPEN_CRASH_RECOVERY_STATE = 221;
constexpr UINT ID_STATUS_OPEN_GAMEPLAY_ACCEPTANCE_REPORT = 222;
constexpr UINT ID_STATUS_OPEN_FRIEND_TRUST_STORE = 223;
constexpr UINT ID_STATUS_COPY_FRIEND_PAIRING = 224;
constexpr UINT ID_STATUS_COPY_FRIEND_MP_SYNC_ENVELOPE = 225;
constexpr UINT ID_STATUS_COPY_FRIEND_MP_SYNC_INBOX_PLAN = 226;
constexpr UINT ID_STATUS_COPY_FRIEND_MP_SYNC_OUTBOX_PLAN = 227;
constexpr UINT ID_STATUS_SWITCH_SKIN = 228;
constexpr UINT ID_STATUS_PAGE_OVERVIEW = 240;
constexpr UINT ID_STATUS_PAGE_GAMEPLAY = 241;
constexpr UINT ID_STATUS_PAGE_ARCHIVE = 242;
constexpr UINT ID_STATUS_PAGE_OVERLAY = 243;
constexpr UINT ID_STATUS_PAGE_MULTIPLAYER = 244;
constexpr UINT ID_STATUS_PAGE_LLM = 245;
constexpr UINT ID_STATUS_PAGE_MAINTENANCE = 246;

enum class StatusPageId : std::size_t {
    Overview = 0,
    Gameplay,
    Archive,
    Overlay,
    Multiplayer,
    Llm,
    Maintenance,
    Count
};

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
    ModelInstallGuidance,
    NextAction,
    NextReason,
    NextHint,
    SupportReportState,
    CrashRecoveryState,
    FriendTrustStoreState,
    FriendMpSyncTransportState,
    HumanControlGuardState,
    MpPackageRefreshState,
    MpPackageZipState,
    MpPackageHandoffStatus,
    Count
};

enum class SncUiLanguage {
    Czech,
    English
};

enum class SncStatusSkinId {
    Classic,
    Starlance
};

struct LocalizedText {
    const wchar_t* cs;
    const wchar_t* en;
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
    std::wstring modelInstallGuidance;
    std::wstring modelPrepareCommand;
    std::wstring modelStatePath;
    std::wstring nextAction;
    std::string nextActionRaw;
    std::wstring nextReason;
    std::wstring nextHint;
    std::wstring nextPlaybook;
    std::wstring nextActionPath;
    std::wstring campaignLibraryPlanPath;
    std::wstring supportReportState;
    std::wstring supportReportPath;
    std::wstring crashRecoveryState;
    std::wstring crashRecoveryReason;
    std::wstring crashRecoveryPath;
    std::wstring friendTrustStoreState;
    std::wstring friendTrustStoreReason;
    std::wstring friendTrustStorePath;
    std::wstring friendTrustStoreControlsState;
    std::wstring friendTrustStoreControlsReason;
    std::wstring friendTrustStoreControlsNextStep;
    std::wstring friendTrustStoreUpdateCommandTemplate;
    std::wstring friendPairingCommandTemplate;
    std::wstring friendMpSyncEnvelopeCommandTemplate;
    std::wstring friendMpSyncInboxPlanCommandTemplate;
    std::wstring friendMpSyncOutboxPlanCommandTemplate;
    std::wstring friendMpSyncInboxPlanState;
    std::wstring friendMpSyncInboxPlanReason;
    bool friendMpSyncInboxAutomaticDownloadEnabled = false;
    bool friendMpSyncInboxPackageStagingAllowed = false;
    std::wstring friendMpSyncTransportState;
    std::wstring friendMpSyncTransportReason;
    std::wstring friendMpSyncTransportNextStep;
    std::wstring friendMpSyncPreflightChecklist;
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
    std::wstring mpPackageHostRotationSyncState;
    std::wstring mpPackageHostRotationSyncReason;
    std::wstring mpPackageHostRotationSyncNextStep;
    std::wstring mpPackageHostReadiness;
    std::wstring mpPackageClientReadinessGate;
    std::wstring mpPackageVerifyCommand;
    std::wstring mpPackageImportCommand;
    std::wstring mpPackageStrictVerifyCommand;
    std::wstring mpPackageStrictImportCommand;
};

struct DashboardSectionSpec {
    LocalizedText title;
    std::vector<StatusFieldId> fields;
};

struct StatusPageSpec {
    StatusPageId id;
    UINT commandId;
    LocalizedText navLabel;
    LocalizedText title;
    LocalizedText helpText;
    std::vector<std::size_t> sections;
    std::vector<UINT> actions;
};

struct StatusActionSpec {
    UINT id;
    LocalizedText defaultLabel;
    LocalizedText tooltip;
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

struct StatusPersistedUiState {
    bool hasWindowPlacement = false;
    RECT normalWindowRect{0, 0, 960, 720};
    bool windowMaximized = false;
    StatusPageId activePage = StatusPageId::Overview;
    SncStatusSkinId activeSkin = SncStatusSkinId::Starlance;
};

struct SncStatusSkinColors {
    COLORREF background = RGB(8, 15, 17);
    COLORREF backgroundTop = RGB(8, 15, 17);
    COLORREF titleBand = RGB(8, 15, 17);
    COLORREF panel = RGB(12, 29, 33);
    COLORREF text = RGB(232, 239, 240);
    COLORREF muted = RGB(146, 162, 164);
    COLORREF accent = RGB(214, 170, 76);
    COLORREF accentHot = RGB(232, 196, 105);
    COLORREF border = RGB(26, 71, 76);
    COLORREF valueBackground = RGB(7, 18, 20);
    COLORREF valueBorder = RGB(39, 72, 74);
    COLORREF buttonFill = RGB(8, 22, 28);
    COLORREF buttonHotFill = RGB(18, 56, 60);
    COLORREF buttonPressedFill = RGB(18, 70, 65);
    COLORREF buttonDisabledFill = RGB(6, 11, 13);
    COLORREF buttonBorder = RGB(164, 122, 69);
    COLORREF buttonText = RGB(211, 228, 221);
    COLORREF buttonDisabledText = RGB(92, 102, 100);
    COLORREF scrollTrack = RGB(10, 28, 34);
    COLORREF scrollThumb = RGB(83, 123, 116);
    COLORREF scrollThumbHover = RGB(96, 140, 133);
    COLORREF scrollThumbDrag = RGB(115, 156, 148);
};

enum class StatusScrollTargetKind {
    None = 0,
    Field,
    Details
};

SncUiLanguage detectSncUiLanguage();
SncStatusSkinColors createStatusSkinColors(SncStatusSkinId skin);
wdui::Theme createStatusTheme(SncStatusSkinId skin);
wdui::StyleCatalog createStatusStyleCatalog(const wdui::Theme& theme, SncStatusSkinId skin);

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
std::filesystem::path g_statusUiStatePath;
constexpr std::size_t kStatusSectionCount = 7;
constexpr std::size_t kStatusPageCount = static_cast<std::size_t>(StatusPageId::Count);
std::array<HWND, static_cast<std::size_t>(StatusFieldId::Count)> g_statusFieldLabels{};
std::array<HWND, static_cast<std::size_t>(StatusFieldId::Count)> g_statusFieldValues{};
std::array<StatusCustomScrollbar, static_cast<std::size_t>(StatusFieldId::Count)> g_statusFieldScrollbars{};
std::array<HWND, kStatusSectionCount> g_statusSectionTitles{};
std::array<HWND, kStatusPageCount> g_statusPageButtons{};
StatusCustomScrollbar g_statusDetailsScrollbar{};
HWND g_statusBottomTitle = nullptr;
HWND g_statusPageTitle = nullptr;
HWND g_statusPageHelp = nullptr;
HWND g_statusRefreshButton = nullptr;
HWND g_statusCopyButton = nullptr;
HWND g_statusOpenArchiveButton = nullptr;
HWND g_statusOpenBriefButton = nullptr;
HWND g_statusOpenMpPackageButton = nullptr;
HWND g_statusPublishGeneratedOverlayButton = nullptr;
HWND g_statusOpenNextActionPathButton = nullptr;
HWND g_statusOpenCampaignLibraryPlanButton = nullptr;
HWND g_statusOpenCrashRecoveryStateButton = nullptr;
HWND g_statusOpenGameplayAcceptanceReportButton = nullptr;
HWND g_statusOpenFriendTrustStoreButton = nullptr;
HWND g_statusCopyFriendPairingButton = nullptr;
HWND g_statusCopyFriendMpSyncEnvelopeButton = nullptr;
HWND g_statusCopyFriendMpSyncInboxPlanButton = nullptr;
HWND g_statusCopyFriendMpSyncOutboxPlanButton = nullptr;
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
HWND g_statusSkinButton = nullptr;
HWND g_statusWindow = nullptr;
HWND g_statusHeader = nullptr;
HWND g_statusSubtitle = nullptr;
HWND g_statusDetails = nullptr;
HWND g_statusTooltip = nullptr;
HMODULE g_statusCommonControlsModule = nullptr;
StatusPageId g_statusActivePage = StatusPageId::Overview;
StatusScrollTargetKind g_statusScrollDragTargetKind = StatusScrollTargetKind::None;
std::size_t g_statusScrollDragFieldIndex = 0;
int g_statusScrollDragOffsetY = 0;
StatusScrollTargetKind g_statusScrollHoverTargetKind = StatusScrollTargetKind::None;
std::size_t g_statusScrollHoverFieldIndex = 0;
bool g_statusScrollHoverTracking = false;
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
SncUiLanguage g_uiLanguage = detectSncUiLanguage();
SncStatusSkinId g_statusSkinId = SncStatusSkinId::Starlance;
SncStatusSkinColors g_statusSkinColors = createStatusSkinColors(g_statusSkinId);
wdui::Theme g_statusTheme = createStatusTheme(g_statusSkinId);
wdui::StyleCatalog g_statusStyleCatalog = createStatusStyleCatalog(g_statusTheme, g_statusSkinId);
StatusPersistedUiState g_statusUiState;
bool g_statusWindowCanPersistPlacement = false;
const std::array<DashboardSectionSpec, kStatusSectionCount> kStatusSections = {
    DashboardSectionSpec{{L"\u017Div\u00E9 hran\u00ED", L"Live Play"}, {StatusFieldId::LiveState, StatusFieldId::LiveStellaris, StatusFieldId::LiveSession, StatusFieldId::LiveStartup}},
    DashboardSectionSpec{{L"Dal\u0161\u00ED krok", L"Next Step"}, {StatusFieldId::NextAction, StatusFieldId::NextReason, StatusFieldId::NextHint, StatusFieldId::CrashRecoveryState}},
    DashboardSectionSpec{{L"Archiv", L"Archive"}, {StatusFieldId::ArchivePath, StatusFieldId::ArchiveVerified, StatusFieldId::ArchiveCopied, StatusFieldId::ArchiveSkipped}},
    DashboardSectionSpec{{L"Overlay", L"Overlay"}, {StatusFieldId::OverlayStaging, StatusFieldId::OverlayGate, StatusFieldId::OverlayAllowed, StatusFieldId::OverlayActive}},
    DashboardSectionSpec{{L"Multiplayer", L"Multiplayer"}, {StatusFieldId::FriendTrustStoreState, StatusFieldId::FriendMpSyncTransportState, StatusFieldId::MpPackageRefreshState, StatusFieldId::MpPackageHandoffStatus}},
    DashboardSectionSpec{{L"Lok\u00E1ln\u00ED LLM", L"Local LLM"}, {StatusFieldId::ModelState, StatusFieldId::ModelRuntime, StatusFieldId::ModelRecommendation, StatusFieldId::ModelMode, StatusFieldId::ModelInstallGuidance}},
    DashboardSectionSpec{{L"\u00DAdr\u017Eba", L"Maintenance"}, {StatusFieldId::SupportReportState, StatusFieldId::HumanControlGuardState}},
};

const std::array<StatusActionSpec, 24> kStatusActionSpecs = {
    StatusActionSpec{ID_STATUS_REFRESH, {L"Obnovit", L"Refresh"}, {L"Znovu na\u010Dte stav z lok\u00E1ln\u00EDch SNC soubor\u016F a p\u0159ekresl\u00ED otev\u0159en\u00E9 okno.", L"Reloads status from local SNC files and redraws the open window."}},
    StatusActionSpec{ID_STATUS_COPY, {L"Kop\u00EDrovat", L"Copy"}, {L"Zkop\u00EDruje kr\u00E1tk\u00FD p\u0159ehled cel\u00E9ho dashboardu do schr\u00E1nky pro posl\u00E1n\u00ED nebo archivaci.", L"Copies a short dashboard overview to the clipboard for sharing or archiving."}},
    StatusActionSpec{ID_STATUS_OPEN_ARCHIVE, {L"Archiv", L"Archive"}, {L"Otev\u0159e slo\u017Eku s autosave archivem a souvisej\u00EDc\u00EDmi v\u00FDstupy.", L"Opens the autosave archive folder and related outputs."}},
    StatusActionSpec{ID_STATUS_OPEN_BRIEF, {L"Souhrn", L"Brief"}, {L"Otev\u0159e posledn\u00ED souhrn dal\u0161\u00EDch krok\u016F, pokud existuje.", L"Opens the latest next-steps brief when available."}},
    StatusActionSpec{ID_STATUS_OPEN_MP_PACKAGE, {L"MP bal\u00ED\u010Dek", L"MP package"}, {L"Otev\u0159e slo\u017Eku multiplayer overlay bal\u00ED\u010Dku.", L"Opens the multiplayer overlay package folder."}},
    StatusActionSpec{ID_STATUS_PUBLISH_GENERATED_OVERLAY, {L"Publikovat", L"Publish"}, {L"Publikuje p\u0159ipraven\u00FD generated overlay jen pokud publish gate \u0159\u00EDk\u00E1, \u017Ee je to bezpe\u010Dn\u00E9.", L"Publishes the staged generated overlay only when the publish gate says it is safe."}},
    StatusActionSpec{ID_STATUS_OPEN_NEXT_ACTION_PATH, {L"Akce cesta", L"Action path"}, {L"Otev\u0159e soubor nebo slo\u017Eku, kter\u00E9 SNC uv\u00E1d\u00ED jako konkr\u00E9tn\u00ED dal\u0161\u00ED krok.", L"Opens the file or folder SNC lists as the concrete next action."}},
    StatusActionSpec{ID_STATUS_OPEN_CAMPAIGN_LIBRARY_PLAN, {L"Knihovna", L"Library"}, {L"Otev\u0159e pl\u00E1n kampa\u0148ov\u00E9 knihovny, pokud je p\u0159ipraven\u00FD.", L"Opens the campaign library plan when it is available."}},
    StatusActionSpec{ID_STATUS_OPEN_CRASH_RECOVERY_STATE, {L"Crash stav", L"Crash status"}, {L"Otev\u0159e stav crash recovery, tedy pro\u010D je obnova zablokovan\u00E1 nebo povolen\u00E1.", L"Opens crash recovery status, including why recovery is blocked or allowed."}},
    StatusActionSpec{ID_STATUS_OPEN_GAMEPLAY_ACCEPTANCE_REPORT, {L"Gameplay", L"Gameplay"}, {L"Otev\u0159e gameplay acceptance report pro kontrolu, jestli v\u00FDstupy d\u00E1vaj\u00ED smysl p\u0159ed pou\u017Eit\u00EDm.", L"Opens the gameplay acceptance report to review outputs before use."}},
    StatusActionSpec{ID_STATUS_OPEN_FRIEND_TRUST_STORE, {L"SNC p\u0159\u00E1tel\u00E9", L"SNC friends"}, {L"Otev\u0159e trust store se schv\u00E1len\u00FDmi SNC p\u0159\u00E1teli a jejich identitou.", L"Opens the trust store for approved SNC friends and identities."}},
    StatusActionSpec{ID_STATUS_COPY_FRIEND_PAIRING, {L"SNC pairing", L"SNC pairing"}, {L"Zkop\u00EDruje n\u00E1vod a CLI \u0161ablonu pro sp\u00E1rov\u00E1n\u00ED p\u0159\u00EDtele bez automatick\u00E9ho syncu.", L"Copies the guide and CLI template for pairing a friend without automatic sync."}},
    StatusActionSpec{ID_STATUS_COPY_FRIEND_MP_SYNC_ENVELOPE, {L"SNC MP sync", L"SNC MP sync"}, {L"Zkop\u00EDruje ru\u010Dn\u00ED metadata envelope pro friend MP sync; nic automaticky nestahuje ani neaplikuje.", L"Copies the manual metadata envelope for friend MP sync; it does not download or apply anything automatically."}},
    StatusActionSpec{ID_STATUS_COPY_FRIEND_MP_SYNC_INBOX_PLAN, {L"SNC inbox", L"SNC inbox"}, {L"Zkop\u00EDruje bezpe\u010Dn\u00FD inbox pl\u00E1n pro p\u0159\u00EDjem metadata bal\u00ED\u010Dku od p\u0159\u00EDtele.", L"Copies the safe inbox plan for receiving friend package metadata."}},
    StatusActionSpec{ID_STATUS_COPY_FRIEND_MP_SYNC_OUTBOX_PLAN, {L"SNC outbox", L"SNC outbox"}, {L"Zkop\u00EDruje bezpe\u010Dn\u00FD outbox pl\u00E1n pro p\u0159\u00EDpravu metadata bal\u00ED\u010Dku pro p\u0159\u00EDtele.", L"Copies the safe outbox plan for preparing package metadata for a friend."}},
    StatusActionSpec{ID_STATUS_EXPORT_MP_PACKAGE, {L"MP export", L"MP export"}, {L"Znovu p\u0159iprav\u00ED multiplayer overlay package z validovan\u00FDch lok\u00E1ln\u00EDch artefakt\u016F.", L"Rebuilds the multiplayer overlay package from validated local artifacts."}},
    StatusActionSpec{ID_STATUS_MP_IMPORT_HANDOFF, {L"MP import n\u00E1vod", L"MP import guide"}, {L"Zkop\u00EDruje import p\u0159\u00EDkaz a otev\u0159e bal\u00ED\u010Dek, aby ho \u0161lo bezpe\u010Dn\u011B p\u0159edat klientovi nebo dal\u0161\u00EDmu hostovi.", L"Copies the import command and opens the package for safe handoff to a client or next host."}},
    StatusActionSpec{ID_STATUS_COPY_MP_VERIFY, {L"MP verify", L"MP verify"}, {L"Zkop\u00EDruje z\u00E1kladn\u00ED p\u0159\u00EDkaz pro ov\u011B\u0159en\u00ED MP bal\u00ED\u010Dku p\u0159ed importem.", L"Copies the basic command for verifying an MP package before import."}},
    StatusActionSpec{ID_STATUS_COPY_MP_IMPORT, {L"MP import", L"MP import"}, {L"Zkop\u00EDruje z\u00E1kladn\u00ED import p\u0159\u00EDkaz; pou\u017Eij a\u017E po verify.", L"Copies the basic import command; use it after verify."}},
    StatusActionSpec{ID_STATUS_COPY_MP_STRICT_VERIFY, {L"MP strict verify", L"MP strict verify"}, {L"Zkop\u00EDruje p\u0159\u00EDsn\u00E9 ov\u011B\u0159en\u00ED MP bal\u00ED\u010Dku s ochranou proti mismatch\u016Fm.", L"Copies strict MP package verification with mismatch protection."}},
    StatusActionSpec{ID_STATUS_COPY_MP_STRICT_IMPORT, {L"MP strict import", L"MP strict import"}, {L"Zkop\u00EDruje p\u0159\u00EDsn\u00FD import p\u0159\u00EDkaz; nejbezpe\u010Dn\u011Bj\u0161\u00ED cesta pro MP pou\u017Eit\u00ED.", L"Copies the strict import command, the safest path for MP use."}},
    StatusActionSpec{ID_STATUS_COPY_LLM_PREPARE, {L"LLM p\u0159\u00EDprava", L"LLM setup"}, {L"Zkop\u00EDruje p\u0159\u00EDkaz/\u0161ablonu pro p\u0159\u00EDpravu doporu\u010Den\u00E9ho lok\u00E1ln\u00EDho LLM runtime.", L"Copies the command/template for preparing the recommended local LLM runtime."}},
    StatusActionSpec{ID_STATUS_SUPPORT_REPORT, {L"Report", L"Diagnostic report"}, {L"P\u0159iprav\u00ED nebo otev\u0159e diagnostick\u00FD support report pro stav SNC.", L"Prepares or opens the diagnostic support report for SNC status."}},
    StatusActionSpec{ID_STATUS_TOGGLE_STARTUP, {L"Start", L"Startup"}, {L"Zapne nebo vypne spou\u0161t\u011Bn\u00ED SNC Companionu p\u0159i startu Windows.", L"Enables or disables SNC Companion startup with Windows."}},
};

const std::array<StatusPageSpec, kStatusPageCount> kStatusPages = {
    StatusPageSpec{
        StatusPageId::Overview,
        ID_STATUS_PAGE_OVERVIEW,
        {L"P\u0159ehled", L"Overview"},
        {L"P\u0159ehled mise", L"Mission Overview"},
        {L"Rychl\u00FD stav: co se d\u011Bje, co chce pozornost a kde je nejbli\u017E\u0161\u00ED bezpe\u010Dn\u00E1 akce.", L"Fast status: what is happening, what needs attention, and the nearest safe action."},
        {0, 1},
        {ID_STATUS_REFRESH, ID_STATUS_COPY, ID_STATUS_OPEN_NEXT_ACTION_PATH, ID_STATUS_OPEN_BRIEF}},
    StatusPageSpec{
        StatusPageId::Gameplay,
        ID_STATUS_PAGE_GAMEPLAY,
        {L"Hran\u00ED", L"Gameplay"},
        {L"Hran\u00ED a rozhodnut\u00ED", L"Gameplay and Decisions"},
        {L"V\u011Bci kolem pr\u00E1v\u011B spu\u0161t\u011Bn\u00E9 hry, dal\u0161\u00EDho kroku a kontrol, kter\u00E9 chr\u00E1n\u00ED kampa\u0148.", L"Current game context, next action, and safety checks that protect the campaign."},
        {0, 1},
        {ID_STATUS_OPEN_GAMEPLAY_ACCEPTANCE_REPORT, ID_STATUS_OPEN_CRASH_RECOVERY_STATE, ID_STATUS_OPEN_CAMPAIGN_LIBRARY_PLAN, ID_STATUS_OPEN_NEXT_ACTION_PATH}},
    StatusPageSpec{
        StatusPageId::Archive,
        ID_STATUS_PAGE_ARCHIVE,
        {L"Archiv", L"Archive"},
        {L"Archiv a souhrny", L"Archive and Briefs"},
        {L"Autosave archiv, ov\u011B\u0159en\u00ED kopi\u00ED a rychl\u00E9 v\u00FDstupy, kter\u00E9 se daj\u00ED poslat nebo pou\u017E\u00EDt p\u0159i nav\u00E1z\u00E1n\u00ED.", L"Autosave archive, copy verification, and quick outputs for sharing or follow-up."},
        {2},
        {ID_STATUS_OPEN_ARCHIVE, ID_STATUS_OPEN_BRIEF, ID_STATUS_COPY, ID_STATUS_SUPPORT_REPORT}},
    StatusPageSpec{
        StatusPageId::Overlay,
        ID_STATUS_PAGE_OVERLAY,
        {L"Overlay", L"Overlay"},
        {L"Generated overlay", L"Generated Overlay"},
        {L"Staging, publish gate a export bal\u00ED\u010Dku. Tady pat\u0159\u00ED akce, kter\u00E9 m\u011Bn\u00ED nebo p\u0159ed\u00E1vaj\u00ED overlay.", L"Staging, publish gate, and package export. Actions that change or hand off the overlay belong here."},
        {3},
        {ID_STATUS_PUBLISH_GENERATED_OVERLAY, ID_STATUS_EXPORT_MP_PACKAGE, ID_STATUS_OPEN_MP_PACKAGE, ID_STATUS_MP_IMPORT_HANDOFF}},
    StatusPageSpec{
        StatusPageId::Multiplayer,
        ID_STATUS_PAGE_MULTIPLAYER,
        {L"MP", L"MP"},
        {L"Multiplayer a p\u0159\u00E1tel\u00E9", L"Multiplayer and Friends"},
        {L"Trust store, friend pairing a MP p\u0159\u00EDkazy. V\u0161e z\u016Fst\u00E1v\u00E1 bezpe\u010Dn\u011B zav\u0159en\u00E9, dokud nen\u00ED jasn\u00E9, kdo co pos\u00EDl\u00E1 a kdy.", L"Trust store, friend pairing, and MP commands. Everything stays safely closed until sender, payload, and timing are clear."},
        {4},
        {ID_STATUS_OPEN_FRIEND_TRUST_STORE, ID_STATUS_COPY_FRIEND_PAIRING, ID_STATUS_COPY_FRIEND_MP_SYNC_ENVELOPE, ID_STATUS_COPY_FRIEND_MP_SYNC_INBOX_PLAN, ID_STATUS_COPY_FRIEND_MP_SYNC_OUTBOX_PLAN, ID_STATUS_COPY_MP_STRICT_VERIFY, ID_STATUS_COPY_MP_STRICT_IMPORT, ID_STATUS_COPY_MP_VERIFY, ID_STATUS_COPY_MP_IMPORT}},
    StatusPageSpec{
        StatusPageId::Llm,
        ID_STATUS_PAGE_LLM,
        {L"LLM", L"LLM"},
        {L"Lok\u00E1ln\u00ED LLM", L"Local LLM"},
        {L"Modely, runtime a p\u0159\u00EDprava lok\u00E1ln\u00ED inference. Sem pat\u0159\u00ED jen akce kolem modelu.", L"Models, runtime, and local inference preparation. Model actions belong here."},
        {5},
        {ID_STATUS_COPY_LLM_PREPARE}},
    StatusPageSpec{
        StatusPageId::Maintenance,
        ID_STATUS_PAGE_MAINTENANCE,
        {L"\u00DAdr\u017Eba", L"Maintenance"},
        {L"\u00DAdr\u017Eba a diagnostika", L"Maintenance and Diagnostics"},
        {L"Start s Windows, support reporty a ochrany p\u0159ed necht\u011Bn\u00FDm z\u00E1sahem do hry.", L"Windows startup, support reports, and guards against unsafe game changes."},
        {6},
        {ID_STATUS_TOGGLE_STARTUP, ID_STATUS_SUPPORT_REPORT, ID_STATUS_REFRESH, ID_STATUS_COPY}},
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
constexpr COLORREF kStatusScrollThumbHoverColor = RGB(96, 140, 133);
constexpr COLORREF kStatusScrollThumbDragColor = RGB(115, 156, 148);
constexpr const wchar_t* kStatusEmptyValue = L"\u2014";
constexpr wchar_t kStatusWindowClassName[] = L"StrategicNexusCompanionStatusWindow";
constexpr wchar_t kTrayWindowClassName[] = L"StrategicNexusCompanionTrayWindow";
constexpr int kStatusTitleBarHeight = 34;
constexpr int kStatusTitleButtonWidth = 42;
constexpr int kStatusResizeBorder = 8;
constexpr int kStatusDefaultWindowWidth = 960;
constexpr int kStatusDefaultWindowHeight = 720;
constexpr int kStatusMinWindowWidth = 900;
constexpr int kStatusMinWindowHeight = 640;
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
std::wstring buildStatusPageDetailsText(StatusPageId page, const StatusDashboardData& data);
std::string buildFriendPairingCommandTemplateUtf8();
std::wstring utf8ToWide(const std::string& value);
std::string wideToUtf8(const std::wstring& value);
SncUiLanguage detectSncUiLanguage();
const char* sncUiLanguageToken(SncUiLanguage language);
const wchar_t* localizedText(const LocalizedText& text);
std::wstring localizedTextWide(const LocalizedText& text);
std::string localizedTextUtf8(const LocalizedText& text);
StatusDashboardData loadStatusDashboardData();
std::optional<std::string> extractSummaryLineValue(const std::string& summary, const char* key);
std::wstring makeRelativeDisplay(const std::filesystem::path& path);
std::wstring compactBoolText(const std::string& value, const wchar_t* yesText = nullptr, const wchar_t* noText = nullptr);
std::wstring compactBoolText(const std::wstring& value, const wchar_t* yesText = nullptr, const wchar_t* noText = nullptr);
std::wstring formatOwnerFacingStatusValue(const std::string& value);
std::wstring formatOwnerFacingStatusReason(const std::string& value);
std::wstring formatOwnerFacingStatusLine(const std::string& value);
std::wstring combineValues(const std::vector<std::wstring>& values, const wchar_t* separator = L" | ");
const char* statusPageToken(StatusPageId page);
StatusPageId statusPageFromToken(const std::string& value);
bool statusWindowRectUsable(const RECT& rect);
std::optional<int> extractJsonInt(const std::string& json, const char* key);
std::optional<bool> extractJsonBool(const std::string& json, const char* key);
void loadPersistedStatusUiState();
void captureStatusUiStateFromWindow(HWND hwnd);
void savePersistedStatusUiState();
void applyPersistedStatusWindowPlacement(HWND hwnd);
std::filesystem::path findRepoRoot();
const wchar_t* statusFieldLabel(StatusFieldId id);
const wchar_t* statusFieldTooltip(StatusFieldId id);
bool statusFieldUsesMultiline(StatusFieldId id);
int statusFieldLineUnits(StatusFieldId id);
void setWindowFont(HWND hwnd, HFONT font);
HWND createStatusStatic(HWND parent, const wchar_t* text, DWORD style = WS_CHILD | WS_VISIBLE);
HWND createStatusValue(HWND parent, StatusFieldId fieldId, const wchar_t* text = L"");
HWND createStatusButton(HWND parent, UINT id, const wchar_t* text);
void drawStatusButton(const DRAWITEMSTRUCT& drawItem);
bool statusCommandIsPage(UINT commandId);
const StatusPageSpec& statusPageSpec(StatusPageId page);
const StatusPageSpec* statusPageSpecByCommand(UINT commandId);
const StatusActionSpec* statusActionSpec(UINT commandId);
HWND statusActionButton(UINT commandId);
std::wstring statusActionLabel(UINT commandId);
void setStatusActivePage(HWND hwnd, StatusPageId page);
void ensureStatusTooltip(HWND hwnd);
void addStatusTooltip(HWND control, const wchar_t* text);
StatusScrollMetrics measureStatusScrollMetrics(HWND control);
StatusCustomScrollbar composeStatusScrollbar(const RECT& trackRect, const StatusScrollMetrics& metrics, bool reserveSpace);
void syncStatusCustomScrollbars(HWND hwnd, bool forceInvalidate = false);
void paintStatusCustomScrollbars(HDC hdc);
HWND statusScrollTargetWindow(StatusScrollTargetKind kind, std::size_t fieldIndex);
StatusCustomScrollbar* statusScrollTargetScrollbar(StatusScrollTargetKind kind, std::size_t fieldIndex);
bool statusPointHitsCustomScrollbar(POINT point, StatusScrollTargetKind* kind, std::size_t* fieldIndex);
void clearStatusScrollDrag();
void clearStatusScrollHover(HWND hwnd);
bool updateStatusScrollHover(HWND hwnd, POINT point);
void scrollStatusTargetByLines(StatusScrollTargetKind kind, std::size_t fieldIndex, int lineDelta);
void scrollStatusTargetByPage(StatusScrollTargetKind kind, std::size_t fieldIndex, bool forward);
void dragStatusScrollbarThumb(HWND hwnd, POINT point);
void openPathWithShell(HWND hwnd, const std::filesystem::path& path);
std::filesystem::path resolveDashboardPath(const std::wstring& path);
bool dashboardPathExists(const std::wstring& path);
void openNextActionPath(HWND hwnd);
void openCrashRecoveryStatePath(HWND hwnd);
void openGameplayAcceptanceReport(HWND hwnd);
void openFriendTrustStore(HWND hwnd);
void openMpOverlayPackageDirectory();
void publishStagedGeneratedOverlay(HWND hwnd);
bool canRefreshMpPackageExport();
void requestMpPackageExportRefresh();
bool copyTextToClipboard(HWND hwnd, const std::wstring& text);
void requestMpPackageImportHandoff(HWND hwnd);
void updateTrayTip(HWND hwnd, const std::wstring& text);
void updateStatusCaptionButtons(HWND hwnd);
void applyStatusSkin(SncStatusSkinId skin, bool persist);
void cycleStatusSkin(HWND hwnd);
void destroyStatusBrushes();
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
    const wchar_t* yes = yesText != nullptr ? yesText : localizedText({L"ano", L"yes"});
    const wchar_t* no = noText != nullptr ? noText : localizedText({L"ne", L"no"});
    if (value == "true") {
        return yes;
    }
    if (value == "false") {
        return no;
    }
    return value.empty() ? kStatusEmptyValue : utf8ToWide(value);
}

std::wstring compactBoolText(const std::wstring& value, const wchar_t* yesText, const wchar_t* noText)
{
    const wchar_t* yes = yesText != nullptr ? yesText : localizedText({L"ano", L"yes"});
    const wchar_t* no = noText != nullptr ? noText : localizedText({L"ne", L"no"});
    if (value == L"true") {
        return yes;
    }
    if (value == L"false") {
        return no;
    }
    return value.empty() ? kStatusEmptyValue : value;
}

bool startsWithLanguageCode(const wchar_t* value, const wchar_t first, const wchar_t second)
{
    if (value == nullptr || value[0] == L'\0' || value[1] == L'\0') {
        return false;
    }

    const auto lowerAscii = [](const wchar_t ch) {
        if (ch >= L'A' && ch <= L'Z') {
            return static_cast<wchar_t>(ch - L'A' + L'a');
        }
        return ch;
    };

    const wchar_t third = value[2];
    return lowerAscii(value[0]) == first &&
        lowerAscii(value[1]) == second &&
        (third == L'\0' || third == L'-' || third == L'_' || third == L'.');
}

SncUiLanguage detectSncUiLanguage()
{
    wchar_t overrideValue[16]{};
    const DWORD overrideLength =
        GetEnvironmentVariableW(L"SNC_UI_LANGUAGE", overrideValue, static_cast<DWORD>(std::size(overrideValue)));
    if (overrideLength > 0 && overrideLength < std::size(overrideValue)) {
        if (startsWithLanguageCode(overrideValue, L'c', L's') ||
            startsWithLanguageCode(overrideValue, L'c', L'z')) {
            return SncUiLanguage::Czech;
        }
        if (startsWithLanguageCode(overrideValue, L'e', L'n')) {
            return SncUiLanguage::English;
        }
    }

    const LANGID uiLanguage = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(uiLanguage) == LANG_CZECH) {
        return SncUiLanguage::Czech;
    }

    wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0 &&
        startsWithLanguageCode(localeName, L'c', L's')) {
        return SncUiLanguage::Czech;
    }

    return SncUiLanguage::English;
}

const char* sncUiLanguageToken(const SncUiLanguage language)
{
    return language == SncUiLanguage::Czech ? "cs" : "en";
}

const wchar_t* localizedText(const LocalizedText& text)
{
    if (g_uiLanguage == SncUiLanguage::Czech) {
        return text.cs != nullptr ? text.cs : L"";
    }
    return text.en != nullptr ? text.en : (text.cs != nullptr ? text.cs : L"");
}

std::wstring localizedTextWide(const LocalizedText& text)
{
    return localizedText(text);
}

std::string localizedTextUtf8(const LocalizedText& text)
{
    return wideToUtf8(localizedTextWide(text));
}

SncStatusSkinColors createStatusSkinColors(const SncStatusSkinId skin)
{
    SncStatusSkinColors colors{};
    if (skin == SncStatusSkinId::Classic) {
        return colors;
    }

    colors.background = RGB(3, 7, 14);
    colors.backgroundTop = RGB(10, 22, 42);
    colors.titleBand = RGB(2, 5, 12);
    colors.panel = RGB(8, 22, 36);
    colors.text = RGB(234, 247, 255);
    colors.muted = RGB(144, 172, 190);
    colors.accent = RGB(248, 166, 72);
    colors.accentHot = RGB(255, 214, 128);
    colors.border = RGB(38, 118, 164);
    colors.valueBackground = RGB(4, 13, 23);
    colors.valueBorder = RGB(28, 78, 108);
    colors.buttonFill = RGB(10, 30, 50);
    colors.buttonHotFill = RGB(18, 62, 92);
    colors.buttonPressedFill = RGB(7, 42, 70);
    colors.buttonDisabledFill = RGB(5, 9, 16);
    colors.buttonBorder = RGB(68, 168, 218);
    colors.buttonText = RGB(224, 244, 255);
    colors.buttonDisabledText = RGB(78, 100, 114);
    colors.scrollTrack = RGB(4, 14, 25);
    colors.scrollThumb = RGB(58, 136, 170);
    colors.scrollThumbHover = RGB(78, 184, 220);
    colors.scrollThumbDrag = RGB(248, 166, 72);
    return colors;
}

wdui::Theme createStatusTheme(const SncStatusSkinId skin)
{
    wdui::Theme theme = wdui::Theme::taskBoardFamily();
    if (skin == SncStatusSkinId::Classic) {
        return theme;
    }

    theme.frame = RGB(58, 168, 222);
    theme.shell = RGB(3, 7, 14);
    theme.panel = RGB(8, 22, 36);
    theme.header = RGB(13, 48, 78);
    theme.control = RGB(10, 30, 50);
    theme.controlHot = RGB(18, 62, 92);
    theme.controlPressed = RGB(7, 42, 70);
    theme.border = RGB(38, 118, 164);
    theme.accent = RGB(248, 166, 72);
    theme.accentHot = RGB(255, 214, 128);
    theme.text = RGB(234, 247, 255);
    theme.textHot = RGB(255, 250, 224);
    theme.mutedText = RGB(144, 172, 190);
    theme.disabledText = RGB(78, 100, 114);
    theme.disabledFill = RGB(5, 9, 16);
    theme.uiText = wdui::TextStyle{L"Bahnschrift", 10, FW_SEMIBOLD, theme.text, theme.disabledText};
    theme.monoText = wdui::TextStyle{L"Cascadia Mono", 10, FW_NORMAL, theme.text, theme.disabledText};
    return theme;
}

wdui::StyleCatalog createStatusStyleCatalog(const wdui::Theme& theme, const SncStatusSkinId skin)
{
    wdui::StyleCatalog catalog = wdui::StyleCatalog::taskBoardFamily(theme);
    if (skin == SncStatusSkinId::Classic) {
        return catalog;
    }

    const COLORREF cyan = RGB(80, 198, 242);
    const COLORREF cyanHot = RGB(132, 232, 255);
    const COLORREF amber = RGB(248, 166, 72);
    const COLORREF amberHot = RGB(255, 214, 128);
    const COLORREF deepTop = RGB(13, 42, 68);
    const COLORREF deepBottom = RGB(4, 14, 25);

    wdui::ButtonStyle button = wdui::ButtonStyle::fromTheme(theme);
    button.surface.normal =
        wdui::SurfaceStyle::gradient(deepTop, deepBottom, RGB(42, 124, 170))
            .withShadow(RGB(0, 0, 0), 4, 5, 96)
            .withGlow(RGB(46, 150, 210), 4, 34)
            .withNoise(RGB(255, 255, 255), 4, 0x5a7101u);
    button.surface.hot =
        wdui::SurfaceStyle::gradient(RGB(22, 70, 104), RGB(7, 28, 46), cyanHot)
            .withShadow(RGB(0, 0, 0), 4, 5, 88)
            .withGlow(cyanHot, 8, 70)
            .withOutline(RGB(180, 240, 255), 1);
    button.surface.pressed =
        wdui::SurfaceStyle::gradient(RGB(4, 27, 47), RGB(13, 54, 85), amber)
            .withShadow(RGB(0, 0, 0), 2, 3, 74)
            .withGlow(amber, 7, 58);
    button.surface.active =
        wdui::SurfaceStyle::gradient(RGB(20, 74, 98), RGB(7, 33, 52), amber)
            .withShadow(RGB(0, 0, 0), 4, 5, 98)
            .withGlow(amberHot, 7, 74)
            .withOutline(RGB(255, 230, 166), 1);
    button.surface.disabled = wdui::SurfaceStyle::solid(theme.disabledFill, RGB(18, 38, 52));
    button.text.normal = theme.uiText;
    button.text.hot = wdui::TextStyle{theme.uiText.fontFamily, theme.uiText.pointSize, FW_SEMIBOLD, RGB(244, 252, 255), theme.disabledText};
    button.text.pressed = wdui::TextStyle{theme.uiText.fontFamily, theme.uiText.pointSize, FW_SEMIBOLD, amberHot, theme.disabledText};
    button.text.active = wdui::TextStyle{theme.uiText.fontFamily, theme.uiText.pointSize, FW_SEMIBOLD, amberHot, theme.disabledText};
    button.text.disabled = wdui::TextStyle{theme.uiText.fontFamily, theme.uiText.pointSize, FW_SEMIBOLD, theme.disabledText, theme.disabledText};
    catalog.setButton(L"default", button);
    catalog.setButton(L"secondary", button);

    wdui::ButtonStyle primary = button;
    primary.surface.normal =
        wdui::SurfaceStyle::gradient(RGB(120, 68, 26), RGB(32, 19, 11), amber, wdui::ButtonShape::Capsule)
            .withShadow(RGB(0, 0, 0), 5, 6, 110)
            .withGlow(amber, 7, 62)
            .withNoise(RGB(255, 245, 210), 5, 0x5a7102u);
    primary.surface.hot = primary.surface.normal.withOutline(amberHot, 1).withGlow(amberHot, 10, 82);
    primary.surface.pressed =
        wdui::SurfaceStyle::gradient(RGB(42, 24, 10), RGB(136, 80, 28), amberHot, wdui::ButtonShape::Capsule)
            .withGlow(amberHot, 7, 68);
    primary.surface.active = primary.surface.hot;
    catalog.setButton(L"primary", primary);
    catalog.setButton(L"primary-action", primary);
    catalog.setButton(L"skin-switch", primary);

    wdui::ButtonStyle titleButton = button;
    titleButton.surface.normal = wdui::SurfaceStyle::solid(RGB(3, 7, 14), RGB(52, 140, 190));
    titleButton.surface.hot = wdui::SurfaceStyle::gradient(RGB(18, 58, 84), RGB(5, 16, 26), cyanHot).withGlow(cyanHot, 5, 54);
    titleButton.surface.pressed = wdui::SurfaceStyle::gradient(RGB(4, 24, 40), RGB(18, 58, 84), amber).withGlow(amber, 5, 48);
    titleButton.surface.active = titleButton.surface.hot;
    catalog.setButton(L"title", titleButton);

    wdui::PanelStyle panel = wdui::PanelStyle::fromTheme(theme);
    panel.surface =
        wdui::SurfaceStyle::gradient(RGB(11, 30, 49), RGB(4, 12, 22), RGB(40, 120, 164))
            .withShadow(RGB(0, 0, 0), 7, 9, 96)
            .withGlow(RGB(42, 132, 188), 6, 32)
            .withNoise(RGB(255, 255, 255), 5, 0x5a7103u);
    catalog.setPanel(L"default", panel);
    catalog.setPanel(L"status-card", panel);

    wdui::ScrollBarStyle scroll = wdui::ScrollBarStyle::fromTheme(theme);
    scroll.showButtons = false;
    scroll.trackPadding = 2;
    scroll.track = wdui::SurfaceStyle::gradient(RGB(3, 12, 22), RGB(2, 7, 14), RGB(26, 74, 104), wdui::ButtonShape::Capsule);
    scroll.thumb.normal =
        wdui::SurfaceStyle::gradient(RGB(44, 122, 160), RGB(18, 54, 78), cyan, wdui::ButtonShape::Capsule)
            .withShadow(RGB(0, 0, 0), 2, 3, 86)
            .withGlow(cyan, 5, 48);
    scroll.thumb.hot = scroll.thumb.normal.withOutline(cyanHot, 1).withGlow(cyanHot, 7, 70);
    scroll.thumb.pressed =
        wdui::SurfaceStyle::gradient(amber, RGB(102, 55, 18), amberHot, wdui::ButtonShape::Capsule)
            .withGlow(amberHot, 7, 74);
    scroll.thumb.active = scroll.thumb.hot;
    catalog.setScrollBar(L"default", scroll);

    return catalog;
}

const char* statusSkinToken(const SncStatusSkinId skin)
{
    return skin == SncStatusSkinId::Classic ? "classic" : "starlance";
}

SncStatusSkinId statusSkinFromToken(const std::string& value)
{
    if (value == "classic" || value == "legacy" || value == "default") {
        return SncStatusSkinId::Classic;
    }
    if (value == "starlance" || value == "game" || value == "gaming") {
        return SncStatusSkinId::Starlance;
    }
    return SncStatusSkinId::Starlance;
}

std::wstring statusSkinDisplayName(const SncStatusSkinId skin)
{
    if (skin == SncStatusSkinId::Classic) {
        return localizedTextWide({L"Classic", L"Classic"});
    }
    return localizedTextWide({L"Starlance", L"Starlance"});
}

std::wstring statusSkinButtonLabel()
{
    return localizedTextWide({L"Skin: ", L"Skin: "}) + statusSkinDisplayName(g_statusSkinId);
}

SncStatusSkinId nextStatusSkin(const SncStatusSkinId skin)
{
    return skin == SncStatusSkinId::Classic ? SncStatusSkinId::Starlance : SncStatusSkinId::Classic;
}

COLORREF blendStatusColor(const COLORREF from, const COLORREF to, const double amount)
{
    const double t = std::clamp(amount, 0.0, 1.0);
    const auto blendChannel = [t](const BYTE a, const BYTE b) {
        return static_cast<BYTE>(static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * t);
    };
    return RGB(
        blendChannel(GetRValue(from), GetRValue(to)),
        blendChannel(GetGValue(from), GetGValue(to)),
        blendChannel(GetBValue(from), GetBValue(to)));
}

void fillStatusGradient(HDC hdc, const RECT& rect, const COLORREF start, const COLORREF end, const wdui::GradientDirection direction)
{
    if (hdc == nullptr || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    TRIVERTEX vertices[2]{};
    vertices[0].x = rect.left;
    vertices[0].y = rect.top;
    vertices[0].Red = static_cast<COLOR16>(GetRValue(start) << 8);
    vertices[0].Green = static_cast<COLOR16>(GetGValue(start) << 8);
    vertices[0].Blue = static_cast<COLOR16>(GetBValue(start) << 8);
    vertices[1].x = rect.right;
    vertices[1].y = rect.bottom;
    vertices[1].Red = static_cast<COLOR16>(GetRValue(end) << 8);
    vertices[1].Green = static_cast<COLOR16>(GetGValue(end) << 8);
    vertices[1].Blue = static_cast<COLOR16>(GetBValue(end) << 8);
    GRADIENT_RECT gradientRect{0, 1};
    const ULONG mode = direction == wdui::GradientDirection::Horizontal ? GRADIENT_FILL_RECT_H : GRADIENT_FILL_RECT_V;
    GradientFill(hdc, vertices, 2, &gradientRect, 1, mode);
}

void fillStatusSolid(HDC hdc, const RECT& rect, const COLORREF color)
{
    const HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

wdui::Rect toWduiRect(const RECT& rect)
{
    return wdui::Rect{rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top};
}

void drawStatusSurface(HDC hdc, const RECT& rect, const wdui::SurfaceStyle& surface)
{
    if (hdc == nullptr || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    if (surface.showGlow) {
        const int radius = std::max(1, surface.glowRadius);
        const int passes = std::min(4, radius);
        for (int pass = passes; pass >= 1; --pass) {
            RECT glowRect = rect;
            InflateRect(&glowRect, pass, pass);
            const HBRUSH glowBrush = CreateSolidBrush(blendStatusColor(g_statusSkinColors.background, surface.glow, 0.18));
            FrameRect(hdc, &glowRect, glowBrush);
            DeleteObject(glowBrush);
        }
    }

    if (surface.showShadow) {
        RECT shadowRect = rect;
        OffsetRect(&shadowRect, surface.shadowOffsetX, surface.shadowOffsetY);
        fillStatusSolid(hdc, shadowRect, blendStatusColor(g_statusSkinColors.background, surface.shadow, 0.45));
    }

    if (!surface.skin.empty()) {
        if (surface.useNineSlice) {
            surface.skin.drawNineSlice(hdc, toWduiRect(rect), surface.nineSliceInsets, surface.opacity);
        } else {
            surface.skin.draw(hdc, toWduiRect(rect), surface.opacity);
        }
    } else if (surface.drawFill) {
        if (surface.useGradient) {
            fillStatusGradient(hdc, rect, surface.gradientStart, surface.gradientEnd, surface.gradientDirection);
        } else {
            fillStatusSolid(hdc, rect, surface.fill);
        }
    }

    if (surface.useNoise && surface.noiseOpacity > 0) {
        const int step = 5;
        for (int y = rect.top + 2; y < rect.bottom; y += step) {
            for (int x = rect.left + 2; x < rect.right; x += step) {
                const std::uint32_t hash =
                    (static_cast<std::uint32_t>(x) * 73856093u) ^
                    (static_cast<std::uint32_t>(y) * 19349663u) ^
                    surface.noiseSeed;
                if ((hash & 0x0fu) == 0u) {
                    SetPixelV(hdc, x, y, blendStatusColor(surface.fill, surface.noise, 0.10));
                }
            }
        }
    }

    if (surface.drawBorder) {
        const HBRUSH borderBrush = CreateSolidBrush(surface.border);
        FrameRect(hdc, &rect, borderBrush);
        DeleteObject(borderBrush);
    }
    if (surface.showOutline && surface.outlineWidth > 0) {
        RECT outlineRect = rect;
        for (int width = 0; width < surface.outlineWidth; ++width) {
            InflateRect(&outlineRect, -1, -1);
            const HBRUSH outlineBrush = CreateSolidBrush(surface.outline);
            FrameRect(hdc, &outlineRect, outlineBrush);
            DeleteObject(outlineBrush);
        }
    }
}

const wdui::ButtonStyle* statusButtonStyle(const UINT commandId)
{
    if (commandId == ID_STATUS_CLOSE || commandId == ID_STATUS_MAXIMIZE || commandId == ID_STATUS_MINIMIZE) {
        return g_statusStyleCatalog.button(L"title");
    }
    if (commandId == ID_STATUS_SWITCH_SKIN) {
        return g_statusStyleCatalog.button(L"skin-switch");
    }
    if (commandId == ID_STATUS_PUBLISH_GENERATED_OVERLAY ||
        commandId == ID_STATUS_EXPORT_MP_PACKAGE ||
        commandId == ID_STATUS_MP_IMPORT_HANDOFF) {
        return g_statusStyleCatalog.button(L"primary-action");
    }
    return g_statusStyleCatalog.button(L"default");
}

void destroyStatusBrushes()
{
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

void applyStatusSkin(const SncStatusSkinId skin, const bool persist)
{
    g_statusSkinId = skin;
    g_statusUiState.activeSkin = skin;
    g_statusSkinColors = createStatusSkinColors(skin);
    g_statusTheme = createStatusTheme(skin);
    g_statusStyleCatalog = createStatusStyleCatalog(g_statusTheme, skin);
    destroyStatusBrushes();

    if (g_statusWindow != nullptr && IsWindow(g_statusWindow)) {
        ensureStatusWindowResources();
        updateStatusCaptionButtons(g_statusWindow);
        InvalidateRect(g_statusWindow, nullptr, TRUE);
        RedrawWindow(g_statusWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }

    if (persist) {
        if (g_statusWindow != nullptr && IsWindow(g_statusWindow)) {
            captureStatusUiStateFromWindow(g_statusWindow);
        }
        g_statusUiState.activeSkin = skin;
        savePersistedStatusUiState();
    }
}

void cycleStatusSkin(HWND hwnd)
{
    applyStatusSkin(nextStatusSkin(g_statusSkinId), true);
    if (hwnd != nullptr) {
        layoutStatusWindow(hwnd);
    }
}

std::wstring formatOwnerFacingFallbackToken(const std::string& value)
{
    std::string text = value;
    for (char& ch : text) {
        if (ch == '_' || ch == '-') {
            ch = ' ';
        }
    }
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') {
        text[0] = static_cast<char>(text[0] - ('a' - 'A'));
    }
    return utf8ToWide(text);
}

std::wstring formatOwnerFacingStatusValue(const std::string& value)
{
    if (value.empty()) {
        return L"";
    }

    struct StatusTranslation {
        const char* value;
        LocalizedText text;
    };

    static constexpr StatusTranslation kTranslations[] = {
        {"true", {L"ano", L"yes"}},
        {"false", {L"ne", L"no"}},
        {"unknown", {L"Nezn\u00E1m\u00E9", L"Unknown"}},
        {"starting", {L"Startuje", L"Starting"}},
        {"capturing", {L"Sleduje aktivn\u00ED hru", L"Watching active game"}},
        {"waiting_for_stellaris", {L"\u010Cek\u00E1 na spu\u0161t\u011Bn\u00ED hry", L"Waiting for the game to start"}},
        {"waiting_for_stellaris_session", {L"\u010Cek\u00E1 na hern\u00ED session", L"Waiting for a game session"}},
        {"waiting_for_staged_overlay", {L"\u010Cek\u00E1 na p\u0159ipraven\u00FD overlay", L"Waiting for a staged overlay"}},
        {"waiting_for_encrypted_payload", {L"\u010Cek\u00E1 na \u0161ifrovan\u00FD bal\u00ED\u010Dek", L"Waiting for encrypted package"}},
        {"waiting_for_owner_approval", {L"\u010Cek\u00E1 na potvrzen\u00ED", L"Waiting for approval"}},
        {"not_attempted", {L"Zat\u00EDm neprob\u011Bhlo", L"Not attempted yet"}},
        {"not_prepared", {L"Zat\u00EDm nep\u0159ipraveno", L"Not prepared yet"}},
        {"not_exported", {L"Zat\u00EDm neexportov\u00E1no", L"Not exported yet"}},
        {"disabled", {L"Vypnuto", L"Disabled"}},
        {"enabled", {L"Zapnuto", L"Enabled"}},
        {"disabled_not_implemented", {L"Zat\u00EDm nen\u00ED implementov\u00E1no", L"Not implemented yet"}},
        {"manual_metadata_only", {L"Pouze ru\u010Dn\u00ED metadata", L"Manual metadata only"}},
        {"blocked_stellaris_running", {L"Blokov\u00E1no, proto\u017Ee b\u011B\u017E\u00ED Stellaris", L"Blocked because Stellaris is running"}},
        {"blocked_by_archive_verification", {L"\u010Cek\u00E1 na ov\u011B\u0159en\u00ED archivu", L"Blocked pending archive verification"}},
        {"blocked", {L"Blokov\u00E1no", L"Blocked"}},
        {"failed", {L"Selhalo", L"Failed"}},
        {"ready", {L"P\u0159ipraveno", L"Ready"}},
        {"ready_conservative", {L"P\u0159ipraveno konzervativn\u011B", L"Conservatively ready"}},
        {"ready_partial", {L"\u010C\u00E1ste\u010Dn\u011B p\u0159ipraveno", L"Partially ready"}},
        {"ready_for_mp", {L"P\u0159ipraveno pro multiplayer", L"Ready for multiplayer"}},
        {"ready_for_review", {L"P\u0159ipraveno ke kontrole", L"Ready for review"}},
        {"ready_disabled", {L"P\u0159ipraveno, automatika je vypnut\u00E1", L"Ready, automation disabled"}},
        {"needs_attention", {L"Vy\u017Eaduje pozornost", L"Needs attention"}},
        {"needs_setup", {L"Vy\u017Eaduje nastaven\u00ED", L"Needs setup"}},
        {"degraded", {L"Omezen\u00FD stav", L"Degraded"}},
        {"warning", {L"Upozorn\u011Bn\u00ED", L"Warning"}},
        {"no_mismatch", {L"Bez zji\u0161t\u011Bn\u00E9ho mismatch rizika", L"No mismatch detected"}},
        {"complete", {L"Dokon\u010Deno", L"Complete"}},
        {"accepted", {L"P\u0159ijato", L"Accepted"}},
        {"accepted_degraded_previous_host_unavailable", {L"P\u0159ijato, ale p\u0159edchoz\u00ED host nen\u00ED dostupn\u00FD", L"Accepted, but previous host is unavailable"}},
        {"degraded_previous_host_unavailable", {L"Omezeno: p\u0159edchoz\u00ED host nen\u00ED dostupn\u00FD", L"Degraded: previous host unavailable"}},
        {"zip_created", {L"ZIP bal\u00ED\u010Dek vytvo\u0159en", L"ZIP package created"}},
        {"post_play_ready", {L"Post-play zpracov\u00E1n\u00ED je p\u0159ipraven\u00E9", L"Post-play processing is ready"}},
        {"post_play_attention", {L"Post-play zpracov\u00E1n\u00ED vy\u017Eaduje pozornost", L"Post-play processing needs attention"}},
        {"post_play_package_ready", {L"Post-play bal\u00ED\u010Dek je p\u0159ipraven\u00FD", L"Post-play package is ready"}},
        {"post_play_package_attention", {L"Post-play bal\u00ED\u010Dek vy\u017Eaduje kontrolu", L"Post-play package needs review"}},
        {"post_play_package_write_failed", {L"Nepoda\u0159ilo se zapsat post-play bal\u00ED\u010Dek", L"Post-play package write failed"}},
        {"entry_points_ambiguous", {L"Vstupn\u00ED body jsou nejednozna\u010Dn\u00E9", L"Entry points are ambiguous"}},
        {"entry_point_analysis_blocked", {L"Anal\u00FDza vstupn\u00EDch bod\u016F je blokovan\u00E1", L"Entry point analysis is blocked"}},
        {"generated_overlay_staged", {L"Overlay je p\u0159ipraven\u00FD ve stagingu", L"Overlay is staged"}},
        {"generated_overlay_staging_failed", {L"Staging overlaye selhal", L"Overlay staging failed"}},
        {"generated_overlay_staging_not_verified", {L"Staging overlaye nen\u00ED ov\u011B\u0159en\u00FD", L"Overlay staging is not verified"}},
        {"staged_verified", {L"Staging je ov\u011B\u0159en\u00FD", L"Staging is verified"}},
        {"published", {L"Publikov\u00E1no", L"Published"}},
        {"review_staged_overlay_and_publish_if_desired", {L"Zkontrolovat p\u0159ipraven\u00FD overlay", L"Review staged overlay"}},
        {"review_staged_overlay_status", {L"Zkontrolovat stav overlaye", L"Review overlay status"}},
        {"review_dsl_draft", {L"Zkontrolovat DSL draft", L"Review DSL draft"}},
        {"review_candidate_decision_package", {L"Zkontrolovat kandid\u00E1tn\u00ED rozhodnut\u00ED", L"Review candidate decisions"}},
        {"review_archive_status", {L"Zkontrolovat stav archivu", L"Review archive status"}},
        {"review_local_llm_model_manager", {L"Otev\u0159\u00EDt spr\u00E1vu lok\u00E1ln\u00EDho LLM modelu", L"Open local LLM model manager"}},
        {"review_entry_point_ambiguity", {L"Vy\u0159e\u0161it nejednozna\u010Dn\u00FD vstupn\u00ED bod", L"Resolve ambiguous entry point"}},
        {"review_entry_point_analysis_failure", {L"Zkontrolovat anal\u00FDzu vstupn\u00EDch bod\u016F", L"Review entry point analysis"}},
        {"review_tray_status", {L"Zkontrolovat stav SNC", L"Review SNC status"}},
        {"wait_for_stellaris_launch", {L"Po\u010Dkat na spu\u0161t\u011Bn\u00ED Stellaris", L"Wait for Stellaris launch"}},
        {"run_monthly_reactive_owner_test", {L"Spustit vlastnick\u00FD monthly reactive test", L"Run the monthly reactive owner test"}},
        {"review_crash_recovery_status", {L"Zkontrolovat crash recovery", L"Review crash recovery"}},
        {"review_memory_recovery_status", {L"Zkontrolovat memory recovery", L"Review memory recovery"}},
        {"staged_overlay_ready_owner_gate_available", {L"P\u0159ipraven\u00FD overlay \u010Dek\u00E1 na ru\u010Dn\u00ED rozhodnut\u00ED", L"Staged overlay is waiting for manual decision"}},
        {"current_staged_overlay_already_published", {L"Aktu\u00E1ln\u00ED staging u\u017E je publikovan\u00FD", L"Current staging is already published"}},
        {"staged_overlay_written_but_publish_gate_not_ready", {L"Overlay je zapsan\u00FD, ale publish gate je\u0161t\u011B nen\u00ED p\u0159ipraven\u00FD", L"Overlay is written, but publish gate is not ready yet"}},
        {"dsl_draft_ready_before_overlay_stage", {L"DSL draft je p\u0159ipraven\u00FD, overlay je\u0161t\u011B nen\u00ED ve stagingu", L"DSL draft is ready before overlay staging"}},
        {"candidate_decisions_ready_before_dsl_draft", {L"Kandid\u00E1tn\u00ED rozhodnut\u00ED jsou p\u0159ipraven\u00E1, DSL draft je\u0161t\u011B ne", L"Candidate decisions are ready before DSL draft"}},
        {"entry_point_branch_ambiguity_detected", {L"Byla zji\u0161t\u011Bna nejednozna\u010Dnost v\u011Btve vstupn\u00EDho bodu", L"Entry point branch ambiguity detected"}},
        {"entry_point_analysis_not_ready", {L"Anal\u00FDza vstupn\u00EDch bod\u016F je\u0161t\u011B nen\u00ED p\u0159ipraven\u00E1", L"Entry point analysis is not ready yet"}},
        {"stellaris_not_running", {L"Stellaris neb\u011B\u017E\u00ED", L"Stellaris is not running"}},
        {"see_status_summary", {L"Podrobnosti jsou ve stavov\u00E9m souhrnu", L"See status summary for details"}},
        {"owner_enabled_start_with_windows", {L"Spou\u0161t\u011Bt p\u0159i startu Windows", L"Start with Windows"}},
        {"manual_start_only", {L"Ru\u010Dn\u00ED spu\u0161t\u011Bn\u00ED", L"Manual start only"}},
        {"runtime_is_ai_yes", {L"Ru\u010Dn\u00ED kontrola vlastn\u00EDkem z\u016Fst\u00E1v\u00E1 povinn\u00E1", L"Owner manual control remains required"}},
        {"override_enabled", {L"Rucn\u011B povoleno", L"Manually enabled"}},
        {"override_disabled", {L"Rucn\u011B vypnuto", L"Manually disabled"}},
        {"shortcut_installed", {L"Z\u00E1stupce je nainstalovan\u00FD", L"Shortcut installed"}},
        {"shortcut_not_installed", {L"Z\u00E1stupce nen\u00ED nainstalovan\u00FD", L"Shortcut not installed"}},
        {"existing_mp_package_already_current", {L"MP bal\u00ED\u010Dek je aktu\u00E1ln\u00ED", L"MP package is current"}},
        {"mp_package_exported_from_staged_overlay", {L"MP bal\u00ED\u010Dek byl exportov\u00E1n ze stagingu", L"MP package exported from staged overlay"}},
        {"missing_campaign_identity_for_mp_package_export", {L"Chyb\u00ED identita kampan\u011B pro MP export", L"Campaign identity is missing for MP export"}},
        {"import_and_verify_before_join", {L"P\u0159ed p\u0159ipojen\u00EDm importovat a ov\u011B\u0159it", L"Import and verify before joining"}},
        {"package_identity_mismatch_detected", {L"Zji\u0161t\u011Bn mismatch identity bal\u00ED\u010Dku", L"Package identity mismatch detected"}},
        {"no_identity_mismatch_detected", {L"Nebyl zji\u0161t\u011Bn mismatch identity bal\u00ED\u010Dku", L"No package identity mismatch detected"}},
        {"model_ready", {L"Model je p\u0159ipraven\u00FD", L"Model ready"}},
        {"no_model_installed", {L"Nen\u00ED nainstalovan\u00FD \u017E\u00E1dn\u00FD podporovan\u00FD model", L"No supported model installed"}},
        {"model_license_not_supported", {L"Licence modelu nen\u00ED podporovan\u00E1", L"Model license is not supported"}},
        {"model_license_requires_user_action", {L"Licence modelu vy\u017Eaduje ru\u010Dn\u00ED potvrzen\u00ED", L"Model license requires user action"}},
        {"model_incompatible_with_hardware", {L"Model nen\u00ED vhodn\u00FD pro tento hardware", L"Model is incompatible with this hardware"}},
        {"model_missing", {L"Model chyb\u00ED", L"Model missing"}},
        {"model_runtime_failed", {L"Runtime modelu selhal", L"Model runtime failed"}},
        {"model_changed_revalidation_needed", {L"Model se zm\u011Bnil a pot\u0159ebuje znovu ov\u011B\u0159it", L"Model changed and needs revalidation"}},
        {"state_unavailable", {L"Stav nen\u00ED dostupn\u00FD", L"Status unavailable"}},
        {"restart_budget_exhausted", {L"Rozpo\u010Det restart\u016F je vy\u010Derpan\u00FD", L"Restart budget exhausted"}},
        {"disabled_until_manual_start", {L"Vypnuto do ru\u010Dn\u00EDho startu", L"Disabled until manual start"}},
    };

    for (const auto& translation : kTranslations) {
        if (value == translation.value) {
            return localizedTextWide(translation.text);
        }
    }

    return formatOwnerFacingFallbackToken(value);
}

std::wstring formatOwnerFacingStatusReason(const std::string& value)
{
    if (value.empty()) {
        return L"";
    }
    if (value == "SNC tray bezi; ceka na stellaris.exe.") {
        return localizedTextWide({
            L"SNC b\u011B\u017E\u00ED na pozad\u00ED a \u010Dek\u00E1 na spu\u0161t\u011Bn\u00ED Stellaris.",
            L"SNC is running in the background and waiting for Stellaris to start."});
    }
    if (value == "waiting for archive to become ready") {
        return localizedTextWide({L"\u010Cek\u00E1 na p\u0159ipraven\u00FD archiv.", L"Waiting for the archive to become ready."});
    }
    if (value == "signed/encrypted friend MP sync transport adapter is not implemented; upload/send/download/staging disabled") {
        return localizedTextWide({
            L"Podepsan\u00FD a \u0161ifrovan\u00FD transport pro p\u0159\u00E1tele zat\u00EDm nen\u00ED hotov\u00FD; automatick\u00E9 odes\u00EDl\u00E1n\u00ED, stahov\u00E1n\u00ED a staging jsou vypnut\u00E9.",
            L"Signed and encrypted friend transport is not implemented yet; automatic upload, download, and staging are disabled."});
    }
    if (value == "Use manual MP package export/import and strict verify until signed/encrypted friend transport is implemented.") {
        return localizedTextWide({
            L"Do dokon\u010Den\u00ED bezpe\u010Dn\u00E9ho transportu pou\u017Eij ru\u010Dn\u00ED MP export/import a p\u0159\u00EDsn\u00E9 ov\u011B\u0159en\u00ED.",
            L"Use manual MP export/import and strict verification until secure friend transport is implemented."});
    }
    if (value == "package identity mismatch detected (campaign/version/mod/hash); run strict verify/import before MP join") {
        return localizedTextWide({
            L"Identita bal\u00ED\u010Dku nesed\u00ED (kampa\u0148/verze/mod/hash); p\u0159ed MP p\u0159ipojen\u00EDm spus\u0165 strict verify/import.",
            L"Package identity does not match (campaign/version/mod/hash); run strict verify/import before joining MP."});
    }
    if (value == "mp overlay package zip not exported by current status source") {
        return localizedTextWide({
            L"Aktu\u00E1ln\u00ED stavov\u00FD zdroj zat\u00EDm neexportoval MP ZIP bal\u00ED\u010Dek.",
            L"The current status source has not exported an MP ZIP package yet."});
    }
    if (value == "mp overlay package zip ready for handoff") {
        return localizedTextWide({
            L"MP overlay ZIP bal\u00ED\u010Dek je p\u0159ipraven\u00FD k p\u0159ed\u00E1n\u00ED.",
            L"MP overlay ZIP package is ready for handoff."});
    }
    if (value == "mp overlay package verified") {
        return localizedTextWide({L"MP overlay bal\u00ED\u010Dek je ov\u011B\u0159en\u00FD.", L"MP overlay package is verified."});
    }
    if (value == "host-owned automatic handoff sync for host rotation is not implemented; manual MP package export/import remains the fallback") {
        return localizedTextWide({
            L"Automatick\u00E9 p\u0159ed\u00E1n\u00ED hosta pro rotaci hosta je\u0161t\u011B nen\u00ED hotov\u00E9; fallback je ru\u010Dn\u00ED MP export/import.",
            L"Host-owned automatic handoff sync for host rotation is not implemented yet; the fallback is manual MP export/import."});
    }
    if (value == "Use the current MP package ZIP, strict verify/import, and manual host rotation handoff until signed/encrypted friend transport is implemented.") {
        return localizedTextWide({
            L"Pou\u017Eij aktu\u00E1ln\u00ED MP ZIP, strict verify/import a ru\u010Dn\u00ED p\u0159ed\u00E1n\u00ED hosta, dokud nebude hotov\u00FD podepsan\u00FD/\u0161ifrovan\u00FD transport.",
            L"Use the current MP ZIP, strict verify/import, and manual host handoff until signed/encrypted friend transport is implemented."});
    }
    if (value == "nacti nejnovnejsi handoff balicek pokud je k dispozici; jinak pouzij starsi overeny archiv nebo klientsky save a nech confidence snizenou") {
        return localizedTextWide({
            L"Na\u010Dti nejnov\u011Bj\u0161\u00ED handoff bal\u00ED\u010Dek, pokud je k dispozici; jinak pou\u017Eij star\u0161\u00ED ov\u011B\u0159en\u00FD archiv nebo klientsk\u00FD save a nech confidence sn\u00ED\u017Eenou.",
            L"Load the newest handoff package if available; otherwise use an older verified archive or client save and keep confidence reduced."});
    }
    if (value == "prepare the local support report preview before manual review or sending it") {
        return localizedTextWide({
            L"P\u0159ed ru\u010Dn\u00ED kontrolou nebo odesl\u00E1n\u00EDm p\u0159iprav lok\u00E1ln\u00ED n\u00E1hled support reportu.",
            L"Prepare the local support report preview before manual review or sending it."});
    }
    if (value == "no supported local model is selected; SNC can preserve saves and validate deterministic artifacts but new LLM interpretation is disabled") {
        return localizedTextWide({
            L"Nen\u00ED vybran\u00FD podporovan\u00FD lok\u00E1ln\u00ED model; SNC m\u016F\u017Ee chr\u00E1nit savy a ov\u011B\u0159ovat deterministick\u00E9 artefakty, ale nov\u00E1 LLM interpretace je vypnut\u00E1.",
            L"No supported local model is selected; SNC can preserve saves and validate deterministic artifacts, but new LLM interpretation is disabled."});
    }
    return formatOwnerFacingStatusValue(value);
}

std::wstring formatOwnerFacingStatusLine(const std::string& value)
{
    if (value.empty()) {
        return L"";
    }

    const std::string separator = " - ";
    const std::size_t separatorPos = value.find(separator);
    if (separatorPos == std::string::npos) {
        return formatOwnerFacingStatusReason(value);
    }

    const std::string state = value.substr(0, separatorPos);
    const std::string reason = value.substr(separatorPos + separator.size());
    const std::wstring displayState = formatOwnerFacingStatusValue(state);
    const std::wstring displayReason = formatOwnerFacingStatusReason(reason);
    if (displayReason.empty()) {
        return displayState;
    }
    if (displayState.empty()) {
        return displayReason;
    }
    return displayState + L". " + displayReason;
}

std::string ownerFacingStatusValueUtf8(const std::string& value)
{
    return wideToUtf8(formatOwnerFacingStatusValue(value));
}

std::string ownerFacingStatusReasonUtf8(const std::string& value)
{
    return wideToUtf8(formatOwnerFacingStatusReason(value));
}

void appendOwnerFacingStatusValueLine(
    std::ostringstream& output,
    const char* key,
    const std::string& value)
{
    if (!value.empty()) {
        output << key << ": " << ownerFacingStatusValueUtf8(value) << "\n";
    }
}

void appendOwnerFacingStatusReasonLine(
    std::ostringstream& output,
    const char* key,
    const std::string& value)
{
    if (!value.empty()) {
        output << key << ": " << ownerFacingStatusReasonUtf8(value) << "\n";
    }
}

void appendOwnerFacingStatusPairLine(
    std::ostringstream& output,
    const char* key,
    const std::string& state,
    const std::string& reason)
{
    output << key << ": " << ownerFacingStatusValueUtf8(state);
    if (!reason.empty()) {
        output << " - " << ownerFacingStatusReasonUtf8(reason);
    }
    output << "\n";
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
    case StatusFieldId::LiveState: return localizedText({L"Stav", L"Status"});
    case StatusFieldId::LiveStellaris: return localizedText({L"Stellaris", L"Stellaris"});
    case StatusFieldId::LiveSession: return localizedText({L"Session", L"Session"});
    case StatusFieldId::LiveStartup: return localizedText({L"Start", L"Startup"});
    case StatusFieldId::ArchivePath: return localizedText({L"Cesta", L"Path"});
    case StatusFieldId::ArchiveVerified: return localizedText({L"Ov\u011B\u0159eno", L"Verified"});
    case StatusFieldId::ArchiveCopied: return localizedText({L"Kopie", L"Copied"});
    case StatusFieldId::ArchiveSkipped: return localizedText({L"P\u0159esko\u010Deno", L"Skipped"});
    case StatusFieldId::OverlayStaging: return localizedText({L"Staging", L"Staging"});
    case StatusFieldId::OverlayGate: return localizedText({L"Gate", L"Gate"});
    case StatusFieldId::OverlayAllowed: return localizedText({L"Povoleno", L"Allowed"});
    case StatusFieldId::OverlayActive: return localizedText({L"Aktivn\u00ED", L"Active"});
    case StatusFieldId::ModelState: return localizedText({L"Stav", L"Status"});
    case StatusFieldId::ModelRuntime: return localizedText({L"Runtime", L"Runtime"});
    case StatusFieldId::ModelRecommendation: return localizedText({L"Doporu\u010Den\u00ED", L"Recommendation"});
    case StatusFieldId::ModelMode: return localizedText({L"Re\u017Eim", L"Mode"});
    case StatusFieldId::ModelInstallGuidance: return localizedText({L"Instalace", L"Install"});
    case StatusFieldId::NextAction: return localizedText({L"Akce", L"Action"});
    case StatusFieldId::NextReason: return localizedText({L"Pro\u010D", L"Why"});
    case StatusFieldId::NextHint: return localizedText({L"Hint", L"Hint"});
    case StatusFieldId::SupportReportState: return localizedText({L"Report", L"Report"});
    case StatusFieldId::CrashRecoveryState: return localizedText({L"Crash", L"Crash"});
    case StatusFieldId::FriendTrustStoreState: return localizedText({L"Trust store", L"Trust store"});
    case StatusFieldId::FriendMpSyncTransportState: return localizedText({L"Sync transport", L"Sync transport"});
    case StatusFieldId::HumanControlGuardState: return localizedText({L"Ru\u010Dn\u00ED kontrola", L"Manual guard"});
    case StatusFieldId::MpPackageRefreshState: return localizedText({L"MP export", L"MP export"});
    case StatusFieldId::MpPackageZipState: return localizedText({L"ZIP", L"ZIP"});
    case StatusFieldId::MpPackageHandoffStatus: return localizedText({L"Handoff", L"Handoff"});
    case StatusFieldId::Count: break;
    }
    return L"";
}

const wchar_t* statusFieldTooltip(const StatusFieldId id)
{
    switch (id) {
    case StatusFieldId::LiveState:
        return localizedText({L"Souhrn \u017Eivosti SNC Companionu: jestli b\u011B\u017E\u00ED worker a co naposledy hl\u00E1s\u00ED.", L"SNC Companion liveness summary: whether the worker is running and what it last reported."});
    case StatusFieldId::LiveStellaris:
        return localizedText({L"Detekce Stellaris procesu. Kdy\u017E hra b\u011B\u017E\u00ED, rizikov\u00E9 akce maj\u00ED b\u00FDt opatrn\u011Bj\u0161\u00ED.", L"Stellaris process detection. When the game is running, risky actions should stay more careful."});
    case StatusFieldId::LiveSession:
        return localizedText({L"Identifikace aktu\u00E1ln\u00ED session nebo posledn\u00EDho zn\u00E1m\u00E9ho hern\u00EDho kontextu.", L"Current session identity or the last known game context."});
    case StatusFieldId::LiveStartup:
        return localizedText({L"Stav spou\u0161t\u011Bn\u00ED SNC p\u0159i startu Windows.", L"SNC startup-with-Windows status."});
    case StatusFieldId::ArchivePath:
        return localizedText({L"Cesta k lok\u00E1ln\u00EDmu autosave archivu.", L"Path to the local autosave archive."});
    case StatusFieldId::ArchiveVerified:
        return localizedText({L"Informace, jestli archiv posledn\u00ED kopie pro\u0161la ov\u011B\u0159en\u00EDm.", L"Whether the latest archive copy passed verification."});
    case StatusFieldId::ArchiveCopied:
        return localizedText({L"Po\u010Det autosave soubor\u016F, kter\u00E9 posledn\u00ED b\u011Bh zkop\u00EDroval.", L"Number of autosave files copied by the last run."});
    case StatusFieldId::ArchiveSkipped:
        return localizedText({L"Po\u010Det soubor\u016F, kter\u00E9 posledn\u00ED b\u011Bh p\u0159esko\u010Dil, typicky proto\u017Ee u\u017E existovaly.", L"Number of files skipped by the last run, usually because they already existed."});
    case StatusFieldId::OverlayStaging:
        return localizedText({L"Stav p\u0159ipraven\u00E9ho generated overlay stagingu p\u0159ed publikac\u00ED.", L"Staged generated overlay status before publishing."});
    case StatusFieldId::OverlayGate:
        return localizedText({L"Bezpe\u010Dnostn\u00ED publish gate: pro\u010D overlay m\u016F\u017Ee nebo nesm\u00ED j\u00EDt do aktivn\u00ED slo\u017Eky.", L"Safety publish gate: why the overlay may or may not go to the active folder."});
    case StatusFieldId::OverlayAllowed:
        return localizedText({L"Rychl\u00E1 informace, jestli gate povoluje publikaci.", L"Quick signal for whether the gate allows publishing."});
    case StatusFieldId::OverlayActive:
        return localizedText({L"Aktu\u00E1ln\u00ED aktivn\u00ED generated overlay slo\u017Eka.", L"Current active generated overlay folder."});
    case StatusFieldId::ModelState:
        return localizedText({L"Stav lok\u00E1ln\u00EDho LLM runtime a dostupnosti modelu.", L"Local LLM runtime and model availability status."});
    case StatusFieldId::ModelRuntime:
        return localizedText({L"Doporu\u010Den\u00FD nebo detekovan\u00FD runtime pro lok\u00E1ln\u00ED model.", L"Recommended or detected runtime for the local model."});
    case StatusFieldId::ModelRecommendation:
        return localizedText({L"Co SNC doporu\u010Duje pro model/runtime podle aktu\u00E1ln\u00ED konfigurace.", L"What SNC recommends for model/runtime based on current configuration."});
    case StatusFieldId::ModelMode:
        return localizedText({L"Re\u017Eim lok\u00E1ln\u00ED inference, nap\u0159. omezen\u00FD nebo standardn\u00ED chod.", L"Local inference mode, such as reduced or standard operation."});
    case StatusFieldId::ModelInstallGuidance:
        return localizedText({L"Konkr\u00E9tn\u011Bj\u0161\u00ED instrukce, co nainstalovat nebo p\u0159ipravit pro LLM.", L"More specific instructions for what to install or prepare for the LLM."});
    case StatusFieldId::NextAction:
        return localizedText({L"Nejbli\u017E\u0161\u00ED akce, kterou SNC pova\u017Euje za smysluplnou.", L"The nearest action SNC considers useful."});
    case StatusFieldId::NextReason:
        return localizedText({L"Pro\u010D je pr\u00E1v\u011B tato akce navr\u017Eena.", L"Why this action is suggested."});
    case StatusFieldId::NextHint:
        return localizedText({L"Praktick\u00E1 n\u00E1pov\u011Bda pro dal\u0161\u00ED krok bez \u010Dten\u00ED cel\u00E9ho reportu.", L"Practical next-step hint without reading the full report."});
    case StatusFieldId::SupportReportState:
        return localizedText({L"Stav diagnostick\u00E9ho reportu SNC.", L"SNC diagnostic report status."});
    case StatusFieldId::CrashRecoveryState:
        return localizedText({L"Stav ochrany proti \u0161patn\u00E9 obnov\u011B po crashi nebo b\u011Bhem hry.", L"Guard status against unsafe recovery after a crash or during gameplay."});
    case StatusFieldId::FriendTrustStoreState:
        return localizedText({L"Stav seznamu d\u016Fv\u011Bryhodn\u00FDch SNC p\u0159\u00E1tel a jejich identit.", L"Status of trusted SNC friends and their identities."});
    case StatusFieldId::FriendMpSyncTransportState:
        return localizedText({L"Stav friend MP sync transportu. Dokud nen\u00ED bezpe\u010Dn\u00FD, sync z\u016Fst\u00E1v\u00E1 bezpe\u010Dn\u011B zav\u0159en\u00FD.", L"Friend MP sync transport status. Until it is safe, sync stays fail-closed."});
    case StatusFieldId::HumanControlGuardState:
        return localizedText({L"Ochrana, kter\u00E1 vy\u017Eaduje ru\u010Dn\u00ED kontrolu p\u0159ed rizikovou zm\u011Bnou.", L"Guard that requires manual review before risky changes."});
    case StatusFieldId::MpPackageRefreshState:
        return localizedText({L"Stav obnoven\u00ED/exportu multiplayer overlay package.", L"Multiplayer overlay package refresh/export status."});
    case StatusFieldId::MpPackageZipState:
        return localizedText({L"Stav ZIP bal\u00ED\u010Dku pro MP p\u0159ed\u00E1n\u00ED.", L"ZIP package status for MP handoff."});
    case StatusFieldId::MpPackageHandoffStatus:
        return localizedText({L"Stav p\u0159ed\u00E1n\u00ED MP bal\u00ED\u010Dku hostovi/klientovi a souvisej\u00EDc\u00ED upozorn\u011Bn\u00ED.", L"MP package handoff status for host/client and related warnings."});
    case StatusFieldId::Count:
        break;
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
    case StatusFieldId::ModelInstallGuidance:
    case StatusFieldId::NextAction:
    case StatusFieldId::NextReason:
    case StatusFieldId::NextHint:
    case StatusFieldId::CrashRecoveryState:
    case StatusFieldId::FriendMpSyncTransportState:
    case StatusFieldId::MpPackageHandoffStatus:
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
    case StatusFieldId::SupportReportState:
    case StatusFieldId::FriendTrustStoreState:
    case StatusFieldId::HumanControlGuardState:
    case StatusFieldId::MpPackageRefreshState:
    case StatusFieldId::MpPackageZipState:
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
        0,
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
    const bool hot = (drawItem.itemState & ODS_HOTLIGHT) != 0;
    const bool focused = (drawItem.itemState & ODS_FOCUS) != 0;
    const auto* pageSpec = statusPageSpecByCommand(drawItem.CtlID);
    const bool activePageButton = pageSpec != nullptr && pageSpec->id == g_statusActivePage;

    RECT rc = drawItem.rcItem;
    const wdui::ButtonStyle* style = statusButtonStyle(drawItem.CtlID);
    if (style != nullptr) {
        drawStatusSurface(drawItem.hDC, rc, style->surface.forState(!disabled, hot, selected, activePageButton));
    } else {
        const COLORREF fillColor = disabled
            ? g_statusSkinColors.buttonDisabledFill
            : ((selected || activePageButton) ? g_statusSkinColors.buttonPressedFill : g_statusSkinColors.buttonFill);
        fillStatusSolid(drawItem.hDC, rc, fillColor);
        const HBRUSH borderBrush = CreateSolidBrush(g_statusSkinColors.buttonBorder);
        FrameRect(drawItem.hDC, &rc, borderBrush);
        DeleteObject(borderBrush);
    }

    if (focused) {
        RECT focusRc = rc;
        InflateRect(&focusRc, -4, -4);
        DrawFocusRect(drawItem.hDC, &focusRc);
    }

    wchar_t label[128]{};
    GetWindowTextW(drawItem.hwndItem, label, static_cast<int>(std::size(label)));

    SetBkMode(drawItem.hDC, TRANSPARENT);
    const COLORREF textColor = style != nullptr
        ? style->text.forState(!disabled, hot, selected, activePageButton).color
        : (disabled ? g_statusSkinColors.buttonDisabledText : (activePageButton ? g_statusSkinColors.accent : g_statusSkinColors.buttonText));
    SetTextColor(drawItem.hDC, textColor);
    const HGDIOBJ oldFont = SelectObject(drawItem.hDC, g_statusSectionFont != nullptr ? g_statusSectionFont : GetStockObject(DEFAULT_GUI_FONT));
    rc.left += 10;
    rc.right -= 10;
    DrawTextW(drawItem.hDC, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (oldFont != nullptr) {
        SelectObject(drawItem.hDC, oldFont);
    }
}

bool statusCommandIsPage(const UINT commandId)
{
    return statusPageSpecByCommand(commandId) != nullptr;
}

const StatusPageSpec& statusPageSpec(const StatusPageId page)
{
    const std::size_t index = static_cast<std::size_t>(page);
    if (index < kStatusPages.size()) {
        return kStatusPages[index];
    }
    return kStatusPages[static_cast<std::size_t>(StatusPageId::Overview)];
}

const StatusPageSpec* statusPageSpecByCommand(const UINT commandId)
{
    for (const auto& page : kStatusPages) {
        if (page.commandId == commandId) {
            return &page;
        }
    }
    return nullptr;
}

const char* statusPageToken(const StatusPageId page)
{
    switch (page) {
    case StatusPageId::Overview: return "overview";
    case StatusPageId::Gameplay: return "gameplay";
    case StatusPageId::Archive: return "archive";
    case StatusPageId::Overlay: return "overlay";
    case StatusPageId::Multiplayer: return "multiplayer";
    case StatusPageId::Llm: return "llm";
    case StatusPageId::Maintenance: return "maintenance";
    case StatusPageId::Count: break;
    }
    return "overview";
}

StatusPageId statusPageFromToken(const std::string& value)
{
    if (value == "gameplay") {
        return StatusPageId::Gameplay;
    }
    if (value == "archive") {
        return StatusPageId::Archive;
    }
    if (value == "overlay") {
        return StatusPageId::Overlay;
    }
    if (value == "multiplayer") {
        return StatusPageId::Multiplayer;
    }
    if (value == "llm") {
        return StatusPageId::Llm;
    }
    if (value == "maintenance") {
        return StatusPageId::Maintenance;
    }
    return StatusPageId::Overview;
}

const StatusActionSpec* statusActionSpec(const UINT commandId)
{
    for (const auto& action : kStatusActionSpecs) {
        if (action.id == commandId) {
            return &action;
        }
    }
    return nullptr;
}

HWND statusActionButton(const UINT commandId)
{
    switch (commandId) {
    case ID_STATUS_REFRESH: return g_statusRefreshButton;
    case ID_STATUS_COPY: return g_statusCopyButton;
    case ID_STATUS_OPEN_ARCHIVE: return g_statusOpenArchiveButton;
    case ID_STATUS_OPEN_BRIEF: return g_statusOpenBriefButton;
    case ID_STATUS_OPEN_MP_PACKAGE: return g_statusOpenMpPackageButton;
    case ID_STATUS_PUBLISH_GENERATED_OVERLAY: return g_statusPublishGeneratedOverlayButton;
    case ID_STATUS_OPEN_NEXT_ACTION_PATH: return g_statusOpenNextActionPathButton;
    case ID_STATUS_OPEN_CAMPAIGN_LIBRARY_PLAN: return g_statusOpenCampaignLibraryPlanButton;
    case ID_STATUS_OPEN_CRASH_RECOVERY_STATE: return g_statusOpenCrashRecoveryStateButton;
    case ID_STATUS_OPEN_GAMEPLAY_ACCEPTANCE_REPORT: return g_statusOpenGameplayAcceptanceReportButton;
    case ID_STATUS_OPEN_FRIEND_TRUST_STORE: return g_statusOpenFriendTrustStoreButton;
    case ID_STATUS_COPY_FRIEND_PAIRING: return g_statusCopyFriendPairingButton;
    case ID_STATUS_COPY_FRIEND_MP_SYNC_ENVELOPE: return g_statusCopyFriendMpSyncEnvelopeButton;
    case ID_STATUS_COPY_FRIEND_MP_SYNC_INBOX_PLAN: return g_statusCopyFriendMpSyncInboxPlanButton;
    case ID_STATUS_COPY_FRIEND_MP_SYNC_OUTBOX_PLAN: return g_statusCopyFriendMpSyncOutboxPlanButton;
    case ID_STATUS_EXPORT_MP_PACKAGE: return g_statusExportMpPackageButton;
    case ID_STATUS_MP_IMPORT_HANDOFF: return g_statusMpImportHandoffButton;
    case ID_STATUS_COPY_MP_VERIFY: return g_statusCopyMpVerifyButton;
    case ID_STATUS_COPY_MP_IMPORT: return g_statusCopyMpImportButton;
    case ID_STATUS_COPY_MP_STRICT_VERIFY: return g_statusCopyMpStrictVerifyButton;
    case ID_STATUS_COPY_MP_STRICT_IMPORT: return g_statusCopyMpStrictImportButton;
    case ID_STATUS_COPY_LLM_PREPARE: return g_statusCopyLlmPrepareButton;
    case ID_STATUS_SUPPORT_REPORT: return g_statusSupportReportButton;
    case ID_STATUS_TOGGLE_STARTUP: return g_statusToggleStartupButton;
    default: return nullptr;
    }
}

std::wstring statusActionLabel(const UINT commandId)
{
    if (commandId == ID_STATUS_TOGGLE_STARTUP) {
        return strategic_nexus::buildSncTrayStartupShortcutToggleButtonLabel(loadStartupShortcutActionStatus());
    }
    if (commandId == ID_STATUS_SUPPORT_REPORT) {
        return strategic_nexus::buildSncTraySupportReportActionButtonLabel(loadSupportReportActionStatus());
    }
    const auto* action = statusActionSpec(commandId);
    return action != nullptr ? localizedTextWide(action->defaultLabel) : L"";
}

void setStatusActivePage(HWND hwnd, const StatusPageId page)
{
    g_statusActivePage = page;
    captureStatusUiStateFromWindow(hwnd);
    g_statusUiState.activePage = page;
    savePersistedStatusUiState();
    layoutStatusWindow(hwnd);
    refreshStatusWindowContent();
}

void ensureStatusTooltip(HWND hwnd)
{
    if (g_statusTooltip != nullptr || hwnd == nullptr) {
        return;
    }

    g_statusCommonControlsModule = LoadLibraryW(L"comctl32.dll");
    if (g_statusCommonControlsModule != nullptr) {
        using InitCommonControlsExFn = BOOL(WINAPI*)(const INITCOMMONCONTROLSEX*);
        const auto initCommonControlsEx = reinterpret_cast<InitCommonControlsExFn>(
            GetProcAddress(g_statusCommonControlsModule, "InitCommonControlsEx"));
        if (initCommonControlsEx != nullptr) {
            INITCOMMONCONTROLSEX controls{};
            controls.dwSize = sizeof(controls);
            controls.dwICC = ICC_WIN95_CLASSES;
            initCommonControlsEx(&controls);
        }
    }

    g_statusTooltip = CreateWindowExW(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASSW,
        nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        hwnd,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (g_statusTooltip != nullptr) {
        SendMessageW(g_statusTooltip, TTM_SETMAXTIPWIDTH, 0, 360);
        SetWindowPos(g_statusTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void addStatusTooltip(HWND control, const wchar_t* text)
{
    if (control == nullptr || g_statusTooltip == nullptr || text == nullptr || text[0] == L'\0') {
        return;
    }

    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tool.hwnd = g_statusWindow != nullptr ? g_statusWindow : GetParent(control);
    tool.uId = reinterpret_cast<UINT_PTR>(control);
    tool.lpszText = const_cast<LPWSTR>(text);
    SendMessageW(g_statusTooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
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

void invalidateStatusScrollbarHover(HWND hwnd, const StatusCustomScrollbar& previous, const StatusCustomScrollbar& current)
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

void clearStatusScrollHover(HWND hwnd)
{
    const auto previousKind = g_statusScrollHoverTargetKind;
    const auto previousFieldIndex = g_statusScrollHoverFieldIndex;
    g_statusScrollHoverTargetKind = StatusScrollTargetKind::None;
    g_statusScrollHoverFieldIndex = 0;

    if (previousKind == StatusScrollTargetKind::None) {
        return;
    }

    if (auto* previousScrollbar = statusScrollTargetScrollbar(previousKind, previousFieldIndex)) {
        invalidateStatusScrollbarHover(hwnd, *previousScrollbar, {});
    }
}

bool updateStatusScrollHover(HWND hwnd, const POINT point)
{
    StatusScrollTargetKind hoverKind = StatusScrollTargetKind::None;
    std::size_t hoverFieldIndex = 0;
    if (!statusPointHitsCustomScrollbar(point, &hoverKind, &hoverFieldIndex)) {
        if (g_statusScrollHoverTargetKind != StatusScrollTargetKind::None) {
            clearStatusScrollHover(hwnd);
            return true;
        }
        return false;
    }

    if (g_statusScrollHoverTargetKind == hoverKind && g_statusScrollHoverFieldIndex == hoverFieldIndex) {
        return false;
    }

    const auto previousKind = g_statusScrollHoverTargetKind;
    const auto previousFieldIndex = g_statusScrollHoverFieldIndex;
    g_statusScrollHoverTargetKind = hoverKind;
    g_statusScrollHoverFieldIndex = hoverFieldIndex;

    if (previousKind == StatusScrollTargetKind::None) {
        if (auto* currentScrollbar = statusScrollTargetScrollbar(hoverKind, hoverFieldIndex)) {
            invalidateStatusScrollbarRect(hwnd, currentScrollbar->trackRect);
        }
        return true;
    }

    if (auto* previousScrollbar = statusScrollTargetScrollbar(previousKind, previousFieldIndex)) {
        if (auto* currentScrollbar = statusScrollTargetScrollbar(hoverKind, hoverFieldIndex)) {
            invalidateStatusScrollbarHover(hwnd, *previousScrollbar, *currentScrollbar);
            return true;
        }
        invalidateStatusScrollbarRect(hwnd, previousScrollbar->trackRect);
    }

    if (auto* currentScrollbar = statusScrollTargetScrollbar(hoverKind, hoverFieldIndex)) {
        invalidateStatusScrollbarRect(hwnd, currentScrollbar->trackRect);
    }

    return true;
}

void paintStatusCustomScrollbars(HDC hdc)
{
    if (hdc == nullptr) {
        return;
    }

    const auto* scrollStyle = g_statusStyleCatalog.scrollBar(L"default");
    const HBRUSH gutterBrush = CreateSolidBrush(g_statusSkinColors.valueBackground);
    const HBRUSH borderBrush = CreateSolidBrush(g_statusSkinColors.valueBorder);
    const HBRUSH trackBrush = CreateSolidBrush(g_statusSkinColors.scrollTrack);

    const auto paintScrollbar = [&](const StatusCustomScrollbar& scrollbar, const bool dragged, const bool hovered) {
        if (!scrollbar.reserved) {
            return;
        }

        FillRect(hdc, &scrollbar.trackRect, gutterBrush);
        FrameRect(hdc, &scrollbar.trackRect, borderBrush);

        RECT innerTrack = scrollbar.trackRect;
        InflateRect(&innerTrack, -2, -2);
        if ((innerTrack.right - innerTrack.left) > 0 && (innerTrack.bottom - innerTrack.top) > 0) {
            if (scrollStyle != nullptr) {
                drawStatusSurface(hdc, innerTrack, scrollStyle->track);
            } else {
                FillRect(hdc, &innerTrack, trackBrush);
            }
        }

        if (!IsRectEmpty(&scrollbar.thumbRect)) {
            if (scrollStyle != nullptr) {
                drawStatusSurface(hdc, scrollbar.thumbRect, scrollStyle->thumb.forState(true, hovered, dragged, false));
            } else {
                const COLORREF thumbColor =
                    dragged ? g_statusSkinColors.scrollThumbDrag : hovered ? g_statusSkinColors.scrollThumbHover : g_statusSkinColors.scrollThumb;
                const HBRUSH activeThumbBrush = CreateSolidBrush(thumbColor);
                FillRect(hdc, &scrollbar.thumbRect, activeThumbBrush);
                DeleteObject(activeThumbBrush);
            }
        }
    };

    for (std::size_t index = 0; index < g_statusFieldScrollbars.size(); ++index) {
        paintScrollbar(
            g_statusFieldScrollbars[index],
            g_statusScrollDragTargetKind == StatusScrollTargetKind::Field && g_statusScrollDragFieldIndex == index,
            g_statusScrollHoverTargetKind == StatusScrollTargetKind::Field && g_statusScrollHoverFieldIndex == index);
    }
    paintScrollbar(
        g_statusDetailsScrollbar,
        g_statusScrollDragTargetKind == StatusScrollTargetKind::Details,
        g_statusScrollHoverTargetKind == StatusScrollTargetKind::Details);

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

void openCampaignLibraryPlan(HWND hwnd)
{
    const auto data = loadStatusDashboardData();
    const auto path = resolveDashboardPath(data.campaignLibraryPlanPath);
    if (path.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    openPathWithShell(hwnd, path);
}

void openCrashRecoveryStatePath(HWND hwnd)
{
    const auto data = loadStatusDashboardData();
    const auto path = resolveDashboardPath(data.crashRecoveryPath);
    if (path.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    openPathWithShell(hwnd, path);
}

void openGameplayAcceptanceReport(HWND hwnd)
{
    openPathWithShell(hwnd, g_gameplayAcceptanceReportPath);
}

void openFriendTrustStore(HWND hwnd)
{
    const auto data = loadStatusDashboardData();
    const auto path = resolveDashboardPath(data.friendTrustStorePath);
    if (path.empty()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    openPathWithShell(hwnd, path);
}

std::string buildFriendPairingCommandTemplateUtf8()
{
    std::string command;
    command += "Strategic Nexus.exe --create-snc-friend-request ";
    command += "\"dist/private_reports/snc_friend_request.snc-friend-request\" ";
    command += "<local_node_id> \"<local_display_name>\" ";
    command += "<signing_public_key> <encryption_public_key> <fingerprint> ";
    command += "<created_at_utc> [expires_at_utc]\n";
    command += "Strategic Nexus.exe --create-snc-friend-acceptance ";
    command += "\"<friend_request_path>\" ";
    command += "\"dist/private_reports/snc_friend_acceptance.snc-friend-acceptance\" ";
    command += "<local_node_id> \"<local_display_name>\" ";
    command += "<signing_public_key> <encryption_public_key> <fingerprint> ";
    command += "<created_at_utc>\n";
    command += "Strategic Nexus.exe --import-snc-friend-acceptance ";
    command += "\"<original_friend_request_path>\" ";
    command += "\"<friend_acceptance_path>\" ";
    command += "\"dist/private_reports/snc_friend_trust_store.json\" ";
    command += "<accepted_at_utc> [local_alias]";
    return command;
}

std::string buildFriendMpSyncEnvelopeCommandTemplateUtf8()
{
    std::string command;
    command += "Strategic Nexus.exe --create-snc-friend-mp-sync-envelope ";
    command += "\"dist/private_reports/snc_friend_mp_sync_envelope.json\" ";
    command += "<sender_node_id> \"<sender_display_name>\" ";
    command += "<sender_signing_public_key> <sender_encryption_public_key> <sender_fingerprint> ";
    command += "<recipient_node_id> \"<recipient_display_name>\" ";
    command += "<recipient_signing_public_key> <recipient_encryption_public_key> <recipient_fingerprint> ";
    command += "<campaign_id> <overlay_version> <package_manifest_hash> <package_zip_hash> ";
    command += "<encrypted_payload_hash> <encrypted_payload_bytes> ed25519 x25519-xsalsa20-poly1305 ";
    command += "<signature> <created_at_utc>\n";
    command += "Strategic Nexus.exe --verify-snc-friend-mp-sync-envelope ";
    command += "\"<friend_mp_sync_envelope_path>\" [stellaris_running:true|false]";
    return command;
}

std::string buildFriendMpSyncInboxPlanCommandTemplateUtf8()
{
    std::string command;
    command += "Strategic Nexus.exe --plan-snc-friend-mp-sync-inbox ";
    command += "\"<friend_mp_sync_envelope_path>\" ";
    command += "\"<encrypted_payload_path>\" ";
    command += "[stellaris_running:true|false] [friend_auto_sync_enabled:true|false]";
    return command;
}

std::string buildFriendMpSyncOutboxPlanCommandTemplateUtf8()
{
    std::string command;
    command += "Strategic Nexus.exe --plan-snc-friend-mp-sync-outbox ";
    command += "\"<friend_mp_sync_envelope_path>\" ";
    command += "\"<encrypted_payload_path>\" ";
    command += "[stellaris_running:true|false] [friend_auto_sync_enabled:true|false]";
    return command;
}

std::string buildFriendTrustStoreUpdateCommandTemplateUtf8()
{
    std::string command;
    command += "Strategic Nexus.exe --update-snc-friend-trust-store-entry ";
    command += "\"dist/private_reports/snc_friend_trust_store.json\" ";
    command += "<friend_node_id> <trusted|revoked|blocked> ";
    command += "<auto_sync_enabled:true|false> <updated_at_utc> [local_alias]";
    return command;
}

std::string buildFriendPairingGuideTextUtf8()
{
    return localizedTextUtf8({
        L"SNC friend pairing n\u00E1vod: 1) vytvo\u0159 request; 2) p\u0159\u00EDtel ov\u011B\u0159\u00ED fingerprint a vytvo\u0159\u00ED acceptance; "
        L"3) importuj acceptance do lok\u00E1ln\u00EDho trust store; 4) do aktivace podepsan\u00E9ho/\u0161ifrovan\u00E9ho transportu pou\u017E\u00EDvej manu\u00E1ln\u00ED MP package export/import a strict verify/import. "
        L"Auto-sync z\u016Fst\u00E1v\u00E1 vypnut\u00FD, dokud nen\u00ED aktivn\u00ED podepsan\u00FD/\u0161ifrovan\u00FD transport.",
        L"SNC friend pairing guide: 1) create request; 2) friend verifies fingerprint and creates acceptance; "
        L"3) import acceptance into local trust store; 4) until signed/encrypted transport is active, use manual MP package export/import and strict verify/import. "
        L"Auto-sync stays disabled until signed/encrypted transport is active."});
}

std::wstring buildFriendPairingGuideText(const StatusDashboardData& data)
{
    std::wstring guide;
    guide += localizedTextWide({L"SNC friend pairing n\u00E1vod:\r\n", L"SNC friend pairing guide:\r\n"});
    guide += localizedTextWide({
        L"1. Prvn\u00ED hr\u00E1\u010D vytvo\u0159\u00ED friend request a po\u0161le ho druh\u00E9mu hr\u00E1\u010Di mimo SNC.\r\n",
        L"1. First player creates a friend request and sends it to the second player outside SNC.\r\n"});
    guide += localizedTextWide({
        L"2. Druh\u00FD hr\u00E1\u010D ov\u011B\u0159\u00ED jm\u00E9no/fingerprint, vytvo\u0159\u00ED friend acceptance a po\u0161le ji zp\u011Bt.\r\n",
        L"2. Second player verifies name/fingerprint, creates friend acceptance, and sends it back.\r\n"});
    guide += localizedTextWide({
        L"3. Prvn\u00ED hr\u00E1\u010D importuje acceptance do lok\u00E1ln\u00EDho trust store.\r\n",
        L"3. First player imports acceptance into the local trust store.\r\n"});
    guide += localizedTextWide({
        L"4. Dokud nen\u00ED aktivn\u00ED podepsan\u00FD/\u0161ifrovan\u00FD transport, pou\u017E\u00EDvej manu\u00E1ln\u00ED MP package export/import a strict verify/import.\r\n",
        L"4. Until signed/encrypted transport is active, use manual MP package export/import and strict verify/import.\r\n"});
    guide += localizedTextWide({L"Stav trust store: ", L"Trust store status: "});
    guide += data.friendTrustStoreState.empty() ? kStatusEmptyValue : data.friendTrustStoreState;
    if (!data.friendTrustStoreReason.empty()) {
        guide += L" - ";
        guide += data.friendTrustStoreReason;
    }
    if (!data.friendTrustStorePath.empty() && data.friendTrustStorePath != kStatusEmptyValue) {
        guide += localizedTextWide({L"\r\nTrust store cesta: ", L"\r\nTrust store path: "});
        guide += data.friendTrustStorePath;
    }
    guide += localizedTextWide({
        L"\r\nAuto-sync z\u016Fst\u00E1v\u00E1 vypnut\u00FD. Podepsan\u00FD/\u0161ifrovan\u00FD transport je\u0161t\u011B nen\u00ED aktivn\u00ED.",
        L"\r\nAuto-sync stays disabled. Signed/encrypted transport is not active yet."});
    if (!data.friendPairingCommandTemplate.empty()) {
        guide += localizedTextWide({L"\r\n\r\nCLI \u0161ablona:\r\n", L"\r\n\r\nCLI template:\r\n"});
        guide += data.friendPairingCommandTemplate;
    }
    return guide;
}

void updateStatusCaptionButtons(HWND hwnd)
{
    if (g_statusSkinButton != nullptr) {
        const auto label = statusSkinButtonLabel();
        SetWindowTextW(g_statusSkinButton, label.c_str());
    }
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
    data.subtitle = localizedTextWide({L"Stav: SNC b\u011B\u017E\u00ED.", L"Status: SNC is running."});
    data.liveState = localizedTextWide({L"SNC b\u011B\u017E\u00ED.", L"SNC is running."});
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
    data.modelInstallGuidance.clear();
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
    data.friendTrustStoreState = L"\u2014";
    data.friendTrustStoreReason = L"\u2014";
    data.friendTrustStorePath = L"\u2014";
    data.friendTrustStoreControlsState = L"\u2014";
    data.friendTrustStoreControlsReason.clear();
    data.friendTrustStoreControlsNextStep.clear();
    data.friendTrustStoreUpdateCommandTemplate = utf8ToWide(buildFriendTrustStoreUpdateCommandTemplateUtf8());
    data.friendPairingCommandTemplate = utf8ToWide(buildFriendPairingCommandTemplateUtf8());
    data.friendMpSyncEnvelopeCommandTemplate = utf8ToWide(buildFriendMpSyncEnvelopeCommandTemplateUtf8());
    data.friendMpSyncInboxPlanCommandTemplate = utf8ToWide(buildFriendMpSyncInboxPlanCommandTemplateUtf8());
    data.friendMpSyncOutboxPlanCommandTemplate = utf8ToWide(buildFriendMpSyncOutboxPlanCommandTemplateUtf8());
    data.friendMpSyncTransportState = formatOwnerFacingStatusValue("disabled_not_implemented");
    data.friendMpSyncTransportReason =
        formatOwnerFacingStatusReason(
            "signed/encrypted friend MP sync transport adapter is not implemented; upload/send/download/staging disabled");
    data.friendMpSyncPreflightChecklist =
        localizedTextWide({
            L"P\u0159ed friend MP sezonou pou\u017Eij aktu\u00E1ln\u00ED MP ZIP, vytvo\u0159/ov\u011B\u0159 metadata friend MP sync envelope, spus\u0165 inbox/outbox pl\u00E1ny se zav\u0159en\u00FDm Stellaris a potom sd\u00EDlej/importuj ru\u010Dn\u011B; automatick\u00FD sync z\u016Fst\u00E1v\u00E1 vypnut\u00FD.",
            L"Before a friend MP season, use the current MP package ZIP, create/verify the friend MP sync envelope metadata, run inbox/outbox plan checks with Stellaris closed, then share/import manually; automatic sync stays disabled and automatic download and package staging stay disabled until signed/encrypted transport exists."});
    data.humanControlGuardState = L"\u2014";

    std::string json;
    if (!strategic_nexus::common::tryReadTextFile(g_trayStatusPath, json)) {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        const std::wstring fallback =
            g_statusText.empty() ? localizedTextWide({L"SNC b\u011B\u017E\u00ED.", L"SNC is running."}) : g_statusText;
        data.subtitle = localizedTextWide({L"Stav: ", L"Status: "}) + fallback;
        data.liveState = fallback;
        return data;
    }

    const auto summaryText = strategic_nexus::common::extractJsonString(json, "status_center_summary_text").value_or("");
    const auto summaryValue = [&](const char* key) -> std::string {
        return extractSummaryLineValue(summaryText, key).value_or("");
    };

    const std::string stateLine = summaryValue("stav");
    if (!stateLine.empty()) {
        data.subtitle = localizedTextWide({L"Stav: ", L"Status: "}) + formatOwnerFacingStatusLine(stateLine);
        data.liveState = formatOwnerFacingStatusLine(stateLine);
    }

    data.liveStellaris = compactBoolText(summaryValue("stellaris_running"));
    data.liveSession = utf8ToWide(summaryValue("session_id"));
    const std::string startupLifecycleState = summaryValue("startup_lifecycle_state");
    data.liveStartup = formatOwnerFacingStatusValue(startupLifecycleState);
    if (data.liveStartup.empty()) {
        data.liveStartup = kStatusEmptyValue;
    }

    const std::string archiveRoot = summaryValue("archive_root");
    if (!archiveRoot.empty()) {
        data.archivePath = utf8ToWide(archiveRoot);
    }
    data.archiveVerified = compactBoolText(summaryValue("archive_verified"));
    data.archiveCopied = utf8ToWide(summaryValue("copied_count_total"));
    data.archiveSkipped = utf8ToWide(summaryValue("skipped_count_total"));

    data.overlayStaging = formatOwnerFacingStatusValue(summaryValue("generated_overlay_staging_readiness"));
    const std::string publishGateState = summaryValue("generated_overlay_publish_gate_state");
    const std::string publishGateReason = summaryValue("generated_overlay_publish_gate_reason");
    if (!publishGateState.empty() && !publishGateReason.empty()) {
        data.overlayGate =
            formatOwnerFacingStatusValue(publishGateState) + L". " + formatOwnerFacingStatusReason(publishGateReason);
    } else if (!publishGateState.empty()) {
        data.overlayGate = formatOwnerFacingStatusValue(publishGateState);
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
        data.modelState = formatOwnerFacingStatusLine(localLlmState);
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
        runtimeParts.push_back(localizedTextWide({L"inference ", L"inference "}) + compactBoolText(canRunInference));
    }
    data.modelRuntime = combineValues(runtimeParts);

    const std::string recommendedDisplayName = summaryValue("local_llm_recommended_display_name");
    const std::string recommendedModelId = summaryValue("local_llm_recommended_model_id");
    const std::string recommendedRuntime = summaryValue("local_llm_recommended_runtime");
    data.modelPrepareCommand = utf8ToWide(summaryValue("local_llm_prepare_command_hint"));
    data.modelStatePath = utf8ToWide(summaryValue("local_llm_model_state_path"));
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
    data.modelInstallGuidance = utf8ToWide(summaryValue("local_llm_install_guidance"));

    std::vector<std::wstring> modeParts;
    if (!reducedMode.empty()) {
        modeParts.push_back(localizedTextWide({L"omezen\u00FD re\u017Eim ", L"reduced mode "}) + compactBoolText(reducedMode));
    }
    if (!userActionRequired.empty()) {
        modeParts.push_back(localizedTextWide({L"ru\u010Dn\u00ED akce ", L"user action "}) + compactBoolText(userActionRequired));
    }
    if (modeParts.empty()) {
        const std::string downloadAllowed = summaryValue("local_llm_download_allowed");
        if (!downloadAllowed.empty()) {
            modeParts.push_back(localizedTextWide({L"download ", L"download "}) + compactBoolText(downloadAllowed));
        }
    }
    data.modelMode = combineValues(modeParts);

    data.nextActionRaw = strategic_nexus::common::extractJsonString(json, "next_action").value_or("");
    data.nextAction = formatOwnerFacingStatusValue(data.nextActionRaw);
    data.nextReason =
        formatOwnerFacingStatusReason(strategic_nexus::common::extractJsonString(json, "next_action_reason").value_or(""));
    data.nextHint = utf8ToWide(strategic_nexus::common::extractJsonString(json, "next_action_command_hint").value_or(""));
    data.nextPlaybook = utf8ToWide(strategic_nexus::common::extractJsonString(json, "owner_test_playbook_path").value_or(""));
    data.nextActionPath = utf8ToWide(strategic_nexus::common::extractJsonString(json, "next_action_path").value_or(""));
    data.campaignLibraryPlanPath =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "campaign_library_plan_path").value_or(""));
    data.supportReportState =
        formatOwnerFacingStatusValue(strategic_nexus::common::extractJsonString(json, "support_report_state").value_or(""));
    data.supportReportPath =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "support_report_preview_path").value_or(""));
    data.crashRecoveryState =
        formatOwnerFacingStatusValue(strategic_nexus::common::extractJsonString(json, "crash_recovery_state").value_or(""));
    data.crashRecoveryReason =
        formatOwnerFacingStatusReason(strategic_nexus::common::extractJsonString(json, "crash_recovery_reason").value_or(""));
    data.crashRecoveryPath =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "crash_recovery_state_path").value_or(""));
    data.friendTrustStoreState =
        formatOwnerFacingStatusValue(strategic_nexus::common::extractJsonString(json, "friend_trust_store_state").value_or(""));
    data.friendTrustStoreReason =
        formatOwnerFacingStatusReason(strategic_nexus::common::extractJsonString(json, "friend_trust_store_reason").value_or(""));
    data.friendTrustStorePath =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "friend_trust_store_path").value_or(""));
    data.friendTrustStoreControlsState =
        formatOwnerFacingStatusValue(strategic_nexus::common::extractJsonString(json, "friend_trust_store_controls_state").value_or(""));
    if (data.friendTrustStoreControlsState.empty()) {
        data.friendTrustStoreControlsState = formatOwnerFacingStatusValue("not_configured");
    }
    data.friendTrustStoreControlsReason =
        formatOwnerFacingStatusReason(strategic_nexus::common::extractJsonString(json, "friend_trust_store_controls_reason").value_or(""));
    data.friendTrustStoreControlsNextStep =
        formatOwnerFacingStatusReason(strategic_nexus::common::extractJsonString(json, "friend_trust_store_controls_next_step").value_or(""));
    data.friendTrustStoreUpdateCommandTemplate =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "friend_trust_store_update_command_template").value_or(""));
    if (data.friendTrustStoreUpdateCommandTemplate.empty()) {
        data.friendTrustStoreUpdateCommandTemplate = utf8ToWide(buildFriendTrustStoreUpdateCommandTemplateUtf8());
    }
    data.friendMpSyncEnvelopeCommandTemplate =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "friend_mp_sync_envelope_command_template").value_or(""));
    if (data.friendMpSyncEnvelopeCommandTemplate.empty()) {
        data.friendMpSyncEnvelopeCommandTemplate = utf8ToWide(buildFriendMpSyncEnvelopeCommandTemplateUtf8());
    }
    data.friendMpSyncInboxPlanCommandTemplate =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "friend_mp_sync_inbox_plan_command_template").value_or(""));
    if (data.friendMpSyncInboxPlanCommandTemplate.empty()) {
        data.friendMpSyncInboxPlanCommandTemplate = utf8ToWide(buildFriendMpSyncInboxPlanCommandTemplateUtf8());
    }
    data.friendMpSyncInboxPlanState =
        formatOwnerFacingStatusValue(strategic_nexus::common::extractJsonString(json, "friend_mp_sync_inbox_plan_state").value_or(""));
    if (data.friendMpSyncInboxPlanState.empty()) {
        data.friendMpSyncInboxPlanState = formatOwnerFacingStatusValue("disabled_not_implemented");
    }
    data.friendMpSyncInboxPlanReason =
        formatOwnerFacingStatusReason(strategic_nexus::common::extractJsonString(json, "friend_mp_sync_inbox_plan_reason").value_or(""));
    if (data.friendMpSyncInboxPlanReason.empty()) {
        data.friendMpSyncInboxPlanReason =
            formatOwnerFacingStatusReason(
                "signed/encrypted friend MP sync transport adapter is not implemented; automatic download and package staging disabled");
    }
    data.friendMpSyncInboxAutomaticDownloadEnabled =
        extractJsonBool(json, "friend_mp_sync_inbox_plan_automatic_download_enabled").value_or(false);
    data.friendMpSyncInboxPackageStagingAllowed =
        extractJsonBool(json, "friend_mp_sync_inbox_plan_package_staging_allowed").value_or(false);
    data.friendMpSyncOutboxPlanCommandTemplate =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "friend_mp_sync_outbox_plan_command_template").value_or(""));
    if (data.friendMpSyncOutboxPlanCommandTemplate.empty()) {
        data.friendMpSyncOutboxPlanCommandTemplate = utf8ToWide(buildFriendMpSyncOutboxPlanCommandTemplateUtf8());
    }
    data.friendMpSyncTransportState =
        formatOwnerFacingStatusValue(strategic_nexus::common::extractJsonString(json, "friend_mp_sync_transport_state").value_or(""));
    if (data.friendMpSyncTransportState.empty()) {
        data.friendMpSyncTransportState = formatOwnerFacingStatusValue("disabled_not_implemented");
    }
    data.friendMpSyncTransportReason =
        formatOwnerFacingStatusReason(strategic_nexus::common::extractJsonString(json, "friend_mp_sync_transport_reason").value_or(""));
    if (data.friendMpSyncTransportReason.empty()) {
        data.friendMpSyncTransportReason =
            formatOwnerFacingStatusReason(
                "signed/encrypted friend MP sync transport adapter is not implemented; upload/send/download/staging disabled");
    }
    data.friendMpSyncTransportNextStep =
        formatOwnerFacingStatusReason(strategic_nexus::common::extractJsonString(json, "friend_mp_sync_transport_next_step").value_or(""));
    if (data.friendMpSyncTransportNextStep.empty()) {
        data.friendMpSyncTransportNextStep =
            formatOwnerFacingStatusReason(
                "Use manual MP package export/import and strict verify until signed/encrypted friend transport is implemented.");
    }
    data.friendMpSyncPreflightChecklist =
        utf8ToWide(strategic_nexus::common::extractJsonString(json, "friend_mp_sync_preflight_checklist").value_or(""));
    if (data.friendMpSyncPreflightChecklist.empty()) {
        data.friendMpSyncPreflightChecklist =
            localizedTextWide({
                L"P\u0159ed friend MP sezonou pou\u017Eij aktu\u00E1ln\u00ED MP ZIP, vytvo\u0159/ov\u011B\u0159 metadata friend MP sync envelope, spus\u0165 inbox/outbox pl\u00E1ny se zav\u0159en\u00FDm Stellaris a potom sd\u00EDlej/importuj ru\u010Dn\u011B; automatick\u00FD sync z\u016Fst\u00E1v\u00E1 vypnut\u00FD.",
                L"Before a friend MP season, use the current MP package ZIP, create/verify the friend MP sync envelope metadata, run inbox/outbox plan checks with Stellaris closed, then share/import manually; automatic sync stays disabled and automatic download and package staging stay disabled until signed/encrypted transport exists."});
    }
    const std::string humanControlGuardState = summaryValue("human_control_guard_state");
    if (!humanControlGuardState.empty()) {
        data.humanControlGuardState = formatOwnerFacingStatusValue(humanControlGuardState);
    }
    data.mpPackageRefreshState = formatOwnerFacingStatusValue(summaryValue("mp_package_refresh_state"));
    data.mpPackageRefreshReason = formatOwnerFacingStatusReason(summaryValue("mp_package_refresh_reason"));
    data.mpPackageDirectory = utf8ToWide(summaryValue("mp_overlay_package_directory"));
    data.mpPackageZipPath = utf8ToWide(summaryValue("mp_package_zip_runtime_path"));
    data.mpPackageZipState = formatOwnerFacingStatusValue(summaryValue("mp_package_zip_state"));
    data.mpPackageZipReason = formatOwnerFacingStatusReason(summaryValue("mp_package_zip_reason"));
    data.mpPackageManifestHash = utf8ToWide(summaryValue("mp_package_manifest_hash"));
    data.mpPackageHandoffStatus = formatOwnerFacingStatusValue(summaryValue("mp_handoff_status"));
    data.mpPackageMismatchWarningState = formatOwnerFacingStatusValue(summaryValue("mp_mismatch_warning_state"));
    data.mpPackageMismatchWarningReason = formatOwnerFacingStatusReason(summaryValue("mp_mismatch_warning_reason"));
    data.mpPackageIdentityMismatchAlert = formatOwnerFacingStatusReason(summaryValue("mp_identity_mismatch_alert"));
    data.mpPackagePreviousHostAvailable = compactBoolText(summaryValue("mp_previous_host_available"));
    data.mpPackagePreviousHostAvailableKnown = compactBoolText(summaryValue("mp_previous_host_available_known"));
    data.mpPackageHostNextStep = formatOwnerFacingStatusReason(summaryValue("mp_host_next_step"));
    data.mpPackageClientNextStep = formatOwnerFacingStatusReason(summaryValue("mp_client_next_step"));
    data.mpPackageHandoffRecoveryHint = formatOwnerFacingStatusReason(summaryValue("mp_handoff_recovery_hint"));
    data.mpPackageHostRotationSyncState = formatOwnerFacingStatusValue(summaryValue("mp_host_rotation_sync_state"));
    if (data.mpPackageHostRotationSyncState.empty()) {
        data.mpPackageHostRotationSyncState = formatOwnerFacingStatusValue("disabled_not_implemented");
    }
    data.mpPackageHostRotationSyncReason = formatOwnerFacingStatusReason(summaryValue("mp_host_rotation_sync_reason"));
    if (data.mpPackageHostRotationSyncReason.empty()) {
        data.mpPackageHostRotationSyncReason =
            formatOwnerFacingStatusReason(
                "host-owned automatic handoff sync for host rotation is not implemented; manual MP package export/import remains the fallback");
    }
    data.mpPackageHostRotationSyncNextStep =
        formatOwnerFacingStatusReason(summaryValue("mp_host_rotation_sync_next_step"));
    if (data.mpPackageHostRotationSyncNextStep.empty()) {
        data.mpPackageHostRotationSyncNextStep =
            formatOwnerFacingStatusReason(
                "Use the current MP package ZIP, strict verify/import, and manual host rotation handoff until signed/encrypted friend transport is implemented.");
    }
    data.mpPackageHostReadiness = formatOwnerFacingStatusValue(summaryValue("mp_host_readiness"));
    data.mpPackageClientReadinessGate = formatOwnerFacingStatusValue(summaryValue("mp_client_readiness_gate"));
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
    text += L"\r\nLLM: ";
    text += data.modelState.empty() ? kStatusEmptyValue : data.modelState;
    if (!data.modelInstallGuidance.empty()) {
        text += L"\r\nLLM instalace: ";
        text += data.modelInstallGuidance;
    }
    text += L"\r\nSNC pratele: ";
    text += data.friendTrustStoreState.empty() ? kStatusEmptyValue : data.friendTrustStoreState;
    text += L"\r\nSNC pratele duvod: ";
    text += data.friendTrustStoreReason.empty() ? kStatusEmptyValue : data.friendTrustStoreReason;
    text += L"\r\nSNC pratele cesta: ";
    text += data.friendTrustStorePath.empty() ? kStatusEmptyValue : data.friendTrustStorePath;
    text += L"\r\nSNC pratele controls: ";
    text += data.friendTrustStoreControlsState.empty() ? kStatusEmptyValue : data.friendTrustStoreControlsState;
    text += L"\r\nSNC pratele controls duvod: ";
    text += data.friendTrustStoreControlsReason.empty() ? kStatusEmptyValue : data.friendTrustStoreControlsReason;
    text += L"\r\nSNC pratele controls dalsi krok: ";
    text += data.friendTrustStoreControlsNextStep.empty() ? kStatusEmptyValue : data.friendTrustStoreControlsNextStep;
    text += L"\r\nSNC pratele update template: ";
    text += data.friendTrustStoreUpdateCommandTemplate.empty() ? kStatusEmptyValue : data.friendTrustStoreUpdateCommandTemplate;
    text += L"\r\nSNC pairing guide: ";
    text += buildFriendPairingGuideText(data);
    text += L"\r\nSNC pairing template: ";
    text += data.friendPairingCommandTemplate.empty() ? kStatusEmptyValue : data.friendPairingCommandTemplate;
    text += localizedTextWide({
        L"\r\nSNC MP sync envelope: pouze ru\u010Dn\u00ED metadata; automatick\u00FD sync, sta\u017Een\u00ED a staging z\u016Fst\u00E1vaj\u00ED vypnut\u00E9.",
        L"\r\nSNC MP sync envelope: manual metadata only; automatic sync, download, and staging stay disabled."});
    text += L"\r\nSNC MP sync envelope template: ";
    text += data.friendMpSyncEnvelopeCommandTemplate.empty() ? kStatusEmptyValue : data.friendMpSyncEnvelopeCommandTemplate;
    text += localizedTextWide({
        L"\r\nSNC MP sync inbox plan: bezpe\u010Dn\u011B uzav\u0159en\u00FD stavov\u00FD pl\u00E1n; bez sta\u017Een\u00ED, de\u0161ifrov\u00E1n\u00ED, stagingu nebo aplikace.",
        L"\r\nSNC MP sync inbox plan: fail-closed status plan; no download, decrypt, staging, or apply."});
    text += L"\r\nSNC MP sync inbox plan template: ";
    text += data.friendMpSyncInboxPlanCommandTemplate.empty() ? kStatusEmptyValue : data.friendMpSyncInboxPlanCommandTemplate;
    text += L"\r\nSNC MP sync inbox plan stav: ";
    text += data.friendMpSyncInboxPlanState.empty() ? kStatusEmptyValue : data.friendMpSyncInboxPlanState;
    text += L"\r\nSNC MP sync inbox plan duvod: ";
    text += data.friendMpSyncInboxPlanReason.empty() ? kStatusEmptyValue : data.friendMpSyncInboxPlanReason;
    text += L"\r\nSNC MP sync inbox plan automatic download: ";
    text += data.friendMpSyncInboxAutomaticDownloadEnabled ? L"true" : L"false";
    text += L"\r\nSNC MP sync inbox plan package staging: ";
    text += data.friendMpSyncInboxPackageStagingAllowed ? L"true" : L"false";
    text += localizedTextWide({
        L"\r\nSNC MP sync outbox plan: bezpe\u010Dn\u011B uzav\u0159en\u00FD stavov\u00FD pl\u00E1n; bez uploadu, odesl\u00E1n\u00ED, sta\u017Een\u00ED, de\u0161ifrov\u00E1n\u00ED, stagingu nebo aplikace.",
        L"\r\nSNC MP sync outbox plan: fail-closed status plan; no upload, send, download, decrypt, staging, or apply."});
    text += L"\r\nSNC MP sync outbox plan template: ";
    text += data.friendMpSyncOutboxPlanCommandTemplate.empty() ? kStatusEmptyValue : data.friendMpSyncOutboxPlanCommandTemplate;
    text += L"\r\nSNC MP sync transport: ";
    text += data.friendMpSyncTransportState.empty() ? kStatusEmptyValue : data.friendMpSyncTransportState;
    text += L"\r\nSNC MP sync transport duvod: ";
    text += data.friendMpSyncTransportReason.empty() ? kStatusEmptyValue : data.friendMpSyncTransportReason;
    text += L"\r\nSNC MP sync transport dalsi krok: ";
    text += data.friendMpSyncTransportNextStep.empty() ? kStatusEmptyValue : data.friendMpSyncTransportNextStep;
    text += L"\r\nSNC MP sync preflight: ";
    text += data.friendMpSyncPreflightChecklist.empty() ? kStatusEmptyValue : data.friendMpSyncPreflightChecklist;
    text += L"\r\nMP handoff sync: ";
    text += data.mpPackageHostRotationSyncState.empty() ? kStatusEmptyValue : data.mpPackageHostRotationSyncState;
    text += L"\r\nMP handoff sync duvod: ";
    text += data.mpPackageHostRotationSyncReason.empty() ? kStatusEmptyValue : data.mpPackageHostRotationSyncReason;
    text += L"\r\nMP handoff sync dalsi krok: ";
    text += data.mpPackageHostRotationSyncNextStep.empty() ? kStatusEmptyValue : data.mpPackageHostRotationSyncNextStep;
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

std::wstring buildStatusPageDetailsText(const StatusPageId page, const StatusDashboardData& data)
{
    auto addLine = [](std::wstring& text, const wchar_t* label, const std::wstring& valueText) {
        text += label;
        text += valueText.empty() ? kStatusEmptyValue : valueText;
        text += L"\r\n";
    };

    std::wstring text;
    switch (page) {
    case StatusPageId::Overview:
        addLine(text, L"Nejblizsi akce: ", data.nextAction);
        addLine(text, L"Proc: ", data.nextReason);
        addLine(text, L"Hint: ", data.nextHint);
        addLine(text, L"Playbook: ", data.nextPlaybook);
        addLine(text, L"Cil: ", data.nextActionPath);
        break;
    case StatusPageId::Gameplay:
        addLine(text, L"Stellaris: ", data.liveStellaris);
        addLine(text, L"Session: ", data.liveSession);
        addLine(text, L"Crash recovery: ", data.crashRecoveryState);
        addLine(text, L"Crash duvod: ", data.crashRecoveryReason);
        addLine(text, L"Crash cesta: ", data.crashRecoveryPath);
        addLine(text, L"Gameplay report: ", makeRelativeDisplay(g_gameplayAcceptanceReportPath));
        break;
    case StatusPageId::Archive:
        addLine(text, L"Archiv cesta: ", data.archivePath);
        addLine(text, L"Overeno: ", data.archiveVerified);
        addLine(text, L"Kopie: ", data.archiveCopied);
        addLine(text, L"Preskoceno: ", data.archiveSkipped);
        addLine(text, L"Souhrn: ", data.nextPlaybook);
        break;
    case StatusPageId::Overlay:
        addLine(text, L"Staging: ", data.overlayStaging);
        addLine(text, L"Publish gate: ", data.overlayGate);
        addLine(text, L"Aktivni overlay: ", data.overlayActive);
        addLine(text, L"MP balicek: ", data.mpPackageRefreshState);
        addLine(text, L"MP balicek duvod: ", data.mpPackageRefreshReason);
        addLine(text, L"MP slozka: ", data.mpPackageDirectory);
        addLine(text, L"MP zip: ", data.mpPackageZipPath);
        addLine(text, L"MP manifest hash: ", data.mpPackageManifestHash);
        break;
    case StatusPageId::Multiplayer:
        addLine(text, L"Trust store: ", data.friendTrustStoreState);
        addLine(text, L"Trust store duvod: ", data.friendTrustStoreReason);
        addLine(text, L"Trust store cesta: ", data.friendTrustStorePath);
        addLine(text, L"Sync transport: ", data.friendMpSyncTransportState);
        addLine(text, L"Sync transport duvod: ", data.friendMpSyncTransportReason);
        addLine(text, L"Sync dalsi krok: ", data.friendMpSyncTransportNextStep);
        addLine(text, L"Preflight: ", data.friendMpSyncPreflightChecklist);
        addLine(text, L"MP handoff: ", data.mpPackageHandoffStatus);
        addLine(text, L"MP handoff sync: ", data.mpPackageHostRotationSyncState);
        addLine(text, L"MP handoff sync duvod: ", data.mpPackageHostRotationSyncReason);
        addLine(text, L"MP handoff sync dalsi krok: ", data.mpPackageHostRotationSyncNextStep);
        addLine(text, L"MP host readiness: ", data.mpPackageHostReadiness);
        addLine(text, L"MP client gate: ", data.mpPackageClientReadinessGate);
        break;
    case StatusPageId::Llm:
        addLine(text, L"Stav modelu: ", data.modelState);
        addLine(text, L"Runtime: ", data.modelRuntime);
        addLine(text, L"Doporučení modelu: ", data.modelRecommendation);
        addLine(text, L"Režim modelu: ", data.modelMode);
        addLine(text, L"Návod k instalaci: ", data.modelInstallGuidance);
        addLine(text, L"Cesta modelu: ", data.modelStatePath);
        if (!data.modelPrepareCommand.empty()) {
            addLine(text, L"Přípravný příkaz: ", data.modelPrepareCommand);
        }
        break;
    case StatusPageId::Maintenance:
        addLine(text, L"Start s Windows: ", data.liveStartup);
        addLine(text, L"Support report: ", data.supportReportState);
        addLine(text, L"Report cesta: ", data.supportReportPath);
        addLine(text, L"Human control guard: ", data.humanControlGuardState);
        addLine(text, L"Crash recovery: ", data.crashRecoveryState);
        addLine(text, L"MP zip stav: ", data.mpPackageZipState);
        addLine(text, L"MP mismatch: ", data.mpPackageMismatchWarningState);
        addLine(text, L"MP mismatch duvod: ", data.mpPackageMismatchWarningReason);
        break;
    case StatusPageId::Count:
        return buildDashboardBottomText(data);
    }

    if (text.empty()) {
        return buildDashboardBottomText(data);
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
    if (!data.modelInstallGuidance.empty()) {
        text += L"LLM instalace: ";
        text += data.modelInstallGuidance;
        text += L"\n";
    }
    text += L"Dal\u0161\u00ED krok: " + data.nextAction + L"\n";
    text += L"Hint: " + data.nextHint + L"\n";
    text += L"Support report: " + data.supportReportState + L"\n";
    text += L"Crash recovery: " + data.crashRecoveryState;
    text += L"\n";
    text += buildFriendPairingGuideText(data);
    text += L"\nSNC pairing template: ";
    text += data.friendPairingCommandTemplate.empty() ? kStatusEmptyValue : data.friendPairingCommandTemplate;
    text += localizedTextWide({
        L"\nSNC MP sync envelope: pouze ru\u010Dn\u00ED metadata; automatick\u00FD sync, sta\u017Een\u00ED a staging z\u016Fst\u00E1vaj\u00ED vypnut\u00E9.",
        L"\nSNC MP sync envelope: manual metadata only; automatic sync, download, and staging stay disabled."});
    text += L"\nSNC MP sync envelope template: ";
    text += data.friendMpSyncEnvelopeCommandTemplate.empty() ? kStatusEmptyValue : data.friendMpSyncEnvelopeCommandTemplate;
    text += localizedTextWide({
        L"\nSNC MP sync inbox plan: bezpe\u010Dn\u011B uzav\u0159en\u00FD stavov\u00FD pl\u00E1n; bez sta\u017Een\u00ED, de\u0161ifrov\u00E1n\u00ED, stagingu nebo aplikace.",
        L"\nSNC MP sync inbox plan: fail-closed status plan; no download, decrypt, staging, or apply."});
    text += L"\nSNC MP sync inbox plan template: ";
    text += data.friendMpSyncInboxPlanCommandTemplate.empty() ? kStatusEmptyValue : data.friendMpSyncInboxPlanCommandTemplate;
    text += L"\nSNC MP sync inbox plan state: ";
    text += data.friendMpSyncInboxPlanState.empty() ? kStatusEmptyValue : data.friendMpSyncInboxPlanState;
    text += L"\nSNC MP sync inbox plan reason: ";
    text += data.friendMpSyncInboxPlanReason.empty() ? kStatusEmptyValue : data.friendMpSyncInboxPlanReason;
    text += L"\nSNC MP sync inbox plan automatic download: ";
    text += data.friendMpSyncInboxAutomaticDownloadEnabled ? L"true" : L"false";
    text += L"\nSNC MP sync inbox plan package staging: ";
    text += data.friendMpSyncInboxPackageStagingAllowed ? L"true" : L"false";
    text += localizedTextWide({
        L"\nSNC MP sync outbox plan: bezpe\u010Dn\u011B uzav\u0159en\u00FD stavov\u00FD pl\u00E1n; bez uploadu, odesl\u00E1n\u00ED, sta\u017Een\u00ED, de\u0161ifrov\u00E1n\u00ED, stagingu nebo aplikace.",
        L"\nSNC MP sync outbox plan: fail-closed status plan; no upload, send, download, decrypt, staging, or apply."});
    text += L"\nSNC MP sync outbox plan template: ";
    text += data.friendMpSyncOutboxPlanCommandTemplate.empty() ? kStatusEmptyValue : data.friendMpSyncOutboxPlanCommandTemplate;
    text += L"\nSNC MP sync transport: ";
    text += data.friendMpSyncTransportState.empty() ? kStatusEmptyValue : data.friendMpSyncTransportState;
    text += L"\nSNC MP sync transport duvod: ";
    text += data.friendMpSyncTransportReason.empty() ? kStatusEmptyValue : data.friendMpSyncTransportReason;
    text += L"\nSNC MP sync transport dalsi krok: ";
    text += data.friendMpSyncTransportNextStep.empty() ? kStatusEmptyValue : data.friendMpSyncTransportNextStep;
    text += L"\nSNC MP sync preflight: ";
    text += data.friendMpSyncPreflightChecklist.empty() ? kStatusEmptyValue : data.friendMpSyncPreflightChecklist;
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
        if (!data.mpPackageHostRotationSyncState.empty()) {
            text += L"\nMP handoff sync: " + data.mpPackageHostRotationSyncState;
        }
        if (!data.mpPackageHostRotationSyncReason.empty()) {
            text += L"\nMP handoff sync duvod: " + data.mpPackageHostRotationSyncReason;
        }
        if (!data.mpPackageHostRotationSyncNextStep.empty()) {
            text += L"\nMP handoff sync dalsi krok: " + data.mpPackageHostRotationSyncNextStep;
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
        if (!data.mpPackageHostRotationSyncState.empty()) {
            text += L"\nMP handoff sync: " + data.mpPackageHostRotationSyncState;
        }
        if (!data.mpPackageHostRotationSyncReason.empty()) {
            text += L"\nMP handoff sync duvod: " + data.mpPackageHostRotationSyncReason;
        }
        if (!data.mpPackageHostRotationSyncNextStep.empty()) {
            text += L"\nMP handoff sync dalsi krok: " + data.mpPackageHostRotationSyncNextStep;
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
        g_statusBackgroundBrush = CreateSolidBrush(g_statusSkinColors.background);
    }
    if (g_statusPanelBrush == nullptr) {
        g_statusPanelBrush = CreateSolidBrush(g_statusSkinColors.panel);
    }
    if (g_statusValueBrush == nullptr) {
        g_statusValueBrush = CreateSolidBrush(g_statusSkinColors.valueBackground);
    }
    if (g_statusAccentBrush == nullptr) {
        g_statusAccentBrush = CreateSolidBrush(g_statusSkinColors.accent);
    }
    if (g_statusBorderBrush == nullptr) {
        g_statusBorderBrush = CreateSolidBrush(g_statusSkinColors.border);
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
    destroyStatusBrushes();
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
    const auto& activePage = statusPageSpec(g_statusActivePage);
    if (g_statusPageTitle != nullptr) {
        SetWindowTextW(g_statusPageTitle, localizedText(activePage.title));
    }
    if (g_statusPageHelp != nullptr) {
        SetWindowTextW(g_statusPageHelp, localizedText(activePage.helpText));
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
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::ModelInstallGuidance)], data.modelInstallGuidance);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::NextAction)], data.nextAction);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::NextReason)], data.nextReason);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::NextHint)], data.nextHint);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::SupportReportState)], data.supportReportState);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::CrashRecoveryState)], data.crashRecoveryState);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::FriendTrustStoreState)], data.friendTrustStoreState);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::FriendMpSyncTransportState)], data.friendMpSyncTransportState);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::HumanControlGuardState)], data.humanControlGuardState);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::MpPackageRefreshState)], data.mpPackageRefreshState);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::MpPackageZipState)], data.mpPackageZipState);
    setField(g_statusFieldValues[static_cast<std::size_t>(StatusFieldId::MpPackageHandoffStatus)], data.mpPackageHandoffStatus);

    if (g_statusBottomTitle != nullptr) {
        SetWindowTextW(g_statusBottomTitle, localizedText(activePage.title));
    }
    if (g_statusDetails != nullptr) {
        const auto details = buildStatusPageDetailsText(g_statusActivePage, data);
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
            data.nextActionRaw == "review_staged_overlay_and_publish_if_desired" &&
            !data.overlayPublishCommand.empty();
        EnableWindow(g_statusPublishGeneratedOverlayButton, publishReady ? TRUE : FALSE);
    }
    if (g_statusOpenNextActionPathButton != nullptr) {
        EnableWindow(g_statusOpenNextActionPathButton, dashboardPathExists(data.nextActionPath) ? TRUE : FALSE);
    }
    if (g_statusOpenCampaignLibraryPlanButton != nullptr) {
        EnableWindow(
            g_statusOpenCampaignLibraryPlanButton,
            dashboardPathExists(data.campaignLibraryPlanPath) ? TRUE : FALSE);
    }
    if (g_statusOpenCrashRecoveryStateButton != nullptr) {
        EnableWindow(
            g_statusOpenCrashRecoveryStateButton,
            dashboardPathExists(data.crashRecoveryPath) ? TRUE : FALSE);
    }
    if (g_statusOpenGameplayAcceptanceReportButton != nullptr) {
        std::error_code error;
        const bool reportReady = !g_gameplayAcceptanceReportPath.empty() &&
            std::filesystem::is_regular_file(g_gameplayAcceptanceReportPath, error) &&
            !error;
        EnableWindow(g_statusOpenGameplayAcceptanceReportButton, reportReady ? TRUE : FALSE);
    }
    if (g_statusOpenFriendTrustStoreButton != nullptr) {
        EnableWindow(
            g_statusOpenFriendTrustStoreButton,
            dashboardPathExists(data.friendTrustStorePath) ? TRUE : FALSE);
    }
    if (g_statusCopyFriendPairingButton != nullptr) {
        EnableWindow(g_statusCopyFriendPairingButton, data.friendPairingCommandTemplate.empty() ? FALSE : TRUE);
    }
    if (g_statusCopyFriendMpSyncEnvelopeButton != nullptr) {
        EnableWindow(
            g_statusCopyFriendMpSyncEnvelopeButton,
            data.friendMpSyncEnvelopeCommandTemplate.empty() ? FALSE : TRUE);
    }
    if (g_statusCopyFriendMpSyncInboxPlanButton != nullptr) {
        EnableWindow(
            g_statusCopyFriendMpSyncInboxPlanButton,
            data.friendMpSyncInboxPlanCommandTemplate.empty() ? FALSE : TRUE);
    }
    if (g_statusCopyFriendMpSyncOutboxPlanButton != nullptr) {
        EnableWindow(
            g_statusCopyFriendMpSyncOutboxPlanButton,
            data.friendMpSyncOutboxPlanCommandTemplate.empty() ? FALSE : TRUE);
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
    const int skinButtonWidth = 144;
    const int titleButtonsWidth = (kStatusTitleButtonWidth * 3) + skinButtonWidth + buttonGap;
    const int availableWidth = width - (margin * 2);

    const int headerWidth = availableWidth - titleButtonsWidth - 12;
    const int subtitleTop = top + kStatusTitleBarHeight + 12;

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
    const int skinLeft = minimizeLeft - buttonGap - skinButtonWidth;
    updateStatusCaptionButtons(hwnd);
    if (g_statusSkinButton != nullptr) {
        MoveWindow(g_statusSkinButton, skinLeft, titleButtonTop, skinButtonWidth, 24, TRUE);
    }
    if (g_statusCloseButton != nullptr) {
        MoveWindow(g_statusCloseButton, closeLeft, titleButtonTop, kStatusTitleButtonWidth, 24, TRUE);
    }
    if (g_statusMaximizeButton != nullptr) {
        MoveWindow(g_statusMaximizeButton, maximizeLeft, titleButtonTop, kStatusTitleButtonWidth, 24, TRUE);
    }
    if (g_statusMinimizeButton != nullptr) {
        MoveWindow(g_statusMinimizeButton, minimizeLeft, titleButtonTop, kStatusTitleButtonWidth, 24, TRUE);
    }

    const int navWidth = 142;
    const int navButtonHeight = 32;
    const int navTop = subtitleTop + subtitleHeight + 16;
    const int contentLeft = margin + navWidth + 18;
    const int contentWidth = width - contentLeft - margin;
    const auto& activePage = statusPageSpec(g_statusActivePage);

    for (std::size_t index = 0; index < kStatusPages.size(); ++index) {
        const auto& page = kStatusPages[index];
        if (g_statusPageButtons[index] != nullptr) {
            SetWindowTextW(g_statusPageButtons[index], localizedText(page.navLabel));
            ShowWindow(g_statusPageButtons[index], SW_SHOW);
            MoveWindow(
                g_statusPageButtons[index],
                margin,
                navTop + static_cast<int>(index) * (navButtonHeight + buttonGap),
                navWidth,
                navButtonHeight,
                TRUE);
        }
    }

    if (g_statusPageTitle != nullptr) {
        MoveWindow(g_statusPageTitle, contentLeft, navTop, contentWidth, 24, TRUE);
    }
    if (g_statusPageHelp != nullptr) {
        MoveWindow(g_statusPageHelp, contentLeft, navTop + 28, contentWidth, 42, TRUE);
    }

    for (const auto& action : kStatusActionSpecs) {
        HWND button = statusActionButton(action.id);
        if (button != nullptr) {
            ShowWindow(button, SW_HIDE);
        }
    }

    constexpr int preferredActionButtonWidth = 132;
    constexpr int minimumActionButtonWidth = 92;
    int actionButtonsPerRow = static_cast<int>(activePage.actions.size());
    if (actionButtonsPerRow < 1) {
        actionButtonsPerRow = 1;
    }
    int actionRows = 1;
    while (actionButtonsPerRow > 1 &&
           (preferredActionButtonWidth * actionButtonsPerRow) + (buttonGap * (actionButtonsPerRow - 1)) > contentWidth) {
        ++actionRows;
        actionButtonsPerRow = (static_cast<int>(activePage.actions.size()) + actionRows - 1) / actionRows;
    }
    int actionButtonWidth = preferredActionButtonWidth;
    if (actionButtonsPerRow > 0) {
        actionButtonWidth =
            (contentWidth - (buttonGap * (actionButtonsPerRow - 1))) / actionButtonsPerRow;
        if (actionButtonWidth > preferredActionButtonWidth) {
            actionButtonWidth = preferredActionButtonWidth;
        }
        if (actionButtonWidth < minimumActionButtonWidth) {
            actionButtonWidth = minimumActionButtonWidth;
        }
    }

    const int actionsTop = navTop + 78;
    for (std::size_t index = 0; index < activePage.actions.size(); ++index) {
        const UINT actionId = activePage.actions[index];
        HWND button = statusActionButton(actionId);
        if (button == nullptr) {
            continue;
        }
        const auto label = statusActionLabel(actionId);
        SetWindowTextW(button, label.c_str());
        ShowWindow(button, SW_SHOW);
        const int row = static_cast<int>(index) / actionButtonsPerRow;
        const int column = static_cast<int>(index) % actionButtonsPerRow;
        MoveWindow(
            button,
            contentLeft + column * (actionButtonWidth + buttonGap),
            actionsTop + row * (buttonHeight + buttonGap),
            actionButtonWidth,
            buttonHeight,
            TRUE);
    }

    const int actionRowsHeight = activePage.actions.empty()
        ? 0
        : (buttonHeight * actionRows) + (buttonGap * (actionRows - 1));
    const int gridTop = actionsTop + actionRowsHeight + 16;

    int bottomHeight = (height / 4) + 80;
    if (bottomHeight < 170) {
        bottomHeight = 170;
    }
    if (bottomHeight > 230) {
        bottomHeight = 230;
    }

    const int bottomTop = height - margin - bottomHeight;
    int gridBottom = bottomTop - 14;
    if (gridBottom <= gridTop) {
        gridBottom = gridTop + 220;
    }

    for (std::size_t sectionIndex = 0; sectionIndex < kStatusSections.size(); ++sectionIndex) {
        if (g_statusSectionTitles[sectionIndex] != nullptr) {
            ShowWindow(g_statusSectionTitles[sectionIndex], SW_HIDE);
        }
        for (const auto field : kStatusSections[sectionIndex].fields) {
            const std::size_t fieldSlot = static_cast<std::size_t>(field);
            if (g_statusFieldLabels[fieldSlot] != nullptr) {
                ShowWindow(g_statusFieldLabels[fieldSlot], SW_HIDE);
            }
            if (g_statusFieldValues[fieldSlot] != nullptr) {
                ShowWindow(g_statusFieldValues[fieldSlot], SW_HIDE);
            }
            g_statusFieldScrollbars[fieldSlot] = {};
        }
    }

    const int cardGap = 14;
    const int visibleSectionCount = static_cast<int>(activePage.sections.size());
    const int columns = visibleSectionCount <= 1 ? 1 : 2;
    const int rows = visibleSectionCount <= 0 ? 1 : (visibleSectionCount + columns - 1) / columns;
    int cardWidth = columns > 1
        ? (contentWidth - (cardGap * (columns - 1))) / columns
        : contentWidth;
    if (cardWidth < 280) {
        cardWidth = 280;
    }
    int cardHeight = rows > 0
        ? (gridBottom - gridTop - (cardGap * (rows - 1))) / rows
        : gridBottom - gridTop;
    if (cardHeight < 118) {
        cardHeight = 118;
    }

    int labelWidth = (cardWidth * 30) / 100;
    if (labelWidth < 84) {
        labelWidth = 84;
    }
    if (labelWidth > 124) {
        labelWidth = 124;
    }
    int valueWidth = cardWidth - labelWidth - 12;
    if (valueWidth < 130) {
        valueWidth = 130;
    }

    for (std::size_t sectionIndex = 0; sectionIndex < kStatusSections.size(); ++sectionIndex) {
        if (std::find(activePage.sections.begin(), activePage.sections.end(), sectionIndex) == activePage.sections.end()) {
            continue;
        }
        const auto visibleIt = std::find(activePage.sections.begin(), activePage.sections.end(), sectionIndex);
        const int visibleIndex = static_cast<int>(std::distance(activePage.sections.begin(), visibleIt));
        const int column = visibleIndex % columns;
        const int row = visibleIndex / columns;
        const int cardLeft = contentLeft + column * (cardWidth + cardGap);
        const int cardTop = gridTop + row * (cardHeight + cardGap);

        if (g_statusSectionTitles[sectionIndex] != nullptr) {
            ShowWindow(g_statusSectionTitles[sectionIndex], SW_SHOW);
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
                ShowWindow(g_statusFieldLabels[fieldSlot], SW_SHOW);
                MoveWindow(
                    g_statusFieldLabels[fieldSlot],
                    cardLeft,
                    currentFieldTop + 4,
                    labelWidth - 4,
                    18,
                    TRUE);
            }
            if (g_statusFieldValues[fieldSlot] != nullptr) {
                ShowWindow(g_statusFieldValues[fieldSlot], SW_SHOW);
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
        MoveWindow(g_statusBottomTitle, contentLeft, bottomTop, contentWidth, 20, TRUE);
    }
    if (g_statusDetails != nullptr) {
        const int detailsTop = bottomTop + 24;
        const int detailsHeight = height - detailsTop - margin;
        int detailsWidth = contentWidth;
        if (contentWidth > (scrollbarReserveWidth + 80)) {
            detailsWidth -= scrollbarReserveWidth;
        }
        MoveWindow(g_statusDetails, contentLeft, detailsTop, detailsWidth, detailsHeight, TRUE);
        const RECT detailsTrackRect = {
            contentLeft + contentWidth - kStatusScrollbarTrackWidth,
            detailsTop,
            contentLeft + contentWidth,
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
        ensureStatusTooltip(hwnd);

        g_statusHeader = createStatusStatic(hwnd, g_statusTitle.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
        g_statusSubtitle = createStatusStatic(hwnd, L"", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
        g_statusPageTitle = createStatusStatic(hwnd, L"", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
        g_statusPageHelp = createStatusStatic(hwnd, L"", WS_CHILD | WS_VISIBLE | SS_LEFT);
        for (std::size_t index = 0; index < kStatusPages.size(); ++index) {
            g_statusPageButtons[index] =
                createStatusButton(hwnd, kStatusPages[index].commandId, localizedText(kStatusPages[index].navLabel));
        }
        g_statusRefreshButton = createStatusButton(hwnd, ID_STATUS_REFRESH, statusActionLabel(ID_STATUS_REFRESH).c_str());
        g_statusCopyButton = createStatusButton(hwnd, ID_STATUS_COPY, statusActionLabel(ID_STATUS_COPY).c_str());
        g_statusOpenArchiveButton = createStatusButton(hwnd, ID_STATUS_OPEN_ARCHIVE, statusActionLabel(ID_STATUS_OPEN_ARCHIVE).c_str());
        g_statusOpenBriefButton = createStatusButton(hwnd, ID_STATUS_OPEN_BRIEF, statusActionLabel(ID_STATUS_OPEN_BRIEF).c_str());
        g_statusOpenMpPackageButton = createStatusButton(hwnd, ID_STATUS_OPEN_MP_PACKAGE, statusActionLabel(ID_STATUS_OPEN_MP_PACKAGE).c_str());
        g_statusPublishGeneratedOverlayButton =
            createStatusButton(hwnd, ID_STATUS_PUBLISH_GENERATED_OVERLAY, statusActionLabel(ID_STATUS_PUBLISH_GENERATED_OVERLAY).c_str());
        g_statusOpenNextActionPathButton =
            createStatusButton(hwnd, ID_STATUS_OPEN_NEXT_ACTION_PATH, statusActionLabel(ID_STATUS_OPEN_NEXT_ACTION_PATH).c_str());
        g_statusOpenCampaignLibraryPlanButton =
            createStatusButton(hwnd, ID_STATUS_OPEN_CAMPAIGN_LIBRARY_PLAN, statusActionLabel(ID_STATUS_OPEN_CAMPAIGN_LIBRARY_PLAN).c_str());
        g_statusOpenCrashRecoveryStateButton =
            createStatusButton(hwnd, ID_STATUS_OPEN_CRASH_RECOVERY_STATE, statusActionLabel(ID_STATUS_OPEN_CRASH_RECOVERY_STATE).c_str());
        g_statusOpenGameplayAcceptanceReportButton =
            createStatusButton(hwnd, ID_STATUS_OPEN_GAMEPLAY_ACCEPTANCE_REPORT, statusActionLabel(ID_STATUS_OPEN_GAMEPLAY_ACCEPTANCE_REPORT).c_str());
        g_statusOpenFriendTrustStoreButton =
            createStatusButton(hwnd, ID_STATUS_OPEN_FRIEND_TRUST_STORE, statusActionLabel(ID_STATUS_OPEN_FRIEND_TRUST_STORE).c_str());
        g_statusCopyFriendPairingButton =
            createStatusButton(hwnd, ID_STATUS_COPY_FRIEND_PAIRING, statusActionLabel(ID_STATUS_COPY_FRIEND_PAIRING).c_str());
        g_statusCopyFriendMpSyncEnvelopeButton =
            createStatusButton(hwnd, ID_STATUS_COPY_FRIEND_MP_SYNC_ENVELOPE, statusActionLabel(ID_STATUS_COPY_FRIEND_MP_SYNC_ENVELOPE).c_str());
        g_statusCopyFriendMpSyncInboxPlanButton =
            createStatusButton(hwnd, ID_STATUS_COPY_FRIEND_MP_SYNC_INBOX_PLAN, statusActionLabel(ID_STATUS_COPY_FRIEND_MP_SYNC_INBOX_PLAN).c_str());
        g_statusCopyFriendMpSyncOutboxPlanButton =
            createStatusButton(hwnd, ID_STATUS_COPY_FRIEND_MP_SYNC_OUTBOX_PLAN, statusActionLabel(ID_STATUS_COPY_FRIEND_MP_SYNC_OUTBOX_PLAN).c_str());
        g_statusExportMpPackageButton = createStatusButton(hwnd, ID_STATUS_EXPORT_MP_PACKAGE, statusActionLabel(ID_STATUS_EXPORT_MP_PACKAGE).c_str());
        g_statusMpImportHandoffButton = createStatusButton(hwnd, ID_STATUS_MP_IMPORT_HANDOFF, statusActionLabel(ID_STATUS_MP_IMPORT_HANDOFF).c_str());
        g_statusCopyMpVerifyButton = createStatusButton(hwnd, ID_STATUS_COPY_MP_VERIFY, statusActionLabel(ID_STATUS_COPY_MP_VERIFY).c_str());
        g_statusCopyMpImportButton = createStatusButton(hwnd, ID_STATUS_COPY_MP_IMPORT, statusActionLabel(ID_STATUS_COPY_MP_IMPORT).c_str());
        g_statusCopyMpStrictVerifyButton = createStatusButton(hwnd, ID_STATUS_COPY_MP_STRICT_VERIFY, statusActionLabel(ID_STATUS_COPY_MP_STRICT_VERIFY).c_str());
        g_statusCopyMpStrictImportButton = createStatusButton(hwnd, ID_STATUS_COPY_MP_STRICT_IMPORT, statusActionLabel(ID_STATUS_COPY_MP_STRICT_IMPORT).c_str());
        g_statusCopyLlmPrepareButton = createStatusButton(hwnd, ID_STATUS_COPY_LLM_PREPARE, statusActionLabel(ID_STATUS_COPY_LLM_PREPARE).c_str());
        g_statusSupportReportButton = createStatusButton(hwnd, ID_STATUS_SUPPORT_REPORT, statusActionLabel(ID_STATUS_SUPPORT_REPORT).c_str());
        g_statusToggleStartupButton = createStatusButton(hwnd, ID_STATUS_TOGGLE_STARTUP, statusActionLabel(ID_STATUS_TOGGLE_STARTUP).c_str());
        g_statusSkinButton = createStatusButton(hwnd, ID_STATUS_SWITCH_SKIN, statusSkinButtonLabel().c_str());
        g_statusCloseButton = createStatusButton(hwnd, ID_STATUS_CLOSE, L"X");
        g_statusMaximizeButton = createStatusButton(hwnd, ID_STATUS_MAXIMIZE, L"[ ]");
        g_statusMinimizeButton = createStatusButton(hwnd, ID_STATUS_MINIMIZE, L"-");
        g_statusBottomTitle =
            createStatusStatic(hwnd, localizedText({L"Dal\u0161\u00ED krok", L"Next Step"}), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
        g_statusDetails = CreateWindowExW(
            0,
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
            g_statusSectionTitles[index] =
                createStatusStatic(hwnd, localizedText(kStatusSections[index].title), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP);
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
        setWindowFont(g_statusPageTitle, g_statusSectionFont);
        setWindowFont(g_statusPageHelp, g_statusSubtitleFont);
        for (const auto button : g_statusPageButtons) {
            setWindowFont(button, g_statusFieldFont);
        }
        setWindowFont(g_statusRefreshButton, g_statusFieldFont);
        setWindowFont(g_statusCopyButton, g_statusFieldFont);
        setWindowFont(g_statusOpenArchiveButton, g_statusFieldFont);
        setWindowFont(g_statusOpenBriefButton, g_statusFieldFont);
        setWindowFont(g_statusOpenMpPackageButton, g_statusFieldFont);
        setWindowFont(g_statusPublishGeneratedOverlayButton, g_statusFieldFont);
        setWindowFont(g_statusOpenNextActionPathButton, g_statusFieldFont);
        setWindowFont(g_statusOpenCampaignLibraryPlanButton, g_statusFieldFont);
        setWindowFont(g_statusOpenCrashRecoveryStateButton, g_statusFieldFont);
        setWindowFont(g_statusOpenGameplayAcceptanceReportButton, g_statusFieldFont);
        setWindowFont(g_statusOpenFriendTrustStoreButton, g_statusFieldFont);
        setWindowFont(g_statusCopyFriendPairingButton, g_statusFieldFont);
        setWindowFont(g_statusCopyFriendMpSyncEnvelopeButton, g_statusFieldFont);
        setWindowFont(g_statusCopyFriendMpSyncInboxPlanButton, g_statusFieldFont);
        setWindowFont(g_statusCopyFriendMpSyncOutboxPlanButton, g_statusFieldFont);
        setWindowFont(g_statusExportMpPackageButton, g_statusFieldFont);
        setWindowFont(g_statusMpImportHandoffButton, g_statusFieldFont);
        setWindowFont(g_statusCopyMpVerifyButton, g_statusFieldFont);
        setWindowFont(g_statusCopyMpImportButton, g_statusFieldFont);
        setWindowFont(g_statusCopyMpStrictVerifyButton, g_statusFieldFont);
        setWindowFont(g_statusCopyMpStrictImportButton, g_statusFieldFont);
        setWindowFont(g_statusCopyLlmPrepareButton, g_statusFieldFont);
        setWindowFont(g_statusSupportReportButton, g_statusFieldFont);
        setWindowFont(g_statusToggleStartupButton, g_statusFieldFont);
        setWindowFont(g_statusSkinButton, g_statusFieldFont);
        setWindowFont(g_statusCloseButton, g_statusFieldFont);
        setWindowFont(g_statusMaximizeButton, g_statusFieldFont);
        setWindowFont(g_statusMinimizeButton, g_statusFieldFont);
        setWindowFont(g_statusBottomTitle, g_statusSectionFont);
        setWindowFont(g_statusDetails, g_statusDetailsFont);
        addStatusTooltip(
            g_statusPageHelp,
            localizedText({
                L"Kr\u00E1tk\u00E9 vysv\u011Btlen\u00ED aktivn\u00ED str\u00E1nky: co sem pat\u0159\u00ED a kdy se sem d\u00EDvat.",
                L"Short explanation of the active page: what belongs here and when to check it."}));
        for (std::size_t index = 0; index < kStatusPages.size(); ++index) {
            addStatusTooltip(g_statusPageButtons[index], localizedText(kStatusPages[index].helpText));
        }
        for (const auto& action : kStatusActionSpecs) {
            addStatusTooltip(statusActionButton(action.id), localizedText(action.tooltip));
        }
        addStatusTooltip(
            g_statusSkinButton,
            localizedText({
                L"P\u0159epne vizu\u00E1ln\u00ED skin SNC okna. Volba se ulo\u017E\u00ED do lok\u00E1ln\u00EDho UI stavu.",
                L"Switches the visual skin for the SNC window. The choice is saved in local UI state."}));
        for (std::size_t index = 0; index < kStatusSections.size(); ++index) {
            addStatusTooltip(g_statusSectionTitles[index], localizedText(kStatusSections[index].title));
            for (const auto field : kStatusSections[index].fields) {
                const std::size_t slot = static_cast<std::size_t>(field);
                addStatusTooltip(g_statusFieldLabels[slot], statusFieldTooltip(field));
                addStatusTooltip(g_statusFieldValues[slot], statusFieldTooltip(field));
            }
        }
        addStatusTooltip(
            g_statusBottomTitle,
            localizedText({
                L"Detailn\u011Bj\u0161\u00ED vysv\u011Btlen\u00ED aktivn\u00ED str\u00E1nky a souvisej\u00EDc\u00EDch cest/p\u0159\u00EDkaz\u016F.",
                L"More detailed explanation of the active page and related paths/commands."}));
        addStatusTooltip(
            g_statusDetails,
            localizedText({
                L"Del\u0161\u00ED kontext k aktivn\u00ED str\u00E1nce. Kdy\u017E nev\u00ED\u0161, pro\u010D tla\u010D\u00EDtko existuje, za\u010Dni tady.",
                L"Longer context for the active page. If a button is unclear, start here."}));

        applyWindowIcons(hwnd);
        SetTimer(hwnd, kStatusScrollSyncTimerId, 120, nullptr);
        layoutStatusWindow(hwnd);
        refreshStatusWindowContent();
        return 0;
    }
    case WM_SIZE:
        layoutStatusWindow(hwnd);
        updateStatusCaptionButtons(hwnd);
        if (g_statusWindowCanPersistPlacement && wParam != SIZE_MINIMIZED) {
            captureStatusUiStateFromWindow(hwnd);
            savePersistedStatusUiState();
        }
        return 0;
    case WM_EXITSIZEMOVE:
        if (g_statusWindowCanPersistPlacement) {
            captureStatusUiStateFromWindow(hwnd);
            savePersistedStatusUiState();
        }
        return 0;
    case WM_COMMAND:
        if (const auto* page = statusPageSpecByCommand(LOWORD(wParam)); page != nullptr) {
            setStatusActivePage(hwnd, page->id);
            return 0;
        }
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
        case ID_STATUS_OPEN_CAMPAIGN_LIBRARY_PLAN:
            openCampaignLibraryPlan(hwnd);
            return 0;
        case ID_STATUS_OPEN_CRASH_RECOVERY_STATE:
            openCrashRecoveryStatePath(hwnd);
            return 0;
        case ID_STATUS_OPEN_GAMEPLAY_ACCEPTANCE_REPORT:
            openGameplayAcceptanceReport(hwnd);
            return 0;
        case ID_STATUS_OPEN_FRIEND_TRUST_STORE:
            openFriendTrustStore(hwnd);
            return 0;
        case ID_STATUS_COPY_FRIEND_PAIRING:
        {
            const auto data = loadStatusDashboardData();
            const auto guide = buildFriendPairingGuideText(data);
            if (guide.empty() || !copyTextToClipboard(hwnd, guide)) {
                MessageBeep(MB_ICONWARNING);
                return 0;
            }
            MessageBoxW(
                hwnd,
                L"SNC friend-pairing guide a CLI sablona jsou zkopirovane do schranky.\n\n"
                L"Dopln lokalni node id, zobrazovaci jmeno, verejne klice, fingerprint a casy. "
                L"Import stale ponecha auto-sync vypnuty, dokud nebude hotovy duveryhodny transport.",
                L"Strategic Nexus Companion",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case ID_STATUS_COPY_FRIEND_MP_SYNC_ENVELOPE:
        {
            const auto data = loadStatusDashboardData();
            std::wstring text;
            text += L"SNC friend MP sync envelope\r\n";
            text += localizedTextWide({
                L"Pouze ru\u010Dn\u00ED metadata. Automatick\u00FD sync, sta\u017Een\u00ED, verify-stage ani aplikace bal\u00ED\u010Dku nejsou povolen\u00E9.\r\n\r\n",
                L"Manual metadata only. Automatic sync, download, verify-stage, and package apply are disabled.\r\n\r\n"});
            text += localizedTextWide({
                L"Kdy\u017E b\u011B\u017E\u00ED Stellaris, nastav stellaris_running:true; ov\u011B\u0159ovac\u00ED cesta pak z\u016Fstane bezpe\u010Dn\u011B zablokovan\u00E1 a neaplikuje gameplay soubory.\r\n\r\n",
                L"When Stellaris is running, set stellaris_running:true; the verification path then stays safely blocked and does not apply gameplay files.\r\n\r\n"});
            text += data.friendMpSyncEnvelopeCommandTemplate.empty()
                        ? utf8ToWide(buildFriendMpSyncEnvelopeCommandTemplateUtf8())
                        : data.friendMpSyncEnvelopeCommandTemplate;
            if (!copyTextToClipboard(hwnd, text)) {
                MessageBeep(MB_ICONWARNING);
                return 0;
            }
            MessageBoxW(
                hwnd,
                localizedText({
                    L"SNC friend MP sync envelope \u0161ablona je zkop\u00EDrovan\u00E1 do schr\u00E1nky.\n\n"
                    L"Je to jen ru\u010Dn\u00ED metadata re\u017Eim. Parametr stellaris_running dr\u017E\u00ED ov\u011B\u0159en\u00ED bezpe\u010Dn\u011B zablokovan\u00E9 b\u011Bhem hry.",
                    L"SNC friend MP sync envelope template was copied to the clipboard.\n\n"
                    L"This is manual metadata mode only. The stellaris_running parameter keeps verification safely blocked during gameplay."}),
                L"Strategic Nexus Companion",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case ID_STATUS_COPY_FRIEND_MP_SYNC_INBOX_PLAN:
        {
            const auto data = loadStatusDashboardData();
            std::wstring text;
            text += L"SNC friend MP sync inbox plan\r\n";
            text += localizedTextWide({
                L"Bezpe\u010Dn\u011B uzav\u0159en\u00FD stavov\u00FD pl\u00E1n. Automatick\u00E9 sta\u017Een\u00ED, de\u0161ifrov\u00E1n\u00ED, staging ani aplikace bal\u00ED\u010Dku se nespou\u0161t\u00ED.\r\n\r\n",
                L"Fail-closed status plan. Automatic download, decrypt, staging, and package apply do not run.\r\n\r\n"});
            text += localizedTextWide({
                L"Kdy\u017E je hra aktivn\u00ED, nastav stellaris_running:true; friend_auto_sync_enabled ber jen z d\u016Fv\u011Bryhodn\u00E9 friend policy.\r\n\r\n",
                L"Set stellaris_running:true while the game is active; set friend_auto_sync_enabled only from the trusted friend policy.\r\n\r\n"});
            text += data.friendMpSyncInboxPlanCommandTemplate.empty()
                        ? utf8ToWide(buildFriendMpSyncInboxPlanCommandTemplateUtf8())
                        : data.friendMpSyncInboxPlanCommandTemplate;
            if (!copyTextToClipboard(hwnd, text)) {
                MessageBeep(MB_ICONWARNING);
                return 0;
            }
            MessageBoxW(
                hwnd,
                localizedText({
                    L"SNC friend MP sync inbox-plan \u0161ablona je zkop\u00EDrovan\u00E1 do schr\u00E1nky.\n\n"
                    L"Je to jen bezpe\u010Dn\u011B uzav\u0159en\u00FD stavov\u00FD pl\u00E1n. Nestahuje, nede\u0161ifruje ani nestageuje gameplay soubory.",
                    L"SNC friend MP sync inbox-plan template was copied to the clipboard.\n\n"
                    L"This is a fail-closed status plan only. It does not download, decrypt, or stage gameplay files."}),
                L"Strategic Nexus Companion",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case ID_STATUS_COPY_FRIEND_MP_SYNC_OUTBOX_PLAN:
        {
            const auto data = loadStatusDashboardData();
            std::wstring text;
            text += L"SNC friend MP sync outbox plan\r\n";
            text += localizedTextWide({
                L"Bezpe\u010Dn\u011B uzav\u0159en\u00FD stavov\u00FD pl\u00E1n. Automatick\u00FD upload, odesl\u00E1n\u00ED, sta\u017Een\u00ED, de\u0161ifrov\u00E1n\u00ED, staging ani aplikace bal\u00ED\u010Dku se nespou\u0161t\u00ED.\r\n\r\n",
                L"Fail-closed status plan. Automatic upload, send, download, decrypt, staging, and package apply do not run.\r\n\r\n"});
            text += localizedTextWide({
                L"Kdy\u017E je hra aktivn\u00ED, nastav stellaris_running:true; friend_auto_sync_enabled ber jen z d\u016Fv\u011Bryhodn\u00E9 friend policy.\r\n\r\n",
                L"Set stellaris_running:true while the game is active; set friend_auto_sync_enabled only from the trusted friend policy.\r\n\r\n"});
            text += data.friendMpSyncOutboxPlanCommandTemplate.empty()
                        ? utf8ToWide(buildFriendMpSyncOutboxPlanCommandTemplateUtf8())
                        : data.friendMpSyncOutboxPlanCommandTemplate;
            if (!copyTextToClipboard(hwnd, text)) {
                MessageBeep(MB_ICONWARNING);
                return 0;
            }
            MessageBoxW(
                hwnd,
                localizedText({
                    L"SNC friend MP sync outbox-plan \u0161ablona je zkop\u00EDrovan\u00E1 do schr\u00E1nky.\n\n"
                    L"Je to jen bezpe\u010Dn\u011B uzav\u0159en\u00FD stavov\u00FD pl\u00E1n. Neuploaduje, nepos\u00EDl\u00E1, nestahuje ani nestageuje gameplay soubory.",
                    L"SNC friend MP sync outbox-plan template was copied to the clipboard.\n\n"
                    L"This is a fail-closed status plan only. It does not upload, send, download, or stage gameplay files."}),
                L"Strategic Nexus Companion",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }
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
        case ID_STATUS_SWITCH_SKIN:
            cycleStatusSkin(hwnd);
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
        RECT skinRect{};
        if ((g_statusCloseButton != nullptr && GetWindowRect(g_statusCloseButton, &closeRect) && PtInRect(&closeRect, point)) ||
            (g_statusMaximizeButton != nullptr && GetWindowRect(g_statusMaximizeButton, &maxRect) && PtInRect(&maxRect, point)) ||
            (g_statusMinimizeButton != nullptr && GetWindowRect(g_statusMinimizeButton, &minRect) && PtInRect(&minRect, point)) ||
            (g_statusSkinButton != nullptr && GetWindowRect(g_statusSkinButton, &skinRect) && PtInRect(&skinRect, point))) {
            return HTCLIENT;
        }

        if (y < kStatusTitleBarHeight) {
            return HTCAPTION;
        }

        return HTCLIENT;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = kStatusMinWindowWidth;
        info->ptMinTrackSize.y = kStatusMinWindowHeight;

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
        COLORREF color = g_statusSkinColors.muted;
        bool accent = false;

        if (control == g_statusHeader) {
            color = g_statusSkinColors.text;
        } else if (control == g_statusPageTitle) {
            accent = true;
        } else if (control == g_statusPageHelp) {
            color = g_statusSkinColors.muted;
        } else if (control == g_statusBottomTitle) {
            accent = true;
        } else if (control == g_statusSubtitle) {
            color = g_statusSkinColors.muted;
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
                        color = g_statusSkinColors.muted;
                        break;
                    }
                }
            }
        }

        SetTextColor(hdc, accent ? g_statusSkinColors.accent : color);
        SetBkMode(hdc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(g_statusBackgroundBrush);
    }
    case WM_CTLCOLOREDIT: {
        const HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, g_statusSkinColors.text);
        SetBkColor(hdc, g_statusSkinColors.valueBackground);
        return reinterpret_cast<LRESULT>(g_statusValueBrush != nullptr ? g_statusValueBrush : g_statusPanelBrush);
    }
    case WM_ERASEBKGND: {
        const HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(hwnd, &client);
        fillStatusGradient(hdc, client, g_statusSkinColors.backgroundTop, g_statusSkinColors.background, wdui::GradientDirection::Vertical);

        RECT titleBand = client;
        titleBand.bottom = titleBand.top + kStatusTitleBarHeight;
        fillStatusGradient(hdc, titleBand, g_statusSkinColors.titleBand, g_statusSkinColors.background, wdui::GradientDirection::Horizontal);

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
        {
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const bool hoverChanged = updateStatusScrollHover(hwnd, point);
            if (hoverChanged && !g_statusScrollHoverTracking) {
                TRACKMOUSEEVENT trackMouseEvent{};
                trackMouseEvent.cbSize = sizeof(trackMouseEvent);
                trackMouseEvent.dwFlags = TME_LEAVE;
                trackMouseEvent.hwndTrack = hwnd;
                if (TrackMouseEvent(&trackMouseEvent)) {
                    g_statusScrollHoverTracking = true;
                }
            } else if (!g_statusScrollHoverTracking) {
                TRACKMOUSEEVENT trackMouseEvent{};
                trackMouseEvent.cbSize = sizeof(trackMouseEvent);
                trackMouseEvent.dwFlags = TME_LEAVE;
                trackMouseEvent.hwndTrack = hwnd;
                if (TrackMouseEvent(&trackMouseEvent)) {
                    g_statusScrollHoverTracking = true;
                }
            }
        }
        return 0;
    case WM_MOUSELEAVE:
        g_statusScrollHoverTracking = false;
        clearStatusScrollHover(hwnd);
        return 0;
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
        g_statusScrollHoverTracking = false;
        clearStatusScrollHover(hwnd);
        syncStatusCustomScrollbars(hwnd, true);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_statusWindowCanPersistPlacement) {
            captureStatusUiStateFromWindow(hwnd);
            savePersistedStatusUiState();
        }
        g_statusWindowCanPersistPlacement = false;
        KillTimer(hwnd, kStatusScrollSyncTimerId);
        clearStatusScrollDrag();
        g_statusScrollHoverTracking = false;
        clearStatusScrollHover(hwnd);
        if (g_statusWindow == hwnd) {
            g_statusWindow = nullptr;
        }
        if (g_statusTooltip != nullptr) {
            DestroyWindow(g_statusTooltip);
            g_statusTooltip = nullptr;
        }
        g_statusHeader = nullptr;
        g_statusSubtitle = nullptr;
        g_statusPageTitle = nullptr;
        g_statusPageHelp = nullptr;
        g_statusDetails = nullptr;
        g_statusBottomTitle = nullptr;
        for (auto& pageButton : g_statusPageButtons) {
            pageButton = nullptr;
        }
        g_statusRefreshButton = nullptr;
        g_statusCopyButton = nullptr;
        g_statusOpenArchiveButton = nullptr;
        g_statusOpenBriefButton = nullptr;
        g_statusOpenMpPackageButton = nullptr;
        g_statusPublishGeneratedOverlayButton = nullptr;
        g_statusOpenNextActionPathButton = nullptr;
        g_statusOpenCampaignLibraryPlanButton = nullptr;
        g_statusOpenCrashRecoveryStateButton = nullptr;
        g_statusOpenGameplayAcceptanceReportButton = nullptr;
        g_statusOpenFriendTrustStoreButton = nullptr;
        g_statusCopyFriendPairingButton = nullptr;
        g_statusCopyFriendMpSyncEnvelopeButton = nullptr;
        g_statusCopyFriendMpSyncInboxPlanButton = nullptr;
        g_statusCopyFriendMpSyncOutboxPlanButton = nullptr;
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
        g_statusSkinButton = nullptr;
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

std::optional<int> extractJsonInt(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return std::nullopt;
    }

    try {
        return std::stoi(match[1].str());
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

bool statusWindowRectUsable(const RECT& rect)
{
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width < kStatusMinWindowWidth || height < kStatusMinWindowHeight) {
        return false;
    }
    if (width > 12000 || height > 12000) {
        return false;
    }

    RECT monitorProbe = rect;
    if (MonitorFromRect(&monitorProbe, MONITOR_DEFAULTTONULL) == nullptr) {
        return false;
    }
    return true;
}

void loadPersistedStatusUiState()
{
    g_statusUiState = {};
    g_statusUiState.normalWindowRect = {0, 0, kStatusDefaultWindowWidth, kStatusDefaultWindowHeight};
    g_statusUiState.activePage = StatusPageId::Overview;
    g_statusUiState.activeSkin = SncStatusSkinId::Starlance;

    std::string json;
    if (!strategic_nexus::common::tryReadTextFile(g_statusUiStatePath, json)) {
        g_statusActivePage = g_statusUiState.activePage;
        applyStatusSkin(g_statusUiState.activeSkin, false);
        return;
    }

    g_statusUiState.activePage =
        statusPageFromToken(strategic_nexus::common::extractJsonString(json, "active_page").value_or(""));
    g_statusUiState.activeSkin =
        statusSkinFromToken(strategic_nexus::common::extractJsonString(json, "skin").value_or(""));
    g_statusUiState.windowMaximized = extractJsonBool(json, "window_maximized").value_or(false);

    const auto left = extractJsonInt(json, "normal_left");
    const auto top = extractJsonInt(json, "normal_top");
    const auto right = extractJsonInt(json, "normal_right");
    const auto bottom = extractJsonInt(json, "normal_bottom");
    if (left.has_value() && top.has_value() && right.has_value() && bottom.has_value()) {
        const RECT rect{*left, *top, *right, *bottom};
        if (statusWindowRectUsable(rect)) {
            g_statusUiState.normalWindowRect = rect;
            g_statusUiState.hasWindowPlacement = true;
        }
    }

    g_statusActivePage = g_statusUiState.activePage;
    applyStatusSkin(g_statusUiState.activeSkin, false);
}

void captureStatusUiStateFromWindow(HWND hwnd)
{
    g_statusUiState.activePage = g_statusActivePage;
    g_statusUiState.activeSkin = g_statusSkinId;
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return;
    }

    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(hwnd, &placement) == 0) {
        return;
    }

    g_statusUiState.windowMaximized = placement.showCmd == SW_SHOWMAXIMIZED || IsZoomed(hwnd);
    if (placement.showCmd == SW_SHOWMINIMIZED || IsIconic(hwnd)) {
        return;
    }

    const RECT rect = placement.rcNormalPosition;
    if (statusWindowRectUsable(rect)) {
        g_statusUiState.normalWindowRect = rect;
        g_statusUiState.hasWindowPlacement = true;
    }
}

void savePersistedStatusUiState()
{
    if (g_statusUiStatePath.empty()) {
        return;
    }

    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"updated_at_local\": \"" << jsonEscape(formatLocalTimestamp()) << "\",\n";
    json << "  \"ui\": {\n";
    json << "    \"active_page\": \"" << jsonEscape(statusPageToken(g_statusUiState.activePage)) << "\",\n";
    json << "    \"skin\": \"" << jsonEscape(statusSkinToken(g_statusUiState.activeSkin)) << "\",\n";
    json << "    \"language_mode\": \"auto\",\n";
    json << "    \"resolved_language\": \"" << sncUiLanguageToken(g_uiLanguage) << "\"\n";
    json << "  },\n";
    json << "  \"status_window\": {\n";
    json << "    \"window_maximized\": " << (g_statusUiState.windowMaximized ? "true" : "false") << ",\n";
    json << "    \"normal_left\": " << g_statusUiState.normalWindowRect.left << ",\n";
    json << "    \"normal_top\": " << g_statusUiState.normalWindowRect.top << ",\n";
    json << "    \"normal_right\": " << g_statusUiState.normalWindowRect.right << ",\n";
    json << "    \"normal_bottom\": " << g_statusUiState.normalWindowRect.bottom << "\n";
    json << "  }\n";
    json << "}\n";

    strategic_nexus::common::writeTextFileAtomically(g_statusUiStatePath, json.str());
}

void applyPersistedStatusWindowPlacement(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || !g_statusUiState.hasWindowPlacement) {
        return;
    }
    if (!statusWindowRectUsable(g_statusUiState.normalWindowRect)) {
        return;
    }

    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    placement.showCmd = g_statusUiState.windowMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    placement.rcNormalPosition = g_statusUiState.normalWindowRect;
    SetWindowPlacement(hwnd, &placement);
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
                << ": " << ownerFacingStatusValueUtf8(readiness.empty() ? "unknown" : readiness)
                << " (" << decisionReadyEntryCount << "/" << entryPointCount << " "
                << localizedTextUtf8({L"p\u0159ipraveno", L"ready"});
        if (branchAmbiguityDetected) {
            summary << ", " << localizedTextUtf8({L"nejednozna\u010Dn\u00E9", L"ambiguous"});
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
    appendOwnerFacingStatusValueLine(output, "mp_package_refresh_state", mpPackageRefreshState);
    appendOwnerFacingStatusReasonLine(output, "mp_package_refresh_reason", mpPackageRefreshReason);
    output << "mp_overlay_package_directory: " << pathString(g_mpOverlayPackageDirectory) << "\n";
    output << "mp_overlay_package_zip_path: " << pathString(g_mpOverlayPackageZipPath) << "\n";
    appendOwnerFacingStatusValueLine(output, "mp_overlay_package_state", mpOverlayPackage.state);
    appendOwnerFacingStatusReasonLine(output, "mp_overlay_package_reason", mpOverlayPackage.reason);
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
        appendOwnerFacingStatusValueLine(output, "mp_handoff_status", mpOverlayPackage.handoffStatus);
    }
    output << "mp_previous_host_available: "
           << (mpOverlayPackage.previousHostAvailable ? "true" : "false") << "\n";
    output << "mp_previous_host_available_known: "
           << (mpOverlayPackage.previousHostAvailableKnown ? "true" : "false") << "\n";
    if (!mpOverlayPackage.readiness.empty()) {
        appendOwnerFacingStatusValueLine(output, "mp_readiness", mpOverlayPackage.readiness);
    }
    if (!mpOverlayPackage.hostReadiness.empty()) {
        appendOwnerFacingStatusValueLine(output, "mp_host_readiness", mpOverlayPackage.hostReadiness);
    }
    if (!mpOverlayPackage.clientReadinessGate.empty()) {
        appendOwnerFacingStatusValueLine(output, "mp_client_readiness_gate", mpOverlayPackage.clientReadinessGate);
    }
    if (!mpOverlayPackage.hostNextStep.empty()) {
        appendOwnerFacingStatusReasonLine(output, "mp_host_next_step", mpOverlayPackage.hostNextStep);
    }
    if (!mpOverlayPackage.clientNextStep.empty()) {
        appendOwnerFacingStatusReasonLine(output, "mp_client_next_step", mpOverlayPackage.clientNextStep);
    }
    if (!mpOverlayPackage.packageManifestHash.empty()) {
        output << "mp_package_manifest_hash: " << mpOverlayPackage.packageManifestHash << "\n";
    }
    if (!mpOverlayPackage.provenanceState.empty()) {
        appendOwnerFacingStatusValueLine(output, "mp_provenance_state", mpOverlayPackage.provenanceState);
    }
    appendOwnerFacingStatusValueLine(output, "human_control_guard_state", mpOverlayPackage.humanControlGuardState);
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
    appendOwnerFacingStatusValueLine(output, "mp_mismatch_warning_state", mpOverlayPackage.mismatchWarningState);
    appendOwnerFacingStatusReasonLine(output, "mp_mismatch_warning_reason", mpOverlayPackage.mismatchWarningReason);
    for (const auto& warningCode : mpOverlayPackage.mismatchWarningCodes) {
        output << "mp_mismatch_warning_code: " << warningCode << "\n";
    }
    if (!mpOverlayPackage.identityMismatchAlert.empty()) {
        appendOwnerFacingStatusReasonLine(output, "mp_identity_mismatch_alert", mpOverlayPackage.identityMismatchAlert);
    }
    if (!mpOverlayPackage.packageZipState.empty()) {
        appendOwnerFacingStatusValueLine(output, "mp_package_zip_state", mpOverlayPackage.packageZipState);
        output << "mp_package_zip_bytes: " << mpOverlayPackage.packageZipBytes << "\n";
    }
    if (!mpOverlayPackage.packageZipReason.empty()) {
        appendOwnerFacingStatusReasonLine(output, "mp_package_zip_reason", mpOverlayPackage.packageZipReason);
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

std::string wideToUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return std::string();
    }

    const int required =
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) {
        return std::string();
    }

    std::string output(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, output.data(), required, nullptr, nullptr);
    if (!output.empty() && output.back() == '\0') {
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
    return action == "review_memory_recovery_status" || action == "review_entry_point_ambiguity";
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
    if (companionNextAction == "review_archive_status") {
        return companionNextAction;
    }
    if (generatedOverlayPublishAllowed || generatedOverlayPublishGateCanPublish) {
        return "review_staged_overlay_and_publish_if_desired";
    }
    if (companionNextAction == "run_monthly_reactive_owner_test") {
        return companionNextAction;
    }
    if (companionNextAction == "review_local_llm_model_manager") {
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
    if (companionNextAction == "review_archive_status") {
        return companionNextActionReason;
    }
    if (generatedOverlayPublishAllowed || generatedOverlayPublishGateCanPublish) {
        return "staged_overlay_ready_owner_gate_available";
    }
    if (companionNextAction == "run_monthly_reactive_owner_test") {
        return companionNextActionReason;
    }
    if (companionNextAction == "review_local_llm_model_manager") {
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
    const std::string& publishCommand,
    const std::filesystem::path& localLlmModelStatePath)
{
    if (nextAction == "run_monthly_reactive_owner_test" && !ownerTestPlaybookPath.empty()) {
        return "open " + pathString(ownerTestPlaybookPath);
    }
    if (nextAction == "review_local_llm_model_manager" && !localLlmModelStatePath.empty()) {
        return "open " + pathString(localLlmModelStatePath);
    }
    if (nextAction == "review_archive_status" && !g_archiveRoot.empty()) {
        return "open " + pathString(g_archiveRoot);
    }
    if (nextAction == "review_staged_overlay_and_publish_if_desired" && !publishCommand.empty()) {
        return publishCommand;
    }
    if (nextAction == "review_staged_overlay_and_publish_if_desired" ||
        nextAction == "review_staged_overlay_status" ||
        nextAction == "review_dsl_draft" ||
        nextAction == "review_candidate_decision_package" ||
        nextAction == "review_archive_status" ||
        nextAction == "review_local_llm_model_manager" ||
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
        if (nextAction == "review_archive_status") {
            return "archive_root_path";
        }
        if (nextAction == "review_local_llm_model_manager") {
            return "local_llm_model_state_path";
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
    const std::filesystem::path& localLlmModelStatePath,
    const std::filesystem::path& generatedOverlayStagingStatusPath,
    const std::filesystem::path& statusPath,
    const std::filesystem::path& companionNextActionPath)
{
    if (nextAction == "run_monthly_reactive_owner_test" && !companionNextActionPath.empty()) {
        return companionNextActionPath;
    }
    if (nextAction == "review_local_llm_model_manager" && !localLlmModelStatePath.empty()) {
        return localLlmModelStatePath;
    }
    if (isCompanionRecoveryNextAction(nextAction) && !companionNextActionPath.empty()) {
        return companionNextActionPath;
    }
    if (nextAction == "review_archive_status" && !g_archiveRoot.empty()) {
        return g_archiveRoot;
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
    const std::string& postPlayPackageCampaignIdentityStateSummary,
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
    const strategic_nexus::CompanionFriendTrustStoreStatus& friendTrustStore,
    const std::string& mpPackageRefreshState,
    const std::string& mpPackageRefreshReason)
{
    std::ostringstream summary;
    summary << "Strategic Nexus Status Center\n";
    summary << "updated_at_local: " << formatLocalTimestamp() << "\n";
    appendOwnerFacingStatusPairLine(summary, "stav", state, reason);
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
    appendOwnerFacingStatusValueLine(summary, "post_play_state", postPlayState);
    if (!entryPointAnalysisPath.empty()) {
        summary << "entry_point_analysis_path: " << pathString(entryPointAnalysisPath) << "\n";
    }
    summary << "entry_point_count: " << entryPointCount << "\n";
    summary << "branch_ambiguity_detected: " << (branchAmbiguityDetected ? "true" : "false") << "\n";
    appendOwnerFacingStatusPairLine(summary, "memory_recovery", memoryRecovery.state, memoryRecovery.reason);
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
        appendOwnerFacingStatusValueLine(summary, "entry_point_readiness", entryPointReadiness);
    }
    if (!postPlayPackagePath.empty()) {
        summary << "post_play_package_path: " << pathString(postPlayPackagePath) << "\n";
    }
    if (!postPlayPackageReadiness.empty()) {
        appendOwnerFacingStatusValueLine(summary, "post_play_package_readiness", postPlayPackageReadiness);
    }
    if (!postPlayPackageCampaignIdentityStateSummary.empty()) {
        appendOwnerFacingStatusValueLine(
            summary,
            "post_play_package_campaign_identity_state_summary",
            postPlayPackageCampaignIdentityStateSummary);
    }
    if (!mpOverlayPackage.handoffStatus.empty()) {
        appendOwnerFacingStatusValueLine(summary, "mp_handoff_status", mpOverlayPackage.handoffStatus);
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
        appendOwnerFacingStatusValueLine(summary, "mp_host_readiness", mpOverlayPackage.hostReadiness);
    }
    if (!mpOverlayPackage.clientReadinessGate.empty()) {
        appendOwnerFacingStatusValueLine(summary, "mp_client_gate", mpOverlayPackage.clientReadinessGate);
    }
    if (!mpOverlayPackage.hostNextStep.empty()) {
        appendOwnerFacingStatusReasonLine(summary, "mp_host_krok", mpOverlayPackage.hostNextStep);
    }
    if (!mpOverlayPackage.clientNextStep.empty()) {
        appendOwnerFacingStatusReasonLine(summary, "mp_client_krok", mpOverlayPackage.clientNextStep);
    }
    if (!mpOverlayPackage.handoffRecoveryHint.empty()) {
        appendOwnerFacingStatusReasonLine(summary, "mp_handoff_recovery_hint", mpOverlayPackage.handoffRecoveryHint);
    }
    summary << "friend_trust_store_controls_state: "
            << ownerFacingStatusValueUtf8(
                   friendTrustStore.controlsState.empty()
                       ? std::string("not_configured")
                       : friendTrustStore.controlsState)
            << "\n";
    if (!friendTrustStore.controlsReason.empty()) {
        summary << "friend_trust_store_controls_reason: "
                << friendTrustStore.controlsReason << "\n";
    }
    if (!friendTrustStore.controlsNextStep.empty()) {
        summary << "friend_trust_store_controls_next_step: "
                << friendTrustStore.controlsNextStep << "\n";
    }
    if (!friendTrustStore.controlsCommandTemplate.empty()) {
        summary << "friend_trust_store_update_command_template: "
                << friendTrustStore.controlsCommandTemplate << "\n";
    }
    const auto friendMeshUpdate = strategic_nexus::buildFriendMeshUpdateStatus(friendTrustStore, mpOverlayPackage);
    summary << "friend_mesh_update_state: " << friendMeshUpdate.state << "\n";
    summary << "friend_mesh_update_reason: " << friendMeshUpdate.reason << "\n";
    appendOwnerFacingStatusReasonLine(summary, "friend_mesh_update_next_step", friendMeshUpdate.nextStep);
    summary << "human_control_guard_state: "
            << ownerFacingStatusValueUtf8(
                   mpOverlayPackage.humanControlGuardState.empty()
                       ? std::string("unknown")
                       : mpOverlayPackage.humanControlGuardState)
            << "\n";
    summary << "post_play_decision_ready_entry_count: " << postPlayDecisionReadyEntryCount << "\n";
    summary << "post_play_campaign_count: " << postPlayCampaignCount << "\n";
    summary << "post_play_ready_campaign_count: " << postPlayReadyCampaignCount << "\n";
    summary << "post_play_partial_campaign_count: " << postPlayPartialCampaignCount << "\n";
    summary << "post_play_blocked_campaign_count: " << postPlayBlockedCampaignCount << "\n";
    if (campaignLibraryPlanPresent) {
        summary << "campaign_library_plan_path: " << pathString(campaignLibraryPlanPath) << "\n";
        summary << "campaign_library_plan_source: " << campaignLibraryPlanSource << "\n";
        appendOwnerFacingStatusValueLine(summary, "campaign_library_plan_readiness", campaignLibraryPlanReadiness);
        appendOwnerFacingStatusReasonLine(summary, "campaign_library_plan_reason", campaignLibraryPlanReason);
        summary << "campaign_library_limit_reached: " << (campaignLibraryLimitReached ? "true" : "false") << "\n";
        summary << "campaign_library_skipped_due_to_limit_count: " << campaignLibrarySkippedDueToLimitCount << "\n";
        summary << "campaign_library_pinned_count: " << postPlayPipeline.campaignLibraryPinnedCount << "\n";
        summary << "campaign_library_pinned_missing_local_save_count: "
                << postPlayPipeline.campaignLibraryPinnedMissingLocalSaveCount << "\n";
        if (!postPlayPipeline.campaignLibraryPinPath.empty()) {
            summary << "campaign_library_pin_path: "
                    << pathString(postPlayPipeline.campaignLibraryPinPath) << "\n";
        }
        summary << "campaign_library_pin_state: " << postPlayPipeline.campaignLibraryPinState << "\n";
        summary << "campaign_library_pin_reason: " << postPlayPipeline.campaignLibraryPinReason << "\n";
        summary << "campaign_library_pin_next_step: " << postPlayPipeline.campaignLibraryPinNextStep << "\n";
        if (!postPlayPipeline.campaignLibraryPinCommandTemplate.empty()) {
            summary << "campaign_library_pin_command_template: "
                    << postPlayPipeline.campaignLibraryPinCommandTemplate << "\n";
        }
        if (campaignLibraryPlanReadiness == "needs_attention") {
            summary << "campaign_library_owner_note: campaign library plan needs attention before SNC should trust active campaign coverage\n";
        } else if (postPlayPipeline.campaignLibraryPinState == "needs_attention") {
            summary << "campaign_library_owner_note: pinned campaign exception manifest needs attention before SNC should trust pinned coverage\n";
        } else if (postPlayPipeline.campaignLibraryPinState == "degraded") {
            summary << "campaign_library_owner_note: pinned campaign exceptions are available, but some pinned campaigns are not locally playable\n";
        } else if (campaignLibraryLimitReached) {
            summary << "campaign_library_owner_note: active generated campaign library is truncated by the configured limit; raise the cap or clean local campaigns before broader coverage tests\n";
            if (postPlayPipeline.campaignLibraryPinState == "unavailable") {
                summary << "campaign_library_owner_note: user-pinned campaign exceptions are not yet available; keep the local save root present or restore it before broader coverage tests\n";
            }
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
        appendOwnerFacingStatusValueLine(summary, "decision_input_package_readiness", decisionInputPackageReadiness);
    }
    summary << "decision_input_count: " << decisionInputCount << "\n";
    if (!candidateDecisionPackagePath.empty()) {
        summary << "candidate_decision_package_path: " << pathString(candidateDecisionPackagePath) << "\n";
    }
    if (!candidateDecisionPackageReadiness.empty()) {
        appendOwnerFacingStatusValueLine(summary, "candidate_decision_package_readiness", candidateDecisionPackageReadiness);
    }
    summary << "candidate_decision_count: " << candidateDecisionCount << "\n";
    if (!dslDraftPath.empty()) {
        summary << "dsl_draft_path: " << pathString(dslDraftPath) << "\n";
    }
    if (!dslDraftReadiness.empty()) {
        appendOwnerFacingStatusValueLine(summary, "dsl_draft_readiness", dslDraftReadiness);
    }
    summary << "dsl_draft_rule_count: " << dslDraftRuleCount << "\n";
    if (!generatedOverlayStagingDirectory.empty()) {
        summary << "generated_overlay_staging_directory: " << pathString(generatedOverlayStagingDirectory) << "\n";
    }
    if (!generatedOverlayStagingStatusPath.empty()) {
        summary << "generated_overlay_staging_status_path: " << pathString(generatedOverlayStagingStatusPath) << "\n";
    }
    if (!generatedOverlayStagingReadiness.empty()) {
        appendOwnerFacingStatusValueLine(summary, "generated_overlay_staging_readiness", generatedOverlayStagingReadiness);
    }
    summary << "generated_overlay_staging_rule_count: " << generatedOverlayStagingRuleCount << "\n";
    summary << "generated_overlay_manifest_verified: " << (generatedOverlayManifestVerified ? "true" : "false") << "\n";
    summary << "generated_overlay_publish_allowed: " << (generatedOverlayPublishAllowed ? "true" : "false") << "\n";
    if (!generatedOverlayManifestHash.empty()) {
        summary << "generated_overlay_manifest_hash: " << generatedOverlayManifestHash << "\n";
    }
    summary << "generated_overlay_reactive_capability: "
            << (generatedOverlayStatus.reactivePolicyPackCapability.empty()
                    ? ownerFacingStatusValueUtf8("unknown")
                    : ownerFacingStatusValueUtf8(generatedOverlayStatus.reactivePolicyPackCapability))
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
    appendOwnerFacingStatusValueLine(summary, "generated_overlay_publish_gate_state", generatedOverlayPublishGate.state);
    appendOwnerFacingStatusReasonLine(summary, "generated_overlay_publish_gate_reason", generatedOverlayPublishGate.reason);
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
    appendOwnerFacingStatusPairLine(summary, "local_llm_model", localLlm.state, localLlm.reason);
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
    if (!localLlm.installGuidance.empty()) {
        summary << "local_llm_install_guidance: " << localLlm.installGuidance << "\n";
    }
    if (!localLlm.summary.empty()) {
        summary << "local_llm_model_manager_summary: " << localLlm.summary << "\n";
    }
    appendStartupRationaleLines(summary, lifecycle);
    appendOwnerFacingStatusValueLine(summary, "support_report_state", supportReport.state);
    appendOwnerFacingStatusReasonLine(summary, "support_report_reason", supportReport.reason);
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
    appendOwnerFacingStatusValueLine(summary, "crash_recovery_state", crashRecovery.state);
    appendOwnerFacingStatusReasonLine(summary, "crash_recovery_reason", crashRecovery.reason);
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
    appendOwnerFacingStatusValueLine(
        summary,
        "friend_mp_sync_transport_state",
        friendTrustStore.mpSyncTransportState);
    appendOwnerFacingStatusReasonLine(
        summary,
        "friend_mp_sync_transport_reason",
        friendTrustStore.mpSyncTransportReason);
    appendOwnerFacingStatusReasonLine(
        summary,
        "friend_mp_sync_transport_next_step",
        friendTrustStore.mpSyncTransportNextStep);
    summary << "friend_mp_sync_preflight_checklist: "
            << friendTrustStore.mpSyncPreflightChecklist << "\n";
    appendOwnerFacingStatusValueLine(
        summary,
        "mp_host_rotation_sync_state",
        mpOverlayPackage.hostRotationSyncState);
    appendOwnerFacingStatusReasonLine(
        summary,
        "mp_host_rotation_sync_reason",
        mpOverlayPackage.hostRotationSyncReason);
    appendOwnerFacingStatusReasonLine(
        summary,
        "mp_host_rotation_sync_next_step",
        mpOverlayPackage.hostRotationSyncNextStep);
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
    const std::string& postPlayPackageCampaignIdentityStateSummary,
    const std::filesystem::path& campaignLibraryPlanPath,
    const bool campaignLibraryPlanPresent,
    const bool campaignLibraryLimitReached,
    const std::size_t campaignLibrarySkippedDueToLimitCount,
    const std::string& campaignLibraryPlanReadiness,
    const std::string& campaignLibraryPlanReason,
    const std::size_t campaignLibraryPinnedCount,
    const std::size_t campaignLibraryPinnedMissingLocalSaveCount,
    const std::filesystem::path& campaignLibraryPinPath,
    const std::string& campaignLibraryPinState,
    const std::string& campaignLibraryPinReason,
    const std::string& campaignLibraryPinNextStep,
    const std::string& campaignLibraryPinCommandTemplate,
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
    const strategic_nexus::CompanionFriendTrustStoreStatus& friendTrustStore,
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
    brief << "Stav: " << ownerFacingStatusValueUtf8(state) << "\n";
    brief << "Post-play stav: " << ownerFacingStatusValueUtf8(postPlayState) << "\n";
    brief << "Dalsi akce: " << ownerFacingStatusValueUtf8(nextAction) << "\n";
    brief << "Duvod: " << ownerFacingStatusReasonUtf8(nextActionReason) << "\n\n";
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
        brief << " (" << ownerFacingStatusValueUtf8(entryPointReadiness) << ")";
    }
    brief << "\n";
    brief << "- Post-play package: " << pathString(postPlayPackagePath);
    if (!postPlayPackageReadiness.empty()) {
        brief << " (" << ownerFacingStatusValueUtf8(postPlayPackageReadiness) << ")";
    }
    brief << "\n";
    if (!postPlayPackageCampaignIdentityStateSummary.empty()) {
        brief << "- Post-play campaign identity state: "
              << ownerFacingStatusValueUtf8(postPlayPackageCampaignIdentityStateSummary) << "\n";
    }
    if (campaignLibraryPlanPresent) {
        brief << "- Campaign library plan: " << pathString(campaignLibraryPlanPath) << "\n";
        brief << "- Campaign library readiness: " << ownerFacingStatusValueUtf8(campaignLibraryPlanReadiness);
        if (!campaignLibraryPlanReason.empty()) {
            brief << " (" << ownerFacingStatusReasonUtf8(campaignLibraryPlanReason) << ")";
        }
        brief << "\n";
        brief << "- Campaign library saturation: "
              << (campaignLibraryLimitReached ? "truncated_by_limit" : "within_limit");
        brief << "\n";
        brief << "- campaign_library_pinned_count: " << campaignLibraryPinnedCount << "\n";
        brief << "- campaign_library_pinned_missing_local_save_count: "
              << campaignLibraryPinnedMissingLocalSaveCount << "\n";
        if (!campaignLibraryPinPath.empty()) {
            brief << "- campaign_library_pin_path: " << pathString(campaignLibraryPinPath) << "\n";
        }
        brief << "- campaign_library_pin_state: " << campaignLibraryPinState << "\n";
        brief << "- campaign_library_pin_reason: " << campaignLibraryPinReason << "\n";
        brief << "- campaign_library_pin_next_step: " << campaignLibraryPinNextStep << "\n";
        if (!campaignLibraryPinCommandTemplate.empty()) {
            brief << "- campaign_library_pin_command_template: "
                  << campaignLibraryPinCommandTemplate << "\n";
        }
        if (campaignLibraryPlanReadiness == "needs_attention") {
            brief << "\n";
            brief << "- Poznamka: sidecar plan knihovny kampani potrebuje pozornost; SNC nema duverovat aktivnimu coverage, dokud se plan neopravi.\n";
        } else if (campaignLibraryPinState == "needs_attention") {
            brief << "- Poznamka: pin manifest knihovny kampani potrebuje pozornost, nez budeme spolehat na pinned coverage.\n";
        } else if (campaignLibraryPinState == "degraded") {
            brief << "- Poznamka: pinned campaign exceptions jsou aktivni, ale cast pinned kampani neni lokalne hratelna.\n";
        } else if (campaignLibraryLimitReached) {
            brief << " (" << campaignLibrarySkippedDueToLimitCount << " campaign(s) skipped by limit)";
            brief << "\n";
            brief << "- Poznamka: aktivni generovana knihovna kampani je zamerne omezena; pokud v pristim testu chybi kampan, zvedni limit nebo uklid lokalni save root.\n";
            if (campaignLibraryPinState == "unavailable") {
                brief << "- Poznamka: user-pinned campaign exceptions zatim nejsou k dispozici; pro sirsi coverage nech local save root dostupny nebo jej obnov.\n";
            }
        } else {
            brief << "\n";
            brief << "- Poznamka: aktivni generovana knihovna kampani se vejde do nastaveneho limitu.\n";
        }
    }
    brief << "- Decision input package: " << pathString(decisionInputPackagePath);
    if (!decisionInputPackageReadiness.empty()) {
        brief << " (" << ownerFacingStatusValueUtf8(decisionInputPackageReadiness) << ")";
    }
    brief << "\n";
    brief << "- Candidate decision package: " << pathString(candidateDecisionPackagePath);
    if (!candidateDecisionPackageReadiness.empty()) {
        brief << " (" << ownerFacingStatusValueUtf8(candidateDecisionPackageReadiness) << ")";
    }
    brief << "\n";
    brief << "- DSL draft: " << pathString(dslDraftPath);
    if (!dslDraftReadiness.empty()) {
        brief << " (" << ownerFacingStatusValueUtf8(dslDraftReadiness) << ")";
    }
    brief << "\n";
    brief << "- SNC staged overlay status: " << pathString(generatedOverlayStagingStatusPath);
    if (!generatedOverlayStagingReadiness.empty()) {
        brief << " (" << ownerFacingStatusValueUtf8(generatedOverlayStagingReadiness) << ")";
    }
    brief << "\n";
    brief << "- Publish gate state: " << ownerFacingStatusValueUtf8(generatedOverlayPublishGate.state);
    if (!generatedOverlayPublishGate.reason.empty()) {
        brief << " (" << ownerFacingStatusReasonUtf8(generatedOverlayPublishGate.reason) << ")";
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
    brief << "- Support report state: " << ownerFacingStatusValueUtf8(supportReport.state);
    if (!supportReport.reason.empty()) {
        brief << " (" << ownerFacingStatusReasonUtf8(supportReport.reason) << ")";
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
    const auto friendMeshUpdate = strategic_nexus::buildFriendMeshUpdateStatus(friendTrustStore, mpOverlayPackage);
    brief << "- Friend mesh update state: " << ownerFacingStatusValueUtf8(friendMeshUpdate.state) << "\n";
    if (!friendMeshUpdate.reason.empty()) {
        brief << "- Friend mesh update reason: "
              << ownerFacingStatusReasonUtf8(friendMeshUpdate.reason) << "\n";
    }
    brief << "- Friend mesh update next step: "
          << ownerFacingStatusReasonUtf8(friendMeshUpdate.nextStep) << "\n";
    brief << "- Crash recovery state: " << ownerFacingStatusValueUtf8(crashRecovery.state);
    if (!crashRecovery.reason.empty()) {
        brief << " (" << ownerFacingStatusReasonUtf8(crashRecovery.reason) << ")";
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
    brief << "- MP package refresh: " << ownerFacingStatusValueUtf8(mpPackageRefreshState)
          << " (" << ownerFacingStatusReasonUtf8(mpPackageRefreshReason) << ")\n";
    brief << "- MP overlay package directory: " << pathString(g_mpOverlayPackageDirectory) << "\n";
    brief << "- MP package zip path: " << pathString(g_mpOverlayPackageZipPath) << "\n";
    if (!mpOverlayPackage.packageZipHash.empty()) {
        brief << "- MP package zip hash: " << mpOverlayPackage.packageZipHash << "\n";
    }
    if (!mpOverlayPackage.packageZipState.empty()) {
        brief << "- MP package zip state: " << ownerFacingStatusValueUtf8(mpOverlayPackage.packageZipState) << "\n";
    }
    if (!mpOverlayPackage.packageZipReason.empty()) {
        brief << "- MP package zip reason: " << ownerFacingStatusReasonUtf8(mpOverlayPackage.packageZipReason) << "\n";
    }
    if (!mpOverlayPackage.packageZipState.empty()) {
        brief << "- MP package zip bytes: " << mpOverlayPackage.packageZipBytes << "\n";
    }
    brief << "- MP overlay package state: " << ownerFacingStatusValueUtf8(mpOverlayPackage.state);
    if (!mpOverlayPackage.reason.empty()) {
        brief << " (" << ownerFacingStatusReasonUtf8(mpOverlayPackage.reason) << ")";
    }
    brief << "\n";
    if (!mpOverlayPackage.handoffStatus.empty()) {
        brief << "- MP handoff status: " << ownerFacingStatusValueUtf8(mpOverlayPackage.handoffStatus) << "\n";
    }
    if (!mpOverlayPackage.handoffRecoveryHint.empty()) {
        brief << "- MP recovery: " << ownerFacingStatusReasonUtf8(mpOverlayPackage.handoffRecoveryHint) << "\n";
    }
    brief << "- MP host rotation sync state: " << ownerFacingStatusValueUtf8(mpOverlayPackage.hostRotationSyncState) << "\n";
    brief << "- MP host rotation sync reason: "
          << ownerFacingStatusReasonUtf8(mpOverlayPackage.hostRotationSyncReason) << "\n";
    brief << "- MP host rotation sync next step: "
          << ownerFacingStatusReasonUtf8(mpOverlayPackage.hostRotationSyncNextStep) << "\n";
    brief << "- " << buildFriendPairingGuideTextUtf8() << "\n";
    brief << "- SNC friend pairing auto-sync: vypnuto, dokud neni hotovy signed/encrypted transport.\n";
    brief << "- SNC friend pairing command template: "
          << buildFriendPairingCommandTemplateUtf8() << "\n";
    brief << "- Human control guard: ";
    brief << ownerFacingStatusValueUtf8(
                 mpOverlayPackage.humanControlGuardState.empty() ? std::string("unknown") : mpOverlayPackage.humanControlGuardState)
          << "\n";
    brief << "- MP previous host available: "
          << (mpOverlayPackage.previousHostAvailable ? "ano" : "ne") << "\n";
    brief << "- MP previous host availability known: "
          << (mpOverlayPackage.previousHostAvailableKnown ? "ano" : "ne") << "\n";
    if (!mpOverlayPackage.readiness.empty()) {
        brief << "- MP readiness: " << ownerFacingStatusValueUtf8(mpOverlayPackage.readiness) << "\n";
    }
    if (!mpOverlayPackage.hostReadiness.empty()) {
        brief << "- MP host readiness: " << ownerFacingStatusValueUtf8(mpOverlayPackage.hostReadiness) << "\n";
    }
    if (!mpOverlayPackage.clientReadinessGate.empty()) {
        brief << "- MP client readiness gate: " << ownerFacingStatusValueUtf8(mpOverlayPackage.clientReadinessGate) << "\n";
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
        brief << "- MP host next step: " << ownerFacingStatusReasonUtf8(mpOverlayPackage.hostNextStep) << "\n";
    }
    if (!mpOverlayPackage.clientNextStep.empty()) {
        brief << "- MP client next step: " << ownerFacingStatusReasonUtf8(mpOverlayPackage.clientNextStep) << "\n";
    }
    if (!mpOverlayPackage.handoffRecoveryHint.empty()) {
        brief << "- MP recovery: " << ownerFacingStatusReasonUtf8(mpOverlayPackage.handoffRecoveryHint) << "\n";
    }
    if (!mpOverlayPackage.identityMismatchAlert.empty()) {
        brief << "- MP mismatch alert: " << ownerFacingStatusReasonUtf8(mpOverlayPackage.identityMismatchAlert) << "\n";
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
    const std::string& postPlayPackageCampaignIdentityStateSummary = std::string(),
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
    const auto effectivePostPlayPackageCampaignIdentityStateSummary =
        chooseString(postPlayPackageCampaignIdentityStateSummary, diskPipeline.postPlayPackageCampaignIdentityStateSummary);
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
            companionSnapshot.generatedOverlayPublishGate.publishCommand,
            companionSnapshot.localLlm.modelStatePath);
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
        companionSnapshot.localLlm.modelStatePath,
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
        effectivePostPlayPackageCampaignIdentityStateSummary,
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
        companionSnapshot.friendTrustStore,
        mpPackageRefreshState,
        mpPackageRefreshReason);
    writeNextStepsBrief(
        state,
        effectivePostPlayState,
        effectiveEntryPointAnalysisPath,
        effectiveEntryPointReadiness,
        effectivePostPlayPackagePath,
        effectivePostPlayPackageReadiness,
        effectivePostPlayPackageCampaignIdentityStateSummary,
        effectiveCampaignLibraryPlanPath,
        effectiveCampaignLibraryPlanPresent,
        effectiveCampaignLibraryLimitReached,
        effectiveCampaignLibrarySkippedDueToLimitCount,
        effectiveCampaignLibraryPlanReadiness,
        effectiveCampaignLibraryPlanReason,
        companionSnapshot.campaignLibraryPinnedCount,
        companionSnapshot.campaignLibraryPinnedMissingLocalSaveCount,
        companionSnapshot.campaignLibraryPinPath,
        companionSnapshot.campaignLibraryPinState,
        companionSnapshot.campaignLibraryPinReason,
        companionSnapshot.campaignLibraryPinNextStep,
        companionSnapshot.campaignLibraryPinCommandTemplate,
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
        companionSnapshot.friendTrustStore,
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
    json << "  \"local_llm_install_guidance\": \""
         << jsonEscape(companionSnapshot.localLlm.installGuidance) << "\",\n";
    json << "  \"local_llm_model_state_path\": \""
         << jsonEscape(pathString(companionSnapshot.localLlm.modelStatePath)) << "\",\n";
    json << "  \"local_llm_model_manager_summary\": \""
         << jsonEscape(companionSnapshot.localLlm.summary) << "\",\n";
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
    json << "  \"friend_trust_store_state\": \""
         << jsonEscape(companionSnapshot.friendTrustStore.state) << "\",\n";
    json << "  \"friend_trust_store_reason\": \""
         << jsonEscape(companionSnapshot.friendTrustStore.reason) << "\",\n";
    json << "  \"friend_trust_store_path\": \""
         << jsonEscape(pathString(companionSnapshot.friendTrustStore.path)) << "\",\n";
    json << "  \"friend_trust_store_trusted_count\": "
         << companionSnapshot.friendTrustStore.trustedFriendCount << ",\n";
    json << "  \"friend_trust_store_revoked_count\": "
         << companionSnapshot.friendTrustStore.revokedFriendCount << ",\n";
    json << "  \"friend_trust_store_blocked_count\": "
         << companionSnapshot.friendTrustStore.blockedFriendCount << ",\n";
    json << "  \"friend_trust_store_auto_sync_enabled_count\": "
         << companionSnapshot.friendTrustStore.autoSyncEnabledCount << ",\n";
    json << "  \"friend_trust_store_auto_sync_available\": "
         << (companionSnapshot.friendTrustStore.autoSyncAvailable ? "true" : "false") << ",\n";
    json << "  \"friend_trust_store_controls_state\": \""
         << jsonEscape(companionSnapshot.friendTrustStore.controlsState) << "\",\n";
    json << "  \"friend_trust_store_controls_reason\": \""
         << jsonEscape(companionSnapshot.friendTrustStore.controlsReason) << "\",\n";
    json << "  \"friend_trust_store_controls_next_step\": \""
         << jsonEscape(companionSnapshot.friendTrustStore.controlsNextStep) << "\",\n";
    json << "  \"friend_trust_store_update_command_template\": \""
         << jsonEscape(companionSnapshot.friendTrustStore.controlsCommandTemplate) << "\",\n";
    json << "  \"friend_pairing_command_template\": \""
         << jsonEscape(buildFriendPairingCommandTemplateUtf8()) << "\",\n";
    json << "  \"friend_mp_sync_envelope_command_template\": \""
         << jsonEscape(buildFriendMpSyncEnvelopeCommandTemplateUtf8()) << "\",\n";
    json << "  \"friend_mp_sync_inbox_plan_command_template\": \""
         << jsonEscape(buildFriendMpSyncInboxPlanCommandTemplateUtf8()) << "\",\n";
    json << "  \"friend_mp_sync_inbox_plan_state\": \"disabled_not_implemented\",\n";
    json << "  \"friend_mp_sync_inbox_plan_reason\": \""
         << jsonEscape(
                "signed/encrypted friend MP sync transport adapter is not implemented; automatic download and package staging disabled")
         << "\",\n";
    json << "  \"friend_mp_sync_inbox_plan_automatic_download_enabled\": false,\n";
    json << "  \"friend_mp_sync_inbox_plan_package_staging_allowed\": false,\n";
    json << "  \"friend_mp_sync_outbox_plan_command_template\": \""
         << jsonEscape(buildFriendMpSyncOutboxPlanCommandTemplateUtf8()) << "\",\n";
    json << "  \"friend_mp_sync_transport_state\": \"disabled_not_implemented\",\n";
    json << "  \"friend_mp_sync_transport_reason\": \""
         << jsonEscape("signed/encrypted friend MP sync transport adapter is not implemented; upload/send/download/staging disabled") << "\",\n";
    json << "  \"friend_mp_sync_transport_next_step\": \""
         << jsonEscape(companionSnapshot.friendTrustStore.mpSyncTransportNextStep) << "\",\n";
    json << "  \"friend_mp_sync_preflight_checklist\": \""
         << jsonEscape(companionSnapshot.friendTrustStore.mpSyncPreflightChecklist) << "\",\n";
    json << "  \"mp_host_rotation_sync_state\": \""
         << jsonEscape(companionSnapshot.mpOverlayPackage.hostRotationSyncState) << "\",\n";
    json << "  \"mp_host_rotation_sync_reason\": \""
         << jsonEscape(companionSnapshot.mpOverlayPackage.hostRotationSyncReason) << "\",\n";
    json << "  \"mp_host_rotation_sync_next_step\": \""
         << jsonEscape(companionSnapshot.mpOverlayPackage.hostRotationSyncNextStep) << "\",\n";
    const auto friendMeshUpdate =
        strategic_nexus::buildFriendMeshUpdateStatus(companionSnapshot.friendTrustStore, companionSnapshot.mpOverlayPackage);
    json << "  \"friend_mesh_update_state\": \"" << jsonEscape(friendMeshUpdate.state) << "\",\n";
    json << "  \"friend_mesh_update_reason\": \"" << jsonEscape(friendMeshUpdate.reason) << "\",\n";
    json << "  \"friend_mesh_update_next_step\": \"" << jsonEscape(friendMeshUpdate.nextStep) << "\",\n";
    json << "  \"friend_pairing_guide_text\": \""
         << jsonEscape(buildFriendPairingGuideTextUtf8()) << "\",\n";
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
    json << "  \"post_play_package_campaign_identity_state_summary\": \""
         << jsonEscape(effectivePostPlayPackageCampaignIdentityStateSummary) << "\",\n";
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
    std::string lastPostPlayPackageCampaignIdentityStateSummary;
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
            lastPostPlayPackageCampaignIdentityStateSummary.clear();
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
            lastPostPlayPackageCampaignIdentityStateSummary = postPlayPackage.campaignIdentityStateSummary;
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
                lastPostPlayPackageCampaignIdentityStateSummary,
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
            if (lastPostPlayPackageCampaignIdentityStateSummary.empty()) {
                std::string postPlayPackageJson;
                if (strategic_nexus::common::tryReadTextFile(g_postPlayPackagePath, postPlayPackageJson)) {
                    lastPostPlayPackageCampaignIdentityStateSummary =
                        strategic_nexus::common::extractJsonString(
                            postPlayPackageJson,
                            "campaign_identity_state_summary")
                            .value_or("");
                }
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
                lastPostPlayPackageCampaignIdentityStateSummary,
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
        lastPostPlayPackageCampaignIdentityStateSummary,
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

    g_statusWindowCanPersistPlacement = false;
    const HWND statusWindow = CreateWindowExW(
        WS_EX_APPWINDOW,
        kStatusWindowClassName,
        g_statusTitle.c_str(),
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kStatusDefaultWindowWidth,
        kStatusDefaultWindowHeight,
        hwnd,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);

    if (statusWindow != nullptr) {
        applyWindowIcons(statusWindow);
        applyPersistedStatusWindowPlacement(statusWindow);
        g_statusWindowCanPersistPlacement = true;
        ShowWindow(statusWindow, g_statusUiState.windowMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
        UpdateWindow(statusWindow);
        SetForegroundWindow(statusWindow);
    }
}

void allowForegroundActivationForWindow(HWND hwnd)
{
    DWORD processId = 0;
    if (hwnd != nullptr && GetWindowThreadProcessId(hwnd, &processId) != 0 && processId != 0) {
        AllowSetForegroundWindow(processId);
    }
}

bool showExternalStatusWindow(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return false;
    }

    allowForegroundActivationForWindow(hwnd);
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOWNORMAL);
    }
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    return true;
}

bool requestExistingInstanceStatusWindow()
{
    const HWND trayWindow = FindWindowW(kTrayWindowClassName, nullptr);
    if (trayWindow != nullptr && IsWindow(trayWindow)) {
        allowForegroundActivationForWindow(trayWindow);
        DWORD_PTR sendResult = 0;
        const LRESULT delivered = SendMessageTimeoutW(
            trayWindow,
            WM_SNC_OPEN_STATUS,
            0,
            0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK,
            2500,
            &sendResult);

        const HWND statusWindow = FindWindowW(kStatusWindowClassName, nullptr);
        if (showExternalStatusWindow(statusWindow)) {
            return true;
        }

        return delivered != 0;
    }

    return showExternalStatusWindow(FindWindowW(kStatusWindowClassName, nullptr));
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

    case WM_SNC_OPEN_STATUS:
        showStatusDialog(hwnd);
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
    g_statusUiStatePath = g_repoRoot / "dist" / "private_reports" / "snc_ui_state.json";
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    initializePaths();
    loadPersistedStatusUiState();
    g_sncTrayIcon = loadSncTrayIcon();
    applySncAppUserModelId();

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"StrategicNexusCompanionTraySingleInstance");
    if (mutex == nullptr) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!requestExistingInstanceStatusWindow()) {
            MessageBoxW(
                nullptr,
                L"SNC u\u017E b\u011B\u017E\u00ED, ale existuj\u00EDc\u00ED okno se nepoda\u0159ilo otev\u0159\u00EDt.",
                L"Strategic Nexus Companion",
                MB_OK | MB_ICONWARNING);
        }
        CloseHandle(mutex);
        return 0;
    }

    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kTrayWindowClassName;
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
