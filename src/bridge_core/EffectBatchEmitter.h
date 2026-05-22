#pragma once

#include "ScriptIntentMapper.h"

#include <string>
#include <vector>

namespace strategic_nexus::bridge_core {

struct EffectBatch {
    bool valid = false;
    std::vector<std::string> lines;
};

class EffectBatchEmitter {
public:
    EffectBatch emit(const ScriptIntent& intent) const;
};

} // namespace strategic_nexus::bridge_core
