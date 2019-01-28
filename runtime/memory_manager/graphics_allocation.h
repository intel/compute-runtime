/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/host_ptr_defines.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/memory_pool.h"
#include "runtime/memory_manager/residency_container.h"
#include "runtime/utilities/idlist.h"
#include "runtime/utilities/stackvec.h"

namespace OCLRT {

using osHandle = unsigned int;
using DevicesBitfield = uint32_t;

enum class AllocationOrigin {
    EXTERNAL_ALLOCATION,
    INTERNAL_ALLOCATION
};

namespace Sharing {
constexpr auto nonSharedResource = 0u;
}

class Gmm;

class GraphicsAllocation : public IDNode<GraphicsAllocation> {
  public:
    OsHandleStorage fragmentsStorage;
    bool is32BitAllocation = false;
    uint64_t gpuBaseAddress = 0;
    Gmm *gmm = nullptr;
    uint64_t allocationOffset = 0u;
    void *driverAllocatedCpuPointer = nullptr;
    DevicesBitfield devicesBitfield = 0;
    bool flushL3Required = false;

    enum class AllocationType {
        UNKNOWN = 0,
        BUFFER_COMPRESSED,
        BUFFER_HOST_MEMORY,
        BUFFER,
        IMAGE,
        TAG_BUFFER,
        LINEAR_STREAM,
        FILL_PATTERN,
        PIPE,
        TIMESTAMP_TAG_BUFFER,
        COMMAND_BUFFER,
        PRINTF_SURFACE,
        GLOBAL_SURFACE,
        PRIVATE_SURFACE,
        CONSTANT_SURFACE,
        SCRATCH_SURFACE,
        INSTRUCTION_HEAP,
        INDIRECT_OBJECT_HEAP,
        SURFACE_STATE_HEAP,
        DYNAMIC_STATE_HEAP,
        SHARED_RESOURCE_COPY,
        SVM,
        KERNEL_ISA,
        INTERNAL_HEAP,
        UNDECIDED,
    };

    virtual ~GraphicsAllocation();
    GraphicsAllocation &operator=(const GraphicsAllocation &) = delete;
    GraphicsAllocation(const GraphicsAllocation &) = delete;

    GraphicsAllocation(void *cpuPtrIn, uint64_t gpuAddress, uint64_t baseAddress, size_t sizeIn, uint32_t osContextCount, bool multiOsContextCapable);

    GraphicsAllocation(void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn, uint32_t osContextCount, bool multiOsContextCapable);

    void *getUnderlyingBuffer() const { return cpuPtr; }
    void setCpuPtrAndGpuAddress(void *cpuPtr, uint64_t gpuAddress) {
        this->cpuPtr = cpuPtr;
        this->gpuAddress = gpuAddress;
    }
    size_t getUnderlyingBufferSize() const { return size; }
    uint64_t getGpuAddress() {
        DEBUG_BREAK_IF(gpuAddress < gpuBaseAddress);
        return gpuAddress + allocationOffset;
    }

    uint64_t getGpuAddressToPatch() const {
        DEBUG_BREAK_IF(gpuAddress < gpuBaseAddress);
        return gpuAddress + allocationOffset - gpuBaseAddress;
    }

    bool isCoherent() { return coherent; };
    void setCoherent(bool coherentIn) { this->coherent = coherentIn; };
    void setSize(size_t size) { this->size = size; }
    osHandle peekSharedHandle() { return sharedHandle; }

    void setAllocationType(AllocationType allocationType);
    AllocationType getAllocationType() const { return allocationType; }

    void setAubWritable(bool writable) { aubWritable = writable; }
    bool isAubWritable() const { return aubWritable; }
    void setAllocDumpable(bool dumpable) { allocDumpable = dumpable; }
    bool isAllocDumpable() const { return allocDumpable; }
    bool isMemObjectsAllocationWithWritableFlags() const { return memObjectsAllocationWithWritableFlags; }
    void setMemObjectsAllocationWithWritableFlags(bool newValue) { memObjectsAllocationWithWritableFlags = newValue; }

    bool isL3Capable();
    void setEvictable(bool evictable) { this->evictable = evictable; }
    bool peekEvictable() const { return evictable; }

    void lock(void *ptr) { this->lockedPtr = ptr; }
    void unlock() { this->lockedPtr = nullptr; }
    bool isLocked() const { return lockedPtr != nullptr; }
    void *getLockedPtr() const { return lockedPtr; }

    void incReuseCount() { reuseCount++; }
    void decReuseCount() { reuseCount--; }
    uint32_t peekReuseCount() const { return reuseCount; }
    MemoryPool::Type getMemoryPool() const {
        return memoryPool;
    }
    bool isUsed() const { return registeredContextsNum > 0; }
    bool isUsedByOsContext(uint32_t contextId) const { return objectNotUsed != getTaskCount(contextId); }
    void updateTaskCount(uint32_t newTaskCount, uint32_t contextId);
    uint32_t getTaskCount(uint32_t contextId) const { return usageInfos[contextId].taskCount; }
    void releaseUsageInOsContext(uint32_t contextId) { updateTaskCount(objectNotUsed, contextId); }
    uint32_t getInspectionId(uint32_t contextId) { return usageInfos[contextId].inspectionId; }
    void setInspectionId(uint32_t newInspectionId, uint32_t contextId) { usageInfos[contextId].inspectionId = newInspectionId; }

    bool isResident(uint32_t contextId) const { return GraphicsAllocation::objectNotResident != getResidencyTaskCount(contextId); }
    void updateResidencyTaskCount(uint32_t newTaskCount, uint32_t contextId) { usageInfos[contextId].residencyTaskCount = newTaskCount; }
    uint32_t getResidencyTaskCount(uint32_t contextId) const { return usageInfos[contextId].residencyTaskCount; }
    void releaseResidencyInOsContext(uint32_t contextId) { updateResidencyTaskCount(objectNotResident, contextId); }
    bool isResidencyTaskCountBelow(uint32_t taskCount, uint32_t contextId) { return !isResident(contextId) || getResidencyTaskCount(contextId) < taskCount; }

    bool isMultiOsContextCapable() const { return multiOsContextCapable; }
    bool isUsedByManyOsContexts() const { return registeredContextsNum > 1u; }

    virtual std::string getAllocationInfoString() const;

  protected:
    constexpr static uint32_t objectNotResident = (uint32_t)-1;
    constexpr static uint32_t objectNotUsed = (uint32_t)-1;

    struct UsageInfo {
        uint32_t taskCount = objectNotUsed;
        uint32_t residencyTaskCount = objectNotResident;
        uint32_t inspectionId = 0u;
    };

    //this variable can only be modified from SubmissionAggregator
    friend class SubmissionAggregator;
    size_t size = 0;
    void *cpuPtr = nullptr;
    uint64_t gpuAddress = 0;
    bool coherent = false;
    osHandle sharedHandle = Sharing::nonSharedResource;
    void *lockedPtr = nullptr;
    uint32_t reuseCount = 0; // GraphicsAllocation can be reused by shared resources
    bool evictable = true;
    MemoryPool::Type memoryPool = MemoryPool::MemoryNull;
    AllocationType allocationType = AllocationType::UNKNOWN;
    bool aubWritable = true;
    bool allocDumpable = false;
    bool memObjectsAllocationWithWritableFlags = false;
    std::vector<UsageInfo> usageInfos;
    std::atomic<uint32_t> registeredContextsNum{0};
    bool multiOsContextCapable = false;
};
} // namespace OCLRT
