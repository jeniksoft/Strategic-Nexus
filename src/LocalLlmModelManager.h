// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct LocalLlmHardwareProfile {
    int systemRamGb = 0;
    int vramGb = 0;
};

struct LocalLlmCatalogEntry {
    std::string modelId;
    std::string displayName;
    std::string provider;
    std::string runtime;
    std::string sourceUrl;
    std::string licenseName;
    std::string licenseUrl;
    std::string usePolicyUrl;
    bool requiresUserLicenseAcceptance = false;
    bool requiresLoginOrGatedAccess = false;
    int recommendedMinRamGb = 0;
    int recommendedMinVramGb = 0;
    std::string qualityTier;
    std::string speedTier;
    std::string privacyMode;
    std::string status;
    std::string lastLicenseReviewDate;
    std::string notes;
};

struct LocalLlmModelState {
    std::string selectedModelId;
    std::string modelVersion;
    std::string runtime;
    std::string sourceUrl;
    std::string licenseUrl;
    std::string usePolicyUrl;
    std::filesystem::path localPath;
    std::string contentHash;
    std::string installedAt;
    std::string lastVerifiedAt;
    std::string hardwareFit;
    std::string status;
    bool userLicenseAccepted = false;
    bool runtimeAvailable = false;
    bool runtimeModelPresent = false;
};

struct LocalLlmReadinessStatus {
    std::string state;
    std::string reason;
    std::string selectedModelId;
    std::string selectedDisplayName;
    std::string runtime;
    std::string catalogStatus;
    std::filesystem::path localPath;
    std::string hardwareFit;
    std::string recommendedModelId;
    std::string recommendedDisplayName;
    std::string recommendedRuntime;
    bool canRunInference = false;
    bool reducedMode = true;
    bool userActionRequired = false;
    bool downloadAllowed = false;
};

std::vector<LocalLlmCatalogEntry> defaultLocalLlmCatalog();
const LocalLlmCatalogEntry* findLocalLlmCatalogEntry(
    const std::vector<LocalLlmCatalogEntry>& catalog,
    const std::string& modelId);
LocalLlmModelState loadLocalLlmModelState(const std::filesystem::path& path);
std::string serializeLocalLlmModelState(const LocalLlmModelState& state);
bool writeLocalLlmModelState(
    const std::filesystem::path& path,
    const LocalLlmModelState& state);
std::string buildLocalLlmPrepareCommandHint(
    const std::string& modelId,
    const std::filesystem::path& statePath,
    const std::string& runtimeUrl = "http://127.0.0.1:11434");
std::string buildLocalLlmInstallGuidance(
    const LocalLlmReadinessStatus& readiness,
    const std::string& prepareCommandHint);
LocalLlmReadinessStatus evaluateLocalLlmReadiness(
    const std::vector<LocalLlmCatalogEntry>& catalog,
    const LocalLlmModelState& state,
    const LocalLlmHardwareProfile& hardware);

} // namespace strategic_nexus
