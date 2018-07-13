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
#define UMDF_USING_NTSTATUS
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include <d3dkmthk.h>

namespace OCLRT {

class Gmm;

struct OsHandle {
    D3DKMT_HANDLE handle;
    D3DGPU_VIRTUAL_ADDRESS gpuPtr;
    Gmm *gmm;
};

const size_t trimListUnusedPosition = (size_t)-1;

class WddmAllocation : public GraphicsAllocation {
  public:
    // OS assigned fields
    D3DKMT_HANDLE handle;              // set by createAllocation
    D3DKMT_HANDLE resourceHandle = 0u; // used by shared resources

    D3DGPU_VIRTUAL_ADDRESS gpuPtr; // set by mapGpuVA
    WddmAllocation(void *cpuPtrIn, size_t sizeIn, void *alignedCpuPtr, size_t alignedSize, void *reservedAddr, MemoryPool::Type pool)
        : GraphicsAllocation(cpuPtrIn, sizeIn),
          handle(0),
          gpuPtr(0),
          alignedCpuPtr(alignedCpuPtr),
          alignedSize(alignedSize) {
        trimListPosition = trimListUnusedPosition;
        reservedAddressSpace = reservedAddr;
        this->memoryPool = pool;
    }

    WddmAllocation(void *cpuPtrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool) : GraphicsAllocation(cpuPtrIn, sizeIn, sharedHandle),
                                                                                                  handle(0),
                                                                                                  gpuPtr(0),
                                                                                                  alignedCpuPtr(nullptr),
                                                                                                  alignedSize(sizeIn) {
        trimListPosition = trimListUnusedPosition;
        reservedAddressSpace = nullptr;
        this->memoryPool = pool;
    }

    WddmAllocation(void *alignedCpuPtr, size_t sizeIn, void *reservedAddress, MemoryPool::Type pool)
        : WddmAllocation(alignedCpuPtr, sizeIn, alignedCpuPtr, sizeIn, reservedAddress, pool) {
    }

    void *getAlignedCpuPtr() const {
        return this->alignedCpuPtr;
    }

    void setAlignedCpuPtr(void *ptr) {
        this->alignedCpuPtr = ptr;
        this->cpuPtr = ptr;
    }

    size_t getAlignedSize() const {
        return this->alignedSize;
    }

    ResidencyData &getResidencyData() {
        return residency;
    }

    void setTrimCandidateListPosition(size_t position) {
        trimListPosition = position;
    }

    size_t getTrimCandidateListPosition() {
        return trimListPosition;
    }

    void *getReservedAddress() const {
        return this->reservedAddressSpace;
    }

    void setReservedAddress(void *reserveMem) {
        this->reservedAddressSpace = reserveMem;
    }

  protected:
    void *alignedCpuPtr;
    size_t alignedSize;
    ResidencyData residency;
    size_t trimListPosition;
    void *reservedAddressSpace;
};
} // namespace OCLRT
