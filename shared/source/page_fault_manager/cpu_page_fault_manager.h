/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/spinlock.h"

#include "memory_properties_flags.h"

#include <memory>
#include <unordered_map>

namespace NEO {
class GraphicsAllocation;
class Device;
class SVMAllocsManager;

class PageFaultManager : public NonCopyableOrMovableClass {
  public:
    static std::unique_ptr<PageFaultManager> create();

    virtual ~PageFaultManager() = default;

    MOCKABLE_VIRTUAL void moveAllocationToGpuDomain(void *ptr);
    MOCKABLE_VIRTUAL void moveAllocationsWithinUMAllocsManagerToGpuDomain(SVMAllocsManager *unifiedMemoryManager);
    void insertAllocation(void *ptr, size_t size, SVMAllocsManager *unifiedMemoryManager, void *cmdQ, const MemoryProperties &memoryProperties);
    void removeAllocation(void *ptr);

    enum class AllocationDomain {
        None,
        Cpu,
        Gpu,
    };

    struct PageFaultData {
        size_t size;
        SVMAllocsManager *unifiedMemoryManager;
        void *cmdQ;
        AllocationDomain domain;
    };

    typedef void (*gpuDomainHandlerFunc)(PageFaultManager *pageFaultHandler, void *alloc, PageFaultData &pageFaultData);

    void setGpuDomainHandler(gpuDomainHandlerFunc gpuHandlerFuncPtr);

    virtual void allowCPUMemoryAccess(void *ptr, size_t size) = 0;
    virtual void protectCPUMemoryAccess(void *ptr, size_t size) = 0;
    MOCKABLE_VIRTUAL void transferToCpu(void *ptr, size_t size, void *cmdQ);

  protected:
    virtual void evictMemoryAfterImplCopy(GraphicsAllocation *allocation, Device *device) = 0;

    MOCKABLE_VIRTUAL bool verifyPageFault(void *ptr);
    MOCKABLE_VIRTUAL void transferToGpu(void *ptr, void *cmdQ);
    MOCKABLE_VIRTUAL void setAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager);

    static void handleGpuDomainTransferForHw(PageFaultManager *pageFaultHandler, void *alloc, PageFaultData &pageFaultData);
    static void handleGpuDomainTransferForAubAndTbx(PageFaultManager *pageFaultHandler, void *alloc, PageFaultData &pageFaultData);
    void selectGpuDomainHandler();
    inline void migrateStorageToGpuDomain(void *ptr, PageFaultData &pageFaultData);
    inline void migrateStorageToCpuDomain(void *ptr, PageFaultData &pageFaultData);

    decltype(&handleGpuDomainTransferForHw) gpuDomainHandler = &handleGpuDomainTransferForHw;

    std::unordered_map<void *, PageFaultData> memoryData;
    SpinLock mtx;
};
} // namespace NEO
