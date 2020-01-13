/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#define UMDF_USING_NTSTATUS
#include "core/helpers/aligned_memory.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/memory_manager/residency.h"
#include "core/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

namespace NEO {

struct OsHandle {
    D3DKMT_HANDLE handle;
    D3DGPU_VIRTUAL_ADDRESS gpuPtr;
    Gmm *gmm;
};

constexpr size_t trimListUnusedPosition = std::numeric_limits<size_t>::max();

class WddmAllocation : public GraphicsAllocation {
  public:
    WddmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, void *reservedAddr, MemoryPool::Type pool)
        : GraphicsAllocation(rootDeviceIndex, allocationType, cpuPtrIn, castToUint64(cpuPtrIn), 0llu, sizeIn, pool), trimCandidateListPositions(MemoryManager::maxOsContextCount, trimListUnusedPosition) {
        reservedAddressRangeInfo.addressPtr = reservedAddr;
        reservedAddressRangeInfo.rangeSize = sizeIn;
    }

    WddmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, void *reservedAddr, MemoryPool::Type pool, uint32_t shareable)
        : GraphicsAllocation(rootDeviceIndex, allocationType, cpuPtrIn, castToUint64(cpuPtrIn), 0llu, sizeIn, pool), shareable(shareable), trimCandidateListPositions(MemoryManager::maxOsContextCount, trimListUnusedPosition) {
        reservedAddressRangeInfo.addressPtr = reservedAddr;
        reservedAddressRangeInfo.rangeSize = sizeIn;
    }

    WddmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, osHandle sharedHandle, MemoryPool::Type pool)
        : GraphicsAllocation(rootDeviceIndex, allocationType, cpuPtrIn, sizeIn, sharedHandle, pool), trimCandidateListPositions(MemoryManager::maxOsContextCount, trimListUnusedPosition) {}

    void *getAlignedCpuPtr() const {
        return alignDown(this->cpuPtr, MemoryConstants::pageSize);
    }

    size_t getAlignedSize() const {
        return alignSizeWholePage(this->cpuPtr, this->size);
    }

    ResidencyData &getResidencyData() {
        return residency;
    }
    const std::array<D3DKMT_HANDLE, EngineLimits::maxHandleCount> &getHandles() const { return handles; }
    D3DKMT_HANDLE &getHandleToModify(uint32_t handleIndex) { return handles[handleIndex]; }
    D3DKMT_HANDLE getDefaultHandle() const { return handles[0]; }
    void setDefaultHandle(D3DKMT_HANDLE handle) {
        handles[0] = handle;
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

    void setGpuAddress(uint64_t graphicsAddress) { this->gpuAddress = graphicsAddress; }
    void setCpuAddress(void *cpuPtr) { this->cpuPtr = cpuPtr; }

    std::string getAllocationInfoString() const override;
    uint64_t &getGpuAddressToModify() { return gpuAddress; }

    // OS assigned fields
    D3DKMT_HANDLE resourceHandle = 0u; // used by shared resources
    bool needsMakeResidentBeforeLock = false;
    D3DGPU_VIRTUAL_ADDRESS reservedGpuVirtualAddress = 0u;
    uint64_t reservedSizeForGpuVirtualAddress = 0u;
    uint32_t shareable = 0u;

  protected:
    std::string getHandleInfoString() const {
        std::stringstream ss;
        for (auto &handle : handles) {
            ss << " Handle: " << handle;
        }
        return ss.str();
    }
    std::array<D3DKMT_HANDLE, EngineLimits::maxHandleCount> handles{};
    ResidencyData residency;
    std::vector<size_t> trimCandidateListPositions;
};
} // namespace NEO
