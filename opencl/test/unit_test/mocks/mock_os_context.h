/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_context.h"

namespace NEO {
class MockOsContext : public OsContext {
  public:
    using OsContext::checkDirectSubmissionSupportsEngine;
    using OsContext::engineType;
    using OsContext::getDeviceBitfield;

    MockOsContext(uint32_t contextId, DeviceBitfield deviceBitfield,
                  EngineTypeUsage typeUsage, PreemptionMode preemptionMode,
                  bool rootDevice)
        : OsContext(contextId, deviceBitfield, typeUsage, preemptionMode, rootDevice) {}
};
} // namespace NEO
