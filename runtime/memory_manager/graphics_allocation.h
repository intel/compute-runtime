/*
 * Copyright (c) 2017, Intel Corporation
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

  public:
    enum AllocationType {
        ALLOCATION_TYPE_UNKNOWN = 0,
        ALLOCATION_TYPE_BUFFER,
        ALLOCATION_TYPE_IMAGE,
        ALLOCATION_TYPE_TAG_BUFFER,
        ALLOCATION_TYPE_WRITABLE = 0x80000000
    };

    virtual ~GraphicsAllocation() = default;
    GraphicsAllocation(void *cpuPtrIn, size_t sizeIn) : size(sizeIn),
                                                        cpuPtr(cpuPtrIn),
                                                        gpuAddress((uint64_t)cpuPtrIn),
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
                                                                                 gpuAddress((uint64_t)cpuPtrIn),
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

    void setAllocationType(int allocationType) { this->allocationType = allocationType; }
    int getAllocationType() const { return allocationType; }

    uint32_t taskCount = ObjectNotUsed;
    OsHandleStorage fragmentsStorage;
    bool isL3Capable();
    bool is32BitAllocation = false;
    uint64_t gpuBaseAddress = 0;
    Gmm *gmm = nullptr;
    uint64_t allocationOffset = 0u;

    int residencyTaskCount = ObjectNotResident;

    bool isResident() {
        return residencyTaskCount != ObjectNotResident;
    }
    void setLocked(bool locked) {
        this->locked = locked;
    }
    bool isLocked() {
        return locked;
    }

  private:
    int allocationType;

    //this variable can only be modified from SubmissionAggregator
    friend class SubmissionAggregator;
    uint32_t inspectionId = 0;
};

using ResidencyContainer = std::vector<GraphicsAllocation *>;

} // namespace OCLRT
