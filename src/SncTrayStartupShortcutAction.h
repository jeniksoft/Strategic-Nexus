#pragma once

#include "common/JsonExtract.h"

#include <filesystem>
#include <string>

namespace strategic_nexus {

struct SncTrayStartupShortcutActionStatus {
    bool stateKnown = false;
    bool startWithWindowsEnabled = false;
    bool toggleAvailable = false;
    std::string lifecycleState = "manual_start_only";
    std::string shortcutState = "shortcut_status_unknown";
    std::filesystem::path shortcutPath;
    std::string toggleCommandHint;
    std::string toggleCommandHintSource = "none";
    std::string enableCommandHint;
    std::string disableCommandHint;
};

inline SncTrayStartupShortcutActionStatus parseSncTrayStartupShortcutActionStatus(const std::string& json)
{
    SncTrayStartupShortcutActionStatus status;

    status.lifecycleState =
        common::extractJsonString(json, "startup_lifecycle_state").value_or(status.lifecycleState);
    status.shortcutState =
        common::extractJsonString(json, "start_with_windows_shortcut_state").value_or(status.shortcutState);
    status.shortcutPath = common::extractJsonString(json, "start_with_windows_shortcut_path").value_or("");
    status.toggleCommandHint =
        common::extractJsonString(json, "start_with_windows_command_hint").value_or("");
    status.toggleCommandHintSource =
        common::extractJsonString(json, "start_with_windows_command_hint_source")
            .value_or(status.toggleCommandHintSource);
    status.enableCommandHint =
        common::extractJsonString(json, "start_with_windows_enable_command_hint").value_or("");
    status.disableCommandHint =
        common::extractJsonString(json, "start_with_windows_disable_command_hint").value_or("");

    status.stateKnown = !status.lifecycleState.empty() || !status.shortcutState.empty() ||
        !status.toggleCommandHint.empty() || !status.enableCommandHint.empty() ||
        !status.disableCommandHint.empty() || !status.shortcutPath.empty();

    status.startWithWindowsEnabled = status.lifecycleState == "owner_enabled_start_with_windows" ||
        status.shortcutState == "shortcut_installed" || status.shortcutState == "override_enabled" ||
        status.toggleCommandHintSource == "startup_shortcut_remove_command";

    const bool unavailable = status.toggleCommandHintSource == "startup_shortcut_unavailable" ||
        status.shortcutState == "shortcut_path_unavailable";
    if (!unavailable && status.toggleCommandHint.empty()) {
        status.toggleCommandHint = status.startWithWindowsEnabled ? status.disableCommandHint
                                                                  : status.enableCommandHint;
        if (!status.toggleCommandHint.empty() && status.toggleCommandHintSource == "none") {
            status.toggleCommandHintSource = status.startWithWindowsEnabled
                ? "startup_shortcut_remove_command"
                : "startup_shortcut_install_command";
        }
    }

    status.toggleAvailable = !unavailable && !status.toggleCommandHint.empty();
    return status;
}

inline bool sncTrayStartupShortcutToggleTargetsEnable(const SncTrayStartupShortcutActionStatus& status)
{
    return !status.startWithWindowsEnabled;
}

inline std::wstring buildSncTrayStartupShortcutToggleMenuLabel(
    const SncTrayStartupShortcutActionStatus& status)
{
    if (!status.toggleAvailable) {
        return L"Start s Windows neni dostupny";
    }
    return status.startWithWindowsEnabled ? L"Vypnout start s Windows" : L"Zapnout start s Windows";
}

inline std::wstring buildSncTrayStartupShortcutToggleButtonLabel(
    const SncTrayStartupShortcutActionStatus& status)
{
    if (!status.toggleAvailable) {
        return L"Start nedostupny";
    }
    return status.startWithWindowsEnabled ? L"Vypnout start" : L"Zapnout start";
}

} // namespace strategic_nexus
