// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "StellarisProcessDetector.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace strategic_nexus {
namespace {

std::string normalizeProcessName(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    const auto slash = value.find_last_of('/');
    if (slash != std::string::npos) {
        value = value.substr(slash + 1);
    }

    std::string normalized;
    normalized.reserve(value.size());
    for (const unsigned char ch : value) {
        normalized.push_back(static_cast<char>(std::tolower(ch)));
    }

    constexpr const char* exeSuffix = ".exe";
    if (normalized.size() > 4 && normalized.substr(normalized.size() - 4) == exeSuffix) {
        normalized.resize(normalized.size() - 4);
    }
    return normalized;
}

bool isKnownStellarisProcessName(const std::string& processName)
{
    return normalizeProcessName(processName) == "stellaris";
}

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const char ch : value) {
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
        default:
            output << ch;
            break;
        }
    }
    return output.str();
}

#ifdef _WIN32
std::string wideToUtf8(const wchar_t* value)
{
    if (value == nullptr || value[0] == L'\0') {
        return std::string();
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) {
        return std::string();
    }

    std::string output(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, output.data(), required, nullptr, nullptr);
    if (!output.empty() && output.back() == '\0') {
        output.pop_back();
    }
    return output;
}
#endif

} // namespace

StellarisProcessStatus StellarisProcessDetector::detectFromProcessNames(const std::vector<std::string>& processNames) const
{
    StellarisProcessStatus status;
    status.detectionAvailable = true;

    for (const auto& processName : processNames) {
        if (isKnownStellarisProcessName(processName)) {
            status.running = true;
            status.matchedProcessNames.push_back(processName);
        }
    }

    status.reason = status.running ? "Stellaris process detected" : "Stellaris process not detected";
    return status;
}

StellarisProcessStatus StellarisProcessDetector::detectFromSystem() const
{
#ifdef _WIN32
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        StellarisProcessStatus status;
        status.reason = "process snapshot unavailable";
        return status;
    }

    std::vector<std::string> processNames;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            processNames.push_back(wideToUtf8(entry.szExeFile));
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    auto status = detectFromProcessNames(processNames);
    status.detectionAvailable = true;
    return status;
#else
    StellarisProcessStatus status;
    status.reason = "process detection unavailable on this platform";
    return status;
#endif
}

std::string serializeStellarisProcessStatus(const StellarisProcessStatus& status)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"detection_available\": " << (status.detectionAvailable ? "true" : "false") << ",\n";
    json << "  \"running\": " << (status.running ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(status.reason) << "\",\n";
    json << "  \"matched_process_names\": [";
    for (std::size_t index = 0; index < status.matchedProcessNames.size(); ++index) {
        json << (index == 0 ? "" : ", ") << "\"" << jsonEscape(status.matchedProcessNames[index]) << "\"";
    }
    json << "]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
