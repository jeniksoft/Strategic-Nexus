// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "ManifestVerifier.h"

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus::generated_overlay {

struct GeneratedOverlayPublishResult {
    bool ok = false;
    std::string reason;
    std::filesystem::path sourceDirectory;
    std::filesystem::path activeDirectory;
    std::string sourceManifestHash;
    std::string publishedManifestHash;
    std::vector<ManifestFileVerification> files;
};

class GeneratedOverlayPublisher {
public:
    GeneratedOverlayPublishResult publish(
        const std::filesystem::path& stagedOverlayDirectory,
        const std::filesystem::path& activeOverlayDirectory,
        bool stellarisRunning) const;
};

} // namespace strategic_nexus::generated_overlay
