/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/mock_gdi/mock_gdi.h"

#include "gtest/gtest.h"

using namespace OCLRT;

WddmMock::~WddmMock() {
    EXPECT_EQ(0, reservedAddresses.size());
}

bool WddmMock::makeResident(D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) {
    makeResidentResult.called++;
    makeResidentResult.handleCount = count;
    for (auto i = 0u; i < count; i++) {
        makeResidentResult.handlePack.push_back(handles[i]);
    }

    return makeResidentResult.success = Wddm::makeResident(handles, count, cantTrimFurther, numberOfBytesToTrim);
}

bool WddmMock::evict(D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim) {
    makeNonResidentResult.called++;
    return makeNonResidentResult.success = Wddm::evict(handles, num, sizeToTrim);
}

bool WddmMock::mapGpuVirtualAddressImpl(Gmm *gmm, D3DKMT_HANDLE handle, void *cpuPtr, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, HeapIndex heapIndex) {
    mapGpuVirtualAddressResult.called++;
    mapGpuVirtualAddressResult.cpuPtrPassed = cpuPtr;
    if (callBaseMapGpuVa) {
        return mapGpuVirtualAddressResult.success = Wddm::mapGpuVirtualAddressImpl(gmm, handle, cpuPtr, gpuPtr, heapIndex);
    } else {
        gpuPtr = reinterpret_cast<D3DGPU_VIRTUAL_ADDRESS>(cpuPtr);
        return mapGpuVaStatus;
    }
}

bool WddmMock::freeGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) {
    freeGpuVirtualAddressResult.called++;
    return freeGpuVirtualAddressResult.success = Wddm::freeGpuVirtualAddress(gpuPtr, size);
}

NTSTATUS WddmMock::createAllocation(WddmAllocation *alloc) {
    createAllocationResult.called++;
    if (callBaseDestroyAllocations) {
        createAllocationStatus = Wddm::createAllocation(alloc);
        createAllocationResult.success = createAllocationStatus == STATUS_SUCCESS;
    } else {
        createAllocationResult.success = true;
        alloc->handle = ALLOCATION_HANDLE;
        return createAllocationStatus;
    }
    return createAllocationStatus;
}

bool WddmMock::createAllocation64k(WddmAllocation *alloc) {
    createAllocationResult.called++;
    return createAllocationResult.success = Wddm::createAllocation64k(alloc);
}

bool WddmMock::destroyAllocations(D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) {
    destroyAllocationResult.called++;
    if (callBaseDestroyAllocations) {
        return destroyAllocationResult.success = Wddm::destroyAllocations(handles, allocationCount, resourceHandle);
    } else {
        return true;
    }
}

bool WddmMock::destroyAllocation(WddmAllocation *alloc, OsContextWin *osContext) {
    D3DKMT_HANDLE *allocationHandles = nullptr;
    uint32_t allocationCount = 0;
    D3DKMT_HANDLE resourceHandle = 0;
    void *reserveAddress = alloc->getReservedAddress();
    if (alloc->peekSharedHandle()) {
        resourceHandle = alloc->resourceHandle;
    } else {
        allocationHandles = &alloc->handle;
        allocationCount = 1;
    }
    auto success = destroyAllocations(allocationHandles, allocationCount, resourceHandle);
    ::alignedFree(alloc->driverAllocatedCpuPointer);
    releaseReservedAddress(reserveAddress);
    return success;
}

bool WddmMock::openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc) {
    if (failOpenSharedHandle) {
        return false;
    } else {
        return Wddm::openSharedHandle(handle, alloc);
    }
}

bool WddmMock::createContext(D3DKMT_HANDLE &context, EngineInstanceT engineType, PreemptionMode preemptionMode) {
    createContextResult.called++;
    return createContextResult.success = Wddm::createContext(context, engineType, preemptionMode);
}

void WddmMock::applyAdditionalContextFlags(CREATECONTEXT_PVTDATA &privateData) {
    applyAdditionalContextFlagsResult.called++;
    Wddm::applyAdditionalContextFlags(privateData);
}

bool WddmMock::destroyContext(D3DKMT_HANDLE context) {
    destroyContextResult.called++;
    return destroyContextResult.success = Wddm::destroyContext(context);
}

bool WddmMock::queryAdapterInfo() {
    queryAdapterInfoResult.called++;
    return queryAdapterInfoResult.success = Wddm::queryAdapterInfo();
}

bool WddmMock::submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) {
    submitResult.called++;
    submitResult.commandBufferSubmitted = commandBuffer;
    submitResult.commandHeaderSubmitted = commandHeader;
    return submitResult.success = Wddm::submit(commandBuffer, size, commandHeader, osContext);
}

bool WddmMock::waitOnGPU(D3DKMT_HANDLE context) {
    waitOnGPUResult.called++;
    return waitOnGPUResult.success = Wddm::waitOnGPU(context);
}

void *WddmMock::lockResource(WddmAllocation &allocation) {
    lockResult.called++;
    auto ptr = Wddm::lockResource(allocation);
    lockResult.success = ptr != nullptr;
    return ptr;
}

void WddmMock::unlockResource(WddmAllocation &allocation) {
    unlockResult.called++;
    unlockResult.success = true;
    Wddm::unlockResource(allocation);
}

void WddmMock::kmDafLock(WddmAllocation *allocation) {
    kmDafLockResult.called++;
    kmDafLockResult.success = true;
    kmDafLockResult.lockedAllocations.push_back(allocation);
    Wddm::kmDafLock(allocation);
}

bool WddmMock::isKmDafEnabled() const {
    return kmDafEnabled;
}

void WddmMock::setKmDafEnabled(bool state) {
    kmDafEnabled = state;
}

void WddmMock::setHwContextId(unsigned long hwContextId) {
    this->hwContextId = hwContextId;
}

bool WddmMock::openAdapter() {
    this->adapter = ADAPTER_HANDLE;
    return true;
}

void WddmMock::setHeap32(uint64_t base, uint64_t size) {
    gfxPartition.Heap32[3].Base = base;
    gfxPartition.Heap32[3].Limit = size;
}

GMM_GFX_PARTITIONING *WddmMock::getGfxPartitionPtr() {
    return &gfxPartition;
}

bool WddmMock::waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence) {
    waitFromCpuResult.called++;
    waitFromCpuResult.uint64ParamPassed = lastFenceValue;
    return waitFromCpuResult.success = Wddm::waitFromCpu(lastFenceValue, monitoredFence);
}

void *WddmMock::virtualAlloc(void *inPtr, size_t size, unsigned long flags, unsigned long type) {
    void *address = Wddm::virtualAlloc(inPtr, size, flags, type);
    virtualAllocAddress = reinterpret_cast<uintptr_t>(address);
    return address;
}

int WddmMock::virtualFree(void *ptr, size_t size, unsigned long flags) {
    int success = Wddm::virtualFree(ptr, size, flags);
    return success;
}

void WddmMock::releaseReservedAddress(void *reservedAddress) {
    releaseReservedAddressResult.called++;
    if (reservedAddress != nullptr) {
        std::set<void *>::iterator it;
        it = reservedAddresses.find(reservedAddress);
        EXPECT_NE(reservedAddresses.end(), it);
        reservedAddresses.erase(it);
    }
    Wddm::releaseReservedAddress(reservedAddress);
}

bool WddmMock::reserveValidAddressRange(size_t size, void *&reservedMem) {
    reserveValidAddressRangeResult.called++;
    bool ret = Wddm::reserveValidAddressRange(size, reservedMem);
    if (reservedMem != nullptr) {
        std::set<void *>::iterator it;
        it = reservedAddresses.find(reservedMem);
        EXPECT_EQ(reservedAddresses.end(), it);
        reservedAddresses.insert(reservedMem);
    }
    return ret;
}
VOID *WddmMock::registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController) {
    registerTrimCallbackResult.called++;
    return Wddm::registerTrimCallback(callback, residencyController);
}

EvictionStatus WddmMock::evictAllTemporaryResources() {
    evictAllTemporaryResourcesResult.called++;
    evictAllTemporaryResourcesResult.status = Wddm::evictAllTemporaryResources();
    return evictAllTemporaryResourcesResult.status;
}

EvictionStatus WddmMock::evictTemporaryResource(WddmAllocation &allocation) {
    evictTemporaryResourceResult.called++;
    evictTemporaryResourceResult.status = Wddm::evictTemporaryResource(allocation);
    return evictTemporaryResourceResult.status;
}

void WddmMock::applyBlockingMakeResident(WddmAllocation &allocation) {
    applyBlockingMakeResidentResult.called++;
    return Wddm::applyBlockingMakeResident(allocation);
}

std::unique_lock<SpinLock> WddmMock::acquireLock(SpinLock &lock) {
    acquireLockResult.called++;
    acquireLockResult.uint64ParamPassed = reinterpret_cast<uint64_t>(&lock);
    return Wddm::acquireLock(lock);
}

GmmMemory *WddmMock::getGmmMemory() const {
    return gmmMemory.get();
}

uint64_t *WddmMock::getPagingFenceAddress() {
    getPagingFenceAddressResult.called++;
    mockPagingFence++;
    return &mockPagingFence;
}

void *GmockWddm::virtualAllocWrapper(void *inPtr, size_t size, uint32_t flags, uint32_t type) {
    void *tmp = reinterpret_cast<void *>(virtualAllocAddress);
    size += MemoryConstants::pageSize;
    size -= size % MemoryConstants::pageSize;
    virtualAllocAddress += size;
    return tmp;
}
