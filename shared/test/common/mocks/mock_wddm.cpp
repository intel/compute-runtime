/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_wddm.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"
#include "shared/test/common/mocks/mock_wddm_residency_allocations_container.h"
#include "shared/test/common/mocks/mock_wddm_residency_logger.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace NEO {
NTSTATUS(*pCallEscape)
(D3DKMT_ESCAPE &escapeCommand) = nullptr;
uint32_t (*pGetTimestampFrequency)() = nullptr;
bool (*pPerfOpenEuStallStream)(uint32_t sampleRate, uint32_t minBufferSize) = nullptr;
bool (*pPerfDisableEuStallStream)() = nullptr;
bool (*pPerfReadEuStallStream)(uint8_t *pRawData, size_t *pRawDataSize) = nullptr;
} // namespace NEO

struct MockHwDeviceId : public HwDeviceIdWddm {
    using HwDeviceIdWddm::osEnvironment;
};

WddmMock::WddmMock(RootDeviceEnvironment &rootDeviceEnvironment) : Wddm(std::make_unique<HwDeviceIdWddm>(ADAPTER_HANDLE, LUID{}, 1u, rootDeviceEnvironment.executionEnvironment.osEnvironment.get(), std::make_unique<UmKmDataTranslator>()), rootDeviceEnvironment) {
    if (!rootDeviceEnvironment.executionEnvironment.osEnvironment.get()) {
        rootDeviceEnvironment.executionEnvironment.osEnvironment = std::make_unique<OsEnvironmentWin>();
    }
    static_cast<MockHwDeviceId *>(this->hwDeviceId.get())->osEnvironment = rootDeviceEnvironment.executionEnvironment.osEnvironment.get();
    this->temporaryResources = std::make_unique<MockWddmResidentAllocationsContainer>(this);
};

WddmMock::~WddmMock() {
    EXPECT_EQ(0u, reservedAddresses.size());
}

bool WddmMock::makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t totalSize) {
    makeResidentResult.called++;
    makeResidentResult.handleCount = count;
    makeResidentResult.cantTrimFurther = cantTrimFurther;
    makeResidentResult.totalSize = totalSize;
    if (handles) {
        for (size_t i = 0; i < count; i++) {
            makeResidentResult.handlePack.push_back(handles[i]);
        }
    }
    makeResidentParamsPassed.push_back(makeResidentResult);
    if (callBaseMakeResident) {
        return Wddm::makeResident(handles, count, cantTrimFurther, numberOfBytesToTrim, totalSize);
    }
    if (numberOfBytesToTrim != nullptr) {
        *numberOfBytesToTrim = makeResidentNumberOfBytesToTrim;
    }
    if (makeResidentResult.called <= makeResidentResults.size()) {
        return makeResidentResults[makeResidentResult.called - 1];
    }
    return makeResidentStatus;
}
bool WddmMock::evict(const D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim, bool evictNeeded) {
    evictResult.called++;
    if (callBaseEvict) {
        evictStatus = Wddm::evict(handles, num, sizeToTrim, evictNeeded);
    }
    return evictStatus;
}
NTSTATUS WddmMock::createAllocationsAndMapGpuVa(OsHandleStorage &osHandles) {
    createAllocationsAndMapGpuVaResult.called++;
    if (callBaseCreateAllocationsAndMapGpuVa) {
        createAllocationsAndMapGpuVaStatus = Wddm::createAllocationsAndMapGpuVa(osHandles);
    }
    return createAllocationsAndMapGpuVaStatus;
}
NTSTATUS WddmMock::escape(D3DKMT_ESCAPE &escapeCommand) {
    if (pCallEscape != nullptr) {
        return pCallEscape(escapeCommand);
    }
    return Wddm::escape(escapeCommand);
}
bool WddmMock::mapGpuVirtualAddress(WddmAllocation *allocation) {
    D3DGPU_VIRTUAL_ADDRESS minimumAddress = gfxPartition.Standard.Base;
    D3DGPU_VIRTUAL_ADDRESS maximumAddress = gfxPartition.Standard.Limit;
    if (allocation->getAlignedCpuPtr()) {
        minimumAddress = 0u;
        maximumAddress = MemoryConstants::maxSvmAddress;
    }
    return mapGpuVirtualAddress(allocation->getDefaultGmm(), allocation->getDefaultHandle(), minimumAddress, maximumAddress,
                                reinterpret_cast<D3DGPU_VIRTUAL_ADDRESS>(allocation->getAlignedCpuPtr()), allocation->getGpuAddressToModify(), allocation->getAllocationType());
}
bool WddmMock::mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, AllocationType type) {
    mapGpuVirtualAddressResult.called++;
    mapGpuVirtualAddressResult.cpuPtrPassed = reinterpret_cast<void *>(preferredAddress);
    mapGpuVirtualAddressResult.uint64ParamPassed = preferredAddress;
    mapGpuVirtualAddressResult.alignment = gmm->resourceParams.BaseAlignment;
    if (callBaseMapGpuVa) {
        return mapGpuVirtualAddressResult.success = Wddm::mapGpuVirtualAddress(gmm, handle, minimumAddress, maximumAddress, preferredAddress, gpuPtr, type);
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
        return createAllocation(wddmAllocation->getAlignedCpuPtr(), wddmAllocation->getDefaultGmm(), wddmAllocation->getHandleToModify(0u), wddmAllocation->getResourceHandleToModify(), wddmAllocation->getSharedHandleToModify());
    }
    return false;
}
NTSTATUS WddmMock::createAllocation(const void *alignedCpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle, D3DKMT_HANDLE &outResourceHandle, uint64_t *outSharedHandle) {
    createAllocationResult.called++;
    if (failCreateAllocation) {
        return STATUS_NO_MEMORY;
    }
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

NTSTATUS WddmMock::createAllocation(const void *alignedCpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle, D3DKMT_HANDLE &outResourceHandle, uint64_t *outSharedHandle, bool createNTHandle) {
    createAllocationResult.called++;
    if (failCreateAllocation) {
        return STATUS_NO_MEMORY;
    }
    if (callBaseDestroyAllocations) {
        createAllocationStatus = Wddm::createAllocation(alignedCpuPtr, gmm, outHandle, outResourceHandle, outSharedHandle, createNTHandle);
        createAllocationResult.success = createAllocationStatus == STATUS_SUCCESS;
        if (createAllocationStatus != STATUS_SUCCESS) {
            destroyAllocationResult.called++;
        }
    } else {
        createAllocationResult.success = true;
        outHandle = ALLOCATION_HANDLE;
        outResourceHandle = ALLOCATION_HANDLE;
        if (outSharedHandle && !createNTHandle) {
            // For shared allocations without NT handle, set a special value
            *outSharedHandle = 1u;
        }
        return createAllocationStatus;
    }
    return createAllocationStatus;
}

bool WddmMock::createAllocation64k(WddmAllocation *wddmAllocation) {
    if (wddmAllocation) {
        return createAllocation(wddmAllocation->getDefaultGmm(), wddmAllocation->getHandleToModify(0u));
    }
    return false;
}
bool WddmMock::createAllocation(const Gmm *gmm, D3DKMT_HANDLE &outHandle) {
    createAllocationResult.called++;
    return createAllocationResult.success = Wddm::createAllocation(gmm, outHandle);
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
        resourceHandle = alloc->getResourceHandle();
    } else {
        allocationHandles = &alloc->getHandles()[0];
        allocationCount = static_cast<uint32_t>(alloc->getHandles().size());
    }
    auto success = destroyAllocations(allocationHandles, allocationCount, resourceHandle);
    ::alignedFree(alloc->getDriverAllocatedCpuPtr());
    releaseReservedAddress(reserveAddress);
    return success;
}

bool WddmMock::openSharedHandle(const MemoryManager::OsHandleData &osHandleData, WddmAllocation *alloc) {
    if (failOpenSharedHandle) {
        return false;
    } else {
        return Wddm::openSharedHandle(osHandleData, alloc);
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

void WddmMock::unlockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock) {
    unlockResult.called++;
    unlockResult.success = true;
    Wddm::unlockResource(handle, applyMakeResidentPriorToLock);
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

bool WddmMock::waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence, bool busyWait) {
    waitFromCpuResult.called++;
    waitFromCpuResult.uint64ParamPassed = lastFenceValue;
    waitFromCpuResult.monitoredFence = &monitoredFence;
    if (callBaseWaitFromCpu) {
        return waitFromCpuResult.success = Wddm::waitFromCpu(lastFenceValue, monitoredFence, busyWait);
    }
    return waitFromCpuResult.success = true;
}

void *WddmMock::virtualAlloc(void *inPtr, size_t size, bool topDownHint) {
    void *address = Wddm::virtualAlloc(inPtr, size, topDownHint);
    virtualAllocAddress = reinterpret_cast<uintptr_t>(address);
    return address;
}

void WddmMock::virtualFree(void *ptr, size_t size) {
    Wddm::virtualFree(ptr, size);
}

void WddmMock::releaseReservedAddress(void *reservedAddress) {
    releaseReservedAddressResult.called++;
    if (reservedAddress != nullptr) {
        std::unordered_multiset<void *>::iterator it;
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
        std::unordered_multiset<void *>::iterator it;
        it = reservedAddresses.find(reservedMem);
        reservedAddresses.insert(reservedMem);
    }
    return ret;
}
VOID *WddmMock::registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController) {
    registerTrimCallbackResult.called++;
    return Wddm::registerTrimCallback(callback, residencyController);
}

NTSTATUS WddmMock::reserveGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS baseAddress, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_SIZE_T size, D3DGPU_VIRTUAL_ADDRESS *reservedAddress) {
    reserveGpuVirtualAddressResult.called++;
    if (failReserveGpuVirtualAddress) {
        D3DGPU_VIRTUAL_ADDRESS nullAddress = {};
        *reservedAddress = nullAddress;
        return STATUS_INVALID_PARAMETER;
    }
    return Wddm::reserveGpuVirtualAddress(baseAddress, minimumAddress, maximumAddress, size, reservedAddress);
}

volatile uint64_t *WddmMock::getPagingFenceAddress() {
    if (NEO::wddmResidencyLoggingAvailable) {
        getPagingFenceAddressResult.called++;
    }
    mockPagingFence++;
    return &mockPagingFence;
}

void WddmMock::waitOnPagingFenceFromCpu(bool isKmdWaitNeeded) {
    waitOnPagingFenceFromCpuResult.called++;
    waitOnPagingFenceFromCpuResult.isKmdWaitNeededPassed = isKmdWaitNeeded;
    Wddm::waitOnPagingFenceFromCpu(isKmdWaitNeeded);
}

void WddmMock::delayPagingFenceFromCpu(int64_t delayTime) {
    delayPagingFenceFromCpuResult.called++;
    Wddm::delayPagingFenceFromCpu(delayTime);
}

void WddmMock::createPagingFenceLogger() {
    if (callBaseCreatePagingLogger) {
        Wddm::createPagingFenceLogger();
    } else {
        if (debugManager.flags.WddmResidencyLogger.get()) {
            residencyLogger = std::make_unique<MockWddmResidencyLogger>(device, pagingFenceAddress, debugManager.flags.WddmResidencyLoggerOutputDirectory.get());
        }
    }
}

bool WddmMock::setAllocationPriority(const D3DKMT_HANDLE *handles, uint32_t allocationCount, uint32_t priority) {
    if (callBaseSetAllocationPriority) {
        auto status = Wddm::setAllocationPriority(handles, allocationCount, priority);
        setAllocationPriorityResult.success = status;
        setAllocationPriorityResult.called++;
        setAllocationPriorityResult.uint64ParamPassed = priority;
        return status;
    }
    return setAllocationPriorityResult.success;
}

bool WddmMock::perfOpenEuStallStream(uint32_t sampleRate, uint32_t minBufferSize) {
    if (pPerfOpenEuStallStream != nullptr) {
        return pPerfOpenEuStallStream(sampleRate, minBufferSize);
    }
    return Wddm::perfOpenEuStallStream(sampleRate, minBufferSize);
}

bool WddmMock::perfDisableEuStallStream() {
    if (pPerfDisableEuStallStream != nullptr) {
        return pPerfDisableEuStallStream();
    }
    return Wddm::perfDisableEuStallStream();
}

bool WddmMock::perfReadEuStallStream(uint8_t *pRawData, size_t *pRawDataSize) {
    if (pPerfReadEuStallStream != nullptr) {
        return pPerfReadEuStallStream(pRawData, pRawDataSize);
    }
    return Wddm::perfReadEuStallStream(pRawData, pRawDataSize);
}

uint32_t WddmMock::getTimestampFrequency() const {
    if (pGetTimestampFrequency != nullptr) {
        return pGetTimestampFrequency();
    }
    return Wddm::getTimestampFrequency();
}