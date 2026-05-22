#pragma once

#include "EffectBatchEmitter.h"

#include <filesystem>
#include <string>

namespace strategic_nexus::bridge_core {

struct EffectBatchWriteResult {
    bool written = false;
    std::string reason;
    std::filesystem::path path;
};

class EffectBatchFileWriter {
public:
    EffectBatchWriteResult writeAtomically(const EffectBatch& batch, const std::filesystem::path& path) const;
};

} // namespace strategic_nexus::bridge_core
