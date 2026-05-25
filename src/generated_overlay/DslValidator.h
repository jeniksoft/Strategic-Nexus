// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "DslRule.h"

namespace strategic_nexus::generated_overlay {

class DslValidator {
public:
    DslValidationResult validate(const DslProgram& program) const;
};

} // namespace strategic_nexus::generated_overlay

