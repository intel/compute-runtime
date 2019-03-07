/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/memory_manager/host_ptr_defines.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/windows_defs.h"

#include "gmock/gmock.h"

#include <set>
#include <vector>

namespace OCLRT {
class GraphicsAllocation;

namespace WddmMockHelpers {
struct CallResult {
    uint32_t called = 0;
    uint64_t uint64ParamPassed = -1;
    bool success = false;
    uint64_t commandBufferSubmitted = 0u;
    void *commandHeaderSubmitted = nullptr;
    void *cpuPtrPassed = nullptr;
};
struct MakeResidentCall : CallResult {
    std::vector<D3DKMT_HANDLE> handlePack;
    uint32_t handleCount = 0;
};
struct EvictCallResult : CallResult {
    EvictionStatus status = EvictionStatus::UNKNOWN;
};
struct KmDafLockCall : CallResult {
    std::vector<D3DKMT_HANDLE> lockedAllocations;
};
struct WaitFromCpuResult : CallResult {
    const MonitoredFence *monitoredFence = nullptr;
};
} // namespace WddmMockHelpers

class WddmMock : public Wddm {
  public:
    using Wddm::adapter;
    using Wddm::currentPagingFenceValue;
    using Wddm::device;
    using Wddm::featureTable;
    using Wddm::gdi;
    using Wddm::getSystemInfo;
    using Wddm::gmmMemory;
    using Wddm::mapGpuVirtualAddress;
    using Wddm::pagingFenceAddress;
    using Wddm::pagingQueue;
    using Wddm::temporaryResources;
    using Wddm::temporaryResourcesLock;
    using Wddm::wddmInterface;

    WddmMock() : Wddm(){};
    ~WddmMock();

    bool makeResident(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) override;
    bool evict(const D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim) override;
    bool mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr) override;
    bool mapGpuVirtualAddress(WddmAllocation *allocation);
    bool freeGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) override;
    NTSTATUS createAllocation(const void *alignedCpuPtr, const Gmm *gmm, D3DKMT_HANDLE &outHandle) override;
    bool createAllocation64k(const Gmm *gmm, D3DKMT_HANDLE &outHandle) override;
    bool destroyAllocations(const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) override;

    NTSTATUS createAllocation(WddmAllocation *wddmAllocation);
    bool createAllocation64k(WddmAllocation *wddmAllocation);
    bool destroyAllocation(WddmAllocation *alloc, OsContextWin *osContext);
    bool openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc) override;
    bool createContext(OsContextWin &osContext) override;
    void applyAdditionalContextFlags(CREATECONTEXT_PVTDATA &privateData, OsContextWin &osContext) override;
    bool destroyContext(D3DKMT_HANDLE context) override;
    bool queryAdapterInfo() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) override;
    bool waitOnGPU(D3DKMT_HANDLE context) override;
    void *lockResource(const D3DKMT_HANDLE &handle, bool applyMakeResidentPriorToLock) override;
    void unlockResource(const D3DKMT_HANDLE &handle) override;
    void kmDafLock(D3DKMT_HANDLE handle) override;
    bool isKmDafEnabled() const override;
    void setKmDafEnabled(bool state);
    void setHwContextId(unsigned long hwContextId);
    bool openAdapter() override;
    void setHeap32(uint64_t base, uint64_t size);
    GMM_GFX_PARTITIONING *getGfxPartitionPtr();
    bool waitFromCpu(uint64_t lastFenceValue, const MonitoredFence &monitoredFence) override;
    void *virtualAlloc(void *inPtr, size_t size, unsigned long flags, unsigned long type) override;
    int virtualFree(void *ptr, size_t size, unsigned long flags) override;
    void releaseReservedAddress(void *reservedAddress) override;
    VOID *registerTrimCallback(PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback, WddmResidencyController &residencyController) override;
    EvictionStatus evictAllTemporaryResources() override;
    EvictionStatus evictTemporaryResource(const D3DKMT_HANDLE &handle) override;
    void applyBlockingMakeResident(const D3DKMT_HANDLE &handle) override;
    void removeTemporaryResource(const D3DKMT_HANDLE &handle) override;
    std::unique_lock<SpinLock> acquireLock(SpinLock &lock) override;
    D3DGPU_VIRTUAL_ADDRESS reserveGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_SIZE_T size) override;
    bool reserveValidAddressRange(size_t size, void *&reservedMem);
    GmmMemory *getGmmMemory() const;
    PLATFORM *getGfxPlatform() { return gfxPlatform.get(); }
    uint64_t *getPagingFenceAddress() override;

    bool configureDeviceAddressSpace() {
        configureDeviceAddressSpaceResult.called++;
        //create context cant be called before configureDeviceAddressSpace
        if (createContextResult.called > 0) {
            return configureDeviceAddressSpaceResult.success = false;
        } else {
            return configureDeviceAddressSpaceResult.success = Wddm::configureDeviceAddressSpace();
        }
    }

    WddmMockHelpers::MakeResidentCall makeResidentResult;
    WddmMockHelpers::CallResult makeNonResidentResult;
    WddmMockHelpers::CallResult mapGpuVirtualAddressResult;
    WddmMockHelpers::CallResult freeGpuVirtualAddressResult;
    WddmMockHelpers::CallResult createAllocationResult;
    WddmMockHelpers::CallResult destroyAllocationResult;
    WddmMockHelpers::CallResult destroyContextResult;
    WddmMockHelpers::CallResult queryAdapterInfoResult;
    WddmMockHelpers::CallResult submitResult;
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
    WddmMockHelpers::EvictCallResult evictAllTemporaryResourcesResult;
    WddmMockHelpers::EvictCallResult evictTemporaryResourceResult;
    WddmMockHelpers::CallResult applyBlockingMakeResidentResult;
    WddmMockHelpers::CallResult removeTemporaryResourceResult;
    WddmMockHelpers::CallResult acquireLockResult;
    WddmMockHelpers::CallResult registerTrimCallbackResult;
    WddmMockHelpers::CallResult getPagingFenceAddressResult;
    WddmMockHelpers::CallResult reserveGpuVirtualAddressResult;

    NTSTATUS createAllocationStatus;
    bool mapGpuVaStatus;
    bool callBaseDestroyAllocations = true;
    bool failOpenSharedHandle = false;
    bool callBaseMapGpuVa = true;
    std::set<void *> reservedAddresses;
    uintptr_t virtualAllocAddress = OCLRT::windowsMinAddress;
    bool kmDafEnabled = false;
    uint64_t mockPagingFence = 0u;
};

struct GmockWddm : WddmMock {
    GmockWddm() {
        virtualAllocAddress = OCLRT::windowsMinAddress;
    }
    ~GmockWddm() = default;
    bool virtualFreeWrapper(void *ptr, size_t size, uint32_t flags) {
        return true;
    }

    void *virtualAllocWrapper(void *inPtr, size_t size, uint32_t flags, uint32_t type);
    uintptr_t virtualAllocAddress;
    MOCK_METHOD4(makeResident, bool(const D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim));
    MOCK_METHOD3(evict, bool(const D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim));
    MOCK_METHOD1(createAllocationsAndMapGpuVa, NTSTATUS(OsHandleStorage &osHandles));

    NTSTATUS baseCreateAllocationAndMapGpuVa(OsHandleStorage &osHandles) {
        return Wddm::createAllocationsAndMapGpuVa(osHandles);
    }
};
} // namespace OCLRT
