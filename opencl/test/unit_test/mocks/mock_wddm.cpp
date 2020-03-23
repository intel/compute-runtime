/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_wddm.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"

#include "opencl/test/unit_test/mock_gdi/mock_gdi.h"
#include "opencl/test/unit_test/mocks/mock_wddm_residency_allocations_container.h"
#include "opencl/test/unit_test/mocks/mock_wddm_residency_logger.h"

#include "gtest/gtest.h"

using namespace NEO;

struct mockHwDeviceId : public HwDeviceId {
    using HwDeviceId::osEnvironment;
};

WddmMock::WddmMock(RootDeviceEnvironment &rootDeviceEnvironment) : Wddm(std::make_unique<HwDeviceId>(ADAPTER_HANDLE, LUID{}, rootDeviceEnvironment.executionEnvironment.osEnvironment.get()), rootDeviceEnvironment) {
    if (!rootDeviceEnvironment.executionEnvironment.osEnvironment.get()) {
        rootDeviceEnvironment.executionEnvironment.osEnvironment = std::make_unique<OsEnvironmentWin>();
    }
    static_cast<mockHwDeviceId *>(this->hwDeviceId.get())->osEnvironment = rootDeviceEnvironment.executionEnvironment.osEnvironment.get();
    this->temporaryResources = std::make_unique<MockWddmResidentAllocationsContainer>(this);
};

WddmMock::~WddmMock() {
    EXPECT_EQ(0, reservedAddresses.size());
}

bool WddmMock::makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t totalSize) {
    makeResidentResult.called++;
    makeResidentResult.handleCount = count;
    for (auto i = 0u; i < count; i++) {
        makeResidentResult.handlePack.push_back(handles[i]);
    }
    if (callBaseMakeResident) {
        return makeResidentResult.success = Wddm::makeResident(handles, count, cantTrimFurther, numberOfBytesToTrim, totalSize);
    } else {
        makeResidentResult.success = makeResidentStatus;
        return makeResidentStatus;
    }
}

bool WddmMock::evict(const D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim) {
    makeNonResidentResult.called++;
    return makeNonResidentResult.success = Wddm::evict(handles, num, sizeToTrim);
}
bool WddmMock::mapGpuVirtualAddress(WddmAllocation *allocation) {
    D3DGPU_VIRTUAL_ADDRESS minimumAddress = gfxPartition.Standard.Base;
    D3DGPU_VIRTUAL_ADDRESS maximumAddress = gfxPartition.Standard.Limit;
    if (allocation->getAlignedCpuPtr()) {
        minimumAddress = 0u;
        maximumAddress = MemoryConstants::maxSvmAddress;
    }
    return mapGpuVirtualAddress(allocation->getDefaultGmm(), allocation->getDefaultHandle(), minimumAddress, maximumAddress,
                                reinterpret_cast<D3DGPU_VIRTUAL_ADDRESS>(allocation->getAlignedCpuPtr()), allocation->getGpuAddressToModify());
}
bool WddmMock::mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr) {
    mapGpuVirtualAddressResult.called++;
    mapGpuVirtualAddressResult.cpuPtrPassed = reinterpret_cast<void *>(preferredAddress);
    mapGpuVirtualAddressResult.uint64ParamPassed = preferredAddress;
    if (callBaseMapGpuVa) {
        return mapGpuVirtualAddressResult.success = Wddm::mapGpuVirtualAddress(gmm, handle, minimumAddress, maximumAddress, preferredAddress, gpuPtr);
    } else {
        gpuPtr = preferredAddress;
        return mapGpuVaStatus;
    }
}

bool WddmMock::freeGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) {
    freeGpuVirtualAddressResult.called++;
    freeGpuVirtualAddressResult.uint64ParamPassed = gpuPtr;
    freeGpuVirtualAddressResult.sizePassed = size;
    return freeGpuVirtualAddressResult.success = Wddm::freeGpuVirtualAddress(gpuPtr, size);
}
NTSTATUS WddmMock::createAllocation(WddmAllocation *wddmAllocation) {
    if (wddmAllocation) {
        return createAllocation(wddmAllocation->getAlignedCpuPtr(), wddmAllocation->getDefaultGmm(), wddmAllocation->getHandleToModify(0u), wddmAllocation->resourceHandle, wddmAllocation->getSharedHandleToModify());
    }
    return false;
}
NTSTATUS WddmMock::createAllocation(const void *alignedCpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle, D3DKMT_HANDLE &outResourceHandle, D3DKMT_HANDLE *outSharedHandle) {
    createAllocationResult.called++;
    if (callBaseDestroyAllocations) {
        createAllocationStatus = Wddm::createAllocation(alignedCpuPtr, gmm, outHandle, outResourceHandle, outSharedHandle);
        createAllocationResult.success = createAllocationStatus == STATUS_SUCCESS;
    } else {
        createAllocationResult.success = true;
        outHandle = ALLOCATION_HANDLE;
        return createAllocationStatus;
    }
    return createAllocationStatus;
}

bool WddmMock::createAllocation64k(WddmAllocation *wddmAllocation) {
    if (wddmAllocation) {
        return createAllocation64k(wddmAllocation->getDefaultGmm(), wddmAllocation->getHandleToModify(0u));
    }
    return false;
}
bool WddmMock::createAllocation64k(const Gmm *gmm, D3DKMT_HANDLE &outHandle) {
    createAllocationResult.called++;
    return createAllocationResult.success = Wddm::createAllocation64k(gmm, outHandle);
}

bool WddmMock::destroyAllocations(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) {
    destroyAllocationResult.called++;
    if (callBaseDestroyAllocations) {
        return destroyAllocationResult.success = Wddm::destroyAllocations(handles, allocationCount, resourceHandle);
    } else {
        return true;
    }
}

bool WddmMock::destroyAllocation(WddmAllocation *alloc, OsContextWin *osContext) {
    const D3DKMT_HANDLE *allocationHandles = nullptr;
    uint32_t allocationCount = 0;
    D3DKMT_HANDLE resourceHandle = 0;
    void *reserveAddress = alloc->getReservedAddressPtr();
    if (alloc->peekSharedHandle()) {
        resourceHandle = alloc->resourceHandle;
    } else {
        allocationHandles = alloc->getHandles().data();
        allocationCount = 1;
    }
    auto success = destroyAllocations(allocationHandles, allocationCount, resourceHandle);
    ::alignedFree(alloc->getDriverAllocatedCpuPtr());
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

bool WddmMock::createContext(OsContextWin &osContext) {
    createContextResult.called++;
    return createContextResult.success = Wddm::createContext(osContext);
}

void WddmMock::applyAdditionalContextFlags(CREATECONTEXT_PVTDATA &privateData, OsContextWin &osContext) {
    applyAdditionalContextFlagsResult.called++;
    Wddm::applyAdditionalContextFlags(privateData, osContext);
}

bool WddmMock::destroyContext(D3DKMT_HANDLE context) {
    destroyContextResult.called++;
    return destroyContextResult.success = Wddm::destroyContext(context);
}

bool WddmMock::queryAdapterInfo() {
    queryAdapterInfoResult.called++;
    return queryAdapterInfoResult.success = Wddm::queryAdapterInfo();
}

bool WddmMock::submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) {
    submitResult.called++;
    submitResult.commandBufferSubmitted = commandBuffer;
    submitResult.size = size;
    submitResult.commandHeaderSubmitted = commandHeader;
    submitResult.submitArgs = submitArguments;
    return submitResult.success = Wddm::submit(commandBuffer, size, commandHeader, submitArguments);
}

bool WddmMock::waitOnGPU(D3DKMT_HANDLE context) {
    waitOnGPUResult.called++;
    return waitOnGPUResult.success = Wddm::waitOnGPU(context);
}

void *WddmMock::lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock, size_t size) {
    lockResult.called++;
    auto ptr = Wddm::lockResource(handle, applyMakeResidentPriorToLock, size);
    lockResult.success = ptr != nullptr;
    lockResult.uint64ParamPassed = applyMakeResidentPriorToLock;
    return ptr;
}

void WddmMock::unlockResource(const D3DKMT_HANDLE &handle) {
    unlockResult.called++;
    unlockResult.success = true;
    Wddm::unlockResource(handle);
}

void WddmMock::kmDafLock(D3DKMT_HANDLE handle) {
    kmDafLockResult.called++;
    kmDafLockResult.success = true;
    kmDafLockResult.lockedAllocations.push_back(handle);
    Wddm::kmDafLock(handle);
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

void WddmMock::resetGdi(Gdi *gdi) {
    static_cast<OsEnvironmentWin *>(this->rootDeviceEnvironment.executionEnvironment.osEnvironment.get())->gdi.reset(gdi);
}

void WddmMock::setHeap32(uint64_t base, uint64_t size) {
    gfxPartition.Heap32[3].Base = base;
    gfxPartition.Heap32[3].Limit = base + size;
}

GMM_GFX_PARTITIONING *WddmMock::getGfxPartitionPtr() {
    return &gfxPartition;
}

bool WddmMock::waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence) {
    waitFromCpuResult.called++;
    waitFromCpuResult.uint64ParamPassed = lastFenceValue;
    waitFromCpuResult.monitoredFence = &monitoredFence;
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

D3DGPU_VIRTUAL_ADDRESS WddmMock::reserveGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_SIZE_T size) {
    reserveGpuVirtualAddressResult.called++;
    return Wddm::reserveGpuVirtualAddress(minimumAddress, maximumAddress, size);
}

uint64_t *WddmMock::getPagingFenceAddress() {
    if (NEO::residencyLoggingAvailable) {
        getPagingFenceAddressResult.called++;
    }
    mockPagingFence++;
    return &mockPagingFence;
}

void WddmMock::waitOnPagingFenceFromCpu() {
    waitOnPagingFenceFromCpuResult.called++;
    Wddm::waitOnPagingFenceFromCpu();
}

void WddmMock::createPagingFenceLogger() {
    if (callBaseCreatePagingLogger) {
        Wddm::createPagingFenceLogger();
    } else {
        if (DebugManager.flags.WddmResidencyLogger.get()) {
            residencyLogger = std::make_unique<MockWddmResidencyLogger>(device, pagingFenceAddress);
        }
    }
}

void *GmockWddm::virtualAllocWrapper(void *inPtr, size_t size, uint32_t flags, uint32_t type) {
    void *tmp = reinterpret_cast<void *>(virtualAllocAddress);
    size += MemoryConstants::pageSize;
    size -= size % MemoryConstants::pageSize;
    virtualAllocAddress += size;
    return tmp;
}

GmockWddm::GmockWddm(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment) {
    virtualAllocAddress = NEO::windowsMinAddress;
}
