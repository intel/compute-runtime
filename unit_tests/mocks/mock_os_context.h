/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/os_context.h"

namespace OCLRT {
class MockOsContext : public OsContext {
  public:
    MockOsContext(OSInterface *osInterface, uint32_t contextId, DeviceBitfield deviceBitfield,
                  EngineInstanceT engineType, PreemptionMode preemptionMode)
        : OsContext(contextId, deviceBitfield, engineType, preemptionMode) {}
};
} // namespace OCLRT
