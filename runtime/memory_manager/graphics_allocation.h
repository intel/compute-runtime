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

  public:
    enum AllocationType {
        ALLOCATION_TYPE_UNKNOWN = 0,
        ALLOCATION_TYPE_BUFFER,
        ALLOCATION_TYPE_IMAGE,
        ALLOCATION_TYPE_TAG_BUFFER,
        ALLOCATION_TYPE_LINEAR_STREAM,
        ALLOCATION_TYPE_FILL_PATTERN,
        ALLOCATION_TYPE_NON_AUB_WRITABLE = 0x40000000,
        ALLOCATION_TYPE_WRITABLE = 0x80000000
    };

    virtual ~GraphicsAllocation() = default;
    GraphicsAllocation &operator=(const GraphicsAllocation &) = delete;
    GraphicsAllocation(const GraphicsAllocation &) = delete;
    GraphicsAllocation(void *cpuPtrIn, size_t sizeIn) : size(sizeIn),
                                                        cpuPtr(cpuPtrIn),
                                                        gpuAddress(castToUint64(cpuPtrIn)),
                                                        sharedHandle(Sharing::nonSharedResource),

                                                        allocationType(ALLOCATION_TYPE_UNKNOWN) {}

    GraphicsAllocation(void *cpuPtrIn, uint64_t gpuAddress, uint64_t baseAddress, size_t sizeIn) : size(sizeIn),
                                                                                                   cpuPtr(cpuPtrIn),
                                                                                                   gpuAddress(gpuAddress),
                                                                                                   sharedHandle(Sharing::nonSharedResource),
                                                                                                   gpuBaseAddress(baseAddress),
                                                                                                   allocationType(ALLOCATION_TYPE_UNKNOWN) {}

    GraphicsAllocation(void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn) : size(sizeIn),
                                                                                 cpuPtr(cpuPtrIn),
                                                                                 gpuAddress(castToUint64(cpuPtrIn)),
                                                                                 sharedHandle(sharedHandleIn),

                                                                                 allocationType(ALLOCATION_TYPE_UNKNOWN) {}

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

    void setAllocationType(uint32_t allocationType) { this->allocationType = allocationType; }
    uint32_t getAllocationType() const { return allocationType; }

    void setTypeAubNonWritable() { this->allocationType |= GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE; }
    void clearTypeAubNonWritable() { this->allocationType &= ~GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE; }
    bool isTypeAubNonWritable() const { return !!(this->allocationType & GraphicsAllocation::ALLOCATION_TYPE_NON_AUB_WRITABLE); }

    uint32_t taskCount = ObjectNotUsed;
    OsHandleStorage fragmentsStorage;
    bool isL3Capable();
    bool is32BitAllocation = false;
    uint64_t gpuBaseAddress = 0;
    Gmm *gmm = nullptr;
    uint64_t allocationOffset = 0u;

    int residencyTaskCount = ObjectNotResident;

    void setEvictable(bool evictable) { this->evictable = evictable; }
    bool peekEvictable() const { return evictable; }

    bool isResident() const { return residencyTaskCount != ObjectNotResident; }
    void setLocked(bool locked) { this->locked = locked; }
    bool isLocked() const { return locked; }

    void incReuseCount() { reuseCount++; }
    void decReuseCount() { reuseCount--; }
    uint32_t peekReuseCount() const { return reuseCount; }
    bool cpuPtrAllocated = false; // flag indicating if cpuPtr is driver-allocated

  protected:
    //this variable can only be modified from SubmissionAggregator
    friend class SubmissionAggregator;
    uint32_t inspectionId = 0;

  private:
    uint32_t allocationType;
};

using ResidencyContainer = std::vector<GraphicsAllocation *>;

} // namespace OCLRT
