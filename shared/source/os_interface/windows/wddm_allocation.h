/*
 * Copyright (C) 2018-2025 Intel Corporation
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

#include <sstream>

namespace NEO {

struct OsHandleWin : OsHandle {
    ~OsHandleWin() override = default;
    D3DGPU_VIRTUAL_ADDRESS gpuPtr = 0;
    Gmm *gmm = nullptr;
    D3DKMT_HANDLE handle = 0;
};

class WddmAllocation : public GraphicsAllocation {
  public:
    struct RegistrationData {
        uint64_t gpuVirtualAddress = 0;
        uint64_t size = 0;
    };

    WddmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, uint64_t canonizedAddress, size_t sizeIn,
                   void *reservedAddr, MemoryPool pool, uint32_t shareable, size_t maxOsContextCount)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, cpuPtrIn, canonizedAddress, 0llu, sizeIn, pool, maxOsContextCount),
          shareable(shareable) {
        reservedAddressRangeInfo.addressPtr = reservedAddr;
        reservedAddressRangeInfo.rangeSize = sizeIn;
        handles.resize(gmms.size());
    }

    WddmAllocation(uint32_t rootDeviceIndex, size_t numGmms, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn,
                   osHandle sharedHandle, MemoryPool pool, size_t maxOsContextCount, uint64_t canonizedGpuAddress)
        : GraphicsAllocation(rootDeviceIndex, numGmms, allocationType, cpuPtrIn, sizeIn, sharedHandle, pool, maxOsContextCount, canonizedGpuAddress) {
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

    void clearInternalHandle(uint32_t handleId) override;

    int createInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle) override;

    int peekInternalHandle(MemoryManager *memoryManager, uint64_t &handle) override {
        handle = ntSecureHandle;
        return handle == 0;
    }

    uint64_t *getSharedHandleToModify() {
        if (shareable) {
            return &ntSecureHandle;
        }
        return nullptr;
    }

    void setGpuAddress(uint64_t graphicsAddress) { this->gpuAddress = graphicsAddress; }
    void setCpuAddress(void *cpuPtr) { this->cpuPtr = cpuPtr; }

    std::string getAllocationInfoString() const override;
    std::string getPatIndexInfoString(const ProductHelper &productHelper) const override;
    uint64_t &getGpuAddressToModify() { return gpuAddress; }
    bool needsMakeResidentBeforeLock() const { return makeResidentBeforeLockRequired; }
    void setMakeResidentBeforeLockRequired(bool makeResidentBeforeLockRequired) { this->makeResidentBeforeLockRequired = makeResidentBeforeLockRequired; }
    bool isAllocInFrontWindowPool() const { return allocInFrontWindowPool; }
    void setAllocInFrontWindowPool(bool allocInFrontWindowPool) { this->allocInFrontWindowPool = allocInFrontWindowPool; }
    bool isShareable() const { return shareable; }
    bool isShareableWithoutNTHandle() const { return shareableWithoutNTHandle; }
    void setShareableWithoutNTHandle(bool shareableWithoutNTHandle) { this->shareableWithoutNTHandle = shareableWithoutNTHandle; }
    bool isPhysicalMemoryReservation() const { return physicalMemoryReservation; }
    void setPhysicalMemoryReservation(bool physicalMemoryReservation) { this->physicalMemoryReservation = physicalMemoryReservation; }
    bool isMappedPhysicalMemoryReservation() const { return mappedPhysicalMemoryReservation; }
    void setMappedPhysicalMemoryReservation(bool mappedPhysicalMemoryReservation) { this->mappedPhysicalMemoryReservation = mappedPhysicalMemoryReservation; }

    D3DGPU_VIRTUAL_ADDRESS getReservedGpuVirtualAddress() const { return reservedGpuVirtualAddress; }
    D3DGPU_VIRTUAL_ADDRESS &getReservedGpuVirtualAddressToModify() { return reservedGpuVirtualAddress; }
    void setReservedGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS reservedGpuVirtualAddress) { this->reservedGpuVirtualAddress = reservedGpuVirtualAddress; }
    uint64_t getReservedSizeForGpuVirtualAddress() const { return reservedSizeForGpuVirtualAddress; }
    void setReservedSizeForGpuVirtualAddress(uint64_t reservedSizeForGpuVirtualAddress) { this->reservedSizeForGpuVirtualAddress = reservedSizeForGpuVirtualAddress; }

    D3DKMT_HANDLE getResourceHandle() const { return resourceHandle; }
    D3DKMT_HANDLE &getResourceHandleToModify() { return resourceHandle; }
    void setResourceHandle(D3DKMT_HANDLE resourceHandle) { this->resourceHandle = resourceHandle; }

  protected:
    // OS assigned fields
    D3DGPU_VIRTUAL_ADDRESS reservedGpuVirtualAddress = 0u;
    uint64_t reservedSizeForGpuVirtualAddress = 0u;
    uint64_t ntSecureHandle = 0u;
    std::string getHandleInfoString() const {
        std::stringstream ss;
        for (auto &handle : handles) {
            ss << " Handle: " << handle;
        }
        return ss.str();
    }

    StackVec<D3DKMT_HANDLE, EngineLimits::maxHandleCount> handles;
    D3DKMT_HANDLE resourceHandle = 0u; // used by shared resources
    uint32_t shareable = 0u;
    bool allocInFrontWindowPool = false;
    bool physicalMemoryReservation = false;
    bool mappedPhysicalMemoryReservation = false;
    bool makeResidentBeforeLockRequired = false;
    bool shareableWithoutNTHandle = false;
};
} // namespace NEO
