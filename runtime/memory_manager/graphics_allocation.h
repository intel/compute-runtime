/*
 * Copyright (C) 2017-2018 Intel Corporation
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

namespace Sharing {
constexpr auto nonSharedResource = 0u;
}

constexpr uint32_t maxOsContextCount = 4u;
const int ObjectNotResident = -1;
const uint32_t ObjectNotUsed = (uint32_t)-1;

class Gmm;

struct UsageInfo {
    uint32_t taskCount = ObjectNotUsed;
    int residencyTaskCount = ObjectNotResident;
};

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
        EVENT_TAG_BUFFER,
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
        SHARED_RESOURCE,
    };

    virtual ~GraphicsAllocation();
    GraphicsAllocation &operator=(const GraphicsAllocation &) = delete;
    GraphicsAllocation(const GraphicsAllocation &) = delete;

    GraphicsAllocation(void *cpuPtrIn, uint64_t gpuAddress, uint64_t baseAddress, size_t sizeIn);

    GraphicsAllocation(void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn);

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

    void setAllocationType(AllocationType allocationType) { this->allocationType = allocationType; }
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

    bool isResident(uint32_t contextId) const { return ObjectNotResident != getResidencyTaskCount(contextId); }
    void setLocked(bool locked) { this->locked = locked; }
    bool isLocked() const { return locked; }

    void incReuseCount() { reuseCount++; }
    void decReuseCount() { reuseCount--; }
    uint32_t peekReuseCount() const { return reuseCount; }
    MemoryPool::Type getMemoryPool() {
        return memoryPool;
    }
    bool peekWasUsed() const { return registeredContextsNum > 0; }
    void updateTaskCount(uint32_t newTaskCount, uint32_t contextId);
    uint32_t getTaskCount(uint32_t contextId) const { return usageInfos[contextId].taskCount; }

    void updateResidencyTaskCount(int newTaskCount, uint32_t contextId) { usageInfos[contextId].residencyTaskCount = newTaskCount; }
    int getResidencyTaskCount(uint32_t contextId) const { return usageInfos[contextId].residencyTaskCount; }
    void resetResidencyTaskCount(uint32_t contextId) { updateResidencyTaskCount(ObjectNotResident, contextId); }

  protected:
    //this variable can only be modified from SubmissionAggregator
    friend class SubmissionAggregator;
    size_t size = 0;
    void *cpuPtr = nullptr;
    uint64_t gpuAddress = 0;
    bool coherent = false;
    osHandle sharedHandle = Sharing::nonSharedResource;
    bool locked = false;
    uint32_t reuseCount = 0; // GraphicsAllocation can be reused by shared resources
    bool evictable = true;
    MemoryPool::Type memoryPool = MemoryPool::MemoryNull;
    uint32_t inspectionId = 0;
    AllocationType allocationType = AllocationType::UNKNOWN;
    bool aubWritable = true;
    bool allocDumpable = false;
    bool memObjectsAllocationWithWritableFlags = false;
    StackVec<UsageInfo, maxOsContextCount> usageInfos{maxOsContextCount};
    std::atomic<uint32_t> registeredContextsNum{0};
};
} // namespace OCLRT
