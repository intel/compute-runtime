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

class CpuPageFaultManager : public NonCopyableClass {
  public:
    static std::unique_ptr<CpuPageFaultManager> create();

    virtual ~CpuPageFaultManager() = default;

    virtual void allowCPUMemoryAccess(void *ptr, size_t size) = 0;
    virtual void protectCPUMemoryAccess(void *ptr, size_t size) = 0;
    virtual void protectCpuMemoryFromWrites(void *ptr, size_t size) = 0;

    MOCKABLE_VIRTUAL void moveAllocationToGpuDomain(void *ptr);
    MOCKABLE_VIRTUAL void moveAllocationsWithinUMAllocsManagerToGpuDomain(SVMAllocsManager *unifiedMemoryManager);
    void insertAllocation(void *ptr, size_t size, SVMAllocsManager *unifiedMemoryManager, void *cmdQ, const MemoryProperties &memoryProperties);
    void removeAllocation(void *ptr);
    virtual void insertAllocation(CommandStreamReceiver *csr, GraphicsAllocation *alloc, uint32_t bank, void *ptr, size_t size) {}
    virtual void removeAllocation(GraphicsAllocation *alloc) {}

    enum class FaultMode {
        cpu,
        tbx
    };

    enum class AllocationDomain {
        none,
        cpu,
        gpu,
    };

    struct PageFaultData {
        FaultMode faultType = FaultMode::cpu;
        size_t size = 0;

        // cpu fault data
        SVMAllocsManager *unifiedMemoryManager = nullptr;
        void *cmdQ = nullptr;
        AllocationDomain domain = AllocationDomain::none;

        // tbx fault data
        bool hasBeenDownloaded = false;
        GraphicsAllocation *gfxAllocation = nullptr;
        uint32_t bank = 0;
        CommandStreamReceiver *csr = nullptr;
    };

    typedef void (*gpuDomainHandlerFunc)(CpuPageFaultManager *pageFaultHandler, void *alloc, PageFaultData &pageFaultData);

    void setGpuDomainHandler(gpuDomainHandlerFunc gpuHandlerFuncPtr);

    MOCKABLE_VIRTUAL void transferToCpu(void *ptr, size_t size, void *cmdQ);

  protected:
    virtual void evictMemoryAfterImplCopy(GraphicsAllocation *allocation, Device *device) = 0;
    virtual void allowCPUMemoryEvictionImpl(bool evict, void *ptr, CommandStreamReceiver &csr, OSInterface *osInterface) = 0;

    virtual bool checkFaultHandlerFromPageFaultManager() = 0;
    virtual void registerFaultHandler() = 0;

    virtual bool verifyAndHandlePageFault(void *ptr, bool handlePageFault);

    virtual void handlePageFault(void *ptr, PageFaultData &faultData);

    MOCKABLE_VIRTUAL void transferToGpu(void *ptr, void *cmdQ);
    MOCKABLE_VIRTUAL void setAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager);
    MOCKABLE_VIRTUAL void setCpuAllocEvictable(bool evictable, void *ptr, SVMAllocsManager *unifiedMemoryManager);
    MOCKABLE_VIRTUAL void allowCPUMemoryEviction(bool evict, void *ptr, PageFaultData &pageFaultData);

    static void transferAndUnprotectMemory(CpuPageFaultManager *pageFaultHandler, void *alloc, PageFaultData &pageFaultData);
    static void unprotectAndTransferMemory(CpuPageFaultManager *pageFaultHandler, void *alloc, PageFaultData &pageFaultData);
    void selectGpuDomainHandler();
    inline void migrateStorageToGpuDomain(void *ptr, PageFaultData &pageFaultData);
    inline void migrateStorageToCpuDomain(void *ptr, PageFaultData &pageFaultData);

    using gpuDomainHandlerType = decltype(&transferAndUnprotectMemory);
    gpuDomainHandlerType gpuDomainHandler = &transferAndUnprotectMemory;

    std::unordered_map<void *, PageFaultData> memoryData;
    SpinLock mtx;
};
} // namespace NEO
