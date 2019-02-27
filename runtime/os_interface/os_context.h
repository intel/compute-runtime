/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/preemption_mode.h"
#include "runtime/utilities/reference_tracked_object.h"

#include "engine_node.h"

#include <memory>

namespace OCLRT {
class OSInterface;

class OsContext : public ReferenceTrackedObject<OsContext> {
  public:
    OsContext() = delete;

    static OsContext *create(OSInterface *osInterface, uint32_t contextId, uint32_t numDevicesSupported, EngineInstanceT engineType, PreemptionMode preemptionMode);
    uint32_t getContextId() const { return contextId; }
    uint32_t getNumDevicesSupported() const { return numDevicesSupported; }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    EngineInstanceT &getEngineType() { return engineType; }

  protected:
    OsContext(uint32_t contextId, uint32_t numDevicesSupported, EngineInstanceT engineType, PreemptionMode preemptionMode)
        : contextId(contextId), numDevicesSupported(numDevicesSupported), preemptionMode(preemptionMode), engineType(engineType){};

    const uint32_t contextId;
    const uint32_t numDevicesSupported;
    const PreemptionMode preemptionMode;
    EngineInstanceT engineType = {EngineType::ENGINE_RCS, 0};
};
} // namespace OCLRT
