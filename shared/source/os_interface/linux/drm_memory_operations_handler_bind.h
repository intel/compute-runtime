/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/common_types.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"

namespace NEO {
struct RootDeviceEnvironment;
class DrmMemoryOperationsHandlerBind : public DrmMemoryOperationsHandler {
  public:
    DrmMemoryOperationsHandlerBind(const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex);
    ~DrmMemoryOperationsHandlerBind() override;

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) override;
    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override;

    MemoryOperationsStatus mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) override;
    std::unique_lock<std::mutex> lockHandlerIfUsed() override;

    MemoryOperationsStatus evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) override;

    uint32_t getRootDeviceIndex() {
        return this->rootDeviceIndex;
    }
    void setRootDeviceIndex(uint32_t index) {
        this->rootDeviceIndex = index;
    }

  protected:
    MOCKABLE_VIRTUAL int evictImpl(OsContext *osContext, GraphicsAllocation &gfxAllocation, DeviceBitfield deviceBitfield);
    MemoryOperationsStatus evictUnusedAllocationsImpl(std::vector<GraphicsAllocation *> &allocationsForEviction, bool waitForCompletion);

    const RootDeviceEnvironment &rootDeviceEnvironment;
    uint32_t rootDeviceIndex = 0;
};
} // namespace NEO
