// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

#pragma once

#include "StrategicRequest.h"

namespace strategic_nexus {

class StrategicWorker {
public:
    int processRequest(const StrategicRequest& request) const;
    int processRequestSafely(const StrategicRequest& request) const;
};

} // namespace strategic_nexus

