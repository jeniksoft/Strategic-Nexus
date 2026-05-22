#pragma once

#include <optional>
#include <string>
#include <vector>

namespace strategic_nexus::common {

std::string decodeJsonStringLiteral(const std::string& rawLiteral);
std::optional<std::string> extractJsonString(const std::string& json, const char* key);
std::vector<std::string> extractJsonStringArray(const std::string& json, const char* key);

} // namespace strategic_nexus::common
