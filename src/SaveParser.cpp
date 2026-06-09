// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "SaveParser.h"

#include "common/FileUtil.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace strategic_nexus {
namespace {

constexpr std::uintmax_t maxGamestateBytes = 256ull * 1024ull * 1024ull;
constexpr std::size_t maxHeadlineFleets = 16;

struct SavePayload {
    bool ok = false;
    std::string reason;
    bool sourceWasArchive = false;
    std::filesystem::path root;
    std::filesystem::path cleanupRoot;
};

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (ch < 0x20) {
                output << "\\u00";
                const char* hex = "0123456789abcdef";
                output << hex[(ch >> 4) & 0x0f] << hex[ch & 0x0f];
            } else {
                output << static_cast<char>(ch);
            }
            break;
        }
    }
    return output.str();
}

void appendJsonString(std::ostringstream& output, const std::string& key, const std::string& value, bool& first)
{
    if (!first) {
        output << ",\n";
    }
    first = false;
    output << "  \"" << key << "\": \"" << jsonEscape(value) << "\"";
}

void appendJsonBool(std::ostringstream& output, const std::string& key, const bool value, bool& first)
{
    if (!first) {
        output << ",\n";
    }
    first = false;
    output << "  \"" << key << "\": " << (value ? "true" : "false");
}

void appendJsonNumber(std::ostringstream& output, const std::string& key, const std::uintmax_t value, bool& first)
{
    if (!first) {
        output << ",\n";
    }
    first = false;
    output << "  \"" << key << "\": " << value;
}

void appendJsonStringArray(std::ostringstream& output, const std::string& key, const std::vector<std::string>& values, bool& first)
{
    if (!first) {
        output << ",\n";
    }
    first = false;
    output << "  \"" << key << "\": [";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << "\"" << jsonEscape(values[index]) << "\"";
    }
    output << "]";
}

void appendJsonFieldAvailabilityArray(
    std::ostringstream& output,
    const std::vector<SaveParserFieldAvailability>& values,
    bool& first)
{
    if (!first) {
        output << ",\n";
    }
    first = false;
    output << "  \"field_availability\": [";
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto& value = values[index];
        if (index > 0) {
            output << ", ";
        }
        output << "{"
               << "\"field_group\":\"" << jsonEscape(value.fieldGroup) << "\","
               << "\"source_quality\":\"" << jsonEscape(value.sourceQuality) << "\","
               << "\"available\":" << (value.available ? "true" : "false") << ",";
        output << "\"missing_reasons\":[";
        for (std::size_t reasonIndex = 0; reasonIndex < value.missingReasons.size(); ++reasonIndex) {
            if (reasonIndex > 0) {
                output << ", ";
            }
            output << "\"" << jsonEscape(value.missingReasons[reasonIndex]) << "\"";
        }
        output << "]}";
    }
    output << "]";
}

void appendMissingIfEmpty(SaveParserSummary& summary, const std::string& fieldName, const std::string& value)
{
    if (value.empty()) {
        summary.missingFields.push_back(fieldName);
    }
}

std::string shellQuote(const std::filesystem::path& path)
{
    const std::string value = path.string();
    if (value.find('"') != std::string::npos) {
        return {};
    }
    return "\"" + value + "\"";
}

bool runTarExtractQuietly(
    const std::filesystem::path& savePath,
    const std::filesystem::path& destination)
{
#ifdef _WIN32
    const auto archive = savePath.wstring();
    const auto output = destination.wstring();
    if (archive.find(L'"') != std::wstring::npos ||
        output.find(L'"') != std::wstring::npos) {
        return false;
    }

    std::wstring command = L"tar -xf \"" + archive + L"\" -C \"" + output + L"\" meta gamestate";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(
            nullptr,
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process) == 0) {
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(process.hProcess, 60000);
    DWORD exitCode = 1;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(process.hProcess, &exitCode);
    } else {
        TerminateProcess(process.hProcess, 1);
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return waitResult == WAIT_OBJECT_0 && exitCode == 0;
#else
    const std::string archive = shellQuote(savePath);
    const std::string output = shellQuote(destination);
    if (archive.empty() || output.empty()) {
        return false;
    }

    const std::string command = "tar -xf " + archive + " -C " + output + " meta gamestate >/dev/null 2>/dev/null";
    return std::system(command.c_str()) == 0;
#endif
}

SavePayload preparePayloadRoot(const std::filesystem::path& savePath)
{
    SavePayload payload;
    if (savePath.empty()) {
        payload.reason = "missing save path";
        return payload;
    }

    std::error_code ec;
    if (std::filesystem::is_directory(savePath, ec) && !ec) {
        payload.ok = true;
        payload.root = savePath;
        payload.sourceWasArchive = false;
        return payload;
    }

    if (!std::filesystem::exists(savePath, ec) || ec) {
        payload.reason = "save path does not exist";
        return payload;
    }

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    payload.cleanupRoot = std::filesystem::temp_directory_path(ec) /
        ("strategic_nexus_save_parse_" + std::to_string(stamp));
    if (ec) {
        payload.reason = "failed to resolve temporary directory";
        return payload;
    }

    std::filesystem::create_directories(payload.cleanupRoot, ec);
    if (ec) {
        payload.reason = "failed to create temporary extraction directory";
        return payload;
    }

    if (!runTarExtractQuietly(savePath, payload.cleanupRoot)) {
        std::filesystem::remove_all(payload.cleanupRoot, ec);
        payload.cleanupRoot.clear();
        payload.reason = "save archive extraction failed";
        return payload;
    }

    payload.ok = true;
    payload.root = payload.cleanupRoot;
    payload.sourceWasArchive = true;
    return payload;
}

std::string readBoundedText(const std::filesystem::path& path, const std::uintmax_t maxBytes, bool& ok, std::string& reason)
{
    ok = false;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        reason = "missing file";
        return {};
    }
    if (std::filesystem::is_directory(path, ec) || ec) {
        reason = "path is not a file";
        return {};
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        reason = "failed to inspect file size";
        return {};
    }
    if (size > maxBytes) {
        reason = "file exceeds parser byte limit";
        return {};
    }
    ok = true;
    return common::readTextFile(path);
}

std::string findBlock(const std::string& text, const std::string& key);

std::string regexValue(const std::string& text, const std::string& key)
{
    const std::regex pattern("(^|\\n)\\s*" + key + "\\s*=\\s*(?:\"([^\"]*)\"|([^\\s{}]+))");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return {};
    }
    if (match.size() > 2 && match[2].matched) {
        return match[2].str();
    }
    if (match.size() > 3 && match[3].matched) {
        return match[3].str();
    }
    return {};
}

std::string localizedOrScalarValue(const std::string& text, const std::string& key)
{
    const std::string nested = findBlock(text, key);
    if (!nested.empty()) {
        const std::string nestedKey = regexValue(nested, "key");
        if (!nestedKey.empty()) {
            return nestedKey;
        }
    }

    return regexValue(text, key);
}

std::string blockAtBrace(const std::string& text, const std::size_t openBrace)
{
    if (openBrace == std::string::npos || openBrace >= text.size() || text[openBrace] != '{') {
        return {};
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = openBrace; index < text.size(); ++index) {
        const char ch = text[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return text.substr(openBrace, index - openBrace + 1);
            }
        }
    }

    return {};
}

std::string findBlock(const std::string& text, const std::string& key)
{
    const std::regex pattern("(^|\\n)\\s*" + key + "\\s*=\\s*\\{");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return {};
    }
    const std::size_t openBrace = static_cast<std::size_t>(match.position(0) + match.length(0) - 1);
    return blockAtBrace(text, openBrace);
}

std::string findNumberedChildBlock(const std::string& text, const std::string& id)
{
    const std::regex pattern("(^|\\n)\\s*" + id + "\\s*=\\s*\\{");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return {};
    }
    const std::size_t openBrace = static_cast<std::size_t>(match.position(0) + match.length(0) - 1);
    return blockAtBrace(text, openBrace);
}

std::vector<std::pair<std::string, std::string>> numberedChildren(const std::string& text, const std::size_t limit = 0)
{
    std::vector<std::pair<std::string, std::string>> children;
    const std::regex pattern("(^|\\n)\\s*(\\d+)\\s*=\\s*\\{");
    auto begin = std::sregex_iterator(text.begin(), text.end(), pattern);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const auto& match = *it;
        const std::size_t openBrace = static_cast<std::size_t>(match.position(0) + match.length(0) - 1);
        children.push_back({ match[2].str(), blockAtBrace(text, openBrace) });
        if (limit > 0 && children.size() >= limit) {
            break;
        }
    }
    return children;
}

void parsePlayer(SaveParserSummary& summary, const std::string& gamestate)
{
    const std::string playerBlock = findBlock(gamestate, "player");
    const std::regex playerPattern("name\\s*=\\s*\"([^\"]*)\"\\s+country\\s*=\\s*(\\d+)");
    std::smatch match;
    if (std::regex_search(playerBlock, match, playerPattern)) {
        summary.playerName = match[1].str();
        summary.playerCountryId = match[2].str();
    }
}

void parseCountry(SaveParserSummary& summary, const std::string& gamestate)
{
    if (summary.playerCountryId.empty()) {
        return;
    }

    const std::string countriesBlock = findBlock(gamestate, "country");
    const std::string countryBlock = findNumberedChildBlock(countriesBlock, summary.playerCountryId);
    if (countryBlock.empty()) {
        summary.missingFields.push_back("player_country_block");
        return;
    }

    summary.empireName = localizedOrScalarValue(countryBlock, "name");

    const std::string governmentBlock = findBlock(countryBlock, "government");
    summary.government = regexValue(governmentBlock, "type");
    summary.authority = regexValue(governmentBlock, "authority");
    if (summary.government.empty()) {
        summary.government = regexValue(countryBlock, "government");
    }
    if (summary.authority.empty()) {
        summary.authority = regexValue(countryBlock, "authority");
    }

    summary.founderSpeciesId = regexValue(countryBlock, "founder_species_ref");
    if (summary.founderSpeciesId.empty()) {
        summary.founderSpeciesId = regexValue(countryBlock, "founder_species");
    }
    summary.capitalPlanetId = regexValue(countryBlock, "capital");
    summary.homeSystemId = regexValue(countryBlock, "starting_system");
    if (summary.homeSystemId.empty()) {
        summary.homeSystemId = regexValue(countryBlock, "home_system");
    }
}

void parseNamedReference(
    std::string& nameOutput,
    const std::string& gamestate,
    const std::string& blockName,
    const std::string& id)
{
    if (id.empty()) {
        return;
    }

    const std::string collection = findBlock(gamestate, blockName);
    const std::string item = findNumberedChildBlock(collection, id);
    nameOutput = localizedOrScalarValue(item, "name");
}

void parsePlanetReference(std::string& nameOutput, const std::string& gamestate, const std::string& id)
{
    if (id.empty()) {
        return;
    }

    const std::string planetsBlock = findBlock(gamestate, "planets");
    const std::string planetCollection = findBlock(planetsBlock, "planet");
    const std::string item = findNumberedChildBlock(planetCollection, id);
    nameOutput = localizedOrScalarValue(item, "name");
}

std::vector<std::string> parseOwnedFleetIds(const std::string& countryBlock)
{
    std::vector<std::string> ids;
    const std::string ownedFleets = findBlock(countryBlock, "owned_fleets");
    const std::regex fleetPattern("fleet\\s*=\\s*(\\d+)");
    auto begin = std::sregex_iterator(ownedFleets.begin(), ownedFleets.end(), fleetPattern);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        ids.push_back((*it)[1].str());
    }
    return ids;
}

void parseFleets(SaveParserSummary& summary, const std::string& gamestate)
{
    if (summary.playerCountryId.empty()) {
        return;
    }

    const std::string countriesBlock = findBlock(gamestate, "country");
    const std::string countryBlock = findNumberedChildBlock(countriesBlock, summary.playerCountryId);
    const std::vector<std::string> ownedFleetIds = parseOwnedFleetIds(countryBlock);
    const std::string fleetsBlock = findBlock(gamestate, "fleet");
    if (fleetsBlock.empty()) {
        summary.uncertainties.push_back("fleet_block_missing_or_not_named_fleet");
        return;
    }

    if (!ownedFleetIds.empty()) {
        summary.ownedFleetCount = ownedFleetIds.size();
        for (const auto& fleetId : ownedFleetIds) {
            if (summary.ownedFleets.size() >= maxHeadlineFleets) {
                break;
            }
            const std::string fleetBlock = findNumberedChildBlock(fleetsBlock, fleetId);
            if (fleetBlock.empty()) {
                summary.uncertainties.push_back("owned_fleet_id_missing_from_fleet_block:" + fleetId);
                continue;
            }

            SaveParserFleetHeadline fleet;
            fleet.id = fleetId;
            fleet.ownerCountryId = summary.playerCountryId;
            fleet.name = localizedOrScalarValue(fleetBlock, "name");
            if (fleet.name.empty()) {
                fleet.name = regexValue(fleetBlock, "shipclass");
            }
            fleet.militaryPower = regexValue(fleetBlock, "military_power");
            if (fleet.militaryPower.empty()) {
                fleet.militaryPower = regexValue(fleetBlock, "fleet_power");
            }
            summary.ownedFleets.push_back(fleet);
        }
        return;
    }

    for (const auto& child : numberedChildren(fleetsBlock)) {
        const std::string owner = regexValue(child.second, "owner");
        if (owner != summary.playerCountryId) {
            continue;
        }

        ++summary.ownedFleetCount;
        if (summary.ownedFleets.size() >= maxHeadlineFleets) {
            continue;
        }

        SaveParserFleetHeadline fleet;
        fleet.id = child.first;
        fleet.ownerCountryId = owner;
        fleet.name = localizedOrScalarValue(child.second, "name");
        fleet.militaryPower = regexValue(child.second, "military_power");
        if (fleet.militaryPower.empty()) {
            fleet.militaryPower = regexValue(child.second, "fleet_power");
        }
        summary.ownedFleets.push_back(fleet);
    }
}

void parseWars(SaveParserSummary& summary, const std::string& gamestate)
{
    const std::string warsBlock = findBlock(gamestate, "war");
    if (warsBlock.empty()) {
        return;
    }
    summary.activeWarCount = numberedChildren(warsBlock).size();
}

std::vector<std::string> buildCountryCoreMissingReasons(const SaveParserSummary& summary)
{
    std::vector<std::string> reasons;
    if (!summary.ok) {
        reasons.push_back(summary.reason.empty()
            ? "headline_summary_unavailable"
            : ("headline_summary_unavailable: " + summary.reason));
        return reasons;
    }

    if (summary.government.empty()) {
        reasons.push_back("country_core_missing_government");
    }
    if (summary.authority.empty()) {
        reasons.push_back("country_core_missing_authority");
    }
    if (summary.founderSpeciesName.empty()) {
        reasons.push_back("country_core_missing_founder_species_name");
    }
    if (summary.capitalPlanetName.empty()) {
        reasons.push_back("country_core_missing_capital_planet_name");
    }
    if (summary.homeSystemName.empty()) {
        reasons.push_back("country_core_missing_home_system_name");
    }

    if (reasons.empty()) {
        reasons.push_back("country_core_fields_unavailable_for_unknown_reason");
    }

    return reasons;
}

std::vector<SaveParserFieldAvailability> buildFieldAvailability(const SaveParserSummary& summary)
{
    std::vector<SaveParserFieldAvailability> availability;
    availability.push_back({
        "identity",
        "headline_identity",
        summary.ok && !summary.playerCountryId.empty() && !summary.empireName.empty(),
        summary.ok && !summary.playerCountryId.empty() && !summary.empireName.empty()
            ? std::vector<std::string>{}
            : std::vector<std::string>{ "player identity not yet resolved from headline summary" }
    });
    availability.push_back({
        "headline",
        "headline_fields_only",
        summary.ok,
        summary.ok ? std::vector<std::string>{} : std::vector<std::string>{ summary.reason }
    });
    availability.push_back({
        "country_core",
        summary.ok && !summary.government.empty() && !summary.authority.empty() &&
            !summary.founderSpeciesName.empty() && !summary.capitalPlanetName.empty() && !summary.homeSystemName.empty()
            ? "headline_country_core"
            : "headline_country_core_partial",
        summary.ok && !summary.government.empty() && !summary.authority.empty() &&
            !summary.founderSpeciesName.empty() && !summary.capitalPlanetName.empty() && !summary.homeSystemName.empty(),
        summary.ok && !summary.government.empty() && !summary.authority.empty() &&
            !summary.founderSpeciesName.empty() && !summary.capitalPlanetName.empty() && !summary.homeSystemName.empty()
            ? std::vector<std::string>{}
            : buildCountryCoreMissingReasons(summary)
    });
    availability.push_back({
        "owned_fleets",
        "headline_owned_fleets",
        summary.ok,
        summary.ok ? std::vector<std::string>{} : std::vector<std::string>{ "owned fleet headlines unavailable without parsed headline summary" }
    });
    availability.push_back({
        "war",
        "headline_war_count",
        summary.ok,
        summary.ok ? std::vector<std::string>{} : std::vector<std::string>{ "active war count unavailable without parsed headline summary" }
    });
    availability.push_back({
        "diplomacy",
        "not_extracted_yet",
        false,
        { "diplomatic_relationships_not_extracted_yet", "parser_extracts_headline_fields_only" }
    });
    availability.push_back({
        "subject",
        "not_extracted_yet",
        false,
        { "subject_relationships_not_extracted_yet", "parser_extracts_headline_fields_only" }
    });
    availability.push_back({
        "federation",
        "not_extracted_yet",
        false,
        { "federation_relationships_not_extracted_yet", "parser_extracts_headline_fields_only" }
    });
    availability.push_back({
        "border",
        "not_extracted_yet",
        false,
        { "border_relationships_not_extracted_yet", "parser_extracts_headline_fields_only" }
    });
    availability.push_back({
        "intel",
        "not_extracted_yet",
        false,
        { "intel_relationships_not_extracted_yet", "parser_extracts_headline_fields_only" }
    });
    return availability;
}

} // namespace

GalaxyState SaveParser::parseSnapshot(const std::filesystem::path& savePath) const
{
    const SaveParserSummary parsed = parseSummary(savePath);

    GalaxyState state;
    if (!parsed.saveDate.empty()) {
        try {
            state.year = std::stoi(parsed.saveDate.substr(0, 4));
        } catch (...) {
            state.year = 2200;
        }
    }

    EmpireState player;
    player.id = parsed.playerCountryId.empty() ? "player" : ("country_" + parsed.playerCountryId);
    player.displayName = parsed.empireName.empty()
        ? (parsed.playerName.empty() ? "Player Empire" : parsed.playerName)
        : parsed.empireName;
    player.power.fleetPower = static_cast<int>(parsed.ownedFleetCount * 100);
    player.power.economicRank = 1;
    player.power.technologyRank = 1;
    player.power.diplomaticInfluence = 1;
    state.empires.push_back(player);

    if (!parsed.reason.empty()) {
        state.history.push_back({ state.year, parsed.ok ? "save_parser_summary" : "save_parser_error", player.id, parsed.reason, parsed.ok ? 0.1 : 1.0 });
    }

    return state;
}

SaveParserSummary SaveParser::parseSummary(const std::filesystem::path& savePath) const
{
    SaveParserSummary summary;
    SavePayload payload = preparePayloadRoot(savePath);
    summary.sourceWasArchive = payload.sourceWasArchive;
    if (!payload.ok) {
        summary.reason = payload.reason;
        return summary;
    }

    const std::filesystem::path metaPath = payload.root / "meta";
    const std::filesystem::path gamestatePath = payload.root / "gamestate";

    std::error_code ec;
    summary.hasMeta = std::filesystem::exists(metaPath, ec) && !ec;
    summary.hasGamestate = std::filesystem::exists(gamestatePath, ec) && !ec;
    if (summary.hasMeta) {
        summary.metaByteCount = std::filesystem::file_size(metaPath, ec);
        if (ec) {
            summary.metaByteCount = 0;
        }
    }
    if (summary.hasGamestate) {
        summary.gamestateByteCount = std::filesystem::file_size(gamestatePath, ec);
        if (ec) {
            summary.gamestateByteCount = 0;
        }
    }

    if (!summary.hasMeta || !summary.hasGamestate) {
        summary.reason = "save payload must contain meta and gamestate";
        if (!payload.cleanupRoot.empty()) {
            std::filesystem::remove_all(payload.cleanupRoot, ec);
        }
        return summary;
    }

    bool readOk = false;
    std::string readReason;
    const std::string meta = readBoundedText(metaPath, 1024 * 1024, readOk, readReason);
    if (!readOk) {
        summary.reason = "failed to read meta: " + readReason;
        if (!payload.cleanupRoot.empty()) {
            std::filesystem::remove_all(payload.cleanupRoot, ec);
        }
        return summary;
    }

    const std::string gamestate = readBoundedText(gamestatePath, maxGamestateBytes, readOk, readReason);
    if (!readOk) {
        summary.reason = "failed to read gamestate: " + readReason;
        if (!payload.cleanupRoot.empty()) {
            std::filesystem::remove_all(payload.cleanupRoot, ec);
        }
        return summary;
    }

    summary.saveVersion = regexValue(meta, "version");
    summary.saveRevision = regexValue(meta, "revision");
    summary.saveName = regexValue(meta, "name");
    summary.saveDate = regexValue(gamestate, "date");
    if (summary.saveDate.empty()) {
        summary.saveDate = regexValue(meta, "date");
    }

    parsePlayer(summary, gamestate);
    parseCountry(summary, gamestate);
    parseNamedReference(summary.founderSpeciesName, gamestate, "species_db", summary.founderSpeciesId);
    if (summary.founderSpeciesName.empty()) {
        parseNamedReference(summary.founderSpeciesName, gamestate, "species", summary.founderSpeciesId);
    }
    parsePlanetReference(summary.capitalPlanetName, gamestate, summary.capitalPlanetId);
    if (summary.capitalPlanetName.empty()) {
        parseNamedReference(summary.capitalPlanetName, gamestate, "planet", summary.capitalPlanetId);
    }
    parseNamedReference(summary.homeSystemName, gamestate, "galactic_object", summary.homeSystemId);
    parseFleets(summary, gamestate);
    parseWars(summary, gamestate);

    appendMissingIfEmpty(summary, "save_version", summary.saveVersion);
    appendMissingIfEmpty(summary, "save_date", summary.saveDate);
    appendMissingIfEmpty(summary, "player_country_id", summary.playerCountryId);
    appendMissingIfEmpty(summary, "empire_name", summary.empireName);
    appendMissingIfEmpty(summary, "founder_species_name", summary.founderSpeciesName);
    appendMissingIfEmpty(summary, "capital_planet_name", summary.capitalPlanetName);
    appendMissingIfEmpty(summary, "home_system_name", summary.homeSystemName);

    summary.uncertainties.push_back("parser_extracts_headline_fields_only");
    summary.uncertainties.push_back("llm_input_must_use_this_bounded_summary_not_raw_gamestate");
    summary.ok = summary.missingFields.empty() || !summary.playerCountryId.empty();
    summary.reason = summary.ok ? "parsed headline summary" : "parsed with missing required player identity";
    summary.fieldAvailability = buildFieldAvailability(summary);

    if (!payload.cleanupRoot.empty()) {
        std::filesystem::remove_all(payload.cleanupRoot, ec);
    }

    return summary;
}

std::string SaveParser::serializeSummaryJson(const SaveParserSummary& summary) const
{
    std::ostringstream output;
    bool first = true;
    output << "{\n";
    appendJsonBool(output, "ok", summary.ok, first);
    appendJsonString(output, "reason", summary.reason, first);
    appendJsonBool(output, "source_was_archive", summary.sourceWasArchive, first);
    appendJsonBool(output, "has_meta", summary.hasMeta, first);
    appendJsonBool(output, "has_gamestate", summary.hasGamestate, first);
    appendJsonNumber(output, "meta_byte_count", summary.metaByteCount, first);
    appendJsonNumber(output, "gamestate_byte_count", summary.gamestateByteCount, first);
    appendJsonString(output, "save_version", summary.saveVersion, first);
    appendJsonString(output, "save_revision", summary.saveRevision, first);
    appendJsonString(output, "save_name", summary.saveName, first);
    appendJsonString(output, "save_date", summary.saveDate, first);
    appendJsonString(output, "player_name", summary.playerName, first);
    appendJsonString(output, "player_country_id", summary.playerCountryId, first);
    appendJsonString(output, "empire_name", summary.empireName, first);
    appendJsonString(output, "government", summary.government, first);
    appendJsonString(output, "authority", summary.authority, first);
    appendJsonString(output, "founder_species_id", summary.founderSpeciesId, first);
    appendJsonString(output, "founder_species_name", summary.founderSpeciesName, first);
    appendJsonString(output, "capital_planet_id", summary.capitalPlanetId, first);
    appendJsonString(output, "capital_planet_name", summary.capitalPlanetName, first);
    appendJsonString(output, "home_system_id", summary.homeSystemId, first);
    appendJsonString(output, "home_system_name", summary.homeSystemName, first);
    appendJsonNumber(output, "owned_fleet_count", summary.ownedFleetCount, first);
    appendJsonNumber(output, "active_war_count", summary.activeWarCount, first);
    appendJsonFieldAvailabilityArray(output, summary.fieldAvailability, first);

    if (!first) {
        output << ",\n";
    }
    first = false;
    output << "  \"owned_fleets\": [";
    for (std::size_t index = 0; index < summary.ownedFleets.size(); ++index) {
        const auto& fleet = summary.ownedFleets[index];
        if (index > 0) {
            output << ", ";
        }
        output << "{"
               << "\"id\":\"" << jsonEscape(fleet.id) << "\","
               << "\"name\":\"" << jsonEscape(fleet.name) << "\","
               << "\"owner_country_id\":\"" << jsonEscape(fleet.ownerCountryId) << "\","
               << "\"military_power\":\"" << jsonEscape(fleet.militaryPower) << "\""
               << "}";
    }
    output << "]";

    appendJsonStringArray(output, "missing_fields", summary.missingFields, first);
    appendJsonStringArray(output, "uncertainties", summary.uncertainties, first);
    output << "\n}\n";
    return output.str();
}

std::string SaveParser::parseSummaryJson(const std::filesystem::path& savePath) const
{
    return serializeSummaryJson(parseSummary(savePath));
}

} // namespace strategic_nexus
