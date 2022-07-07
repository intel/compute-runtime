/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#define UMDF_USING_NTSTATUS
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

namespace NEO {

struct OsHandleWin : OsHandle {
    ~OsHandleWin() override = default;
    D3DKMT_HANDLE handle = 0;
    D3DGPU_VIRTUAL_ADDRESS gpuPtr = 0;
    Gmm *gmm = nullptr;
};

constexpr size_t trimListUnusedPosition = std::numeric_limits<size_t>::max();

class WddmAllocation : public GraphicsAllocation {
  public:
    struct RegistrationData {
        uint64_t gpuVirtualAddress = 0;
        uint64_t size = 0;
    };

    WddmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn, uint64_t canonizedAddress, size_t sizeIn, void *reservedAddr, MemoryPool pool, uint32_t shareable, size_t maxOsContextCount)
        : WddmAllocation(rootDeviceIndex, 1, allocationType, cpuPtrIn, canonizedAddress, sizeIn, reservedAddr, pool, shareable, maxOsContextCount) {}

    WddmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, uint64_t canonizedAddress, size_t sizeIn,
                   void *reservedAddr, MemoryPool pool, uint32_t shareable, size_t maxOsContextCount)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, cpuPtrIn, canonizedAddress, 0llu, sizeIn, pool, maxOsContextCount),
          shareable(shareable), trimCandidateListPositions(maxOsContextCount, trimListUnusedPosition) {
        reservedAddressRangeInfo.addressPtr = reservedAddr;
        reservedAddressRangeInfo.rangeSize = sizeIn;
        handles.resize(gmms.size());
    }

    WddmAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, osHandle sharedHandle,
                   MemoryPool pool, size_t maxOsContextCount, uint64_t canonizedGpuAddress)
        : WddmAllocation(rootDeviceIndex, 1, allocationType, cpuPtrIn, sizeIn, sharedHandle, pool, maxOsContextCount, canonizedGpuAddress) {}

    WddmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn,
                   osHandle sharedHandle, MemoryPool pool, size_t maxOsContextCount, uint64_t canonizedGpuAddress)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, cpuPtrIn, sizeIn, sharedHandle, pool, maxOsContextCount, canonizedGpuAddress),
          trimCandidateListPositions(maxOsContextCount, trimListUnusedPosition) {
        handles.resize(gmms.size());
    }

    void *getAlignedCpuPtr() const {
        return alignDown(this->cpuPtr, MemoryConstants::pageSize);
    }

    size_t getAlignedSize() const {
        return alignSizeWholePage(this->cpuPtr, this->size);
    }

    const StackVec<D3DKMT_HANDLE, EngineLimits::maxHandleCount> &getHandles() const { return handles; }
    D3DKMT_HANDLE &getHandleToModify(uint32_t handleIndex) { return handles[handleIndex]; }
    D3DKMT_HANDLE getDefaultHandle() const { return handles[0]; }
    D3DKMT_HANDLE getHandle(uint32_t handleIndex) const { return handles[handleIndex]; }
    void setDefaultHandle(D3DKMT_HANDLE handle) {
        setHandle(handle, 0);
    }
    void setHandle(D3DKMT_HANDLE handle, uint32_t handleIndex) {
        handles[handleIndex] = handle;
    }

    uint64_t peekInternalHandle(MemoryManager *memoryManager) override {
        return ntSecureHandle;
    }

    uint64_t *getSharedHandleToModify() {
        if (shareable) {
            return &ntSecureHandle;
        }
        return nullptr;
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
    bool allocInFrontWindowPool = false;

  protected:
    uint64_t ntSecureHandle = 0u;
    std::string getHandleInfoString() const {
        std::stringstream ss;
        for (auto &handle : handles) {
            ss << " Handle: " << handle;
        }
        return ss.str();
    }
    std::vector<size_t> trimCandidateListPositions;
    StackVec<D3DKMT_HANDLE, EngineLimits::maxHandleCount> handles;
};
} // namespace NEO
