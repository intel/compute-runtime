/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/utilities/reference_tracked_object.h"
#include <memory>

namespace OCLRT {
class OSInterface;
class OsContext : public ReferenceTrackedObject<OsContext> {
  public:
    class OsContextImpl;
    OsContext(OSInterface *osInterface, uint32_t contextId);
    ~OsContext() override;
    OsContextImpl *get() const {
        return osContextImpl.get();
    };

    uint32_t getContextId() { return contextId; }

  protected:
    std::unique_ptr<OsContextImpl> osContextImpl;
    uint32_t contextId = 0;
};
} // namespace OCLRT
