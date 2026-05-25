// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#include "EffectBatchFileWriter.h"

#include "common/FileUtil.h"

#include <sstream>

namespace strategic_nexus::bridge_core {

EffectBatchWriteResult EffectBatchFileWriter::writeAtomically(
    const EffectBatch& batch,
    const std::filesystem::path& path) const
{
    EffectBatchWriteResult result;
    result.path = path;

    if (path.empty()) {
        result.reason = "missing output path";
        return result;
    }

    if (!batch.valid || batch.lines.empty()) {
        std::error_code typeError;
        if (std::filesystem::is_directory(path, typeError)) {
            result.reason = "invalid batch, output path is directory";
            return result;
        }

        std::error_code removeError;
        std::filesystem::remove(path, removeError);
        result.reason = "invalid batch, output cleared";
        return result;
    }

    std::ostringstream text;
    for (const std::string& line : batch.lines) {
        text << line << '\n';
    }

    if (!common::writeTextFileAtomically(path, text.str())) {
        result.reason = "failed to write output atomically";
        return result;
    }

    result.written = true;
    result.reason = "written";
    return result;
}

} // namespace strategic_nexus::bridge_core

