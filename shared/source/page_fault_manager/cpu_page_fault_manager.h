/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/spinlock.h"

#include <memory>
#include <unordered_map>

namespace NEO {
struct MemoryProperties;
class CommandStreamReceiver;
class GraphicsAllocation;
class Device;
class SVMAllocsManager;
class OSInterface;

class PageFaultManager : public NonCopyableOrMovableClass {
  public:
    static std::unique_ptr<PageFaultManager> create();

    virtual ~PageFaultManager() = default;

    MOCKABLE_VIRTUAL void moveAllocationToGpuDomain(void *ptr);
    MOCKABLE_VIRTUAL void moveAllocationsWithinUMAllocsManagerToGpuDomain(SVMAllocsManager *unifiedMemoryManager);
    void insertAllocation(void *ptr, size_t size, SVMAllocsManager *unifiedMemoryManager, void *cmdQ, const MemoryProperties &memoryProperties);
    void removeAllocation(void *ptr);

    enum class AllocationDomain {
        none,
        cpu,
        gpu,
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
    virtual bool checkFaultHandlerFromPageFaultManager() = 0;
    virtual void registerFaultHandler() = 0;
    virtual void evictMemoryAfterImplCopy(GraphicsAllocation *allocation, Device *device) = 0;
    virtual void allowCPUMemoryEvictionImpl(bool evict, void *ptr, CommandStreamReceiver &csr, OSInterface *osInterface) = 0;

    MOCKABLE_VIRTUAL bool verifyAndHandlePageFault(void *ptr, bool handlePageFault);
    MOCKABLE_VIRTUAL void transferToGpu(void *ptr, void *cmdQ);
    MOCKABLE_VIRTUAL void setAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager);
    MOCKABLE_VIRTUAL void setCpuAllocEvictable(bool evictable, void *ptr, SVMAllocsManager *unifiedMemoryManager);
    MOCKABLE_VIRTUAL void allowCPUMemoryEviction(bool evict, void *ptr, PageFaultData &pageFaultData);

    static void transferAndUnprotectMemory(PageFaultManager *pageFaultHandler, void *alloc, PageFaultData &pageFaultData);
    static void unprotectAndTransferMemory(PageFaultManager *pageFaultHandler, void *alloc, PageFaultData &pageFaultData);
    void selectGpuDomainHandler();
    inline void migrateStorageToGpuDomain(void *ptr, PageFaultData &pageFaultData);
    inline void migrateStorageToCpuDomain(void *ptr, PageFaultData &pageFaultData);

    decltype(&transferAndUnprotectMemory) gpuDomainHandler = &transferAndUnprotectMemory;

    std::unordered_map<void *, PageFaultData> memoryData;
    SpinLock mtx;
};
} // namespace NEO
