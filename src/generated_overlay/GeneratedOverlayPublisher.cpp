// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "GeneratedOverlayPublisher.h"

#include "common/FileUtil.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace strategic_nexus::generated_overlay {
namespace {

constexpr const char* manifestFileName = "strategic_nexus_generated_manifest.json";

bool samePath(const std::filesystem::path& left, const std::filesystem::path& right)
{
    std::error_code error;
    const auto canonicalLeft = std::filesystem::weakly_canonical(left, error);
    if (error) {
        return false;
    }
    const auto canonicalRight = std::filesystem::weakly_canonical(right, error);
    if (error) {
        return false;
    }
    return canonicalLeft == canonicalRight;
}

bool copyTextFile(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::string text;
    if (!common::tryReadTextFile(source, text)) {
        return false;
    }
    return common::writeTextFileAtomically(destination, text);
}

std::filesystem::path tempSiblingFor(const std::filesystem::path& target)
{
    const auto parent = target.parent_path();
    const auto name = target.filename().empty() ? std::filesystem::path("generated_overlay") : target.filename();
    return parent / (name.string() + ".publish_tmp");
}

bool removeDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::remove_all(path, error);
    return !error;
}

bool ensureDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::create_directories(path, error);
    return !error;
}

} // namespace

GeneratedOverlayPublishResult GeneratedOverlayPublisher::publish(
    const std::filesystem::path& stagedOverlayDirectory,
    const std::filesystem::path& activeOverlayDirectory,
    const bool stellarisRunning) const
{
    GeneratedOverlayPublishResult result;
    result.sourceDirectory = stagedOverlayDirectory;
    result.activeDirectory = activeOverlayDirectory;

    if (stagedOverlayDirectory.empty() || activeOverlayDirectory.empty()) {
        result.reason = "missing generated overlay publish input";
        return result;
    }

    if (stellarisRunning) {
        result.reason = "Stellaris is running; generated overlay publish deferred";
        return result;
    }

    if (samePath(stagedOverlayDirectory, activeOverlayDirectory)) {
        result.reason = "staged and active overlay directories must be different";
        return result;
    }

    const ManifestVerifier verifier;
    const auto sourceVerification = verifier.verify(stagedOverlayDirectory);
    result.sourceManifestHash = sourceVerification.manifestHash;
    if (!sourceVerification.ok) {
        result.reason = "staged generated overlay verification failed: " + sourceVerification.reason;
        return result;
    }

    const auto targetParent = activeOverlayDirectory.parent_path();
    if (!targetParent.empty() && !ensureDirectory(targetParent)) {
        result.reason = "failed to create active overlay parent directory";
        return result;
    }

    const auto tempDirectory = tempSiblingFor(activeOverlayDirectory);
    if (samePath(tempDirectory, stagedOverlayDirectory) || samePath(tempDirectory, activeOverlayDirectory)) {
        result.reason = "unsafe generated overlay publish temp directory";
        return result;
    }

    if (!removeDirectory(tempDirectory)) {
        result.reason = "failed to clean generated overlay publish temp directory";
        return result;
    }

    const auto sourceManifestPath = stagedOverlayDirectory / manifestFileName;
    const auto tempManifestPath = tempDirectory / manifestFileName;
    if (!copyTextFile(sourceManifestPath, tempManifestPath)) {
        removeDirectory(tempDirectory);
        result.reason = "failed to copy generated overlay manifest";
        return result;
    }

    for (const auto& file : sourceVerification.files) {
        const auto relativePath = std::filesystem::path(file.path);
        const auto sourcePath = stagedOverlayDirectory / relativePath;
        const auto tempPath = tempDirectory / relativePath;
        if (!copyTextFile(sourcePath, tempPath)) {
            removeDirectory(tempDirectory);
            result.reason = "failed to copy generated overlay file";
            return result;
        }
    }

    const auto tempVerification = verifier.verify(tempDirectory);
    if (!tempVerification.ok) {
        removeDirectory(tempDirectory);
        result.reason = "temporary generated overlay verification failed: " + tempVerification.reason;
        return result;
    }

    if (!removeDirectory(activeOverlayDirectory)) {
        removeDirectory(tempDirectory);
        result.reason = "failed to replace active generated overlay directory";
        return result;
    }

    std::error_code renameError;
    std::filesystem::rename(tempDirectory, activeOverlayDirectory, renameError);
    if (renameError) {
        removeDirectory(tempDirectory);
        result.reason = "failed to publish generated overlay directory";
        return result;
    }

    const auto publishedVerification = verifier.verify(activeOverlayDirectory);
    result.publishedManifestHash = publishedVerification.manifestHash;
    result.files = publishedVerification.files;
    if (!publishedVerification.ok) {
        result.reason = "published generated overlay verification failed: " + publishedVerification.reason;
        return result;
    }

    result.ok = true;
    result.reason = "published";
    return result;
}

} // namespace strategic_nexus::generated_overlay
