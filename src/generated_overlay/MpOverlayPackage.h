// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace strategic_nexus::generated_overlay {

struct MpOverlayPackageFileVerification {
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

struct MpOverlayPackageVerificationResult {
    bool ok = false;
    std::string reason;
    std::vector<MpOverlayPackageFileVerification> files;
    std::vector<std::string> unexpectedFiles;
};

struct MpOverlayPackageExportResult {
    bool ok = false;
    std::string reason;
    std::vector<MpOverlayPackageFileVerification> files;
};

class MpOverlayPackageExporter {
public:
    MpOverlayPackageExportResult exportPackage(
        const std::filesystem::path& sourceOverlayDirectory,
        const std::string& campaignId,
        const std::string& overlayVersion,
        const std::string& gameVersion,
        const std::string& strategicNexusModVersion,
        const std::filesystem::path& outputPackageDirectory,
        bool previousHostAvailable) const;
};

class MpOverlayPackageVerifier {
public:
    MpOverlayPackageVerificationResult verify(const std::filesystem::path& packageDirectory) const;
};

} // namespace strategic_nexus::generated_overlay

