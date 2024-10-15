/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

class MemoryAllocation : public GraphicsAllocation {
  public:
    const unsigned long long id;
    uint64_t internalHandle = 0;
    size_t sizeToFree = 0;
    const bool uncacheable;

    MemoryAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, uint64_t canonizedGpuAddress, uint64_t baseAddress, size_t sizeIn,
                     MemoryPool pool, size_t maxOsContextCount)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, cpuPtrIn, canonizedGpuAddress, baseAddress, sizeIn, pool, maxOsContextCount),
          id(0), uncacheable(false) {}

    MemoryAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn, MemoryPool pool, size_t maxOsContextCount, uint64_t canonizedGpuAddress)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, cpuPtrIn, sizeIn, sharedHandleIn, pool, maxOsContextCount, canonizedGpuAddress),
          id(0), uncacheable(false) {}

    MemoryAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *driverAllocatedCpuPointer, void *pMem, uint64_t canonizedGpuAddress, size_t memSize,
                     uint64_t count, MemoryPool pool, bool uncacheable, bool flushL3Required, size_t maxOsContextCount)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, pMem, canonizedGpuAddress, 0u, memSize, pool, maxOsContextCount),
          id(count), uncacheable(uncacheable) {

        this->driverAllocatedCpuPointer = driverAllocatedCpuPointer;
        overrideMemoryPool(pool);
        allocationInfo.flags.flushL3Required = flushL3Required;
    }

    void overrideMemoryPool(MemoryPool pool);

    void clearUsageInfo() {
        for (auto &info : usageInfos) {
            info.inspectionId = 0u;
            info.residencyTaskCount = objectNotResident;
            info.taskCount = objectNotUsed;
        }
    }

    int peekInternalHandle(MemoryManager *memoryManager, uint64_t &handle) override {
        if (internalHandle == std::numeric_limits<uint64_t>::max()) {
            return -1;
        }

        handle = internalHandle;
        return 0;
    }
};

} // namespace NEO
