// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include <filesystem>
#include <string>

namespace strategic_nexus::common {

std::string readTextFile(const std::filesystem::path& path);
bool tryReadTextFile(const std::filesystem::path& path, std::string& output);
bool writeTextFileAtomically(const std::filesystem::path& path, const std::string& text);

} // namespace strategic_nexus::common

