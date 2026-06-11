// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SaveEntryPointAnalyzer.h"

#include "AutosaveArchiveVerifier.h"
#include "SaveParser.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

namespace strategic_nexus {
namespace {

constexpr std::size_t kMaxEvidenceSamplesPerEntry = 8;

bool isSavFile(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".sav";
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

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

std::string stableKeyFromName(const std::string& name)
{
    std::string key;
    bool previousUnderscore = false;
    for (const char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            previousUnderscore = false;
        } else if (!previousUnderscore) {
            key.push_back('_');
            previousUnderscore = true;
        }
    }
    while (!key.empty() && key.front() == '_') {
        key.erase(key.begin());
    }
    while (!key.empty() && key.back() == '_') {
        key.pop_back();
    }
    return key.empty() ? "campaign" : key;
}

std::string genericPath(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::string hashFileFnv1a64Hex(const std::filesystem::path& path)
{
    constexpr std::uint64_t offsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offsetBasis;

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    char buffer[4096];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= prime;
        }
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string classifySaveKind(const std::filesystem::path& path)
{
    const auto filename = toLower(path.filename().string());
    if (filename.rfind("autosave", 0) == 0) {
        return "autosave";
    }
    if (filename == "ironman.sav") {
        return "ironman";
    }

    static const std::regex dateNamedPattern(R"(^\d{4}\.\d{1,2}\.\d{1,2}\.sav$)");
    if (std::regex_match(filename, dateNamedPattern)) {
        return "date_named_save";
    }
    return "manual_save";
}

std::string extractSaveDateFromText(const std::string& value)
{
    static const std::regex datePattern(R"((\d{4})[._-](\d{1,2})[._-](\d{1,2}))");
    std::smatch match;
    if (!std::regex_search(value, match, datePattern)) {
        return {};
    }

    const int month = std::stoi(match[2].str());
    const int day = std::stoi(match[3].str());
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        return {};
    }

    std::ostringstream output;
    output << match[1].str() << "."
           << std::setw(2) << std::setfill('0') << month << "."
           << std::setw(2) << std::setfill('0') << day;
    return output.str();
}

bool dateLessOrEqual(const std::string& left, const std::string& right)
{
    if (left.empty() || right.empty()) {
        return false;
    }
    return left <= right;
}

bool dateGreater(const std::string& left, const std::string& right)
{
    if (left.empty() || right.empty()) {
        return false;
    }
    return left > right;
}

bool directoryExists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error) && std::filesystem::is_directory(path, error);
}

std::string firstRelativeComponent(const std::filesystem::path& relativePath)
{
    for (const auto& part : relativePath) {
        const auto text = part.string();
        if (!text.empty() && text != "." && text != "..") {
            return text;
        }
    }
    return {};
}

std::string campaignNameForSavePath(
    const std::filesystem::path& saveRoot,
    const std::filesystem::path& savePath,
    const std::filesystem::path& relativePath)
{
    const auto parent = savePath.parent_path();
    std::error_code error;
    const auto relativeParent = std::filesystem::relative(parent, saveRoot, error);
    if (!error && !relativeParent.empty() && relativeParent != ".") {
        const auto first = firstRelativeComponent(relativeParent);
        if (!first.empty()) {
            return first;
        }
    }
    return savePath.stem().string();
}

std::string stripTrailingHash(std::string stem)
{
    const auto lastUnderscore = stem.find_last_of('_');
    if (lastUnderscore == std::string::npos || lastUnderscore + 1 >= stem.size()) {
        return stem;
    }

    const auto suffix = stem.substr(lastUnderscore + 1);
    if (suffix.size() != 12) {
        return stem;
    }
    for (const char ch : suffix) {
        if (std::isxdigit(static_cast<unsigned char>(ch)) == 0) {
            return stem;
        }
    }
    return stem.substr(0, lastUnderscore);
}

std::string campaignKeyFromArchivedFilename(const std::filesystem::path& archivedPath)
{
    std::string stem = archivedPath.stem().string();
    if (stem.rfind("live_", 0) == 0) {
        stem = stem.substr(5);
    } else {
        const auto underscore = stem.find('_');
        if (underscore != std::string::npos) {
            const auto prefix = stem.substr(0, underscore);
            if (!prefix.empty() && std::all_of(prefix.begin(), prefix.end(), [](const unsigned char ch) {
                    return std::isdigit(ch) != 0;
                })) {
                stem = stem.substr(underscore + 1);
            }
        }
    }

    stem = stripTrailingHash(stem);
    const auto autosaveMarker = stem.find("_autosave");
    if (autosaveMarker != std::string::npos) {
        return stableKeyFromName(stem.substr(0, autosaveMarker));
    }

    const std::string ironmanMarker = "_ironman";
    if (stem.size() > ironmanMarker.size() &&
        stem.compare(stem.size() - ironmanMarker.size(), ironmanMarker.size(), ironmanMarker) == 0) {
        return stableKeyFromName(stem.substr(0, stem.size() - ironmanMarker.size()));
    }

    return stableKeyFromName(stem);
}

void addWarning(std::vector<std::string>& warnings, const std::string& warning);

void appendEvidenceSample(
    std::vector<ArchivedSaveEvidenceReference>& samples,
    const ArchivedSaveEvidence& evidence)
{
    if (samples.size() >= kMaxEvidenceSamplesPerEntry) {
        return;
    }

    ArchivedSaveEvidenceReference sample;
    sample.archivedPath = evidence.archivedPath;
    sample.saveName = evidence.saveName;
    sample.saveDate = evidence.saveDate;
    sample.contentHash = evidence.contentHash;
    sample.byteCount = evidence.byteCount;
    samples.push_back(std::move(sample));
}

SaveEntryPoint makeEntryPoint(
    const std::filesystem::path& saveRoot,
    const std::filesystem::path& savePath)
{
    std::error_code error;
    auto relativePath = std::filesystem::relative(savePath, saveRoot, error);
    if (error || relativePath.empty()) {
        relativePath = savePath.filename();
        error.clear();
    }

    SaveEntryPoint entry;
    entry.sourceRoot = genericPath(saveRoot);
    entry.relativePath = genericPath(relativePath);
    entry.sourceKind = classifySaveKind(savePath);
    entry.saveName = savePath.filename().string();
    entry.saveDate = extractSaveDateFromText(entry.saveName);
    entry.campaignKey = stableKeyFromName(campaignNameForSavePath(saveRoot, savePath, relativePath));
    entry.hashAlgorithm = "fnv1a64";
    entry.contentHash = hashFileFnv1a64Hex(savePath);
    entry.byteCount = std::filesystem::file_size(savePath, error);
    if (error) {
        entry.byteCount = 0;
        error.clear();
    }

    entry.parseStatus = entry.saveDate.empty() ? "needs_deferred_parse" : "fast_filename_scan";

    const SaveParser parser;
    const auto parsed = parser.parseSummary(savePath);
    entry.empireState.parsed = parsed.ok;
    entry.empireState.parserReason = parsed.reason;
    entry.empireState.playerCountryId = parsed.playerCountryId;
    entry.empireState.empireName = parsed.empireName;
    entry.empireState.government = parsed.government;
    entry.empireState.authority = parsed.authority;
    entry.empireState.founderSpeciesName = parsed.founderSpeciesName;
    entry.empireState.capitalPlanetName = parsed.capitalPlanetName;
    entry.empireState.homeSystemName = parsed.homeSystemName;
    entry.empireState.ownedFleetCount = parsed.ownedFleetCount;
    entry.empireState.activeWarCount = parsed.activeWarCount;
    entry.empireState.missingFields = parsed.missingFields;
    entry.empireState.uncertainties = parsed.uncertainties;
    if (parsed.ok) {
        entry.parseStatus = "headline_parsed";
        entry.playerCountryId = parsed.playerCountryId;
        entry.empireName = parsed.empireName;
        if (!parsed.saveDate.empty()) {
            entry.saveDate = parsed.saveDate;
        }
    } else {
        entry.empireState.parseStatus = entry.parseStatus;
        addWarning(entry.warningCodes, "entry_point_headline_parse_unavailable");
    }
    if (entry.empireState.parseStatus.empty()) {
        entry.empireState.parseStatus = entry.parseStatus;
    }

    entry.id =
        entry.campaignKey + "::" + stableKeyFromName(entry.saveName) + "::" +
        (entry.contentHash.empty() ? "missing_hash" : entry.contentHash.substr(0, std::min<std::size_t>(12, entry.contentHash.size())));
    return entry;
}

std::vector<SaveEntryPoint> scanEntryPoints(const std::vector<std::filesystem::path>& saveRoots, std::size_t& existingRootCount)
{
    std::vector<SaveEntryPoint> entries;
    existingRootCount = 0;

    for (const auto& root : saveRoots) {
        if (!directoryExists(root)) {
            continue;
        }
        ++existingRootCount;

        std::error_code error;
        const auto options = std::filesystem::directory_options::skip_permission_denied;
        for (auto iterator = std::filesystem::recursive_directory_iterator(root, options, error);
             iterator != std::filesystem::recursive_directory_iterator();
             iterator.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }

            std::error_code entryError;
            if (!iterator->is_regular_file(entryError) || entryError || !isSavFile(iterator->path())) {
                continue;
            }
            entries.push_back(makeEntryPoint(root, iterator->path()));
        }
    }

    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        if (left.campaignKey != right.campaignKey) {
            return left.campaignKey < right.campaignKey;
        }
        if (left.saveDate != right.saveDate) {
            return left.saveDate < right.saveDate;
        }
        return left.relativePath < right.relativePath;
    });
    return entries;
}

std::vector<ArchivedSaveEvidence> buildArchivedEvidence(
    const std::filesystem::path& sessionArchiveDirectory,
    const AutosaveArchiveVerificationResult& verification)
{
    std::vector<ArchivedSaveEvidence> evidence;
    for (const auto& file : verification.files) {
        ArchivedSaveEvidence item;
        item.archivedPath = file.path;
        item.saveName = std::filesystem::path(file.path).filename().string();
        item.saveDate = extractSaveDateFromText(item.saveName);
        item.campaignKey = campaignKeyFromArchivedFilename(std::filesystem::path(file.path));
        item.hashAlgorithm = "fnv1a64";
        item.contentHash = file.actualHash;
        item.byteCount = file.actualByteCount;
        evidence.push_back(std::move(item));
    }

    std::sort(evidence.begin(), evidence.end(), [](const auto& left, const auto& right) {
        if (left.campaignKey != right.campaignKey) {
            return left.campaignKey < right.campaignKey;
        }
        if (left.saveDate != right.saveDate) {
            return left.saveDate < right.saveDate;
        }
        if (left.contentHash != right.contentHash) {
            return left.contentHash < right.contentHash;
        }
        return left.archivedPath < right.archivedPath;
    });
    return evidence;
}

void addWarning(std::vector<std::string>& warnings, const std::string& warning)
{
    if (std::find(warnings.begin(), warnings.end(), warning) == warnings.end()) {
        warnings.push_back(warning);
    }
}

std::map<std::string, SaveEntryPointCampaignAnalysis> buildCampaignMap(
    const std::vector<SaveEntryPoint>& entries,
    const std::vector<ArchivedSaveEvidence>& evidence)
{
    std::map<std::string, SaveEntryPointCampaignAnalysis> campaigns;
    for (const auto& entry : entries) {
        auto& campaign = campaigns[entry.campaignKey];
        campaign.campaignKey = entry.campaignKey;
        ++campaign.entryPointCount;
    }
    for (const auto& item : evidence) {
        auto& campaign = campaigns[item.campaignKey];
        campaign.campaignKey = item.campaignKey;
        ++campaign.archivedEvidenceCount;
    }

    std::map<std::string, std::map<std::string, std::set<std::string>>> hashesByCampaignDate;
    for (const auto& item : evidence) {
        if (item.saveDate.empty() || item.contentHash.empty()) {
            continue;
        }
        hashesByCampaignDate[item.campaignKey][item.saveDate].insert(item.contentHash);
    }

    for (const auto& [campaignKey, byDate] : hashesByCampaignDate) {
        for (const auto& [date, hashes] : byDate) {
            if (hashes.size() > 1) {
                auto& campaign = campaigns[campaignKey];
                campaign.branchAmbiguityDetected = true;
                addWarning(campaign.warningCodes, "same_campaign_date_has_multiple_archived_hashes");
            }
        }
    }

    return campaigns;
}

void evaluateEntryPointAgainstEvidence(
    SaveEntryPoint& entry,
    const std::vector<ArchivedSaveEvidence>& evidence,
    const bool campaignAmbiguous)
{
    for (const auto& item : evidence) {
        if (item.campaignKey != entry.campaignKey) {
            continue;
        }

        if (!entry.contentHash.empty() && item.contentHash == entry.contentHash) {
            ++entry.compatibleArchivedEvidenceCount;
            appendEvidenceSample(entry.compatibleArchivedEvidenceSamples, item);
            continue;
        }

        if (dateLessOrEqual(item.saveDate, entry.saveDate)) {
            ++entry.compatibleArchivedEvidenceCount;
            appendEvidenceSample(entry.compatibleArchivedEvidenceSamples, item);
        } else if (dateGreater(item.saveDate, entry.saveDate)) {
            ++entry.laterArchivedEvidenceCount;
            appendEvidenceSample(entry.laterArchivedEvidenceSamples, item);
        }
    }

    entry.analysisState = "ready";
    if (entry.saveDate.empty()) {
        entry.analysisState = "needs_parse";
        addWarning(entry.warningCodes, "entry_point_save_date_unknown");
    }
    if (entry.laterArchivedEvidenceCount > 0) {
        entry.analysisState = "ready_conservative";
        addWarning(entry.warningCodes, "later_archived_evidence_excluded");
    }
    if (campaignAmbiguous) {
        entry.analysisState = "ambiguous";
        addWarning(entry.warningCodes, "campaign_branch_ambiguity_detected");
    }
    if (entry.compatibleArchivedEvidenceCount == 0) {
        addWarning(entry.warningCodes, "no_compatible_archived_evidence");
    }
    if (entry.compatibleArchivedEvidenceCount > entry.compatibleArchivedEvidenceSamples.size()) {
        addWarning(entry.warningCodes, "compatible_archived_evidence_samples_truncated");
    }
    if (entry.laterArchivedEvidenceCount > entry.laterArchivedEvidenceSamples.size()) {
        addWarning(entry.warningCodes, "later_archived_evidence_samples_truncated");
    }
}

void writeStringArray(std::ostringstream& output, const std::vector<std::string>& values)
{
    output << "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << "\"" << jsonEscape(values[index]) << "\"";
    }
    output << "]";
}

void writeArchivedEvidenceReferences(
    std::ostringstream& output,
    const std::vector<ArchivedSaveEvidenceReference>& references)
{
    output << "[\n";
    for (std::size_t index = 0; index < references.size(); ++index) {
        const auto& reference = references[index];
        output << "        {\n";
        output << "          \"archived_path\": \"" << jsonEscape(reference.archivedPath) << "\",\n";
        output << "          \"save_name\": \"" << jsonEscape(reference.saveName) << "\",\n";
        output << "          \"save_date\": \"" << jsonEscape(reference.saveDate) << "\",\n";
        output << "          \"content_hash\": \"" << jsonEscape(reference.contentHash) << "\",\n";
        output << "          \"byte_count\": " << reference.byteCount << "\n";
        output << "        }" << (index + 1 < references.size() ? "," : "") << "\n";
    }
    output << "      ]";
}

} // namespace

SaveEntryPointAnalysis SaveEntryPointAnalyzer::analyze(
    const std::filesystem::path& sessionArchiveDirectory,
    const std::vector<std::filesystem::path>& saveRoots) const
{
    SaveEntryPointAnalysis analysis;
    analysis.sessionArchiveDirectory = sessionArchiveDirectory;
    analysis.saveRoots = saveRoots;

    const AutosaveArchiveVerifier verifier;
    const auto verification = verifier.verify(sessionArchiveDirectory);
    analysis.archiveVerified = verification.ok;
    if (!verification.ok) {
        analysis.reason = verification.reason.empty() ? "archive verification failed" : verification.reason;
        analysis.readiness = "blocked";
        addWarning(analysis.warningCodes, "archive_verification_failed");
        return analysis;
    }

    analysis.archivedEvidence = buildArchivedEvidence(sessionArchiveDirectory, verification);
    analysis.archivedEvidenceCount = analysis.archivedEvidence.size();
    analysis.entryPoints = scanEntryPoints(saveRoots, analysis.existingRootCount);
    analysis.entryPointCount = analysis.entryPoints.size();

    auto campaignsByKey = buildCampaignMap(analysis.entryPoints, analysis.archivedEvidence);
    for (auto& [campaignKey, campaign] : campaignsByKey) {
        if (campaign.branchAmbiguityDetected) {
            analysis.branchAmbiguityDetected = true;
            addWarning(analysis.warningCodes, "campaign_branch_ambiguity_detected");
        }
    }

    for (auto& entry : analysis.entryPoints) {
        const auto campaign = campaignsByKey.find(entry.campaignKey);
        const bool campaignAmbiguous =
            campaign != campaignsByKey.end() && campaign->second.branchAmbiguityDetected;
        evaluateEntryPointAgainstEvidence(entry, analysis.archivedEvidence, campaignAmbiguous);
        for (const auto& warning : entry.warningCodes) {
            addWarning(analysis.warningCodes, warning);
        }
    }

    for (const auto& [campaignKey, campaign] : campaignsByKey) {
        analysis.campaigns.push_back(campaign);
    }

    if (analysis.entryPointCount == 0) {
        analysis.ok = true;
        analysis.reason = "archive verified but no loadable save entry points found";
        analysis.readiness = "needs_attention";
        addWarning(analysis.warningCodes, "no_loadable_entry_points_found");
        return analysis;
    }

    analysis.ok = true;
    if (analysis.branchAmbiguityDetected) {
        analysis.reason = "entry points scanned; branch ambiguity requires conservative evidence selection";
        analysis.readiness = "ambiguous";
    } else if (std::find(analysis.warningCodes.begin(), analysis.warningCodes.end(), "later_archived_evidence_excluded") !=
               analysis.warningCodes.end()) {
        analysis.reason = "entry points scanned; later archived evidence excluded where needed";
        analysis.readiness = "ready_conservative";
    } else {
        analysis.reason = "entry points scanned";
        analysis.readiness = "ready";
    }
    return analysis;
}

std::string serializeSaveEntryPointAnalysis(const SaveEntryPointAnalysis& analysis)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema_version\": " << analysis.schemaVersion << ",\n";
    json << "  \"source_schema_version\": " << analysis.sourceSchemaVersion << ",\n";
    json << "  \"schema_compatibility_state\": \"" << jsonEscape(analysis.schemaCompatibilityState) << "\",\n";
    json << "  \"schema_compatibility_note\": \"" << jsonEscape(analysis.schemaCompatibilityNote) << "\",\n";
    json << "  \"ok\": " << (analysis.ok ? "true" : "false") << ",\n";
    json << "  \"reason\": \"" << jsonEscape(analysis.reason) << "\",\n";
    json << "  \"readiness\": \"" << jsonEscape(analysis.readiness) << "\",\n";
    json << "  \"archive_verified\": " << (analysis.archiveVerified ? "true" : "false") << ",\n";
    json << "  \"session_archive_directory\": \"" << jsonEscape(genericPath(analysis.sessionArchiveDirectory)) << "\",\n";
    json << "  \"existing_root_count\": " << analysis.existingRootCount << ",\n";
    json << "  \"entry_point_count\": " << analysis.entryPointCount << ",\n";
    json << "  \"archived_evidence_count\": " << analysis.archivedEvidenceCount << ",\n";
    json << "  \"branch_ambiguity_detected\": " << (analysis.branchAmbiguityDetected ? "true" : "false") << ",\n";
    json << "  \"warning_codes\": ";
    writeStringArray(json, analysis.warningCodes);
    json << ",\n";
    json << "  \"save_roots\": [\n";
    for (std::size_t index = 0; index < analysis.saveRoots.size(); ++index) {
        json << "    \"" << jsonEscape(genericPath(analysis.saveRoots[index])) << "\""
             << (index + 1 < analysis.saveRoots.size() ? "," : "") << "\n";
    }
    json << "  ],\n";
    json << "  \"campaigns\": [\n";
    for (std::size_t index = 0; index < analysis.campaigns.size(); ++index) {
        const auto& campaign = analysis.campaigns[index];
        json << "    {\n";
        json << "      \"campaign_key\": \"" << jsonEscape(campaign.campaignKey) << "\",\n";
        json << "      \"entry_point_count\": " << campaign.entryPointCount << ",\n";
        json << "      \"archived_evidence_count\": " << campaign.archivedEvidenceCount << ",\n";
        json << "      \"branch_ambiguity_detected\": " << (campaign.branchAmbiguityDetected ? "true" : "false") << ",\n";
        json << "      \"warning_codes\": ";
        writeStringArray(json, campaign.warningCodes);
        json << "\n";
        json << "    }" << (index + 1 < analysis.campaigns.size() ? "," : "") << "\n";
    }
    json << "  ],\n";
    json << "  \"entry_points\": [\n";
    for (std::size_t index = 0; index < analysis.entryPoints.size(); ++index) {
        const auto& entry = analysis.entryPoints[index];
        json << "    {\n";
        json << "      \"id\": \"" << jsonEscape(entry.id) << "\",\n";
        json << "      \"campaign_key\": \"" << jsonEscape(entry.campaignKey) << "\",\n";
        json << "      \"source_root\": \"" << jsonEscape(entry.sourceRoot) << "\",\n";
        json << "      \"relative_path\": \"" << jsonEscape(entry.relativePath) << "\",\n";
        json << "      \"source_kind\": \"" << jsonEscape(entry.sourceKind) << "\",\n";
        json << "      \"save_name\": \"" << jsonEscape(entry.saveName) << "\",\n";
        json << "      \"save_date\": \"" << jsonEscape(entry.saveDate) << "\",\n";
        json << "      \"hash_algorithm\": \"" << jsonEscape(entry.hashAlgorithm) << "\",\n";
        json << "      \"content_hash\": \"" << jsonEscape(entry.contentHash) << "\",\n";
        json << "      \"byte_count\": " << entry.byteCount << ",\n";
        json << "      \"parse_status\": \"" << jsonEscape(entry.parseStatus) << "\",\n";
        json << "      \"player_country_id\": \"" << jsonEscape(entry.playerCountryId) << "\",\n";
        json << "      \"empire_name\": \"" << jsonEscape(entry.empireName) << "\",\n";
        json << "      \"empire_state_parsed\": " << (entry.empireState.parsed ? "true" : "false") << ",\n";
        json << "      \"empire_state_parse_status\": \"" << jsonEscape(entry.empireState.parseStatus) << "\",\n";
        json << "      \"empire_state_parser_reason\": \"" << jsonEscape(entry.empireState.parserReason) << "\",\n";
        json << "      \"empire_state_government\": \"" << jsonEscape(entry.empireState.government) << "\",\n";
        json << "      \"empire_state_authority\": \"" << jsonEscape(entry.empireState.authority) << "\",\n";
        json << "      \"empire_state_founder_species_name\": \"" << jsonEscape(entry.empireState.founderSpeciesName) << "\",\n";
        json << "      \"empire_state_capital_planet_name\": \"" << jsonEscape(entry.empireState.capitalPlanetName) << "\",\n";
        json << "      \"empire_state_home_system_name\": \"" << jsonEscape(entry.empireState.homeSystemName) << "\",\n";
        json << "      \"empire_state_owned_fleet_count\": " << entry.empireState.ownedFleetCount << ",\n";
        json << "      \"empire_state_active_war_count\": " << entry.empireState.activeWarCount << ",\n";
        json << "      \"empire_state_missing_fields\": ";
        writeStringArray(json, entry.empireState.missingFields);
        json << ",\n";
        json << "      \"empire_state_uncertainties\": ";
        writeStringArray(json, entry.empireState.uncertainties);
        json << ",\n";
        json << "      \"compatible_archived_evidence_count\": " << entry.compatibleArchivedEvidenceCount << ",\n";
        json << "      \"later_archived_evidence_count\": " << entry.laterArchivedEvidenceCount << ",\n";
        json << "      \"compatible_archived_evidence_samples\": ";
        writeArchivedEvidenceReferences(json, entry.compatibleArchivedEvidenceSamples);
        json << ",\n";
        json << "      \"later_archived_evidence_samples\": ";
        writeArchivedEvidenceReferences(json, entry.laterArchivedEvidenceSamples);
        json << ",\n";
        json << "      \"analysis_state\": \"" << jsonEscape(entry.analysisState) << "\",\n";
        json << "      \"warning_codes\": ";
        writeStringArray(json, entry.warningCodes);
        json << "\n";
        json << "    }" << (index + 1 < analysis.entryPoints.size() ? "," : "") << "\n";
    }
    json << "  ],\n";
    json << "  \"archived_evidence\": [\n";
    for (std::size_t index = 0; index < analysis.archivedEvidence.size(); ++index) {
        const auto& item = analysis.archivedEvidence[index];
        json << "    {\n";
        json << "      \"campaign_key\": \"" << jsonEscape(item.campaignKey) << "\",\n";
        json << "      \"archived_path\": \"" << jsonEscape(item.archivedPath) << "\",\n";
        json << "      \"save_name\": \"" << jsonEscape(item.saveName) << "\",\n";
        json << "      \"save_date\": \"" << jsonEscape(item.saveDate) << "\",\n";
        json << "      \"hash_algorithm\": \"" << jsonEscape(item.hashAlgorithm) << "\",\n";
        json << "      \"content_hash\": \"" << jsonEscape(item.contentHash) << "\",\n";
        json << "      \"byte_count\": " << item.byteCount << "\n";
        json << "    }" << (index + 1 < analysis.archivedEvidence.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace strategic_nexus
