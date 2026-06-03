// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "DslRule.h"

namespace strategic_nexus::generated_overlay {

class OverlayCompiler {
public:
    GeneratedOverlayFiles compile(const DslProgram& program) const;
};

std::vector<std::string> findUnsupportedRuntimeConditionErrors(const DslProgram& program);

} // namespace strategic_nexus::generated_overlay

