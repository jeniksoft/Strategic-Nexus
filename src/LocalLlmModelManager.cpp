// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "LocalLlmModelManager.h"

#include "common/FileUtil.h"
#include "common/JsonExtract.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

namespace strategic_nexus {
namespace {

bool extractJsonBool(const std::string& json, const char* key)
{
    const std::regex pattern("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return false;
    }
    return match[1].str() == "true";
}

std::string lowerAscii(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

bool fileExistsIfConfigured(const std::filesystem::path& path)
{
    if (path.empty()) {
        return true;
    }

    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

bool hardwareMeetsMinimum(const LocalLlmCatalogEntry& entry, const LocalLlmHardwareProfile& hardware)
{
    if (hardware.systemRamGb > 0 && entry.recommendedMinRamGb > 0
        && hardware.systemRamGb < entry.recommendedMinRamGb) {
        return false;
    }
    if (hardware.vramGb > 0 && entry.recommendedMinVramGb > 0
        && hardware.vramGb < entry.recommendedMinVramGb) {
        return false;
    }
    return true;
}

const LocalLlmCatalogEntry* recommendCatalogEntry(
    const std::vector<LocalLlmCatalogEntry>& catalog,
    const LocalLlmHardwareProfile& hardware)
{
    const LocalLlmCatalogEntry* fallback = nullptr;
    for (const auto& entry : catalog) {
        if (entry.status == "unsupported" || entry.status == "restricted") {
            continue;
        }
        if (fallback == nullptr) {
            fallback = &entry;
        }
        if (hardwareMeetsMinimum(entry, hardware)) {
            return &entry;
        }
    }
    return fallback;
}

LocalLlmReadinessStatus reducedStatus(
    std::string state,
    std::string reason,
    const LocalLlmCatalogEntry* recommended)
{
    LocalLlmReadinessStatus status;
    status.state = std::move(state);
    status.reason = std::move(reason);
    status.reducedMode = true;
    status.canRunInference = false;
    if (recommended != nullptr) {
        status.recommendedModelId = recommended->modelId;
        status.recommendedDisplayName = recommended->displayName;
        status.recommendedRuntime = recommended->runtime;
    }
    return status;
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

} // namespace

std::vector<LocalLlmCatalogEntry> defaultLocalLlmCatalog()
{
    return {
        {
            "ollama:llama3.2:3b",
            "Llama 3.2 3B via Ollama",
            "Meta/Ollama",
            "ollama",
            "https://ollama.com/library/llama3.2",
            "see_provider_page",
            "https://ollama.com/library/llama3.2",
            "https://www.llama.com/llama3/license/",
            true,
            false,
            8,
            0,
            "small",
            "fast",
            "local_runtime",
            "requires_user_license_acceptance",
            "2026-06-06",
            "Small first-run candidate. The companion must require explicit user license review before install."
        },
        {
            "ollama:qwen2.5:7b",
            "Qwen2.5 7B via Ollama",
            "Alibaba Cloud/Ollama",
            "ollama",
            "https://ollama.com/library/qwen2.5",
            "see_provider_page",
            "https://ollama.com/library/qwen2.5",
            "https://ollama.com/library/qwen2.5",
            true,
            false,
            16,
            0,
            "medium",
            "balanced",
            "local_runtime",
            "requires_user_license_acceptance",
            "2026-06-06",
            "Stronger local candidate. Use only after the user accepts the model and provider terms."
        }
    };
}

const LocalLlmCatalogEntry* findLocalLlmCatalogEntry(
    const std::vector<LocalLlmCatalogEntry>& catalog,
    const std::string& modelId)
{
    for (const auto& entry : catalog) {
        if (entry.modelId == modelId) {
            return &entry;
        }
    }
    return nullptr;
}

LocalLlmModelState loadLocalLlmModelState(const std::filesystem::path& path)
{
    LocalLlmModelState state;
    if (path.empty()) {
        return state;
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error) || error) {
        return state;
    }

    const auto json = common::readTextFile(path);
    state.selectedModelId = common::extractJsonString(json, "selected_model_id").value_or("");
    state.modelVersion = common::extractJsonString(json, "model_version").value_or("");
    state.runtime = common::extractJsonString(json, "runtime").value_or("");
    state.sourceUrl = common::extractJsonString(json, "source_url").value_or("");
    state.licenseUrl = common::extractJsonString(json, "license_url").value_or("");
    state.usePolicyUrl = common::extractJsonString(json, "use_policy_url").value_or("");
    state.localPath = common::extractJsonString(json, "local_path").value_or("");
    state.contentHash = common::extractJsonString(json, "content_hash").value_or("");
    state.installedAt = common::extractJsonString(json, "installed_at").value_or("");
    state.lastVerifiedAt = common::extractJsonString(json, "last_verified_at").value_or("");
    state.hardwareFit = common::extractJsonString(json, "hardware_fit").value_or("");
    state.status = common::extractJsonString(json, "status").value_or("");
    state.userLicenseAccepted = extractJsonBool(json, "user_license_accepted");
    state.runtimeAvailable = extractJsonBool(json, "runtime_available");
    state.runtimeModelPresent = extractJsonBool(json, "runtime_model_present");
    return state;
}

std::string serializeLocalLlmModelState(const LocalLlmModelState& state)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"selected_model_id\": \"" << escapeJsonString(state.selectedModelId) << "\",\n";
    output << "  \"model_version\": \"" << escapeJsonString(state.modelVersion) << "\",\n";
    output << "  \"runtime\": \"" << escapeJsonString(state.runtime) << "\",\n";
    output << "  \"source_url\": \"" << escapeJsonString(state.sourceUrl) << "\",\n";
    output << "  \"license_url\": \"" << escapeJsonString(state.licenseUrl) << "\",\n";
    output << "  \"use_policy_url\": \"" << escapeJsonString(state.usePolicyUrl) << "\",\n";
    output << "  \"local_path\": \"" << escapeJsonString(state.localPath.generic_string()) << "\",\n";
    output << "  \"content_hash\": \"" << escapeJsonString(state.contentHash) << "\",\n";
    output << "  \"installed_at\": \"" << escapeJsonString(state.installedAt) << "\",\n";
    output << "  \"last_verified_at\": \"" << escapeJsonString(state.lastVerifiedAt) << "\",\n";
    output << "  \"hardware_fit\": \"" << escapeJsonString(state.hardwareFit) << "\",\n";
    output << "  \"status\": \"" << escapeJsonString(state.status) << "\",\n";
    output << "  \"user_license_accepted\": " << (state.userLicenseAccepted ? "true" : "false") << ",\n";
    output << "  \"runtime_available\": " << (state.runtimeAvailable ? "true" : "false") << ",\n";
    output << "  \"runtime_model_present\": " << (state.runtimeModelPresent ? "true" : "false") << "\n";
    output << "}\n";
    return output.str();
}

bool writeLocalLlmModelState(
    const std::filesystem::path& path,
    const LocalLlmModelState& state)
{
    if (path.empty()) {
        return false;
    }

    return common::writeTextFileAtomically(path, serializeLocalLlmModelState(state));
}

std::string buildLocalLlmPrepareCommandHint(
    const std::string& modelId,
    const std::filesystem::path& statePath,
    const std::string& runtimeUrl)
{
    if (modelId.empty() || statePath.empty()) {
        return std::string();
    }

    std::ostringstream output;
    output << "Strategic Nexus.exe --prepare-local-llm-model "
           << modelId << ' '
           << '"' << statePath.string() << '"' << ' '
           << "accept-license download "
           << runtimeUrl;
    return output.str();
}

std::string buildLocalLlmInstallGuidance(
    const LocalLlmReadinessStatus& readiness,
    const std::string& prepareCommandHint)
{
    if (readiness.canRunInference) {
        return {};
    }

    std::ostringstream output;
    if (!readiness.recommendedDisplayName.empty()) {
        output << "Doporuceny model: " << readiness.recommendedDisplayName << ". ";
    } else if (!readiness.recommendedModelId.empty()) {
        output << "Doporuceny model: " << readiness.recommendedModelId << ". ";
    }
    if (!readiness.recommendedRuntime.empty()) {
        output << "Doporuceny runtime: " << readiness.recommendedRuntime << ". ";
    }
    if (!prepareCommandHint.empty()) {
        output << "Priprav model pres: " << prepareCommandHint;
    } else if (!readiness.recommendedRuntime.empty()) {
        output << "Priprav model v podporovanem runtime a znovu over readiness.";
    }
    return output.str();
}

LocalLlmReadinessStatus evaluateLocalLlmReadiness(
    const std::vector<LocalLlmCatalogEntry>& catalog,
    const LocalLlmModelState& state,
    const LocalLlmHardwareProfile& hardware)
{
    const auto* recommended = recommendCatalogEntry(catalog, hardware);
    if (state.selectedModelId.empty()) {
        return reducedStatus(
            "no_model_installed",
            "no supported local model is selected; SNC can preserve saves and validate deterministic artifacts but new LLM interpretation is disabled",
            recommended);
    }

    const auto* entry = findLocalLlmCatalogEntry(catalog, state.selectedModelId);
    if (entry == nullptr) {
        auto status = reducedStatus(
            "model_license_not_supported",
            "selected local model is not in the supported catalog; SNC will not route gameplay-affecting decisions through it",
            recommended);
        status.selectedModelId = state.selectedModelId;
        status.runtime = state.runtime;
        status.localPath = state.localPath;
        return status;
    }

    LocalLlmReadinessStatus status;
    status.selectedModelId = entry->modelId;
    status.selectedDisplayName = entry->displayName;
    status.runtime = entry->runtime;
    status.catalogStatus = entry->status;
    status.localPath = state.localPath;
    status.hardwareFit = state.hardwareFit;
    status.recommendedModelId = recommended != nullptr ? recommended->modelId : "";
    status.recommendedDisplayName = recommended != nullptr ? recommended->displayName : "";
    status.recommendedRuntime = recommended != nullptr ? recommended->runtime : "";
    status.reducedMode = true;
    status.canRunInference = false;

    const auto catalogStatus = lowerAscii(entry->status);
    if (catalogStatus == "unsupported" || catalogStatus == "restricted") {
        status.state = "model_license_not_supported";
        status.reason = "selected model catalog status is " + entry->status + "; SNC will stay in reduced mode";
        status.userActionRequired = true;
        return status;
    }

    if (entry->requiresLoginOrGatedAccess) {
        status.state = "model_license_requires_user_action";
        status.reason = "selected model requires gated access or login approval before SNC can install or use it";
        status.userActionRequired = true;
        return status;
    }

    if (entry->requiresUserLicenseAcceptance && !state.userLicenseAccepted) {
        status.state = "model_license_requires_user_action";
        status.reason = "selected model requires explicit user license and use-policy acceptance before install or inference";
        status.userActionRequired = true;
        return status;
    }

    if (!hardwareMeetsMinimum(*entry, hardware)) {
        status.state = "model_incompatible_with_hardware";
        status.reason = "selected model exceeds the detected hardware recommendation";
        status.userActionRequired = true;
        return status;
    }

    if (!fileExistsIfConfigured(state.localPath)) {
        status.state = "model_missing";
        status.reason = "selected model local path is configured but missing";
        return status;
    }

    if (!state.runtimeAvailable) {
        status.state = "model_runtime_failed";
        status.reason = "selected model runtime is not available";
        return status;
    }

    if (!state.runtimeModelPresent) {
        status.state = "model_missing";
        status.reason = "selected runtime does not report the selected model as installed";
        status.downloadAllowed = true;
        return status;
    }

    const bool stateClaimsReady = state.status.empty() || state.status == "model_ready";
    if (!stateClaimsReady) {
        status.state = "model_changed_revalidation_needed";
        status.reason = "selected model state is " + state.status + "; SNC requires revalidation before inference";
        return status;
    }

    status.state = "model_ready";
    status.reason = "selected local model runtime and catalog policy are ready; model output remains untrusted until validators accept it";
    status.canRunInference = true;
    status.reducedMode = false;
    status.downloadAllowed = false;
    return status;
}

} // namespace strategic_nexus
