/*
 * Copyright (C) 2019-2022 Intel Corporation
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
    DrmMemoryOperationsHandler(uint32_t rootDeviceIndex) : rootDeviceIndex(rootDeviceIndex){};
    ~DrmMemoryOperationsHandler() override = default;

    virtual MemoryOperationsStatus mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) = 0;
    virtual std::unique_lock<std::mutex> lockHandlerIfUsed() = 0;

    virtual MemoryOperationsStatus evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) = 0;

    static std::unique_ptr<DrmMemoryOperationsHandler> create(Drm &drm, uint32_t rootDeviceIndex);

    uint32_t getRootDeviceIndex() const {
        return this->rootDeviceIndex;
    }
    void setRootDeviceIndex(uint32_t index) {
        this->rootDeviceIndex = index;
    }

  protected:
    std::mutex mutex;
    uint32_t rootDeviceIndex = 0;
};
} // namespace NEO
