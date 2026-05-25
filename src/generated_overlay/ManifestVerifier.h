// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus::generated_overlay {

struct ManifestFileVerification {
    std::string path;
    std::string checksumRelevance;
    std::string expectedHash;
    std::string actualHash;
    std::size_t expectedByteCount = 0;
    std::size_t actualByteCount = 0;
    bool exists = false;
    bool hashMatches = false;
    bool byteCountMatches = false;
};

struct ManifestVerificationResult {
    bool ok = false;
    std::string reason;
    std::vector<ManifestFileVerification> files;
    std::vector<std::string> unexpectedFiles;
};

class ManifestVerifier {
public:
    ManifestVerificationResult verify(const std::filesystem::path& overlayDirectory) const;
};

std::string fnv1a64Hex(const std::string& text);

} // namespace strategic_nexus::generated_overlay

