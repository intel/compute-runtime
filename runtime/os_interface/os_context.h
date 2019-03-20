/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/preemption_mode.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/utilities/reference_tracked_object.h"

#include "engine_node.h"

#include <memory>

namespace OCLRT {
class OSInterface;

class OsContext : public ReferenceTrackedObject<OsContext> {
  public:
    OsContext() = delete;

    static OsContext *create(OSInterface *osInterface, uint32_t contextId, DeviceBitfield deviceBitfield,
                             EngineType engineType, PreemptionMode preemptionMode, bool lowPriority);
    uint32_t getContextId() const { return contextId; }
    uint32_t getNumSupportedDevices() const { return numSupportedDevices; }
    DeviceBitfield getDeviceBitfield() const { return deviceBitfield; }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    EngineType &getEngineType() { return engineType; }
    bool isLowPriority() const { return lowPriority; }

  protected:
    OsContext(uint32_t contextId, DeviceBitfield deviceBitfield, EngineType engineType, PreemptionMode preemptionMode, bool lowPriority)
        : contextId(contextId),
          deviceBitfield(deviceBitfield),
          preemptionMode(preemptionMode),
          numSupportedDevices(static_cast<uint32_t>(deviceBitfield.count())),
          engineType(engineType),
          lowPriority(lowPriority) {}

    const uint32_t contextId;
    const DeviceBitfield deviceBitfield;
    const PreemptionMode preemptionMode;
    const uint32_t numSupportedDevices;
    EngineType engineType = EngineType::ENGINE_RCS;
    const bool lowPriority;
};
} // namespace OCLRT
