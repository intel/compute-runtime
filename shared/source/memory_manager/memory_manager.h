/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/alignment_selector.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/host_ptr_defines.h"
#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/source/memory_manager/memadvise_flags.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

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

struct AddressRange {
    uint64_t address;
    size_t size;
};

constexpr size_t paddingBufferSize = 2 * MemoryConstants::megaByte;

namespace MemoryTransferHelper {
bool transferMemoryToAllocation(bool useBlitter, const Device &device, GraphicsAllocation *dstAllocation, size_t dstOffset, const void *srcMemory, size_t srcSize);
bool transferMemoryToAllocationBanks(const Device &device, GraphicsAllocation *dstAllocation, size_t dstOffset, const void *srcMemory,
                                     size_t srcSize, DeviceBitfield dstMemoryBanks);
} // namespace MemoryTransferHelper

class MemoryManager {
  public:
    enum AllocationStatus {
        Success = 0,
        Error,
        InvalidHostPointer,
        RetryInNonDevicePool
    };

    MemoryManager(ExecutionEnvironment &executionEnvironment);
    bool isInitialized() const { return initialized; }

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

    GraphicsAllocation *allocateInternalGraphicsMemoryWithHostCopy(uint32_t rootDeviceIndex, DeviceBitfield bitField, const void *ptr, size_t size);

    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryInPreferredPool(const AllocationProperties &properties, const void *hostPtr);

    virtual bool verifyHandle(osHandle handle, uint32_t rootDeviceIndex, bool) { return true; }
    virtual bool isNTHandle(osHandle handle, uint32_t rootDeviceIndex) { return false; }
    virtual GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) = 0;
    virtual void closeSharedHandle(GraphicsAllocation *graphicsAllocation){};
    virtual GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) = 0;

    virtual bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation);

    void *lockResource(GraphicsAllocation *graphicsAllocation);
    void unlockResource(GraphicsAllocation *graphicsAllocation);
    MOCKABLE_VIRTUAL bool peek32bit() {
        return is32bit;
    }
    MOCKABLE_VIRTUAL bool isLimitedGPU(uint32_t rootDeviceIndex) {
        return peek32bit() && !peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->isFullRangeSvm();
    }
    MOCKABLE_VIRTUAL bool isLimitedGPUOnType(uint32_t rootDeviceIndex, AllocationType type) {
        return isLimitedGPU(rootDeviceIndex) &&
               (type != AllocationType::MAP_ALLOCATION) &&
               (type != AllocationType::IMAGE);
    }

    void cleanGraphicsMemoryCreatedFromHostPtr(GraphicsAllocation *);
    GraphicsAllocation *createGraphicsAllocationWithPadding(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding);
    virtual GraphicsAllocation *createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding);

    MOCKABLE_VIRTUAL void *createMultiGraphicsAllocationInSystemMemoryPool(std::vector<uint32_t> &rootDeviceIndices, AllocationProperties &properties, MultiGraphicsAllocation &multiGraphicsAllocation, void *ptr);
    MOCKABLE_VIRTUAL void *createMultiGraphicsAllocationInSystemMemoryPool(std::vector<uint32_t> &rootDeviceIndices, AllocationProperties &properties, MultiGraphicsAllocation &multiGraphicsAllocation) {
        return createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphicsAllocation, nullptr);
    }
    virtual GraphicsAllocation *createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation);

    virtual AllocationStatus populateOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) = 0;
    virtual void cleanOsHandles(OsHandleStorage &handleStorage, uint32_t rootDeviceIndex) = 0;

    void freeSystemMemory(void *ptr);

    virtual void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation) = 0;
    virtual void freeGraphicsMemoryImpl(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) = 0;
    MOCKABLE_VIRTUAL void freeGraphicsMemory(GraphicsAllocation *gfxAllocation);
    MOCKABLE_VIRTUAL void freeGraphicsMemory(GraphicsAllocation *gfxAllocation, bool isImportedAllocation);
    virtual void handleFenceCompletion(GraphicsAllocation *allocation){};

    void checkGpuUsageAndDestroyGraphicsAllocations(GraphicsAllocation *gfxAllocation);

    virtual uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) = 0;
    virtual uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) = 0;
    virtual double getPercentOfGlobalMemoryAvailable(uint32_t rootDeviceIndex) = 0;

    uint64_t getMaxApplicationAddress() { return is64bit ? MemoryConstants::max64BitAppAddress : MemoryConstants::max32BitAppAddress; };
    MOCKABLE_VIRTUAL uint64_t getInternalHeapBaseAddress(uint32_t rootDeviceIndex, bool useLocalMemory) { return getGfxPartition(rootDeviceIndex)->getHeapBase(selectInternalHeap(useLocalMemory)); }
    uint64_t getExternalHeapBaseAddress(uint32_t rootDeviceIndex, bool useLocalMemory) { return getGfxPartition(rootDeviceIndex)->getHeapBase(selectExternalHeap(useLocalMemory)); }

    bool isLimitedRange(uint32_t rootDeviceIndex) { return getGfxPartition(rootDeviceIndex)->isLimitedRange(); }

    bool peek64kbPagesEnabled(uint32_t rootDeviceIndex) const;
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
    MOCKABLE_VIRTUAL void waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation);
    void cleanTemporaryAllocationListOnAllEngines(bool waitForCompletion);

    bool isAsyncDeleterEnabled() const;
    bool isLocalMemorySupported(uint32_t rootDeviceIndex) const;
    virtual bool isMemoryBudgetExhausted() const;

    virtual bool isKmdMigrationAvailable(uint32_t rootDeviceIndex) { return false; }

    virtual AlignedMallocRestrictions *getAlignedMallocRestrictions() {
        return nullptr;
    }

    MOCKABLE_VIRTUAL void *alignedMallocWrapper(size_t bytes, size_t alignment) {
        return ::alignedMalloc(bytes, alignment);
    }

    MOCKABLE_VIRTUAL void alignedFreeWrapper(void *ptr) {
        ::alignedFree(ptr);
    }

    MOCKABLE_VIRTUAL bool isHostPointerTrackingEnabled(uint32_t rootDeviceIndex);

    void setForceNonSvmForExternalHostPtr(bool mode) {
        forceNonSvmForExternalHostPtr = mode;
    }

    const ExecutionEnvironment &peekExecutionEnvironment() const { return executionEnvironment; }

    MOCKABLE_VIRTUAL OsContext *createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                                           const EngineDescriptor &engineDescriptor);
    uint32_t getRegisteredEnginesCount() const { return static_cast<uint32_t>(registeredEngines.size()); }
    EngineControlContainer &getRegisteredEngines();
    EngineControl *getRegisteredEngineForCsr(CommandStreamReceiver *commandStreamReceiver);
    void unregisterEngineForCsr(CommandStreamReceiver *commandStreamReceiver);
    HostPtrManager *getHostPtrManager() const { return hostPtrManager.get(); }
    void setDefaultEngineIndex(uint32_t rootDeviceIndex, uint32_t engineIndex) { defaultEngineIndex[rootDeviceIndex] = engineIndex; }
    virtual bool copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy);
    virtual bool copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask);
    HeapIndex selectHeap(const GraphicsAllocation *allocation, bool hasPointer, bool isFullRangeSVM, bool useFrontWindow);
    static std::unique_ptr<MemoryManager> createMemoryManager(ExecutionEnvironment &executionEnvironment, DriverModelType driverModel = DriverModelType::UNKNOWN);
    virtual void *reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) { return nullptr; };
    virtual void releaseReservedCpuAddressRange(void *reserved, size_t size, uint32_t rootDeviceIndex){};
    void *getReservedMemory(size_t size, size_t alignment);
    GfxPartition *getGfxPartition(uint32_t rootDeviceIndex) { return gfxPartitions.at(rootDeviceIndex).get(); }
    virtual AddressRange reserveGpuAddress(size_t size, uint32_t rootDeviceIndex) = 0;
    virtual void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) = 0;
    static HeapIndex selectInternalHeap(bool useLocalMemory) { return useLocalMemory ? HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY : HeapIndex::HEAP_INTERNAL; }
    static HeapIndex selectExternalHeap(bool useLocalMemory) { return useLocalMemory ? HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY : HeapIndex::HEAP_EXTERNAL; }

    static uint32_t maxOsContextCount;
    virtual void commonCleanup(){};
    virtual bool isCpuCopyRequired(const void *ptr) { return false; }

    virtual void registerSysMemAlloc(GraphicsAllocation *allocation){};
    virtual void registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex){};

    virtual bool setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) { return true; }
    virtual bool setMemPrefetch(GraphicsAllocation *gfxAllocation, uint32_t rootDeviceIndex) { return true; }

    bool isExternalAllocation(AllocationType allocationType);
    LocalMemoryUsageBankSelector *getLocalMemoryUsageBankSelector(AllocationType allocationType, uint32_t rootDeviceIndex);

    bool isLocalMemoryUsedForIsa(uint32_t rootDeviceIndex);
    MOCKABLE_VIRTUAL bool isNonSvmBuffer(const void *hostPtr, AllocationType allocationType, uint32_t rootDeviceIndex) {
        return !force32bitAllocations && hostPtr && !isHostPointerTrackingEnabled(rootDeviceIndex) && (allocationType == AllocationType::BUFFER_HOST_MEMORY);
    }

    virtual void releaseDeviceSpecificMemResources(uint32_t rootDeviceIndex){};
    virtual void createDeviceSpecificMemResources(uint32_t rootDeviceIndex){};
    void reInitLatestContextId() {
        latestContextId = std::numeric_limits<uint32_t>::max();
    }

    virtual bool allowIndirectAllocationsAsPack(uint32_t rootDeviceIndex) {
        return false;
    }

    bool isKernelBinaryReuseEnabled() {
        auto reuseBinaries = false;

        if (DebugManager.flags.ReuseKernelBinaries.get() != -1) {
            reuseBinaries = DebugManager.flags.ReuseKernelBinaries.get();
        }

        return reuseBinaries;
    }

    struct KernelAllocationInfo {
        KernelAllocationInfo(GraphicsAllocation *allocation, uint32_t reuseCounter) : kernelAllocation(allocation), reuseCounter(reuseCounter) {}

        GraphicsAllocation *kernelAllocation;
        uint32_t reuseCounter;
    };

    std::unordered_map<std::string, KernelAllocationInfo> &getKernelAllocationMap() { return this->kernelAllocationMap; };
    std::unique_lock<std::mutex> lockKernelAllocationMap() { return std::unique_lock<std::mutex>(this->kernelAllocationMutex); };

  protected:
    bool getAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const void *hostPtr, const StorageInfo &storageInfo);
    static void overrideAllocationData(AllocationData &allocationData, const AllocationProperties &properties);

    static bool isCopyRequired(ImageInfo &imgInfo, const void *hostPtr);

    bool useNonSvmHostPtrAlloc(AllocationType allocationType, uint32_t rootDeviceIndex);
    StorageInfo createStorageInfoFromProperties(const AllocationProperties &properties);

    virtual GraphicsAllocation *createGraphicsAllocation(OsHandleStorage &handleStorage, const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemoryForNonSvmHostPtr(const AllocationData &allocationData) = 0;
    GraphicsAllocation *allocateGraphicsMemory(const AllocationData &allocationData);
    virtual GraphicsAllocation *allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData);
    virtual GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocateUSMHostGraphicsMemory(const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemory64kb(const AllocationData &allocationData) = 0;
    virtual GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) = 0;
    virtual GraphicsAllocation *allocateGraphicsMemoryWithGpuVa(const AllocationData &allocationData) = 0;

    GraphicsAllocation *allocateGraphicsMemoryForImageFromHostPtr(const AllocationData &allocationData);
    MOCKABLE_VIRTUAL GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData);
    virtual GraphicsAllocation *allocateGraphicsMemoryForImageImpl(const AllocationData &allocationData, std::unique_ptr<Gmm> gmm) = 0;
    virtual GraphicsAllocation *allocateMemoryByKMD(const AllocationData &allocationData) = 0;
    virtual void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) = 0;
    virtual void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) = 0;
    virtual void freeAssociatedResourceImpl(GraphicsAllocation &graphicsAllocation) { return unlockResourceImpl(graphicsAllocation); };
    virtual void registerAllocationInOs(GraphicsAllocation *allocation) {}
    bool isAllocationTypeToCapture(AllocationType type) const;
    void zeroCpuMemoryIfRequested(const AllocationData &allocationData, void *cpuPtr, size_t size) {
        if (allocationData.flags.zeroMemory) {
            memset(cpuPtr, 0, size);
        }
    }
    void updateLatestContextIdForRootDevice(uint32_t rootDeviceIndex);

    bool initialized = false;
    bool forceNonSvmForExternalHostPtr = false;
    bool force32bitAllocations = false;
    bool virtualPaddingAvailable = false;
    std::unique_ptr<DeferredDeleter> deferredDeleter;
    bool asyncDeleterEnabled = false;
    std::vector<bool> enable64kbpages;
    std::vector<bool> localMemorySupported;
    std::vector<uint32_t> defaultEngineIndex;
    bool supportsMultiStorageResources = true;
    ExecutionEnvironment &executionEnvironment;
    EngineControlContainer registeredEngines;
    std::unique_ptr<HostPtrManager> hostPtrManager;
    uint32_t latestContextId = std::numeric_limits<uint32_t>::max();
    std::map<uint32_t, uint32_t> rootDeviceIndexToContextId; // This map will contain initial value of latestContextId for each rootDeviceIndex
    std::unique_ptr<DeferredDeleter> multiContextResourceDestructor;
    std::vector<std::unique_ptr<GfxPartition>> gfxPartitions;
    std::vector<std::unique_ptr<LocalMemoryUsageBankSelector>> internalLocalMemoryUsageBankSelector;
    std::vector<std::unique_ptr<LocalMemoryUsageBankSelector>> externalLocalMemoryUsageBankSelector;
    void *reservedMemory = nullptr;
    std::unique_ptr<PageFaultManager> pageFaultManager;
    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    HeapAssigner heapAssigner;
    AlignmentSelector alignmentSelector = {};
    std::unique_ptr<std::once_flag[]> checkIsaPlacementOnceFlags;
    std::vector<bool> isaInLocalMemory;
    std::unordered_map<std::string, KernelAllocationInfo> kernelAllocationMap;
    std::mutex kernelAllocationMutex;
};

std::unique_ptr<DeferredDeleter> createDeferredDeleter();
} // namespace NEO
