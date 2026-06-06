// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "LocalLlmRuntimeAdapter.h"

#include "LocalLlmModelManager.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")

namespace strategic_nexus {
namespace {

struct HttpResponse {
    bool ok = false;
    int statusCode = 0;
    std::string body;
    std::string error;
};

struct ParsedRuntimeUrl {
    std::wstring host;
    INTERNET_PORT port = 0;
    bool secure = false;
};

std::wstring widenUtf8(const std::string& value)
{
    if (value.empty()) {
        return std::wstring();
    }

    const int size = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (size <= 0) {
        return std::wstring();
    }

    std::wstring output(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        output.data(),
        size);
    return output;
}

std::string winHttpErrorMessage(const char* prefix)
{
    std::ostringstream output;
    output << prefix << " failed with Windows error " << GetLastError();
    return output.str();
}

bool parseRuntimeUrl(const std::string& url, ParsedRuntimeUrl& output, std::string& error)
{
    const auto wideUrl = widenUtf8(url.empty() ? "http://127.0.0.1:11434" : url);
    if (wideUrl.empty()) {
        error = "runtime URL is empty or not valid UTF-8";
        return false;
    }

    wchar_t hostBuffer[512]{};
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.lpszHostName = hostBuffer;
    components.dwHostNameLength = static_cast<DWORD>(sizeof(hostBuffer) / sizeof(hostBuffer[0]));

    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components)) {
        error = winHttpErrorMessage("WinHttpCrackUrl");
        return false;
    }

    if (components.nScheme != INTERNET_SCHEME_HTTP &&
        components.nScheme != INTERNET_SCHEME_HTTPS) {
        error = "only http and https Ollama runtime URLs are supported";
        return false;
    }

    if (components.dwHostNameLength == 0) {
        error = "runtime URL does not include a host";
        return false;
    }

    output.host.assign(hostBuffer, hostBuffer + components.dwHostNameLength);
    output.port = components.nPort;
    output.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    if (output.port == 0) {
        output.port = output.secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    }
    return true;
}

std::string escapeJsonString(const std::string& value)
{
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) {
                output << "\\u"
                       << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(character)
                       << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    return output.str();
}

std::string escapeRegexLiteral(const std::string& value)
{
    std::ostringstream output;
    for (const char character : value) {
        switch (character) {
        case '\\':
        case '.':
        case '^':
        case '$':
        case '|':
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case '*':
        case '+':
        case '?':
            output << '\\' << character;
            break;
        default:
            output << character;
            break;
        }
    }
    return output.str();
}

HttpResponse sendOllamaRequest(
    const std::string& runtimeUrl,
    const wchar_t* method,
    const wchar_t* path,
    const std::string& requestBody,
    const DWORD receiveTimeoutMs)
{
    ParsedRuntimeUrl parsed;
    std::string parseError;
    if (!parseRuntimeUrl(runtimeUrl, parsed, parseError)) {
        return {false, 0, std::string(), parseError};
    }

    HINTERNET session = WinHttpOpen(
        L"StrategicNexus/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session == nullptr) {
        return {false, 0, std::string(), winHttpErrorMessage("WinHttpOpen")};
    }

    WinHttpSetTimeouts(session, 5000, 5000, 10000, receiveTimeoutMs);

    HINTERNET connection = WinHttpConnect(session, parsed.host.c_str(), parsed.port, 0);
    if (connection == nullptr) {
        const auto error = winHttpErrorMessage("WinHttpConnect");
        WinHttpCloseHandle(session);
        return {false, 0, std::string(), error};
    }

    const DWORD flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(
        connection,
        method,
        path,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);
    if (request == nullptr) {
        const auto error = winHttpErrorMessage("WinHttpOpenRequest");
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return {false, 0, std::string(), error};
    }

    const wchar_t* headers = requestBody.empty()
        ? WINHTTP_NO_ADDITIONAL_HEADERS
        : L"Content-Type: application/json\r\n";
    const DWORD headersLength = requestBody.empty() ? 0 : static_cast<DWORD>(-1L);
    const LPVOID body = requestBody.empty()
        ? WINHTTP_NO_REQUEST_DATA
        : const_cast<char*>(requestBody.data());
    const DWORD bodySize = static_cast<DWORD>(requestBody.size());

    if (!WinHttpSendRequest(
            request,
            headers,
            headersLength,
            body,
            bodySize,
            bodySize,
            0)) {
        const auto error = winHttpErrorMessage("WinHttpSendRequest");
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return {false, 0, std::string(), error};
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        const auto error = winHttpErrorMessage("WinHttpReceiveResponse");
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return {false, 0, std::string(), error};
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        const auto error = winHttpErrorMessage("WinHttpQueryHeaders");
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return {false, 0, std::string(), error};
    }

    std::string responseBody;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            const auto error = winHttpErrorMessage("WinHttpQueryDataAvailable");
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return {false, static_cast<int>(statusCode), responseBody, error};
        }
        if (available == 0) {
            break;
        }

        std::vector<char> buffer(static_cast<std::size_t>(available));
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) {
            const auto error = winHttpErrorMessage("WinHttpReadData");
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return {false, static_cast<int>(statusCode), responseBody, error};
        }
        responseBody.append(buffer.data(), buffer.data() + read);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    const bool httpOk = statusCode >= 200 && statusCode < 300;
    return {
        httpOk,
        static_cast<int>(statusCode),
        responseBody,
        httpOk ? std::string() : "Ollama runtime returned HTTP " + std::to_string(statusCode)
    };
}

std::string nowIsoLocal()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (localtime_s(&localTime, &timeValue) != 0) {
        return std::string();
    }

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

LocalLlmModelState buildStateFromEntry(
    const LocalLlmCatalogEntry& entry,
    const std::string& runtimeModelName,
    const bool userLicenseAccepted,
    const bool runtimeAvailable,
    const bool runtimeModelPresent,
    const std::string& state)
{
    LocalLlmModelState modelState;
    modelState.selectedModelId = entry.modelId;
    modelState.modelVersion = runtimeModelName;
    modelState.runtime = entry.runtime;
    modelState.sourceUrl = entry.sourceUrl;
    modelState.licenseUrl = entry.licenseUrl;
    modelState.usePolicyUrl = entry.usePolicyUrl;
    modelState.installedAt = runtimeModelPresent ? nowIsoLocal() : "";
    modelState.lastVerifiedAt = nowIsoLocal();
    modelState.hardwareFit = "not_checked";
    modelState.status = state;
    modelState.userLicenseAccepted = userLicenseAccepted;
    modelState.runtimeAvailable = runtimeAvailable;
    modelState.runtimeModelPresent = runtimeModelPresent;
    return modelState;
}

void writeStateIfPossible(
    LocalLlmPrepareResult& result,
    const LocalLlmCatalogEntry& entry,
    const bool userLicenseAccepted,
    const bool runtimeAvailable,
    const bool runtimeModelPresent,
    const std::string& state)
{
    if (result.stateOutputPath.empty()) {
        result.stateWritten = false;
        return;
    }

    const auto modelState = buildStateFromEntry(
        entry,
        result.runtimeModelName,
        userLicenseAccepted,
        runtimeAvailable,
        runtimeModelPresent,
        state);
    result.stateWritten = writeLocalLlmModelState(result.stateOutputPath, modelState);
}

} // namespace

std::string ollamaModelNameFromCatalogId(const std::string& modelId)
{
    const std::string prefix = "ollama:";
    if (modelId.rfind(prefix, 0) == 0) {
        return modelId.substr(prefix.size());
    }
    return std::string();
}

bool ollamaTagsContainModel(const std::string& tagsJson, const std::string& modelName)
{
    if (modelName.empty()) {
        return false;
    }

    const std::regex modelPattern(
        "\"(?:name|model)\"\\s*:\\s*\"" + escapeRegexLiteral(modelName) + "\"");
    return std::regex_search(tagsJson, modelPattern);
}

LocalLlmPrepareResult prepareLocalLlmModel(const LocalLlmPrepareRequest& request)
{
    LocalLlmPrepareResult result;
    result.modelId = request.modelId;
    result.runtimeUrl = request.runtimeUrl.empty() ? "http://127.0.0.1:11434" : request.runtimeUrl;
    result.stateOutputPath = request.stateOutputPath;
    result.runtime = "ollama";
    result.runtimeModelName = ollamaModelNameFromCatalogId(request.modelId);

    if (request.modelId.empty() || result.stateOutputPath.empty()) {
        result.state = "invalid_request";
        result.reason = "model id and state output path are required";
        return result;
    }

    const auto catalog = defaultLocalLlmCatalog();
    const auto* entry = findLocalLlmCatalogEntry(catalog, request.modelId);
    if (entry == nullptr) {
        result.state = "model_license_not_supported";
        result.reason = "requested model is not in the supported local LLM catalog";
        return result;
    }

    result.runtime = entry->runtime;
    if (entry->runtime != "ollama" || result.runtimeModelName.empty()) {
        result.state = "model_runtime_failed";
        result.reason = "requested model does not map to the Ollama runtime adapter";
        return result;
    }

    if (entry->requiresLoginOrGatedAccess) {
        result.state = "model_license_requires_user_action";
        result.reason = "requested model requires gated access or login; automatic install is blocked";
        return result;
    }

    if (entry->requiresUserLicenseAcceptance && !request.userLicenseAccepted) {
        result.state = "model_license_requires_user_action";
        result.reason = "requested model requires explicit license and use-policy acceptance before install";
        return result;
    }

    const auto initialTags = sendOllamaRequest(result.runtimeUrl, L"GET", L"/api/tags", std::string(), 10000);
    result.runtimeAvailable = initialTags.ok;
    if (!initialTags.ok) {
        result.state = "model_runtime_failed";
        result.reason = initialTags.error.empty() ? "Ollama runtime is not available" : initialTags.error;
        writeStateIfPossible(result, *entry, request.userLicenseAccepted, false, false, result.state);
        return result;
    }

    result.runtimeModelPresent = ollamaTagsContainModel(initialTags.body, result.runtimeModelName);
    if (!result.runtimeModelPresent && request.allowDownload) {
        result.downloadAttempted = true;
        const std::string body =
            "{\"model\":\"" + escapeJsonString(result.runtimeModelName) + "\",\"stream\":false}";
        const auto pull = sendOllamaRequest(result.runtimeUrl, L"POST", L"/api/pull", body, 1800000);
        result.downloadSucceeded = pull.ok && (
            pull.body.find("\"success\"") != std::string::npos ||
            pull.body.find("\"status\":\"success\"") != std::string::npos ||
            pull.body.find("\"status\": \"success\"") != std::string::npos);

        if (!pull.ok) {
            result.state = "model_missing";
            result.reason = pull.error.empty() ? "Ollama pull failed" : pull.error;
            writeStateIfPossible(result, *entry, request.userLicenseAccepted, true, false, result.state);
            return result;
        }

        const auto tagsAfterPull = sendOllamaRequest(result.runtimeUrl, L"GET", L"/api/tags", std::string(), 10000);
        result.runtimeAvailable = tagsAfterPull.ok;
        result.runtimeModelPresent = tagsAfterPull.ok && ollamaTagsContainModel(tagsAfterPull.body, result.runtimeModelName);
        if (!result.runtimeModelPresent) {
            result.state = "model_missing";
            result.reason = "Ollama pull returned but the selected model is still not listed by /api/tags";
            writeStateIfPossible(result, *entry, request.userLicenseAccepted, result.runtimeAvailable, false, result.state);
            return result;
        }
    }

    if (!result.runtimeModelPresent) {
        result.state = "model_missing";
        result.reason = "selected Ollama model is not installed; rerun with explicit download after reviewing terms";
        writeStateIfPossible(result, *entry, request.userLicenseAccepted, true, false, result.state);
        return result;
    }

    result.state = "model_ready";
    result.reason = "selected Ollama model is present; output remains untrusted until deterministic validators accept it";
    writeStateIfPossible(result, *entry, request.userLicenseAccepted, true, true, result.state);
    result.ok = result.stateWritten;
    if (!result.stateWritten) {
        result.reason = "model is ready but state file could not be written";
    }
    return result;
}

} // namespace strategic_nexus
