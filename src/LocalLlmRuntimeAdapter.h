// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>

namespace strategic_nexus {

struct LocalLlmPrepareRequest {
    std::string modelId;
    std::filesystem::path stateOutputPath;
    std::string runtimeUrl = "http://127.0.0.1:11434";
    bool userLicenseAccepted = false;
    bool allowDownload = false;
};

struct LocalLlmPrepareResult {
    bool ok = false;
    bool runtimeAvailable = false;
    bool runtimeModelPresent = false;
    bool downloadAttempted = false;
    bool downloadSucceeded = false;
    bool stateWritten = false;
    std::string state;
    std::string reason;
    std::string modelId;
    std::string runtime;
    std::string runtimeModelName;
    std::string runtimeUrl;
    std::filesystem::path stateOutputPath;
};

std::string ollamaModelNameFromCatalogId(const std::string& modelId);
bool ollamaTagsContainModel(const std::string& tagsJson, const std::string& modelName);
LocalLlmPrepareResult prepareLocalLlmModel(const LocalLlmPrepareRequest& request);

} // namespace strategic_nexus
