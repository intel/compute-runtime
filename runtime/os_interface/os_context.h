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
    class OsContextImpl;
    OsContext(OSInterface *osInterface, uint32_t contextId, uint32_t numDevicesSupported, EngineInstanceT engineType, PreemptionMode preemptionMode);
    ~OsContext() override;
    OsContextImpl *get() const {
        return osContextImpl.get();
    };

    uint32_t getContextId() const { return contextId; }
    uint32_t getNumDevicesSupported() const { return numDevicesSupported; }
    EngineInstanceT &getEngineType() { return engineType; }

  protected:
    std::unique_ptr<OsContextImpl> osContextImpl;
    const uint32_t contextId;
    const uint32_t numDevicesSupported;
    EngineInstanceT engineType = {EngineType::ENGINE_RCS, 0};
};
} // namespace OCLRT
