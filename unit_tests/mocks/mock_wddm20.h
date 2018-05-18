/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include "runtime/os_interface/windows/wddm/wddm.h"
#include <vector>
#include <set>

namespace OCLRT {
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
struct KmDafLockCall : public CallResult {
    std::vector<GraphicsAllocation *> lockedAllocations;
};
} // namespace WddmMockHelpers

class WddmMock : public Wddm20 {
  public:
    using Wddm::adapter;
    using Wddm::context;
    using Wddm::createHwQueue;
    using Wddm::createMonitoredFence;
    using Wddm::device;
    using Wddm::gdi;
    using Wddm::getSystemInfo;
    using Wddm::gmmMemory;
    using Wddm::hwQueuesSupported;
    using Wddm::pagingQueue;

    WddmMock() : Wddm20(){};
    ~WddmMock();

    bool makeResident(D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) override;
    bool evict(D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim) override;
    bool mapGpuVirtualAddressImpl(Gmm *gmm, D3DKMT_HANDLE handle, void *cpuPtr, uint64_t size, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, bool allocation32Bit, bool use64kbPages, bool useHeap1) override;
    bool freeGpuVirtualAddres(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) override;
    NTSTATUS createAllocation(WddmAllocation *alloc) override;
    bool createAllocation64k(WddmAllocation *alloc) override;
    bool destroyAllocations(D3DKMT_HANDLE *handles, uint32_t allocationCount, uint64_t lastFenceValue, D3DKMT_HANDLE resourceHandle) override;
    bool destroyAllocation(WddmAllocation *alloc);
    bool openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc) override;
    bool createContext() override;
    bool createHwQueue() override;
    bool destroyContext(D3DKMT_HANDLE context) override;
    bool queryAdapterInfo() override;
    bool submit(uint64_t commandBuffer, size_t size, void *commandHeader) override;
    bool waitOnGPU() override;
    void *lockResource(WddmAllocation *allocation) override;
    void unlockResource(WddmAllocation *allocation) override;
    void kmDafLock(WddmAllocation *allocation) override;
    bool isKmDafEnabled() override;
    void setKmDafEnabled(bool state);
    void setHwContextId(unsigned long hwContextId);
    bool openAdapter() override;
    void setHeap32(uint64_t base, uint64_t size);
    GMM_GFX_PARTITIONING *getGfxPartitionPtr();
    bool waitFromCpu(uint64_t lastFenceValue) override;
    void *virtualAlloc(void *inPtr, size_t size, unsigned long flags, unsigned long type) override;
    int virtualFree(void *ptr, size_t size, unsigned long flags) override;
    void releaseReservedAddress(void *reservedAddress) override;
    bool reserveValidAddressRange(size_t size, void *&reservedMem);
    GmmMemory *getGmmMemory() const;

    template <typename GfxFamily>
    bool configureDeviceAddressSpace() {
        configureDeviceAddressSpaceResult.called++;
        //create context cant be called before configureDeviceAddressSpace
        if (createContextResult.called > 0) {
            return configureDeviceAddressSpaceResult.success = false;
        } else {
            return configureDeviceAddressSpaceResult.success = Wddm::configureDeviceAddressSpace<GfxFamily>();
        }
    }

    WddmMockHelpers::MakeResidentCall makeResidentResult;
    WddmMockHelpers::CallResult makeNonResidentResult;
    WddmMockHelpers::CallResult mapGpuVirtualAddressResult;
    WddmMockHelpers::CallResult freeGpuVirtualAddresResult;
    WddmMockHelpers::CallResult createAllocationResult;
    WddmMockHelpers::CallResult destroyAllocationResult;
    WddmMockHelpers::CallResult destroyContextResult;
    WddmMockHelpers::CallResult queryAdapterInfoResult;
    WddmMockHelpers::CallResult submitResult;
    WddmMockHelpers::CallResult waitOnGPUResult;
    WddmMockHelpers::CallResult configureDeviceAddressSpaceResult;
    WddmMockHelpers::CallResult createContextResult;
    WddmMockHelpers::CallResult lockResult;
    WddmMockHelpers::CallResult unlockResult;
    WddmMockHelpers::KmDafLockCall kmDafLockResult;
    WddmMockHelpers::CallResult waitFromCpuResult;
    WddmMockHelpers::CallResult releaseReservedAddressResult;
    WddmMockHelpers::CallResult reserveValidAddressRangeResult;
    WddmMockHelpers::CallResult createHwQueueResult;

    NTSTATUS createAllocationStatus;
    bool mapGpuVaStatus;
    bool callBaseDestroyAllocations = true;
    bool failOpenSharedHandle = false;
    bool callBaseMapGpuVa = true;
    std::set<void *> reservedAddresses;
    uintptr_t virtualAllocAddress = OCLRT::windowsMinAddress;
    bool kmDafEnabled = false;
};

using WddmMock20 = WddmMock;
} // namespace OCLRT
