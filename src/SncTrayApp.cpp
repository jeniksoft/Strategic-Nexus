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
#include "StellarisProcessDetector.h"
#include "StellarisSavePathResolver.h"
#include "common/FileUtil.h"

#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <mutex>
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
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &localTime);
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
    const bool archiveVerified)
{
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
    json << "  \"archive_verified\": " << (archiveVerified ? "true" : "false") << "\n";
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

    bool wasRunning = false;
    std::string sessionId;
    std::filesystem::path sessionDirectory;
    std::size_t copiedTotal = 0;
    std::size_t skippedTotal = 0;
    bool archiveVerified = false;

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
                "not_started",
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
                "not_started",
                archiveVerified);
        } else if (wasRunning) {
            const auto verification = verifier.verify(sessionDirectory);
            const auto summary = summarizer.summarize(sessionDirectory);
            writeLastSummary(summary);
            archiveVerified = verification.ok;
            const std::string reason = verification.ok
                ? "Stellaris skoncil; archiv je overeny a pripraveny pro post-play analyzu."
                : "Stellaris skoncil; archiv vyzaduje pozornost: " + verification.reason;

            writeStatus(
                hwnd,
                verification.ok ? "post_play_ready" : "post_play_attention",
                reason,
                false,
                sessionId,
                sessionDirectory,
                0,
                0,
                copiedTotal,
                skippedTotal,
                verification.ok ? "analysis_pending" : "blocked_by_archive_verification",
                archiveVerified);
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
                archiveVerified ? "analysis_pending" : "not_started",
                archiveVerified);
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
        archiveVerified ? "analysis_pending" : "not_started",
        archiveVerified);
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

    MessageBoxW(hwnd, text.c_str(), g_statusTitle.c_str(), MB_OK | MB_ICONINFORMATION);
}

void openArchiveDirectory()
{
    std::error_code error;
    std::filesystem::create_directories(g_archiveRoot, error);
    ShellExecuteW(nullptr, L"open", g_archiveRoot.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void showTrayMenu(HWND hwnd)
{
    POINT point{};
    GetCursorPos(&point);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_STATUS, L"Stav");
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_ARCHIVE, L"Otevrit archiv");
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
