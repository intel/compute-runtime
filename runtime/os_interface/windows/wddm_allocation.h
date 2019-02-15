/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#define UMDF_USING_NTSTATUS
#include "runtime/helpers/aligned_memory.h"
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
    WddmAllocation(void *cpuPtrIn, size_t sizeIn, void *reservedAddr, MemoryPool::Type pool, bool multiOsContextCapable)
        : GraphicsAllocation(cpuPtrIn, castToUint64(cpuPtrIn), 0llu, sizeIn, multiOsContextCapable),
          handle(0),
          gpuPtr(0),
          trimCandidateListPositions(maxOsContextCount, trimListUnusedPosition) {
        reservedAddressSpace = reservedAddr;
        this->memoryPool = pool;
    }

    WddmAllocation(void *cpuPtrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool, bool multiOsContextCapable)
        : GraphicsAllocation(cpuPtrIn, sizeIn, sharedHandle, multiOsContextCapable),
          handle(0),
          gpuPtr(0),
          trimCandidateListPositions(maxOsContextCount, trimListUnusedPosition) {
        reservedAddressSpace = nullptr;
        this->memoryPool = pool;
    }

    void *getAlignedCpuPtr() const {
        return alignDown(this->cpuPtr, MemoryConstants::pageSize);
    }

    size_t getAlignedSize() const {
        return alignSizeWholePage(this->cpuPtr, this->size);
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
    void setGpuAddress(uint64_t graphicsAddress) { this->gpuAddress = graphicsAddress; }
    void setCpuAddress(void *cpuPtr) { this->cpuPtr = cpuPtr; }
    bool needsMakeResidentBeforeLock = false;

    std::string getAllocationInfoString() const {
        std::stringstream ss;
        ss << " Handle: " << handle;
        ss << std::endl;
        return ss.str();
    }

  protected:
    ResidencyData residency;
    std::vector<size_t> trimCandidateListPositions;
    void *reservedAddressSpace;
};
} // namespace OCLRT
