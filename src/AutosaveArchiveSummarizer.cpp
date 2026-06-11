// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "AutosaveArchiveSummarizer.h"

#include "AutosaveArchiveVerifier.h"

#include <sstream>

namespace strategic_nexus {
namespace {

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const char ch : value) {
        switch (ch) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                output << ' ';
            } else {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

} // namespace

AutosaveArchiveSummary AutosaveArchiveSummarizer::summarize(
    const std::filesystem::path& sessionArchiveDirectory) const
{
    AutosaveArchiveSummary summary;
    summary.sessionArchiveDirectory = sessionArchiveDirectory;

    const AutosaveArchiveVerifier verifier;
    const auto verification = verifier.verify(sessionArchiveDirectory);
    summary.schemaVersion = verification.schemaVersion;
    summary.schemaCompatibilityState = verification.schemaCompatibilityState;
    summary.schemaCompatibilityNote = verification.schemaCompatibilityNote;
    if (!verification.ok) {
        summary.reason = verification.reason;
        return summary;
    }

    summary.ok = true;
    summary.reason = "accepted";
    summary.copiedSaveCount = verification.files.size();
    for (const auto& file : verification.files) {
        summary.totalByteCount += file.actualByteCount;
    }

    if (!verification.files.empty()) {
        const auto& first = verification.files.front();
        const auto& last = verification.files.back();
        summary.firstArchivedPath = first.path;
        summary.firstContentHash = first.actualHash;
        summary.lastArchivedPath = last.path;
        summary.lastContentHash = last.actualHash;
    }

    return summary;
}

std::string serializeAutosaveArchiveSummary(const AutosaveArchiveSummary& summary)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": 1,\n";
    json << "  \"ok\": " << (summary.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(summary.reason) << "\",\n";
    json << "  \"source_schema_version\": " << summary.schemaVersion << ",\n";
    json << "  \"schema_compatibility_state\": \"" << jsonEscape(summary.schemaCompatibilityState) << "\",\n";
    json << "  \"schema_compatibility_note\": \"" << jsonEscape(summary.schemaCompatibilityNote) << "\",\n";
    json << "  \"session_archive_directory\": \"" << jsonEscape(summary.sessionArchiveDirectory.generic_string()) << "\",\n";
    json << "  \"copied_save_count\": " << summary.copiedSaveCount << ",\n";
    json << "  \"total_byte_count\": " << summary.totalByteCount << ",\n";
    json << "  \"first_archived_path\": \"" << jsonEscape(summary.firstArchivedPath) << "\",\n";
    json << "  \"first_content_hash\": \"" << jsonEscape(summary.firstContentHash) << "\",\n";
    json << "  \"last_archived_path\": \"" << jsonEscape(summary.lastArchivedPath) << "\",\n";
    json << "  \"last_content_hash\": \"" << jsonEscape(summary.lastContentHash) << "\"\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus

