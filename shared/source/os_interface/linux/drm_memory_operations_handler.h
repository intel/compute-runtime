/*
 * Copyright (C) 2019-2021 Intel Corporation
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
class Drm;
class OsContext;
class DrmMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    DrmMemoryOperationsHandler() = default;
    ~DrmMemoryOperationsHandler() override = default;

    virtual void mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) = 0;
    virtual std::unique_lock<std::mutex> lockHandlerIfUsed() = 0;

    virtual void evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) = 0;

    static std::unique_ptr<DrmMemoryOperationsHandler> create(Drm &drm, uint32_t rootDeviceIndex);

  protected:
    std::mutex mutex;
};
} // namespace NEO
