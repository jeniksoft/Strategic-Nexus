// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "FileUtil.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace strategic_nexus::common {
namespace {

bool replaceFileWithTemp(const std::filesystem::path& tempPath, const std::filesystem::path& path)
{
#ifdef _WIN32
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (MoveFileExW(
                tempPath.c_str(),
                path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    return false;
#else
    std::error_code renameError;
    std::filesystem::rename(tempPath, path, renameError);
    return !renameError;
#endif
}

} // namespace

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool tryReadTextFile(const std::filesystem::path& path, std::string& output)
{
    output.clear();
    if (path.empty()) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    output = buffer.str();
    return true;
}

bool writeTextFileAtomically(const std::filesystem::path& path, const std::string& text)
{
    if (path.empty()) {
        return false;
    }

    std::error_code typeError;
    if (std::filesystem::is_directory(path, typeError)) {
        return false;
    }

    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code directoryError;
        std::filesystem::create_directories(parent, directoryError);
        if (directoryError) {
            return false;
        }
    }

    const std::filesystem::path tempPath = path.string() + ".tmp";

    {
        std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }
        output << text;
    }

    if (!replaceFileWithTemp(tempPath, path)) {
        std::error_code cleanupError;
        std::filesystem::remove(tempPath, cleanupError);
        return false;
    }

    return true;
}

} // namespace strategic_nexus::common

