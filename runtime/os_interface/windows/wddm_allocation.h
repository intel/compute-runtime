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

struct OsHandle {
    D3DKMT_HANDLE handle;
    D3DGPU_VIRTUAL_ADDRESS gpuPtr;
    Gmm *gmm;
};

constexpr size_t trimListUnusedPosition = std::numeric_limits<size_t>::max();

class WddmAllocation : public GraphicsAllocation {
  public:
    // OS assigned fields
    D3DKMT_HANDLE handle = 0u;         // set by createAllocation
    D3DKMT_HANDLE resourceHandle = 0u; // used by shared resources

    WddmAllocation(AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, void *reservedAddr, MemoryPool::Type pool, bool multiOsContextCapable)
        : GraphicsAllocation(allocationType, cpuPtrIn, castToUint64(cpuPtrIn), 0llu, sizeIn, pool, multiOsContextCapable), reservedAddressSpace(reservedAddr) {
        trimCandidateListPositions.fill(trimListUnusedPosition);
    }

    WddmAllocation(AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool, bool multiOsContextCapable)
        : GraphicsAllocation(allocationType, cpuPtrIn, sizeIn, sharedHandle, pool, multiOsContextCapable) {
        trimCandidateListPositions.fill(trimListUnusedPosition);
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

    std::string getAllocationInfoString() const override;
    uint64_t &getGpuAddressToModify() { return gpuAddress; }

  protected:
    ResidencyData residency;
    std::array<size_t, maxOsContextCount> trimCandidateListPositions;
    void *reservedAddressSpace = nullptr;
};
} // namespace OCLRT
