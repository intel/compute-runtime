/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/memory_manager/residency_container.h"

#include <memory>
#include <mutex>

namespace NEO {
class OsContext;
class DrmMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    DrmMemoryOperationsHandler() = default;
    ~DrmMemoryOperationsHandler() override = default;

    virtual MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) = 0;
    virtual void mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) = 0;
    virtual std::unique_lock<std::mutex> lockHandlerForExecWA() = 0;

    static std::unique_ptr<DrmMemoryOperationsHandler> create();

  protected:
    std::mutex mutex;
};
} // namespace NEO
