// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus {

struct AutosaveArchiveFileVerification {
    std::string path;
    std::string expectedHash;
    std::string actualHash;
    std::uintmax_t expectedByteCount = 0;
    std::uintmax_t actualByteCount = 0;
    bool exists = false;
    bool hashMatches = false;
    bool byteCountMatches = false;
};

struct AutosaveArchiveVerificationResult {
    bool ok = false;
    std::string reason;
    std::size_t schemaVersion = 1;
    std::string schemaCompatibilityState = "current";
    std::string schemaCompatibilityNote;
    std::vector<AutosaveArchiveFileVerification> files;
};

class AutosaveArchiveVerifier {
public:
    AutosaveArchiveVerificationResult verify(const std::filesystem::path& sessionArchiveDirectory) const;
};

} // namespace strategic_nexus

