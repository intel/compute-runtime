/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/definitions/engine_limits.h"
#include "shared/source/memory_manager/definitions/storage_info.h"
#include "shared/source/memory_manager/host_ptr_defines.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/utilities/idlist.h"

namespace NEO {

using osHandle = unsigned int;
inline osHandle toOsHandle(const void *handle) {

    return static_cast<osHandle>(castToUint64(handle));
}

enum class HeapIndex : uint32_t;

namespace Sharing {
constexpr auto nonSharedResource = 0u;
}

class Gmm;
class MemoryManager;
class CommandStreamReceiver;
class GraphicsAllocation;
class ProductHelper;

struct AllocationData;
struct AllocationProperties;

struct AubInfo {
    uint32_t aubWritable = std::numeric_limits<uint32_t>::max();
    uint32_t tbxWritable = std::numeric_limits<uint32_t>::max();
    bool allocDumpable = false;
    bool bcsDumpOnly = false;
    bool memObjectsAllocationWithWritableFlags = false;
    bool writeMemoryOnly = false;
};

struct SurfaceStateInHeapInfo {
    GraphicsAllocation *heapAllocation;
    uint64_t surfaceStateOffset;
    void *ssPtr;
    size_t ssSize;
};

class GraphicsAllocation : public IDNode<GraphicsAllocation>, NEO::NonCopyableAndNonMovableClass {
  public:
    enum UsmInitialPlacement {
        DEFAULT,
        CPU,
        GPU
    };

    ~GraphicsAllocation() override;

    GraphicsAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn,
                       uint64_t canonizedGpuAddress, uint64_t baseAddress, size_t sizeIn, MemoryPool pool, size_t maxOsContextCount);

    GraphicsAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn,
                       size_t sizeIn, osHandle sharedHandleIn, MemoryPool pool, size_t maxOsContextCount, uint64_t canonizedGpuAddress);

    uint32_t getRootDeviceIndex() const { return rootDeviceIndex; }
    void *getUnderlyingBuffer() const { return cpuPtr; }
    void *getDriverAllocatedCpuPtr() const { return driverAllocatedCpuPointer; }
    void setDriverAllocatedCpuPtr(void *allocatedCpuPtr) { driverAllocatedCpuPointer = allocatedCpuPtr; }

    void setCpuPtrAndGpuAddress(void *cpuPtr, uint64_t canonizedGpuAddress) {
        this->cpuPtr = cpuPtr;
        this->gpuAddress = canonizedGpuAddress;
    }
    void setGpuPtr(uint64_t canonizedGpuAddress) {
        this->gpuAddress = canonizedGpuAddress;
    }
    size_t getUnderlyingBufferSize() const { return size; }
    void setSize(size_t size) { this->size = size; }

    uint64_t getAllocationOffset() const {
        return allocationOffset;
    }
    void setAllocationOffset(uint64_t offset) {
        allocationOffset = offset;
    }

    uint64_t getGpuBaseAddress() const {
        return gpuBaseAddress;
    }
    void setGpuBaseAddress(uint64_t baseAddress) {
        gpuBaseAddress = baseAddress;
    }
    uint64_t getGpuAddress() const {
        DEBUG_BREAK_IF(gpuAddress < gpuBaseAddress);
        return gpuAddress + allocationOffset;
    }
    uint64_t getGpuAddressToPatch() const {
        DEBUG_BREAK_IF(gpuAddress < gpuBaseAddress);
        return gpuAddress + allocationOffset - gpuBaseAddress;
    }

    void lock(void *ptr) { lockedPtr = ptr; }
    void unlock() { lockedPtr = nullptr; }
    bool isLocked() const { return lockedPtr != nullptr; }
    void *getLockedPtr() const { return lockedPtr; }

    bool isCoherent() const { return allocationInfo.flags.coherent; }
    void setCoherent(bool coherentIn) { allocationInfo.flags.coherent = coherentIn; }
    void setEvictable(bool evictable) { allocationInfo.flags.evictable = evictable; }
    bool peekEvictable() const { return allocationInfo.flags.evictable; }
    bool isFlushL3Required() const { return allocationInfo.flags.flushL3Required; }
    void setFlushL3Required(bool flushL3Required) { allocationInfo.flags.flushL3Required = flushL3Required; }
    bool isLockedMemory() const { return allocationInfo.flags.lockedMemory; }
    void setLockedMemory(bool locked) { allocationInfo.flags.lockedMemory = locked; }

    bool isUncacheable() const { return allocationInfo.flags.uncacheable; }
    void setUncacheable(bool uncacheable) { allocationInfo.flags.uncacheable = uncacheable; }
    bool is32BitAllocation() const { return allocationInfo.flags.is32BitAllocation; }
    void set32BitAllocation(bool is32BitAllocation) { allocationInfo.flags.is32BitAllocation = is32BitAllocation; }

    void setAubWritable(bool writable, uint32_t banks);
    bool isAubWritable(uint32_t banks) const;
    void setTbxWritable(bool writable, uint32_t banks);
    bool isTbxWritable(uint32_t banks) const;
    void setAllocDumpable(bool dumpable, bool bcsDumpOnly) {
        aubInfo.allocDumpable = dumpable;
        aubInfo.bcsDumpOnly = bcsDumpOnly;
    }
    void setWriteMemoryOnly(bool writeMemoryOnly) {
        aubInfo.writeMemoryOnly = writeMemoryOnly;
    }
    bool isAllocDumpable() const { return aubInfo.allocDumpable; }
    bool isMemObjectsAllocationWithWritableFlags() const { return aubInfo.memObjectsAllocationWithWritableFlags; }
    void setMemObjectsAllocationWithWritableFlags(bool newValue) { aubInfo.memObjectsAllocationWithWritableFlags = newValue; }

    void incReuseCount() { sharingInfo.reuseCount++; }
    void decReuseCount() { sharingInfo.reuseCount--; }
    uint32_t peekReuseCount() const { return sharingInfo.reuseCount; }
    osHandle peekSharedHandle() const { return sharingInfo.sharedHandle; }
    void setSharedHandle(osHandle handle) { sharingInfo.sharedHandle = handle; }

    void setAllocationType(AllocationType allocationType);
    AllocationType getAllocationType() const { return allocationType; }

    MemoryPool getMemoryPool() const { return memoryPool; }
    virtual void setAsReadOnly(){};

    bool isUsed() const { return registeredContextsNum > 0; }
    bool isUsedByManyOsContexts() const { return registeredContextsNum > 1u; }
    bool isUsedByOsContext(uint32_t contextId) const { return objectNotUsed != getTaskCount(contextId); }
    uint32_t getNumRegisteredContexts() const { return registeredContextsNum.load(); }
    MOCKABLE_VIRTUAL void updateTaskCount(TaskCountType newTaskCount, uint32_t contextId);
    MOCKABLE_VIRTUAL TaskCountType getTaskCount(uint32_t contextId) const {
        if (contextId >= usageInfos.size()) {
            return objectNotUsed;
        }

        return usageInfos[contextId].taskCount;
    }
    void releaseUsageInOsContext(uint32_t contextId) { updateTaskCount(objectNotUsed, contextId); }
    uint32_t getInspectionId(uint32_t contextId) const { return usageInfos[contextId].inspectionId; }
    void setInspectionId(uint32_t newInspectionId, uint32_t contextId) { usageInfos[contextId].inspectionId = newInspectionId; }

    MOCKABLE_VIRTUAL bool isResident(uint32_t contextId) const { return GraphicsAllocation::objectNotResident != getResidencyTaskCount(contextId); }
    bool isAlwaysResident(uint32_t contextId) const { return GraphicsAllocation::objectAlwaysResident == getResidencyTaskCount(contextId); }
    void updateResidencyTaskCount(TaskCountType newTaskCount, uint32_t contextId) {
        if (usageInfos[contextId].residencyTaskCount != GraphicsAllocation::objectAlwaysResident || newTaskCount == GraphicsAllocation::objectNotResident) {
            usageInfos[contextId].residencyTaskCount = newTaskCount;
        }
    }
    TaskCountType getResidencyTaskCount(uint32_t contextId) const { return usageInfos[contextId].residencyTaskCount; }
    void releaseResidencyInOsContext(uint32_t contextId) { updateResidencyTaskCount(objectNotResident, contextId); }
    bool isResidencyTaskCountBelow(TaskCountType taskCount, uint32_t contextId) const { return !isResident(contextId) || getResidencyTaskCount(contextId) < taskCount; }

    virtual std::string getAllocationInfoString() const;
    virtual std::string getPatIndexInfoString(const ProductHelper &) const;
    virtual int createInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle) { return 0; }
    virtual int peekInternalHandle(MemoryManager *memoryManager, uint64_t &handle) { return 0; }
    virtual void clearInternalHandle(uint32_t handleId) { return; }

    virtual int peekInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle) {
        return 0;
    }

    virtual uint32_t getNumHandles() {
        return 0u;
    }

    virtual void setNumHandles(uint32_t numHandles) {
    }

    virtual uint64_t getHandleAddressBase(uint32_t handleIndex) {
        return 0lu;
    }

    virtual size_t getHandleSize(uint32_t handleIndex) {
        return 0lu;
    }

    static bool isCpuAccessRequired(AllocationType allocationType) {
        return allocationType == AllocationType::commandBuffer ||
               allocationType == AllocationType::constantSurface ||
               allocationType == AllocationType::globalSurface ||
               allocationType == AllocationType::internalHeap ||
               allocationType == AllocationType::linearStream ||
               allocationType == AllocationType::pipe ||
               allocationType == AllocationType::printfSurface ||
               allocationType == AllocationType::timestampPacketTagBuffer ||
               allocationType == AllocationType::ringBuffer ||
               allocationType == AllocationType::semaphoreBuffer ||
               allocationType == AllocationType::debugContextSaveArea ||
               allocationType == AllocationType::debugSbaTrackingBuffer ||
               allocationType == AllocationType::gpuTimestampDeviceBuffer ||
               allocationType == AllocationType::debugModuleArea ||
               allocationType == AllocationType::assertBuffer ||
               allocationType == AllocationType::syncDispatchToken ||
               allocationType == AllocationType::syncBuffer;
    }
    static bool isLockable(AllocationType allocationType) {
        return isCpuAccessRequired(allocationType) ||
               isIsaAllocationType(allocationType) ||
               allocationType == AllocationType::bufferHostMemory ||
               allocationType == AllocationType::sharedResourceCopy;
    }

    static bool isKernelIsaAllocationType(AllocationType type) {
        return type == AllocationType::kernelIsa ||
               type == AllocationType::kernelIsaInternal;
    }

    static bool isIsaAllocationType(AllocationType type) {
        return isKernelIsaAllocationType(type) ||
               type == AllocationType::debugModuleArea;
    }

    static bool isDebugSurfaceAllocationType(AllocationType type) {
        return type == AllocationType::debugContextSaveArea ||
               type == AllocationType::debugSbaTrackingBuffer;
    }

    static bool isConstantOrGlobalSurfaceAllocationType(AllocationType type) {
        return type == AllocationType::constantSurface ||
               type == AllocationType::globalSurface;
    }

    static bool is2MBPageAllocationType(AllocationType type) {
        return type == AllocationType::timestampPacketTagBuffer ||
               type == AllocationType::gpuTimestampDeviceBuffer ||
               type == AllocationType::profilingTagBuffer;
    }

    static bool isAccessedFromCommandStreamer(AllocationType allocationType) {
        return allocationType == AllocationType::commandBuffer ||
               allocationType == AllocationType::ringBuffer ||
               allocationType == AllocationType::semaphoreBuffer;
    }

    static uint32_t getNumHandlesForKmdSharedAllocation(uint32_t numBanks);

    void *getReservedAddressPtr() const {
        return this->reservedAddressRangeInfo.addressPtr;
    }
    size_t getReservedAddressSize() const {
        return this->reservedAddressRangeInfo.rangeSize;
    }
    void setReservedAddressRange(void *reserveAddress, size_t size) {
        this->reservedAddressRangeInfo.addressPtr = reserveAddress;
        this->reservedAddressRangeInfo.rangeSize = size;
    }

    void prepareHostPtrForResidency(CommandStreamReceiver *csr);

    Gmm *getDefaultGmm() const {
        return getGmm(0u);
    }
    Gmm *getGmm(uint32_t handleId) const {
        return gmms[handleId];
    }
    void setDefaultGmm(Gmm *gmm) {
        setGmm(gmm, 0u);
    }
    void setGmm(Gmm *gmm, uint32_t handleId) {
        gmms[handleId] = gmm;
    }
    void resizeGmms(uint32_t size) {
        gmms.resize(size);
    }

    uint32_t getNumGmms() const {
        return static_cast<uint32_t>(gmms.size());
    }

    uint32_t getUsedPageSize() const;

    bool isAllocatedInLocalMemoryPool() const { return (this->memoryPool == MemoryPool::localMemory); }
    bool isAllocationLockable() const;

    const AubInfo &getAubInfo() const { return aubInfo; }

    bool isCompressionEnabled() const;

    ResidencyData &getResidencyData() {
        return residency;
    }

    uint64_t getBindlessOffset() {
        if (bindlessInfo.heapAllocation == nullptr) {
            return std::numeric_limits<uint64_t>::max();
        }
        return bindlessInfo.surfaceStateOffset;
    }

    void setBindlessInfo(const SurfaceStateInHeapInfo &info) {
        bindlessInfo = info;
    }

    const SurfaceStateInHeapInfo &getBindlessInfo() const {
        return bindlessInfo;
    }
    bool canBeReadOnly() {
        return !cantBeReadOnly;
    }
    void setAsCantBeReadOnly(bool cantBeReadOnly) {
        this->cantBeReadOnly = cantBeReadOnly;
    }
    MOCKABLE_VIRTUAL void updateCompletionDataForAllocationAndFragments(uint64_t newFenceValue, uint32_t contextId);
    void setShareableHostMemory(bool shareableHostMemory) { this->shareableHostMemory = shareableHostMemory; }
    bool isShareableHostMemory() const { return shareableHostMemory; }
    MOCKABLE_VIRTUAL bool hasAllocationReadOnlyType();
    MOCKABLE_VIRTUAL void checkAllocationTypeReadOnlyRestrictions(const AllocationData &allocData);

    OsHandleStorage fragmentsStorage;
    StorageInfo storageInfo = {};

    static constexpr uint32_t defaultBank = 0b1u;
    static constexpr uint32_t allBanks = 0xffffffff;
    constexpr static TaskCountType objectNotResident = std::numeric_limits<TaskCountType>::max();
    constexpr static TaskCountType objectNotUsed = std::numeric_limits<TaskCountType>::max();
    constexpr static TaskCountType objectAlwaysResident = std::numeric_limits<TaskCountType>::max() - 1;

    std::atomic<uint32_t> hostPtrTaskCountAssignment{0};

    bool isExplicitlyMadeResident() const {
        return this->explicitlyMadeResident;
    }
    void setExplicitlyMadeResident(bool explicitlyMadeResident) {
        this->explicitlyMadeResident = explicitlyMadeResident;
    }

  protected:
    struct UsageInfo {
        TaskCountType taskCount = objectNotUsed;
        TaskCountType residencyTaskCount = objectNotResident;
        uint32_t inspectionId = 0u;
    };

    struct SharingInfo {
        uint32_t reuseCount = 0;
        osHandle sharedHandle = Sharing::nonSharedResource;
    };
    struct AllocationInfo {
        union {
            struct {
                uint32_t coherent : 1;
                uint32_t evictable : 1;
                uint32_t flushL3Required : 1;
                uint32_t uncacheable : 1;
                uint32_t is32BitAllocation : 1;
                uint32_t lockedMemory : 1;
                uint32_t reserved : 26;
            } flags;
            uint32_t allFlags = 0u;
        };
        static_assert(sizeof(AllocationInfo::flags) == sizeof(AllocationInfo::allFlags), "");
        AllocationInfo() {
            flags.coherent = false;
            flags.evictable = true;
            flags.flushL3Required = true;
            flags.is32BitAllocation = false;
            flags.lockedMemory = false;
        }
    };

    struct ReservedAddressRange {
        void *addressPtr = nullptr;
        size_t rangeSize = 0;
    };

    friend class SubmissionAggregator;

    const uint32_t rootDeviceIndex;
    AllocationInfo allocationInfo;
    AubInfo aubInfo;
    SharingInfo sharingInfo;
    ReservedAddressRange reservedAddressRangeInfo;
    SurfaceStateInHeapInfo bindlessInfo = {nullptr, 0, nullptr};

    uint64_t allocationOffset = 0u;
    uint64_t gpuBaseAddress = 0;
    uint64_t gpuAddress = 0;
    void *driverAllocatedCpuPointer = nullptr;
    size_t size = 0;
    void *cpuPtr = nullptr;
    void *lockedPtr = nullptr;

    MemoryPool memoryPool = MemoryPool::memoryNull;
    AllocationType allocationType = AllocationType::unknown;

    StackVec<UsageInfo, 32> usageInfos;
    StackVec<Gmm *, EngineLimits::maxHandleCount> gmms;
    ResidencyData residency;
    std::atomic<uint32_t> registeredContextsNum{0};
    bool shareableHostMemory = false;
    bool cantBeReadOnly = false;
    bool explicitlyMadeResident = false;
};

static_assert(NEO::NonCopyableAndNonMovable<GraphicsAllocation>);

} // namespace NEO
