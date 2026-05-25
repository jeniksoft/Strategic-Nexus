// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "OverlayCompiler.h"

#include "ManifestVerifier.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace strategic_nexus::generated_overlay {
namespace {

std::string symbol(const std::string& value)
{
    std::string output;
    for (const char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            output.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else {
            output.push_back('_');
        }
    }
    return output;
}

std::string ruleSymbol(const DslRule& rule)
{
    return symbol(rule.campaignId + "_" + rule.empireId + "_" + rule.ruleId);
}

std::string flagForPreference(const DslPreference& preference)
{
    return "strategic_nexus_pref_" + symbol(preference.domain) + "_" + symbol(preference.value);
}

std::string jsonEscape(const std::string_view value)
{
    std::ostringstream output;
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            output << "\\\\";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(static_cast<unsigned char>(ch))
                       << std::dec << std::setfill(' ');
            } else {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

struct ManifestEntry {
    std::string path;
    std::string role;
    std::string checksumRelevance;
    std::string text;
};

std::string buildManifest(const DslProgram& program, const std::vector<ManifestEntry>& entries)
{
    std::vector<std::string> campaignIds;
    for (const auto& rule : program.rules) {
        if (std::find(campaignIds.begin(), campaignIds.end(), rule.campaignId) == campaignIds.end()) {
            campaignIds.push_back(rule.campaignId);
        }
    }
    std::sort(campaignIds.begin(), campaignIds.end());

    std::ostringstream manifest;
    manifest << "{\n";
    manifest << "  \"schema_version\": 1,\n";
    manifest << "  \"generator\": \"strategic_nexus_generated_overlay_v0\",\n";
    manifest << "  \"snapshot_kind\": \"complete_replacement\",\n";
    manifest << "  \"rule_count\": " << program.rules.size() << ",\n";
    manifest << "  \"campaign_ids\": [";
    for (std::size_t i = 0; i < campaignIds.size(); ++i) {
        if (i > 0) {
            manifest << ", ";
        }
        manifest << "\"" << jsonEscape(campaignIds[i]) << "\"";
    }
    manifest << "],\n";
    manifest << "  \"multiplayer_requirement\": \"byte_identical_gameplay_affecting_files\",\n";
    manifest << "  \"checksum_policy\": \"generated_gameplay_files_assumed_checksum_relevant_until_proven_otherwise\",\n";
    manifest << "  \"files\": [\n";
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        manifest << "    {\n";
        manifest << "      \"path\": \"" << jsonEscape(entry.path) << "\",\n";
        manifest << "      \"role\": \"" << jsonEscape(entry.role) << "\",\n";
        manifest << "      \"checksum_relevance\": \"" << jsonEscape(entry.checksumRelevance) << "\",\n";
        manifest << "      \"hash_algorithm\": \"fnv1a64\",\n";
        manifest << "      \"hash\": \"" << fnv1a64Hex(entry.text) << "\",\n";
        manifest << "      \"byte_count\": " << entry.text.size() << "\n";
        manifest << "    }" << (i + 1 < entries.size() ? "," : "") << "\n";
    }
    manifest << "  ]\n";
    manifest << "}\n";
    return manifest.str();
}

} // namespace

GeneratedOverlayFiles OverlayCompiler::compile(const DslProgram& program) const
{
    std::ostringstream events;
    std::ostringstream effects;
    std::ostringstream triggers;

    events << "# Strategic Nexus generated dispatcher surface\n";
    events << "# Complete replacement snapshot. Do not edit by hand.\n\n";
    events << "namespace = strategic_nexus_generated\n\n";
    events << "country_event = {\n";
    events << "    id = strategic_nexus_generated.1\n";
    events << "    hide_window = yes\n";
    events << "    is_triggered_only = yes\n";
    events << "    immediate = {\n";

    effects << "# Strategic Nexus generated scripted effects\n";
    effects << "# Complete replacement snapshot. Do not edit by hand.\n\n";

    triggers << "# Strategic Nexus generated scripted triggers\n";
    triggers << "# Complete replacement snapshot. Do not edit by hand.\n\n";

    for (const auto& rule : program.rules) {
        const std::string name = ruleSymbol(rule);
        const std::string triggerName = "strategic_nexus_generated_trigger_" + name;
        const std::string effectName = "strategic_nexus_generated_effect_" + name;
        const std::string campaignFlag = "strategic_nexus_campaign_" + symbol(rule.campaignId);
        const std::string empireFlag = "strategic_nexus_empire_" + symbol(rule.empireId);

        triggers << triggerName << " = {\n";
        triggers << "    has_global_flag = " << campaignFlag << "\n";
        triggers << "    has_country_flag = " << empireFlag << "\n";
        triggers << "}\n\n";

        effects << effectName << " = {\n";
        effects << "    if = {\n";
        effects << "        limit = { " << triggerName << " = yes }\n";
        for (const auto& preference : rule.preferences) {
            effects << "        set_country_flag = " << flagForPreference(preference) << "\n";
        }
        effects << "    }\n";
        effects << "}\n\n";

        events << "        " << effectName << " = yes\n";
    }

    events << "    }\n";
    events << "}\n";

    GeneratedOverlayFiles files;
    files.eventsText = events.str();
    files.scriptedEffectsText = effects.str();
    files.scriptedTriggersText = triggers.str();
    files.manifestText = buildManifest(
        program,
        {
            {"events/strategic_nexus_generated_events.txt", "dispatcher_events", "gameplay_affecting", files.eventsText},
            {"common/scripted_effects/strategic_nexus_generated_effects.txt", "scripted_effects", "gameplay_affecting", files.scriptedEffectsText},
            {"common/scripted_triggers/strategic_nexus_generated_triggers.txt", "scripted_triggers", "gameplay_affecting", files.scriptedTriggersText},
        });

    return files;
}

} // namespace strategic_nexus::generated_overlay

