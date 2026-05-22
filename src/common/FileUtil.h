#pragma once

#include <filesystem>
#include <string>

namespace strategic_nexus::common {

std::string readTextFile(const std::filesystem::path& path);
bool writeTextFileAtomically(const std::filesystem::path& path, const std::string& text);

} // namespace strategic_nexus::common
