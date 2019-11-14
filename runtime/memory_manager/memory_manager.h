/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "common/helpers/bit_helpers.h"
#include "core/command_stream/preemption_mode.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/common_types.h"
#include "core/memory_manager/gfx_partition.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/memory_manager/host_ptr_defines.h"
#include "core/memory_manager/local_memory_usage.h"
#include "core/page_fault_manager/cpu_page_fault_manager.h"
#include "public/cl_ext_private.h"
#include "runtime/helpers/engine_control.h"
#include "runtime/memory_manager/allocation_properties.h"

#include "engine_node.h"

#include <bitset>
#include <cstdint>
#include <mutex>
#include <vector>

namespace NEO {
class DeferredDeleter;
class ExecutionEnvironment;
class Gmm;
class HostPtrManager;
class OsContext;

enum AllocationUsage {
    TEMPORARY_ALLOCATION,
    REUSABLE_ALLOCATION
};

struct AlignedMallocRestrictions {
    uintptr_t minAddress;
};

constexpr size_t paddingBufferSize = 2 * MemoryConstants::megaByte;

class MemoryManager {
  public:
    enum AllocationStatus {
        Success = 0,
        Error,
        InvalidHostPointer,
        RetryInNonDevicePool
    };

    MemoryManager(ExecutionEnvironment &executionEnvironment);

    virtual ~MemoryManager();
    MOCKABLE_VIRTUAL void *allocateSystemMemory(size_t size, size_t alignment);

    virtual void addAllocationToHostPtrManager(GraphicsAllocation *memory) = 0;
    virtual void removeAllocationFromHostPtrManager(GraphicsAllocation *memory) = 0;

    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) {
        return allocateGraphicsMemoryInPreferredPool(properties, nullptr);
    }

    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) {
        return allocateGraphicsMemoryInPreferredPool(properties, ptr);
    }

    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryInPreferredPool(const AllocationProperties &properties, const void *hostPtr);

    virtual GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) = 0;

    virtual GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex) = 0;

    virtual bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) { return false; }

    void *lockResource(GraphicsAllocation *graphicsAllocation);
    void unlockResource(GraphicsAllocation *graphicsAllocation);

    void cleanGraphicsMemoryCreatedFromHostPtr(GraphicsAllocation *);
    GraphicsAllocation *createGraphicsAllocationWithPadding(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding);
    virtual GraphicsAllocation *createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding);

    virtual AllocationStatus populateOsHandles(OsHandleStorage &handleStorage) = 0;
    virtual void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) = 0;

    void freeSystemMemory(void *ptr);

    virtual void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) = 0;
    MOCKABLE_VIRTUAL void freeGraphicsMemory(GraphicsAllocation *gfxAllocation);
    virtual void handleFenceCompletion(GraphicsAllocation *allocation){};

    void checkGpuUsageAndDestroyGraphicsAllocations(GraphicsAllocation *gfxAllocation);

    virtual uint64_t getSystemSharedMemory() = 0;
    virtual uint64_t getLocalMemorySize() = 0;

    uint64_t getMaxApplicationAddress() { return is64bit ? MemoryConstants::max64BitAppAddress : MemoryConstants::max32BitAppAddress; };
    uint64_t getInternalHeapBaseAddress(uint32_t rootDeviceIndex) { return getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY); }
    uint64_t getExternalHeapBaseAddress(uint32_t rootDeviceIndex) { return getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_EXTERNAL); }

    bool isLimitedRange(uint32_t rootDeviceIndex) { return getGfxPartition(rootDeviceIndex)->isLimitedRange(); }

    bool peek64kbPagesEnabled() const { return enable64kbpages; }
    bool peekForce32BitAllocations() const { return force32bitAllocations; }
    void setForce32BitAllocations(bool newValue) { force32bitAllocations = newValue; }

    bool peekVirtualPaddingSupport() const { return virtualPaddingAvailable; }
    void setVirtualPaddingSupport(bool virtualPaddingSupport) { virtualPaddingAvailable = virtualPaddingSupport; }

    DeferredDeleter *getDeferredDeleter() const {
        return deferredDeleter.get();
    }

    PageFaultManager *getPageFaultManager() const {
        return pageFaultManager.get();
    }

    void waitForDeletions();
    void waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation);
    void cleanTemporaryAllocationListOnAllEngines(bool waitForCompletion);

    bool isAsyncDeleterEnabled() const;
    bool isLocalMemorySupported() const;
    virtual bool isMemoryBudgetExhausted() const;

    virtual AlignedMallocRestrictions *getAlignedMallocRestrictions() {
        return nullptr;
    }

    MOCKABLE_VIRTUAL void *alignedMallocWrapper(size_t bytes, size_t alignment) {
        return ::alignedMalloc(bytes, alignment);
    }

    MOCKABLE_VIRTUAL void alignedFreeWrapper(void *ptr) {
        ::alignedFree(ptr);
    }

    MOCKABLE_VIRTUAL bool isHostPointerTrackingEnabled();

    const ExecutionEnvironment &peekExecutionEnvironment() const { return executionEnvironment; }

    OsContext *createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver, aub_stream::EngineType engineType,
                                          DeviceBitfield deviceBitfield, PreemptionMode preemptionMode, bool lowPriority);
    uint32_t getRegisteredEnginesCount() const { return static_cast<uint32_t>(registeredEngines.size()); }
    EngineControlContainer &getRegisteredEngines();
    EngineControl *getRegisteredEngineForCsr(CommandStreamReceiver *commandStreamReceiver);
    void unregisterEngineForCsr(CommandStreamReceiver *commandStreamReceiver);
    HostPtrManager *getHostPtrManager() const { return hostPtrManager.get(); }
    void setDefaultEngineIndex(uint32_t index) { defaultEngineIndex = index; }
    virtual bool copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, const void *memoryToCopy, size_t sizeToCopy);
    static HeapIndex selectHeap(const GraphicsAllocation *allocation, bool hasPointer, bool isFullRangeSVM);
    static std::unique_ptr<MemoryManager> createMemoryManager(ExecutionEnvironment &executionEnvironment);
    virtual void *reserveCpuAddressRange(size_t size) { return nullptr; };
    virtual void releaseReservedCpuAddressRange(void *reserved, size_t size){};
    void *getReservedMemory(size_t size, size_t alignment);
    GfxPartition *getGfxPartition(uint32_t rootDeviceIndex) { return gfxPartitions.at(rootDeviceIndex).get(); }

  protected:
    struct AllocationData {
        union {
            struct {
                uint32_t allocateMemory : 1;
                uint32_t allow64kbPages : 1;
                uint32_t allow32Bit : 1;
                uint32_t useSystemMemory : 1;
                uint32_t forcePin : 1;
                uint32_t uncacheable : 1;
                uint32_t flushL3 : 1;
                uint32_t preferRenderCompressed : 1;
                uint32_t multiOsContextCapable : 1;
                uint32_t requiresCpuAccess : 1;
                uint32_t shareable : 1;
                uint32_t reserved : 21;
            } flags;
            uint32_t allFlags = 0;
        };
        static_assert(sizeof(AllocationData::flags) == sizeof(AllocationData::allFlags), "");
        GraphicsAllocation::AllocationType type = GraphicsAllocation::AllocationType::UNKNOWN;
        const void *hostPtr = nullptr;
        size_t size = 0;
        size_t alignment = 0;
        StorageInfo storageInfo = {};
        ImageInfo *imgInfo = nullptr;
        uint32_t rootDeviceIndex = 0;
    };

    static bool getAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const void *hostPtr, const StorageInfo &storageInfo);
    static bool useInternal32BitAllocator(GraphicsAllocation::AllocationType allocationType) {
        return allocationType == GraphicsAllocation::AllocationType::KERNEL_ISA ||
               allocationType == GraphicsAllocation::AllocationType::INTERNAL_HEAP;
    }
    StorageInfo createStorageInfoFromProperties(const AllocationProperties &properties);

    virtual GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) = 0;
    GraphicsAllocation *allocateGraphicsMemory(const AllocationData &allocationData);
    virtual GraphicsAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData);
    virtual GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) = 0;
    GraphicsAllocation *allocateGraphicsMemoryForImageFromHostPtr(const AllocationData &allocationData);
    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData);
    virtual GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) = 0;
    virtual GraphicsAllocation *allocateShareableMemory(const AllocationData &allocationData) = 0;
    virtual void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) = 0;
    virtual void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) = 0;
    virtual void freeAssociatedResourceImpl(GraphicsAllocation &graphicsAllocation) { return unlockResourceImpl(graphicsAllocation); };
    uint32_t getBanksCount();

    bool force32bitAllocations = false;
    bool virtualPaddingAvailable = false;
    std::unique_ptr<DeferredDeleter> deferredDeleter;
    bool asyncDeleterEnabled = false;
    bool enable64kbpages = false;
    bool localMemorySupported = false;
    bool supportsMultiStorageResources = true;
    ExecutionEnvironment &executionEnvironment;
    EngineControlContainer registeredEngines;
    std::unique_ptr<HostPtrManager> hostPtrManager;
    uint32_t latestContextId = std::numeric_limits<uint32_t>::max();
    uint32_t defaultEngineIndex = 0;
    std::unique_ptr<DeferredDeleter> multiContextResourceDestructor;
    std::vector<std::unique_ptr<GfxPartition>> gfxPartitions;
    std::unique_ptr<LocalMemoryUsageBankSelector> localMemoryUsageBankSelector;
    void *reservedMemory = nullptr;
    std::unique_ptr<PageFaultManager> pageFaultManager;
};

std::unique_ptr<DeferredDeleter> createDeferredDeleter();
} // namespace NEO
