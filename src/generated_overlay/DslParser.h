// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "DslRule.h"

#include <string>

namespace strategic_nexus::generated_overlay {

class DslParser {
public:
    DslParseResult parse(const std::string& text) const;
};

} // namespace strategic_nexus::generated_overlay

