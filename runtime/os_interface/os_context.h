/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "common/helpers/bit_helpers.h"
#include "runtime/command_stream/preemption_mode.h"
#include "runtime/utilities/reference_tracked_object.h"

#include "engine_node.h"

#include <limits>
#include <memory>

namespace OCLRT {
class OSInterface;

class OsContext : public ReferenceTrackedObject<OsContext> {
  public:
    OsContext() = delete;

    static OsContext *create(OSInterface *osInterface, uint32_t contextId, uint32_t deviceBitfiled, EngineInstanceT engineType, PreemptionMode preemptionMode);
    uint32_t getContextId() const { return contextId; }
    uint32_t getNumSupportedDevices() const { return numSupportedDevices; }
    uint8_t getDeviceBitfiled() const { return deviceBitfiled; }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    EngineInstanceT &getEngineType() { return engineType; }

  protected:
    OsContext(uint32_t contextId, uint32_t deviceBitfiled, EngineInstanceT engineType, PreemptionMode preemptionMode)
        : contextId(contextId), deviceBitfiled(deviceBitfiled), preemptionMode(preemptionMode), engineType(engineType) {
        constexpr uint32_t maxIndex = std::numeric_limits<decltype(deviceBitfiled)>::digits;
        for (uint32_t deviceIndex = 0; deviceIndex < maxIndex; deviceIndex++) {
            if (isBitSet(deviceBitfiled, deviceIndex)) {
                numSupportedDevices++;
            }
        }
    };

    const uint32_t contextId;
    const uint32_t deviceBitfiled;
    const PreemptionMode preemptionMode;
    uint32_t numSupportedDevices = 0;
    EngineInstanceT engineType = {EngineType::ENGINE_RCS, 0};
};
} // namespace OCLRT
