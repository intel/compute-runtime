/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/host_ptr_defines.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_residency_allocations_container.h"
#include "shared/source/os_interface/windows/windows_defs.h"
#include "shared/test/common/mocks/wddm_mock_helpers.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <limits>
#include <memory>
#include <set>

namespace NEO {
class GraphicsAllocation;

constexpr auto virtualAllocAddress = is64bit ? 0x7FFFF0000000 : 0xFF000000;

class WddmMock : public Wddm {
  public:
    using Wddm::adapterBDF;
    using Wddm::createPagingFenceLogger;
    using Wddm::currentPagingFenceValue;
    using Wddm::dedicatedVideoMemory;
    using Wddm::device;
    using Wddm::deviceRegistryPath;
    using Wddm::enablePreemptionRegValue;
    using Wddm::featureTable;
    using Wddm::getSystemInfo;
    using Wddm::gmmMemory;
    using Wddm::hwDeviceId;
    using Wddm::mapGpuVirtualAddress;
    using Wddm::minAddress;
    using Wddm::pagingFenceAddress;
    using Wddm::pagingQueue;
    using Wddm::residencyLogger;
    using Wddm::rootDeviceEnvironment;
    using Wddm::temporaryResources;
    using Wddm::timestampFrequency;
    using Wddm::wddmInterface;

    WddmMock(std::unique_ptr<HwDeviceIdWddm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) : Wddm(std::move(hwDeviceId), rootDeviceEnvironment) {}
    WddmMock(RootDeviceEnvironment &rootDeviceEnvironment);
    ~WddmMock();

    bool mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr) override;
    bool mapGpuVirtualAddress(WddmAllocation *allocation);
    bool freeGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) override;
    NTSTATUS createAllocation(const void *alignedCpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle, D3DKMT_HANDLE &outResource, uint64_t *outSharedHandle) override;
    bool createAllocation(const Gmm *gmm, D3DKMT_HANDLE &outHandle) override;
    bool destroyAllocations(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) override;

    NTSTATUS createAllocation(WddmAllocation *wddmAllocation);
    bool createAllocation64k(WddmAllocation *wddmAllocation);
    bool destroyAllocation(WddmAllocation *alloc, OsContextWin *osContext);
    bool openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc) override;
    bool createContext(OsContextWin &osContext) override;
    void applyAdditionalContextFlags(CREATECONTEXT_PVTDATA &privateData, OsContextWin &osContext, const HardwareInfo &hwInfo) override;
    bool destroyContext(D3DKMT_HANDLE context) override;
    bool queryAdapterInfo() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) override;
    bool waitOnGPU(D3DKMT_HANDLE context) override;
    void *lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock, size_t size) override;
    void unlockResource(const D3DKMT_HANDLE &handle) override;
    void kmDafLock(D3DKMT_HANDLE handle) override;
    bool isKmDafEnabled() const override;
    void setKmDafEnabled(bool state);
    void setHwContextId(unsigned long hwContextId);
    void setHeap32(uint64_t base, uint64_t size);
    GMM_GFX_PARTITIONING *getGfxPartitionPtr();
    bool waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence) override;
    void *virtualAlloc(void *inPtr, size_t size, bool topDownHint) override;
    void virtualFree(void *ptr, size_t size) override;
    void releaseReservedAddress(void *reservedAddress) override;
    VOID *registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController) override;
    D3DGPU_VIRTUAL_ADDRESS reserveGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_SIZE_T size) override;
    bool reserveValidAddressRange(size_t size, void *&reservedMem);
    PLATFORM *getGfxPlatform() { return gfxPlatform.get(); }
    uint64_t *getPagingFenceAddress() override;
    void waitOnPagingFenceFromCpu() override;
    void createPagingFenceLogger() override;
    bool verifyAdapterLuid(LUID adapterLuid) const override {
        if (callBaseVerifyAdapterLuid) {
            return Wddm::verifyAdapterLuid(adapterLuid);
        }
        return verifyAdapterLuidReturnValue;
    }
    bool setAllocationPriority(const D3DKMT_HANDLE *handles, uint32_t allocationCount, uint32_t priority) override;

    bool configureDeviceAddressSpace() {
        configureDeviceAddressSpaceResult.called++;
        //create context cant be called before configureDeviceAddressSpace
        if (createContextResult.called > 0) {
            return configureDeviceAddressSpaceResult.success = false;
        } else {
            return configureDeviceAddressSpaceResult.success = Wddm::configureDeviceAddressSpace();
        }
    }

    uint32_t counterVerifyNTHandle = 0;
    uint32_t counterVerifySharedHandle = 0;
    bool verifyNTHandle(HANDLE handle) override {
        ++counterVerifyNTHandle;
        return Wddm::verifyNTHandle(handle);
    }
    bool verifySharedHandle(D3DKMT_HANDLE osHandle) override {
        ++counterVerifySharedHandle;
        return Wddm::verifySharedHandle(osHandle);
    }

    bool isShutdownInProgress() override {
        return shutdownStatus;
    };

    void resetGdi(Gdi *gdi);
    bool makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim, size_t totalSize) override;
    bool evict(const D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim) override;
    NTSTATUS createAllocationsAndMapGpuVa(OsHandleStorage &osHandles) override;

    WddmMockHelpers::MakeResidentCall makeResidentResult;
    WddmMockHelpers::CallResult evictResult;
    WddmMockHelpers::CallResult createAllocationsAndMapGpuVaResult;
    WddmMockHelpers::CallResult mapGpuVirtualAddressResult;
    WddmMockHelpers::FreeGpuVirtualAddressCall freeGpuVirtualAddressResult;
    WddmMockHelpers::CallResult createAllocationResult;
    WddmMockHelpers::CallResult destroyAllocationResult;
    WddmMockHelpers::CallResult destroyContextResult;
    WddmMockHelpers::CallResult queryAdapterInfoResult;
    WddmMockHelpers::SubmitResult submitResult;
    WddmMockHelpers::CallResult waitOnGPUResult;
    WddmMockHelpers::CallResult configureDeviceAddressSpaceResult;
    WddmMockHelpers::CallResult createContextResult;
    WddmMockHelpers::CallResult applyAdditionalContextFlagsResult;
    WddmMockHelpers::CallResult lockResult;
    WddmMockHelpers::CallResult unlockResult;
    WddmMockHelpers::KmDafLockCall kmDafLockResult;
    WddmMockHelpers::WaitFromCpuResult waitFromCpuResult;
    WddmMockHelpers::CallResult releaseReservedAddressResult;
    WddmMockHelpers::CallResult reserveValidAddressRangeResult;
    WddmMockHelpers::CallResult registerTrimCallbackResult;
    WddmMockHelpers::CallResult getPagingFenceAddressResult;
    WddmMockHelpers::CallResult reserveGpuVirtualAddressResult;
    WddmMockHelpers::CallResult waitOnPagingFenceFromCpuResult;
    WddmMockHelpers::CallResult setAllocationPriorityResult;

    StackVec<WddmMockHelpers::MakeResidentCall, 2> makeResidentParamsPassed{};
    bool makeResidentStatus = true;
    bool callBaseMakeResident = false;
    std::vector<bool> makeResidentResults = {};
    uint64_t makeResidentNumberOfBytesToTrim = 0;
    bool callBaseEvict = false;
    bool evictStatus = true;
    bool callBaseCreateAllocationsAndMapGpuVa = true;
    NTSTATUS createAllocationsAndMapGpuVaStatus = STATUS_UNSUCCESSFUL;
    NTSTATUS createAllocationStatus = STATUS_SUCCESS;
    bool verifyAdapterLuidReturnValue = true;
    bool callBaseVerifyAdapterLuid = false;
    bool mapGpuVaStatus = true;
    bool callBaseDestroyAllocations = true;
    bool failOpenSharedHandle = false;
    bool callBaseMapGpuVa = true;
    std::set<void *> reservedAddresses;
    uintptr_t virtualAllocAddress = NEO::windowsMinAddress;
    bool kmDafEnabled = false;
    uint64_t mockPagingFence = 0u;
    bool callBaseCreatePagingLogger = true;
    bool shutdownStatus = false;
    bool callBaseSetAllocationPriority = true;
};
} // namespace NEO
