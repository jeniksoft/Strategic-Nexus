#include "SncTrayStartupShortcutAction.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void requireCondition(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(1);
    }
}

} // namespace

int main()
{
    {
        const std::string json = R"({
  "startup_lifecycle_state": "owner_enabled_start_with_windows",
  "start_with_windows_shortcut_state": "shortcut_installed",
  "start_with_windows_shortcut_path": "C:/Users/test/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Startup/Strategic Nexus Companion.lnk",
  "start_with_windows_command_hint": "cmd /c tools\\remove_snc_tray_startup_shortcut.cmd",
  "start_with_windows_command_hint_source": "startup_shortcut_remove_command",
  "start_with_windows_enable_command_hint": "cmd /c tools\\install_snc_tray_startup_shortcut.cmd",
  "start_with_windows_disable_command_hint": "cmd /c tools\\remove_snc_tray_startup_shortcut.cmd"
})";

        const auto status = strategic_nexus::parseSncTrayStartupShortcutActionStatus(json);
        requireCondition(status.stateKnown, "enabled JSON should be recognized");
        requireCondition(status.startWithWindowsEnabled, "enabled JSON should report enabled state");
        requireCondition(status.toggleAvailable, "enabled JSON should keep toggle available");
        requireCondition(
            status.toggleCommandHint == "cmd /c tools\\remove_snc_tray_startup_shortcut.cmd",
            "enabled JSON should keep remove command as active toggle");
        requireCondition(
            strategic_nexus::buildSncTrayStartupShortcutToggleMenuLabel(status) ==
                L"Vypnout start s Windows",
            "enabled JSON should expose disable menu label");
        requireCondition(
            strategic_nexus::buildSncTrayStartupShortcutToggleButtonLabel(status) ==
                L"Vypnout start",
            "enabled JSON should expose disable button label");
    }

    {
        const std::string json = R"({
  "startup_lifecycle_state": "manual_start_only",
  "start_with_windows_shortcut_state": "shortcut_not_installed",
  "start_with_windows_shortcut_path": "C:/Users/test/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Startup/Strategic Nexus Companion.lnk",
  "start_with_windows_command_hint_source": "startup_shortcut_install_command",
  "start_with_windows_enable_command_hint": "cmd /c tools\\install_snc_tray_startup_shortcut.cmd",
  "start_with_windows_disable_command_hint": "cmd /c tools\\remove_snc_tray_startup_shortcut.cmd"
})";

        const auto status = strategic_nexus::parseSncTrayStartupShortcutActionStatus(json);
        requireCondition(status.stateKnown, "disabled JSON should be recognized");
        requireCondition(!status.startWithWindowsEnabled, "disabled JSON should report disabled state");
        requireCondition(status.toggleAvailable, "disabled JSON should keep toggle available");
        requireCondition(
            status.toggleCommandHint == "cmd /c tools\\install_snc_tray_startup_shortcut.cmd",
            "disabled JSON should derive install command as active toggle");
        requireCondition(
            strategic_nexus::buildSncTrayStartupShortcutToggleMenuLabel(status) ==
                L"Zapnout start s Windows",
            "disabled JSON should expose enable menu label");
        requireCondition(
            strategic_nexus::buildSncTrayStartupShortcutToggleButtonLabel(status) ==
                L"Zapnout start",
            "disabled JSON should expose enable button label");
    }

    {
        const std::string json = R"({
  "startup_lifecycle_state": "manual_start_only",
  "start_with_windows_shortcut_state": "shortcut_path_unavailable",
  "start_with_windows_shortcut_path": "",
  "start_with_windows_command_hint_source": "startup_shortcut_unavailable",
  "start_with_windows_enable_command_hint": "cmd /c tools\\install_snc_tray_startup_shortcut.cmd",
  "start_with_windows_disable_command_hint": "cmd /c tools\\remove_snc_tray_startup_shortcut.cmd"
})";

        const auto status = strategic_nexus::parseSncTrayStartupShortcutActionStatus(json);
        requireCondition(status.stateKnown, "unavailable JSON should be recognized");
        requireCondition(!status.toggleAvailable, "unavailable JSON should disable toggle");
        requireCondition(
            strategic_nexus::buildSncTrayStartupShortcutToggleMenuLabel(status) ==
                L"Start s Windows neni dostupny",
            "unavailable JSON should expose unavailable menu label");
        requireCondition(
            strategic_nexus::buildSncTrayStartupShortcutToggleButtonLabel(status) ==
                L"Start nedostupny",
            "unavailable JSON should expose unavailable button label");
    }

    return 0;
}
