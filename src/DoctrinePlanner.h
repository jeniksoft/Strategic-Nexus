// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "CoreTypes.h"

namespace strategic_nexus {

class DoctrinePlanner {
public:
    DoctrineDecision chooseDoctrine(const StrategicSummary& summary) const;
};

const char* toString(DoctrineType type);

} // namespace strategic_nexus

