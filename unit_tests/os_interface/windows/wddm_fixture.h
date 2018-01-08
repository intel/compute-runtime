/*
 * Copyright (c) 2017, Intel Corporation
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
#if defined(_WIN32)
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/wddm.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/fixtures/gmm_fixture.h"
#include "unit_tests/mock_gdi/mock_gdi.h"
#include "unit_tests/mocks/mock_gmm_memory.h"
#include "unit_tests//os_interface/windows/mock_gdi_interface.h"
#pragma warning(push)
#pragma warning(disable : 4005)
#include <ntstatus.h>
#pragma warning(pop)

using namespace OCLRT;

OsLibrary *setAdapterInfo(const void *platform, const void *gtSystemInfo);

class WddmMock : public Wddm {

    struct CallResult {
        uint32_t called = 0;
        uint64_t uint64ParamPassed = -1;
        bool success = false;
        void *commandBufferSubmitted = nullptr;
        void *commandHeaderSubmitted = nullptr;
        void *cpuPtrPassed = nullptr;
    };
    struct MakeResidentCall : public CallResult {
        std::vector<D3DKMT_HANDLE> handlePack;
        uint32_t handleCount = 0;
    };

  public:
    using Wddm::adapter;
    using Wddm::adapterInfo;
    using Wddm::createMonitoredFence;
    using Wddm::device;
    using Wddm::gdi;
    using Wddm::getSystemInfo;

    WddmMock() : makeResidentResult(),
                 makeNonResidentResult(),
                 mapGpuVirtualAddressResult(),
                 freeGpuVirtualAddresResult(),
                 createAllocationResult(),
                 destroyAllocationResult(),
                 destroyContextResult(),
                 queryAdapterInfoResult(),
                 submitResult(),
                 waitOnGPUResult(),
                 configureDeviceAddressSpaceResult(),
                 createContextResult(),
                 lockResult(),
                 unlockResult(),
                 waitFromCpuResult() {}

    WddmMock(Gdi *gdi) : Wddm(gdi) {
    }

    bool makeResident(D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) override {
        makeResidentResult.called++;
        makeResidentResult.handleCount = count;
        for (auto i = 0u; i < count; i++) {
            makeResidentResult.handlePack.push_back(handles[i]);
        }

        return makeResidentResult.success = Wddm::makeResident(handles, count, cantTrimFurther, numberOfBytesToTrim);
    }
    bool evict(D3DKMT_HANDLE *handles, uint32_t num, uint64_t &sizeToTrim) override {
        makeNonResidentResult.called++;
        return makeNonResidentResult.success = Wddm::evict(handles, num, sizeToTrim);
    }
    bool mapGpuVirtualAddressImpl(Gmm *gmm, D3DKMT_HANDLE handle, void *cpuPtr, uint64_t size, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, bool allocation32Bit, bool use64kbPages) override {
        mapGpuVirtualAddressResult.called++;
        mapGpuVirtualAddressResult.cpuPtrPassed = cpuPtr;
        return mapGpuVirtualAddressResult.success = Wddm::mapGpuVirtualAddressImpl(gmm, handle, cpuPtr, size, gpuPtr, allocation32Bit, use64kbPages);
    }
    bool freeGpuVirtualAddres(D3DGPU_VIRTUAL_ADDRESS &gpuPtr, uint64_t size) override {
        freeGpuVirtualAddresResult.called++;
        return freeGpuVirtualAddresResult.success = Wddm::freeGpuVirtualAddres(gpuPtr, size);
    }
    NTSTATUS createAllocation(WddmAllocation *alloc) override {
        createAllocationResult.called++;
        if (callBaseDestroyAllocations) {
            createAllocationResult.success = Wddm::createAllocation(alloc) == STATUS_SUCCESS;
        } else {
            createAllocationResult.success = true;
            return createAllocationStatus;
        }
        return STATUS_SUCCESS;
    }
    bool createAllocation64k(WddmAllocation *alloc) override {
        createAllocationResult.called++;
        return createAllocationResult.success = Wddm::createAllocation64k(alloc);
    }
    bool destroyAllocations(D3DKMT_HANDLE *handles, uint32_t allocationCount, uint64_t lastFenceValue, D3DKMT_HANDLE resourceHandle) override {
        destroyAllocationResult.called++;
        if (callBaseDestroyAllocations) {
            return destroyAllocationResult.success = Wddm::destroyAllocations(handles, allocationCount, lastFenceValue, resourceHandle);
        } else {
            return true;
        }
    }
    bool destroyAllocation(WddmAllocation *alloc) {
        D3DKMT_HANDLE *allocationHandles = nullptr;
        uint32_t allocationCount = 0;
        D3DKMT_HANDLE resourceHandle = 0;
        void *cpuPtr = nullptr;
        void *gpuPtr = nullptr;
        if (alloc->peekSharedHandle()) {
            resourceHandle = alloc->resourceHandle;
            if (is32bit) {
                gpuPtr = (void *)alloc->gpuPtr;
            }
        } else {
            allocationHandles = &alloc->handle;
            allocationCount = 1;
            if (alloc->cpuPtrAllocated) {
                cpuPtr = alloc->getAlignedCpuPtr();
            }
        }
        auto success = destroyAllocations(allocationHandles, allocationCount, alloc->getResidencyData().lastFence, resourceHandle);
        ::alignedFree(cpuPtr);
        releaseGpuPtr(gpuPtr);
        return success;
    }

    bool openSharedHandle(D3DKMT_HANDLE handle, WddmAllocation *alloc) {
        if (failOpenSharedHandle) {
            return false;
        } else {
            return Wddm::openSharedHandle(handle, alloc);
        }
    }

    D3DKMT_HANDLE createContext() override {
        createContextResult.called++;
        D3DKMT_HANDLE context = Wddm::createContext();
        createContextResult.success = context != 0;
        return context;
    }
    bool destroyContext(D3DKMT_HANDLE context) override {
        destroyContextResult.called++;
        return destroyContextResult.success = Wddm::destroyContext(context);
    }
    bool queryAdapterInfo() override {
        queryAdapterInfoResult.called++;
        return queryAdapterInfoResult.success = Wddm::queryAdapterInfo();
    }

    bool submit(void *commandBuffer, size_t size, void *commandHeader) override {
        submitResult.called++;
        submitResult.commandBufferSubmitted = commandBuffer;
        submitResult.commandHeaderSubmitted = commandHeader;
        return submitResult.success = Wddm::submit(commandBuffer, size, commandHeader);
    }
    bool waitOnGPU() override {
        waitOnGPUResult.called++;
        return waitOnGPUResult.success = Wddm::waitOnGPU();
    }

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
    void *lockResource(WddmAllocation *allocation) override {
        lockResult.called++;
        auto ptr = Wddm::lockResource(allocation);
        lockResult.success = ptr != nullptr;
        return ptr;
    }
    void unlockResource(WddmAllocation *allocation) override {
        unlockResult.called++;
        unlockResult.success = true;
        Wddm::unlockResource(allocation);
    }
    void setHwContextId(unsigned long hwContextId) {
        this->hwContextId = hwContextId;
    }

    bool openAdapter() override {
        this->adapter = ADAPTER_HANDLE;
        return true;
    }

    void setHeap32(uint64_t base, uint64_t size) {
        adapterInfo->GfxPartition.Heap32[0].Base = base;
        adapterInfo->GfxPartition.Heap32[0].Limit = size;
    }

    bool waitFromCpu(uint64_t lastFenceValue) override {
        waitFromCpuResult.called++;
        waitFromCpuResult.uint64ParamPassed = lastFenceValue;
        return waitFromCpuResult.success = Wddm::waitFromCpu(lastFenceValue);
    }

    void releaseGpuPtr(void *gpuPtr) override {
        releaseGpuPtrResult.called++;
        Wddm::releaseGpuPtr(gpuPtr);
    }

    GmmMemory *getGmmMemory() {
        return gmmMemory.get();
    }

    GmmPageTableMngr *getPageTableManager() const { return pageTableManager.get(); }
    bool peekIsPageTableManagerInitialized() const { return pageTableManagerInitialized; }

    MakeResidentCall makeResidentResult;
    CallResult makeNonResidentResult;
    CallResult mapGpuVirtualAddressResult;
    CallResult freeGpuVirtualAddresResult;
    CallResult createAllocationResult;
    CallResult destroyAllocationResult;
    CallResult destroyContextResult;
    CallResult queryAdapterInfoResult;
    CallResult submitResult;
    CallResult waitOnGPUResult;
    CallResult configureDeviceAddressSpaceResult;
    CallResult createContextResult;
    CallResult lockResult;
    CallResult unlockResult;
    CallResult waitFromCpuResult;
    CallResult releaseGpuPtrResult;
    NTSTATUS createAllocationStatus;
    bool callBaseDestroyAllocations = true;
    bool failOpenSharedHandle = false;
};

class WddmFixture {
  protected:
    Wddm *wddm;
    WddmMock *mockWddm = nullptr;
    OsLibrary *mockGdiDll;
    decltype(&MockSetSizes) setSizesFunction;
    decltype(&GetMockSizes) getSizesFunction;
    decltype(&GetMockLastDestroyedResHandle) getMockLastDestroyedResHandleFcn;
    decltype(&SetMockLastDestroyedResHandle) setMockLastDestroyedResHandleFcn;
    decltype(&GetMockCreateDeviceParams) getMockCreateDeviceParamsFcn;
    decltype(&SetMockCreateDeviceParams) setMockCreateDeviceParamsFcn;
    decltype(&getMockAllocation) getMockAllocationFcn;
    decltype(&getAdapterInfoAddress) getAdapterInfoAddressFcn;

  public:
    virtual void SetUp() {
        const HardwareInfo hwInfo = *platformDevices[0];
        mockGdiDll = setAdapterInfo(reinterpret_cast<const void *>(hwInfo.pPlatform), reinterpret_cast<const void *>(hwInfo.pSysInfo));

        setSizesFunction = reinterpret_cast<decltype(&MockSetSizes)>(mockGdiDll->getProcAddress("MockSetSizes"));
        getSizesFunction = reinterpret_cast<decltype(&GetMockSizes)>(mockGdiDll->getProcAddress("GetMockSizes"));
        getMockLastDestroyedResHandleFcn =
            reinterpret_cast<decltype(&GetMockLastDestroyedResHandle)>(mockGdiDll->getProcAddress("GetMockLastDestroyedResHandle"));
        setMockLastDestroyedResHandleFcn =
            reinterpret_cast<decltype(&SetMockLastDestroyedResHandle)>(mockGdiDll->getProcAddress("SetMockLastDestroyedResHandle"));
        getMockCreateDeviceParamsFcn =
            reinterpret_cast<decltype(&GetMockCreateDeviceParams)>(mockGdiDll->getProcAddress("GetMockCreateDeviceParams"));
        setMockCreateDeviceParamsFcn =
            reinterpret_cast<decltype(&SetMockCreateDeviceParams)>(mockGdiDll->getProcAddress("SetMockCreateDeviceParams"));
        getMockAllocationFcn = reinterpret_cast<decltype(&getMockAllocation)>(mockGdiDll->getProcAddress("getMockAllocation"));
        getAdapterInfoAddressFcn = reinterpret_cast<decltype(&getAdapterInfoAddress)>(mockGdiDll->getProcAddress("getAdapterInfoAddress"));
        wddm = Wddm::createWddm();
        mockWddm = static_cast<WddmMock *>(wddm);
        wddm->registryReader.reset(new RegistryReaderMock());
        setMockLastDestroyedResHandleFcn((D3DKMT_HANDLE)0);
    }

    virtual void SetUp(Gdi *gdi) {
        mockGdiDll = nullptr;
        wddm = Wddm::createWddm(gdi);
        mockWddm = static_cast<WddmMock *>(wddm);
        wddm->registryReader.reset(new RegistryReaderMock());
    }

    virtual void TearDown() {
        if (wddm != nullptr)
            delete wddm;
        if (mockGdiDll != nullptr)
            delete mockGdiDll;
    }
};

class WddmFixtureMock {
  protected:
    WddmMock *wddm;
    OsLibrary *mockGdiDll;

  public:
    virtual void SetUp() {
        const HardwareInfo hwInfo = *platformDevices[0];
        mockGdiDll = setAdapterInfo(reinterpret_cast<const void *>(hwInfo.pPlatform), reinterpret_cast<const void *>(hwInfo.pSysInfo));
        wddm = new WddmMock();
    }

    virtual void TearDown() {
        delete mockGdiDll;
    }
};

class WddmGmmFixture : public GmmFixture, public WddmFixture {
  public:
    virtual void SetUp() {
        GmmFixture::SetUp();
        WddmFixture::SetUp();
    }

    virtual void TearDown() {
        WddmFixture::TearDown();
        GmmFixture::TearDown();
    }
};

class WddmInstrumentationGmmFixture : public WddmGmmFixture {
  public:
    virtual void SetUp() {
        MockGmmMemory::MockGmmMemoryFlag = MockGmmMemory::MockType::MockInstrumentation;
        WddmGmmFixture::SetUp();
    }
    virtual void TearDown() {
        WddmGmmFixture::TearDown();
        MockGmmMemory::MockGmmMemoryFlag = MockGmmMemory::MockType::MockDummy;
    }
};

typedef ::testing::Test WddmDummyFixture;

#endif
