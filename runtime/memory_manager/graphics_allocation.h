/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/host_ptr_defines.h"
#include "runtime/memory_manager/memory_pool.h"
#include "runtime/memory_manager/residency_container.h"
#include "runtime/utilities/idlist.h"

namespace OCLRT {

typedef unsigned int osHandle;
namespace Sharing {
constexpr auto nonSharedResource = 0u;
}

const int ObjectNotResident = -1;
const uint32_t ObjectNotUsed = (uint32_t)-1;

class Gmm;

class GraphicsAllocation : public IDNode<GraphicsAllocation> {
  protected:
    size_t size = 0;
    void *cpuPtr = nullptr;
    uint64_t gpuAddress = 0;
    bool coherent = false;
    osHandle sharedHandle;
    bool locked = false;
    uint32_t reuseCount = 0; // GraphicsAllocation can be reused by shared resources
    bool evictable = true;
    MemoryPool::Type memoryPool = MemoryPool::MemoryNull;

  public:
    uint32_t taskCount = ObjectNotUsed;
    OsHandleStorage fragmentsStorage;
    bool is32BitAllocation = false;
    uint64_t gpuBaseAddress = 0;
    Gmm *gmm = nullptr;
    uint64_t allocationOffset = 0u;
    int residencyTaskCount = ObjectNotResident;
    bool cpuPtrAllocated = false; // flag indicating if cpuPtr is driver-allocated

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

    virtual ~GraphicsAllocation() = default;
    GraphicsAllocation &operator=(const GraphicsAllocation &) = delete;
    GraphicsAllocation(const GraphicsAllocation &) = delete;
    GraphicsAllocation(void *cpuPtrIn, size_t sizeIn) : size(sizeIn),
                                                        cpuPtr(cpuPtrIn),
                                                        gpuAddress(castToUint64(cpuPtrIn)),
                                                        sharedHandle(Sharing::nonSharedResource) {}

    GraphicsAllocation(void *cpuPtrIn, uint64_t gpuAddress, uint64_t baseAddress, size_t sizeIn) : size(sizeIn),
                                                                                                   cpuPtr(cpuPtrIn),
                                                                                                   gpuAddress(gpuAddress),
                                                                                                   sharedHandle(Sharing::nonSharedResource),
                                                                                                   gpuBaseAddress(baseAddress) {}

    GraphicsAllocation(void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn) : size(sizeIn),
                                                                                 cpuPtr(cpuPtrIn),
                                                                                 gpuAddress(castToUint64(cpuPtrIn)),
                                                                                 sharedHandle(sharedHandleIn) {}

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
    void setGpuAddress(uint64_t graphicsAddress) { this->gpuAddress = graphicsAddress; }
    void setSize(size_t size) { this->size = size; }
    osHandle peekSharedHandle() { return sharedHandle; }

    void setAllocationType(AllocationType allocationType) { this->allocationType = allocationType; }
    AllocationType getAllocationType() const { return allocationType; }

    void setAubWritable(bool writable) { aubWritable = writable; }
    bool isAubWritable() const { return aubWritable; }
    bool isMemObjectsAllocationWithWritableFlags() const { return memObjectsAllocationWithWritableFlags; }
    void setMemObjectsAllocationWithWritableFlags(bool newValue) { memObjectsAllocationWithWritableFlags = newValue; }

    bool isL3Capable();
    void setEvictable(bool evictable) { this->evictable = evictable; }
    bool peekEvictable() const { return evictable; }

    bool isResident() const { return residencyTaskCount != ObjectNotResident; }
    void setLocked(bool locked) { this->locked = locked; }
    bool isLocked() const { return locked; }

    void incReuseCount() { reuseCount++; }
    void decReuseCount() { reuseCount--; }
    uint32_t peekReuseCount() const { return reuseCount; }
    MemoryPool::Type getMemoryPool() {
        return memoryPool;
    }

  protected:
    //this variable can only be modified from SubmissionAggregator
    friend class SubmissionAggregator;
    uint32_t inspectionId = 0;
    AllocationType allocationType = AllocationType::UNKNOWN;
    bool aubWritable = true;
    bool memObjectsAllocationWithWritableFlags = false;
};
} // namespace OCLRT
