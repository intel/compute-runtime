/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/utilities/reference_tracked_object.h"
#include "engine_node.h"
#include <memory>

namespace OCLRT {
class OSInterface;
enum class PreemptionMode : uint32_t;

class OsContext : public ReferenceTrackedObject<OsContext> {
  public:
    class OsContextImpl;
    OsContext(OSInterface *osInterface, uint32_t contextId, EngineInstanceT engineType, PreemptionMode preemptionMode);
    ~OsContext() override;
    OsContextImpl *get() const {
        return osContextImpl.get();
    };

    uint32_t getContextId() const { return contextId; }
    EngineInstanceT &getEngineType() { return engineType; }

  protected:
    std::unique_ptr<OsContextImpl> osContextImpl;
    uint32_t contextId = 0;
    EngineInstanceT engineType;
};
} // namespace OCLRT
