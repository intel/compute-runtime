/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_context.h"

namespace NEO {
class MockOsContext : public OsContext {
  public:
    MockOsContext(uint32_t contextId, DeviceBitfield deviceBitfield,
                  aub_stream::EngineType engineType, PreemptionMode preemptionMode,
                  bool lowPriority, bool internalEngine, bool rootDevice)
        : OsContext(contextId, deviceBitfield, engineType, preemptionMode,
                    lowPriority, internalEngine, rootDevice) {}
};
} // namespace NEO
