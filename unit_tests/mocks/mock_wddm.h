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
#include <vector>
#include <set>

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
struct MakeResidentCall : public CallResult {
    std::vector<D3DKMT_HANDLE> handlePack;
    uint32_t handleCount = 0;
};
struct EvictCallResult : public CallResult {
    EvictionStatus status = EvictionStatus::UNKNOWN;
};
struct KmDafLockCall : public CallResult {
    std::vector<GraphicsAllocation *> lockedAllocations;
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
    using Wddm::pagingFenceAddress;
    using Wddm::pagingQueue;
    using Wddm::temporaryResources;
    using Wddm::temporaryResourcesLock;
    using Wddm::wddmInterface;

    WddmMock() : Wddm(){};
    ~WddmMock();

    bool makeResident(D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) override;
    bool evict(D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim) override;
    bool mapGpuVirtualAddressImpl(Gmm *gmm, D3DKMT_HANDLE handle, void *cpuPtr, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, HeapIndex heapIndex) override;
    bool freeGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) override;
    NTSTATUS createAllocation(WddmAllocation *alloc) override;
    bool createAllocation64k(WddmAllocation *alloc) override;
    bool destroyAllocations(D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle) override;
    bool destroyAllocation(WddmAllocation *alloc, OsContextWin *osContext);
    bool openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc) override;
    bool createContext(D3DKMT_HANDLE &context, EngineInstanceT engineType, PreemptionMode preemptionMode) override;
    void applyAdditionalContextFlags(CREATECONTEXT_PVTDATA &privateData) override;
    bool destroyContext(D3DKMT_HANDLE context) override;
    bool queryAdapterInfo() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, OsContextWin &osContext) override;
    bool waitOnGPU(D3DKMT_HANDLE context) override;
    void *lockResource(WddmAllocation &allocation) override;
    void unlockResource(WddmAllocation &allocation) override;
    void kmDafLock(WddmAllocation *allocation) override;
    bool isKmDafEnabled() override;
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
    EvictionStatus evictTemporaryResource(WddmAllocation &allocation) override;
    void applyBlockingMakeResident(WddmAllocation &allocation) override;
    std::unique_lock<SpinLock> acquireLock(SpinLock &lock) override;
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
    WddmMockHelpers::CallResult waitFromCpuResult;
    WddmMockHelpers::CallResult releaseReservedAddressResult;
    WddmMockHelpers::CallResult reserveValidAddressRangeResult;
    WddmMockHelpers::EvictCallResult evictAllTemporaryResourcesResult;
    WddmMockHelpers::EvictCallResult evictTemporaryResourceResult;
    WddmMockHelpers::CallResult applyBlockingMakeResidentResult;
    WddmMockHelpers::CallResult acquireLockResult;
    WddmMockHelpers::CallResult registerTrimCallbackResult;
    WddmMockHelpers::CallResult getPagingFenceAddressResult;

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
    MOCK_METHOD4(makeResident, bool(D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim));
    MOCK_METHOD3(evict, bool(D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim));
    MOCK_METHOD1(createAllocationsAndMapGpuVa, NTSTATUS(OsHandleStorage &osHandles));

    NTSTATUS baseCreateAllocationAndMapGpuVa(OsHandleStorage &osHandles) {
        return Wddm::createAllocationsAndMapGpuVa(osHandles);
    }
};
} // namespace OCLRT
