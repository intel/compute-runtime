/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
    WddmAllocation(void *cpuPtrIn, size_t sizeIn, void *alignedCpuPtr, size_t alignedSize, void *reservedAddr, MemoryPool::Type pool, size_t osContextsCount)
        : GraphicsAllocation(cpuPtrIn, sizeIn),
          handle(0),
          gpuPtr(0),
          alignedCpuPtr(alignedCpuPtr),
          alignedSize(alignedSize),
          trimCandidateListPositions(osContextsCount, trimListUnusedPosition) {
        reservedAddressSpace = reservedAddr;
        this->memoryPool = pool;
    }

    WddmAllocation(void *cpuPtrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool, size_t osContextsCount)
        : GraphicsAllocation(cpuPtrIn, sizeIn, sharedHandle),
          handle(0),
          gpuPtr(0),
          alignedCpuPtr(nullptr),
          alignedSize(sizeIn),
          trimCandidateListPositions(osContextsCount, trimListUnusedPosition) {
        reservedAddressSpace = nullptr;
        this->memoryPool = pool;
    }

    WddmAllocation(void *alignedCpuPtr, size_t sizeIn, void *reservedAddress, MemoryPool::Type pool, size_t osContextsCount)
        : WddmAllocation(alignedCpuPtr, sizeIn, alignedCpuPtr, sizeIn, reservedAddress, pool, osContextsCount) {
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

    void setTrimCandidateListPosition(uint32_t osContextId, size_t position) {
        trimCandidateListPositions[osContextId] = position;
    }

    size_t getTrimCandidateListPosition(uint32_t osContextId) const {
        if (osContextId < trimCandidateListPositions.size()) {
            return trimCandidateListPositions[osContextId];
        }
        return trimListUnusedPosition;
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
    std::vector<size_t> trimCandidateListPositions;
    void *reservedAddressSpace;
};
} // namespace OCLRT
