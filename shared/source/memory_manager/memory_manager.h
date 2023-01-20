/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/memory_manager/alignment_selector.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memadvise_flags.h"
#include "shared/source/os_interface/os_memory.h"

#include "memory_properties_flags.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace NEO {
class MultiGraphicsAllocation;
class PageFaultManager;
class GfxPartition;
struct ImageInfo;
struct AllocationData;
class GmmHelper;
enum class DriverModelType;
struct AllocationProperties;
class LocalMemoryUsageBankSelector;
class DeferredDeleter;
class ExecutionEnvironment;
class Gmm;
class HostPtrManager;
class OsContext;
class PrefetchManager;

enum AllocationUsage {
    TEMPORARY_ALLOCATION,
    REUSABLE_ALLOCATION,
    DEFERRED_DEALLOCATION
};

struct AlignedMallocRestrictions {
    uintptr_t minAddress;
};

struct AddressRange {
    uint64_t address;
    size_t size;
};

struct VirtualMemoryReservation {
    AddressRange virtualAddressRange;
    MemoryFlags flags;
    bool mapped;
    uint32_t rootDeviceIndex;
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
    virtual GraphicsAllocation *createGraphicsAllocationFromMultipleSharedHandles(const std::vector<osHandle> &handles, AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) = 0;
    virtual GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) = 0;
    virtual void closeSharedHandle(GraphicsAllocation *graphicsAllocation){};
    virtual GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) = 0;

    virtual bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation);

    void *lockResource(GraphicsAllocation *graphicsAllocation);
    void unlockResource(GraphicsAllocation *graphicsAllocation);
    MOCKABLE_VIRTUAL bool peek32bit() {
        return is32bit;
    }
    MOCKABLE_VIRTUAL bool isLimitedGPU(uint32_t rootDeviceIndex);
    MOCKABLE_VIRTUAL bool isLimitedGPUOnType(uint32_t rootDeviceIndex, AllocationType type);

    void cleanGraphicsMemoryCreatedFromHostPtr(GraphicsAllocation *);

    MOCKABLE_VIRTUAL void *createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices, AllocationProperties &properties, MultiGraphicsAllocation &multiGraphicsAllocation, void *ptr);
    MOCKABLE_VIRTUAL void *createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices, AllocationProperties &properties, MultiGraphicsAllocation &multiGraphicsAllocation) {
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
    MOCKABLE_VIRTUAL uint64_t getInternalHeapBaseAddress(uint32_t rootDeviceIndex, bool useLocalMemory);
    uint64_t getExternalHeapBaseAddress(uint32_t rootDeviceIndex, bool useLocalMemory);

    bool isLimitedRange(uint32_t rootDeviceIndex);

    bool peek64kbPagesEnabled(uint32_t rootDeviceIndex) const;
    bool peekForce32BitAllocations() const { return force32bitAllocations; }
    void setForce32BitAllocations(bool newValue) { force32bitAllocations = newValue; }

    DeferredDeleter *getDeferredDeleter() const {
        return deferredDeleter.get();
    }

    PageFaultManager *getPageFaultManager() const {
        return pageFaultManager.get();
    }

    PrefetchManager *getPrefetchManager() const {
        return prefetchManager.get();
    }

    void waitForDeletions();
    MOCKABLE_VIRTUAL void waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation);
    MOCKABLE_VIRTUAL bool allocInUse(GraphicsAllocation &graphicsAllocation);
    void cleanTemporaryAllocationListOnAllEngines(bool waitForCompletion);

    bool isAsyncDeleterEnabled() const;
    bool isLocalMemorySupported(uint32_t rootDeviceIndex) const;
    virtual bool isMemoryBudgetExhausted() const;

    virtual bool isKmdMigrationAvailable(uint32_t rootDeviceIndex) { return false; }

    virtual AlignedMallocRestrictions *getAlignedMallocRestrictions() {
        return nullptr;
    }

    MOCKABLE_VIRTUAL void *alignedMallocWrapper(size_t bytes, size_t alignment);

    MOCKABLE_VIRTUAL void alignedFreeWrapper(void *ptr);

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
    OsContext *getDefaultEngineContext(uint32_t rootDeviceIndex, DeviceBitfield subdevicesBitfield);

    virtual bool copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy);
    virtual bool copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask);
    HeapIndex selectHeap(const GraphicsAllocation *allocation, bool hasPointer, bool isFullRangeSVM, bool useFrontWindow);
    static std::unique_ptr<MemoryManager> createMemoryManager(ExecutionEnvironment &executionEnvironment, DriverModelType driverModel);
    virtual void *reserveCpuAddressRange(size_t size, uint32_t rootDeviceIndex) { return nullptr; };
    virtual void releaseReservedCpuAddressRange(void *reserved, size_t size, uint32_t rootDeviceIndex){};
    void *getReservedMemory(size_t size, size_t alignment);
    GfxPartition *getGfxPartition(uint32_t rootDeviceIndex) { return gfxPartitions.at(rootDeviceIndex).get(); }
    GmmHelper *getGmmHelper(uint32_t rootDeviceIndex);
    virtual AddressRange reserveGpuAddress(const void *requiredStartAddress, size_t size, RootDeviceIndicesContainer rootDeviceIndices, uint32_t *reservedOnRootDeviceIndex) = 0;
    virtual void freeGpuAddress(AddressRange addressRange, uint32_t rootDeviceIndex) = 0;
    static HeapIndex selectInternalHeap(bool useLocalMemory);
    static HeapIndex selectExternalHeap(bool useLocalMemory);

    static uint32_t maxOsContextCount;
    virtual void commonCleanup();
    virtual bool isCpuCopyRequired(const void *ptr) { return false; }
    virtual bool isWCMemory(const void *ptr) { return false; }

    virtual AllocationStatus registerSysMemAlloc(GraphicsAllocation *allocation) { return AllocationStatus::Success; };
    virtual AllocationStatus registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) { return AllocationStatus::Success; };

    virtual bool setMemAdvise(GraphicsAllocation *gfxAllocation, MemAdviseFlags flags, uint32_t rootDeviceIndex) { return true; }
    virtual bool setMemPrefetch(GraphicsAllocation *gfxAllocation, SubDeviceIdsVec &subDeviceIds, uint32_t rootDeviceIndex) { return true; }

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

    bool isKernelBinaryReuseEnabled();

    struct KernelAllocationInfo {
        KernelAllocationInfo(GraphicsAllocation *allocation, uint32_t reuseCounter) : kernelAllocation(allocation), reuseCounter(reuseCounter) {}

        GraphicsAllocation *kernelAllocation;
        uint32_t reuseCounter;
    };

    std::unordered_map<std::string, KernelAllocationInfo> &getKernelAllocationMap() { return this->kernelAllocationMap; };
    [[nodiscard]] std::unique_lock<std::mutex> lockKernelAllocationMap() { return std::unique_lock<std::mutex>(this->kernelAllocationMutex); };
    std::map<void *, VirtualMemoryReservation *> &getVirtualMemoryReservationMap() { return this->virtualMemoryReservationMap; };
    [[nodiscard]] std::unique_lock<std::mutex> lockVirtualMemoryReservationMap() { return std::unique_lock<std::mutex>(this->virtualMemoryReservationMapMutex); };

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
    void zeroCpuMemoryIfRequested(const AllocationData &allocationData, void *cpuPtr, size_t size);
    void updateLatestContextIdForRootDevice(uint32_t rootDeviceIndex);

    bool initialized = false;
    bool forceNonSvmForExternalHostPtr = false;
    bool force32bitAllocations = false;
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
    std::unique_ptr<PrefetchManager> prefetchManager;
    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    HeapAssigner heapAssigner;
    AlignmentSelector alignmentSelector = {};
    std::unique_ptr<std::once_flag[]> checkIsaPlacementOnceFlags;
    std::vector<bool> isaInLocalMemory;
    std::unordered_map<std::string, KernelAllocationInfo> kernelAllocationMap;
    std::mutex kernelAllocationMutex;
    std::map<void *, VirtualMemoryReservation *> virtualMemoryReservationMap;
    std::mutex virtualMemoryReservationMapMutex;
};

std::unique_ptr<DeferredDeleter> createDeferredDeleter();
} // namespace NEO