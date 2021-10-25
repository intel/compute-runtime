/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/definitions/engine_limits.h"
#include "shared/source/memory_manager/definitions/storage_info.h"
#include "shared/source/memory_manager/host_ptr_defines.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/utilities/idlist.h"
#include "shared/source/utilities/stackvec.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <vector>

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

struct AubInfo {
    uint32_t aubWritable = std::numeric_limits<uint32_t>::max();
    uint32_t tbxWritable = std::numeric_limits<uint32_t>::max();
    bool allocDumpable = false;
    bool bcsDumpOnly = false;
    bool memObjectsAllocationWithWritableFlags = false;
};

class GraphicsAllocation : public IDNode<GraphicsAllocation> {
  public:
    enum class AllocationType {
        UNKNOWN = 0,
        BUFFER,
        BUFFER_COMPRESSED,
        BUFFER_HOST_MEMORY,
        COMMAND_BUFFER,
        CONSTANT_SURFACE,
        DEVICE_QUEUE_BUFFER,
        EXTERNAL_HOST_PTR,
        FILL_PATTERN,
        GLOBAL_SURFACE,
        IMAGE,
        INDIRECT_OBJECT_HEAP,
        INSTRUCTION_HEAP,
        INTERNAL_HEAP,
        INTERNAL_HOST_MEMORY,
        KERNEL_ISA,
        KERNEL_ISA_INTERNAL,
        LINEAR_STREAM,
        MAP_ALLOCATION,
        MCS,
        PIPE,
        PREEMPTION,
        PRINTF_SURFACE,
        PRIVATE_SURFACE,
        PROFILING_TAG_BUFFER,
        SCRATCH_SURFACE,
        SHARED_BUFFER,
        SHARED_CONTEXT_IMAGE,
        SHARED_IMAGE,
        SHARED_RESOURCE_COPY,
        SURFACE_STATE_HEAP,
        SVM_CPU,
        SVM_GPU,
        SVM_ZERO_COPY,
        TAG_BUFFER,
        GLOBAL_FENCE,
        TIMESTAMP_PACKET_TAG_BUFFER,
        WRITE_COMBINED,
        RING_BUFFER,
        SEMAPHORE_BUFFER,
        DEBUG_CONTEXT_SAVE_AREA,
        DEBUG_SBA_TRACKING_BUFFER,
        DEBUG_MODULE_AREA,
        UNIFIED_SHARED_MEMORY,
        WORK_PARTITION_SURFACE,
        GPU_TIMESTAMP_DEVICE_BUFFER,
        SW_TAG_BUFFER,
        COUNT
    };

    ~GraphicsAllocation() override;
    GraphicsAllocation &operator=(const GraphicsAllocation &) = delete;
    GraphicsAllocation(const GraphicsAllocation &) = delete;

    GraphicsAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn,
                       uint64_t gpuAddress, uint64_t baseAddress, size_t sizeIn, MemoryPool::Type pool, size_t maxOsContextCount)
        : GraphicsAllocation(rootDeviceIndex, 1, allocationType, cpuPtrIn, gpuAddress, baseAddress, sizeIn, pool, maxOsContextCount) {}

    GraphicsAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn,
                       size_t sizeIn, osHandle sharedHandleIn, MemoryPool::Type pool, size_t maxOsContextCount)
        : GraphicsAllocation(rootDeviceIndex, 1, allocationType, cpuPtrIn, sizeIn, sharedHandleIn, pool, maxOsContextCount) {}

    GraphicsAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn,
                       uint64_t gpuAddress, uint64_t baseAddress, size_t sizeIn, MemoryPool::Type pool, size_t maxOsContextCount);

    GraphicsAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn,
                       size_t sizeIn, osHandle sharedHandleIn, MemoryPool::Type pool, size_t maxOsContextCount);

    uint32_t getRootDeviceIndex() const { return rootDeviceIndex; }
    void *getUnderlyingBuffer() const { return cpuPtr; }
    void *getDriverAllocatedCpuPtr() const { return driverAllocatedCpuPointer; }
    void setDriverAllocatedCpuPtr(void *allocatedCpuPtr) { driverAllocatedCpuPointer = allocatedCpuPtr; }

    void setCpuPtrAndGpuAddress(void *cpuPtr, uint64_t gpuAddress) {
        this->cpuPtr = cpuPtr;
        this->gpuAddress = GmmHelper::canonize(gpuAddress);
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

    MemoryPool::Type getMemoryPool() const { return memoryPool; }

    bool isUsed() const { return registeredContextsNum > 0; }
    bool isUsedByManyOsContexts() const { return registeredContextsNum > 1u; }
    bool isUsedByOsContext(uint32_t contextId) const { return objectNotUsed != getTaskCount(contextId); }
    void updateTaskCount(uint32_t newTaskCount, uint32_t contextId);
    uint32_t getTaskCount(uint32_t contextId) const { return usageInfos[contextId].taskCount; }
    void releaseUsageInOsContext(uint32_t contextId) { updateTaskCount(objectNotUsed, contextId); }
    uint32_t getInspectionId(uint32_t contextId) const { return usageInfos[contextId].inspectionId; }
    void setInspectionId(uint32_t newInspectionId, uint32_t contextId) { usageInfos[contextId].inspectionId = newInspectionId; }

    bool isResident(uint32_t contextId) const { return GraphicsAllocation::objectNotResident != getResidencyTaskCount(contextId); }
    bool isAlwaysResident(uint32_t contextId) const { return GraphicsAllocation::objectAlwaysResident == getResidencyTaskCount(contextId); }
    void updateResidencyTaskCount(uint32_t newTaskCount, uint32_t contextId) {
        if (usageInfos[contextId].residencyTaskCount != GraphicsAllocation::objectAlwaysResident || newTaskCount == GraphicsAllocation::objectNotResident) {
            usageInfos[contextId].residencyTaskCount = newTaskCount;
        }
    }
    uint32_t getResidencyTaskCount(uint32_t contextId) const { return usageInfos[contextId].residencyTaskCount; }
    void releaseResidencyInOsContext(uint32_t contextId) { updateResidencyTaskCount(objectNotResident, contextId); }
    bool isResidencyTaskCountBelow(uint32_t taskCount, uint32_t contextId) const { return !isResident(contextId) || getResidencyTaskCount(contextId) < taskCount; }

    virtual std::string getAllocationInfoString() const;
    virtual uint64_t peekInternalHandle(MemoryManager *memoryManager) { return 0llu; }

    static bool isCpuAccessRequired(AllocationType allocationType) {
        return allocationType == AllocationType::COMMAND_BUFFER ||
               allocationType == AllocationType::CONSTANT_SURFACE ||
               allocationType == AllocationType::GLOBAL_SURFACE ||
               allocationType == AllocationType::INTERNAL_HEAP ||
               allocationType == AllocationType::LINEAR_STREAM ||
               allocationType == AllocationType::PIPE ||
               allocationType == AllocationType::PRINTF_SURFACE ||
               allocationType == AllocationType::TIMESTAMP_PACKET_TAG_BUFFER ||
               allocationType == AllocationType::RING_BUFFER ||
               allocationType == AllocationType::SEMAPHORE_BUFFER ||
               allocationType == AllocationType::DEBUG_CONTEXT_SAVE_AREA ||
               allocationType == AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER ||
               allocationType == AllocationType::DEBUG_MODULE_AREA;
    }
    static bool isLockable(AllocationType allocationType) {
        return isCpuAccessRequired(allocationType) ||
               isIsaAllocationType(allocationType) ||
               allocationType == AllocationType::BUFFER_HOST_MEMORY ||
               allocationType == AllocationType::SHARED_RESOURCE_COPY;
    }

    static bool isIsaAllocationType(GraphicsAllocation::AllocationType type) {
        return type == GraphicsAllocation::AllocationType::KERNEL_ISA ||
               type == GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL ||
               type == AllocationType::DEBUG_MODULE_AREA;
    }

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

    bool isAllocatedInLocalMemoryPool() const { return (this->memoryPool == MemoryPool::LocalMemory); }
    bool isAllocationLockable() const;

    const AubInfo &getAubInfo() const { return aubInfo; }

    OsHandleStorage fragmentsStorage;
    StorageInfo storageInfo = {};

    static constexpr uint32_t defaultBank = 0b1u;
    static constexpr uint32_t allBanks = 0xffffffff;
    constexpr static uint32_t objectNotResident = std::numeric_limits<uint32_t>::max();
    constexpr static uint32_t objectNotUsed = std::numeric_limits<uint32_t>::max();
    constexpr static uint32_t objectAlwaysResident = std::numeric_limits<uint32_t>::max() - 1;

  protected:
    struct UsageInfo {
        uint32_t taskCount = objectNotUsed;
        uint32_t residencyTaskCount = objectNotResident;
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
                uint32_t reserved : 27;
            } flags;
            uint32_t allFlags = 0u;
        };
        static_assert(sizeof(AllocationInfo::flags) == sizeof(AllocationInfo::allFlags), "");
        AllocationInfo() {
            flags.coherent = false;
            flags.evictable = true;
            flags.flushL3Required = true;
            flags.is32BitAllocation = false;
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

    uint64_t allocationOffset = 0u;
    uint64_t gpuBaseAddress = 0;
    uint64_t gpuAddress = 0;
    void *driverAllocatedCpuPointer = nullptr;
    size_t size = 0;
    void *cpuPtr = nullptr;
    void *lockedPtr = nullptr;

    MemoryPool::Type memoryPool = MemoryPool::MemoryNull;
    AllocationType allocationType = AllocationType::UNKNOWN;

    StackVec<UsageInfo, 32> usageInfos;
    std::atomic<uint32_t> registeredContextsNum{0};
    StackVec<Gmm *, EngineLimits::maxHandleCount> gmms;
};
} // namespace NEO
