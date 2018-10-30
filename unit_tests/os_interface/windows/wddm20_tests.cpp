/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/windows/wddm_fixture.h"

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/os_library.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"

#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"

#include "gtest/gtest.h"
#include "runtime/os_interface/os_time.h"

#include <memory>
#include <functional>

using namespace OCLRT;

namespace GmmHelperFunctions {
Gmm *getGmm(void *ptr, size_t size) {
    size_t alignedSize = alignSizeWholePage(ptr, size);
    void *alignedPtr = alignUp(ptr, 4096);

    Gmm *gmm = new Gmm(alignedPtr, alignedSize, false);
    EXPECT_NE(gmm->gmmResourceInfo.get(), nullptr);
    return gmm;
}
} // namespace GmmHelperFunctions

using Wddm20Tests = WddmTest;
using Wddm20WithMockGdiDllTestsWithoutWddmInit = WddmTestWithMockGdiDll;
using Wddm20InstrumentationTest = WddmInstrumentationTest;

struct Wddm20WithMockGdiDllTests : public Wddm20WithMockGdiDllTestsWithoutWddmInit {
    using Wddm20WithMockGdiDllTestsWithoutWddmInit::TearDown;
    void SetUp() override {
        Wddm20WithMockGdiDllTestsWithoutWddmInit::SetUp();
        init();
    }
};

TEST_F(Wddm20Tests, givenMinWindowsAddressWhenWddmIsInitializedThenWddmUseThisAddress) {
    uintptr_t expectedAddress = 0x200000;
    EXPECT_EQ(expectedAddress, OCLRT::windowsMinAddress);
    EXPECT_EQ(expectedAddress, wddm->getWddmMinAddress());
}

TEST_F(Wddm20Tests, doubleCreation) {
    EXPECT_EQ(1u, wddm->createContextResult.called);
    auto error = wddm->init();
    EXPECT_EQ(1u, wddm->createContextResult.called);

    EXPECT_TRUE(error);
    EXPECT_TRUE(wddm->isInitialized());
}

TEST_F(Wddm20Tests, givenNullPageTableManagerAndRenderCompressedResourceWhenMappingGpuVaThenDontUpdateAuxTable) {
    wddm->resetPageTableManager(nullptr);

    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    void *fakePtr = reinterpret_cast<void *>(0x100);
    WddmAllocation allocation(fakePtr, 0x2100, nullptr, MemoryPool::MemoryNull, 1u);
    allocation.gmm = gmm.get();
    allocation.handle = ALLOCATION_HANDLE;

    EXPECT_TRUE(wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.is32BitAllocation, false, false));
}

TEST(Wddm20EnumAdaptersTest, expectTrue) {
    HardwareInfo outHwInfo;

    const HardwareInfo hwInfo = *platformDevices[0];
    OsLibrary *mockGdiDll = setAdapterInfo(hwInfo.pPlatform, hwInfo.pSysInfo);

    std::unique_ptr<Wddm> wddm(Wddm::createWddm());
    bool success = wddm->enumAdapters(outHwInfo);

    EXPECT_TRUE(success);

    const HardwareInfo *hwinfo = *platformDevices;

    ASSERT_NE(nullptr, outHwInfo.pPlatform);
    EXPECT_EQ(outHwInfo.pPlatform->eDisplayCoreFamily, hwinfo->pPlatform->eDisplayCoreFamily);
    delete mockGdiDll;

    delete outHwInfo.pPlatform;
    delete outHwInfo.pSkuTable;
    delete outHwInfo.pSysInfo;
    delete outHwInfo.pWaTable;
}

TEST(Wddm20EnumAdaptersTest, givenEmptyHardwareInfoWhenEnumAdapterIsCalledThenCapabilityTableIsSet) {
    HardwareInfo outHwInfo = {};

    auto hwInfo = *platformDevices[0];
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(hwInfo.pPlatform, hwInfo.pSysInfo));

    std::unique_ptr<Wddm> wddm(Wddm::createWddm());
    bool success = wddm->enumAdapters(outHwInfo);
    EXPECT_TRUE(success);

    const HardwareInfo *hwinfo = *platformDevices;

    ASSERT_NE(nullptr, outHwInfo.pPlatform);
    EXPECT_EQ(outHwInfo.pPlatform->eDisplayCoreFamily, hwinfo->pPlatform->eDisplayCoreFamily);

    EXPECT_EQ(outHwInfo.capabilityTable.defaultProfilingTimerResolution, hwInfo.capabilityTable.defaultProfilingTimerResolution);
    EXPECT_EQ(outHwInfo.capabilityTable.clVersionSupport, hwInfo.capabilityTable.clVersionSupport);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify, hwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, hwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep, hwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds, hwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);

    delete outHwInfo.pPlatform;
    delete outHwInfo.pSkuTable;
    delete outHwInfo.pSysInfo;
    delete outHwInfo.pWaTable;
}

TEST(Wddm20EnumAdaptersTest, givenUnknownPlatformWhenEnumAdapterIsCalledThenFalseIsReturnedAndOutputIsEmpty) {
    HardwareInfo outHwInfo;

    memset(&outHwInfo, 0, sizeof(outHwInfo));

    HardwareInfo hwInfo = *platformDevices[0];
    auto bkp = hwInfo.pPlatform->eProductFamily;
    PLATFORM platform = *(hwInfo.pPlatform);
    platform.eProductFamily = IGFX_UNKNOWN;

    std::unique_ptr<OsLibrary, std::function<void(OsLibrary *)>> mockGdiDll(
        setAdapterInfo(&platform, hwInfo.pSysInfo),
        [&](OsLibrary *ptr) {
            platform.eProductFamily = bkp;
            typedef void(__stdcall * pfSetAdapterInfo)(const void *, const void *);
            pfSetAdapterInfo fSetAdpaterInfo = reinterpret_cast<pfSetAdapterInfo>(ptr->getProcAddress("MockSetAdapterInfo"));

            fSetAdpaterInfo(&platform, hwInfo.pSysInfo);
            delete ptr;
        });
    std::unique_ptr<Wddm> wddm(Wddm::createWddm());
    auto ret = wddm->enumAdapters(outHwInfo);
    EXPECT_FALSE(ret);
    EXPECT_EQ(nullptr, outHwInfo.pPlatform);
    EXPECT_EQ(nullptr, outHwInfo.pSkuTable);
    EXPECT_EQ(nullptr, outHwInfo.pSysInfo);
    EXPECT_EQ(nullptr, outHwInfo.pWaTable);
}

TEST_F(Wddm20Tests, whenInitializeWddmThenContextIsCreated) {
    auto context = osContextWin->getContext();
    EXPECT_TRUE(context != static_cast<D3DKMT_HANDLE>(0));
}

TEST_F(Wddm20Tests, allocation) {
    OsAgnosticMemoryManager mm(false, false, executionEnvironment);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr, MemoryPool::MemoryNull, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    auto error = wddm->destroyAllocation(&allocation, osContextWin);
    EXPECT_TRUE(error);

    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20WithMockGdiDllTests, givenAllocationSmallerUnderlyingThanAlignedSizeWhenCreatedThenWddmUseAligned) {
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    size_t underlyingSize = 0x2100;
    size_t alignedSize = 0x3000;

    size_t underlyingPages = underlyingSize / MemoryConstants::pageSize;
    size_t alignedPages = alignedSize / MemoryConstants::pageSize;

    WddmAllocation allocation(ptr, 0x2100, nullptr, MemoryPool::MemoryNull, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_NE(0, allocation.handle);

    bool ret = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.is32BitAllocation, false, false);
    EXPECT_TRUE(ret);

    EXPECT_EQ(alignedPages, getLastCallMapGpuVaArgFcn()->SizeInPages);
    EXPECT_NE(underlyingPages, getLastCallMapGpuVaArgFcn()->SizeInPages);

    ret = wddm->destroyAllocation(&allocation, osContextWin);
    EXPECT_TRUE(ret);

    delete gmm;
}

TEST_F(Wddm20WithMockGdiDllTests, givenWddmAllocationWhenMappingGpuVaThenUseGmmSize) {
    void *fakePtr = reinterpret_cast<void *>(0x123);
    WddmAllocation allocation(fakePtr, 100, nullptr, MemoryPool::MemoryNull, 1u);
    std::unique_ptr<Gmm> gmm(GmmHelperFunctions::getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize()));

    allocation.gmm = gmm.get();
    auto status = wddm->createAllocation(&allocation);

    auto mockResourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockResourceInfo->overrideReturnedSize(allocation.getAlignedSize() + (2 * MemoryConstants::pageSize));

    wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.is32BitAllocation, false, false);

    uint64_t expectedSizeInPages = static_cast<uint64_t>(mockResourceInfo->getSizeAllocation() / MemoryConstants::pageSize);
    EXPECT_EQ(expectedSizeInPages, getLastCallMapGpuVaArgFcn()->SizeInPages);
}

TEST_F(Wddm20Tests, createAllocation32bit) {
    uint64_t heap32baseAddress = 0x40000;
    uint64_t heap32Size = 0x40000;
    wddm->setHeap32(heap32baseAddress, heap32Size);

    void *alignedPtr = (void *)0x12000;
    size_t alignedSize = 0x2000;

    WddmAllocation allocation(alignedPtr, alignedSize, nullptr, MemoryPool::MemoryNull, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    allocation.is32BitAllocation = true; // mark 32 bit allocation

    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    bool ret = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.is32BitAllocation, false, false);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, wddm->mapGpuVirtualAddressResult.called);

    EXPECT_LE(heap32baseAddress, allocation.gpuPtr);
    EXPECT_GT(heap32baseAddress + heap32Size, allocation.gpuPtr);

    auto success = wddm->destroyAllocation(&allocation, osContextWin);
    EXPECT_TRUE(success);

    delete gmm;
}

TEST_F(Wddm20Tests, givenGraphicsAllocationWhenItIsMappedInHeap1ThenItHasGpuAddressWithingHeap1Limits) {
    void *alignedPtr = (void *)0x12000;
    size_t alignedSize = 0x2000;
    WddmAllocation allocation(alignedPtr, alignedSize, nullptr, MemoryPool::MemoryNull, 1u);

    allocation.handle = ALLOCATION_HANDLE;
    allocation.gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    bool ret = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), false, false, true);
    EXPECT_TRUE(ret);

    auto cannonizedHeapBase = GmmHelper::canonize(this->wddm->getGfxPartition().Heap32[1].Base);
    auto cannonizedHeapEnd = GmmHelper::canonize(this->wddm->getGfxPartition().Heap32[1].Limit);

    EXPECT_GE(allocation.gpuPtr, cannonizedHeapBase);
    EXPECT_LE(allocation.gpuPtr, cannonizedHeapEnd);
    delete allocation.gmm;
}

TEST_F(Wddm20WithMockGdiDllTests, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
    OsHandleStorage storage;
    OsHandle osHandle1 = {0};
    OsHandle osHandle2 = {0};
    OsHandle osHandle3 = {0};

    osHandle1.handle = ALLOCATION_HANDLE;
    osHandle2.handle = ALLOCATION_HANDLE;
    osHandle3.handle = ALLOCATION_HANDLE;

    storage.fragmentStorageData[0].osHandleStorage = &osHandle1;
    storage.fragmentStorageData[0].freeTheFragment = true;

    storage.fragmentStorageData[1].osHandleStorage = &osHandle2;
    storage.fragmentStorageData[1].freeTheFragment = false;

    storage.fragmentStorageData[2].osHandleStorage = &osHandle3;
    storage.fragmentStorageData[2].freeTheFragment = true;

    D3DKMT_HANDLE handles[3] = {ALLOCATION_HANDLE, ALLOCATION_HANDLE, ALLOCATION_HANDLE};
    bool retVal = wddm->destroyAllocations(handles, 3, 0);
    EXPECT_TRUE(retVal);

    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);
}

TEST_F(Wddm20Tests, mapAndFreeGpuVa) {
    OsAgnosticMemoryManager mm(false, false, executionEnvironment);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr, MemoryPool::MemoryNull, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    auto error = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), false, false, false);
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.gpuPtr != 0);

    error = wddm->freeGpuVirtualAddres(allocation.gpuPtr, allocation.getUnderlyingBufferSize());
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.gpuPtr == 0);

    error = wddm->destroyAllocation(&allocation, osContextWin);
    EXPECT_TRUE(error);
    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20Tests, givenNullAllocationWhenCreateThenAllocateAndMap) {
    OsAgnosticMemoryManager mm(false, false, executionEnvironment);

    WddmAllocation allocation(nullptr, 100, nullptr, MemoryPool::MemoryNull, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);

    bool ret = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.is32BitAllocation, false, false);
    EXPECT_TRUE(ret);

    EXPECT_NE(0u, allocation.gpuPtr);
    EXPECT_EQ(allocation.gpuPtr, GmmHelper::canonize(allocation.gpuPtr));

    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20Tests, makeResidentNonResident) {
    OsAgnosticMemoryManager mm(false, false, executionEnvironment);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr, MemoryPool::MemoryNull, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    auto error = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), false, false, false);
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.gpuPtr != 0);

    error = wddm->makeResident(&allocation.handle, 1, false, nullptr);
    EXPECT_TRUE(error);

    uint64_t sizeToTrim;
    error = wddm->evict(&allocation.handle, 1, sizeToTrim);
    EXPECT_TRUE(error);

    auto monitoredFence = osContextWin->getResidencyController().getMonitoredFence();
    UINT64 fenceValue = 100;
    monitoredFence.cpuAddress = &fenceValue;
    monitoredFence.currentFenceValue = 101;

    error = wddm->destroyAllocation(&allocation, osContextWin);

    EXPECT_TRUE(error);
    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenGraphicsAllocationWithSharedPropertiesIsCreated) {
    void *pSysMem = (void *)0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    WddmMemoryManager mm(false, false, wddm, executionEnvironment);

    auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(ALLOCATION_HANDLE, false);
    auto wddmAllocation = (WddmAllocation *)graphicsAllocation;
    ASSERT_NE(nullptr, wddmAllocation);

    EXPECT_EQ(ALLOCATION_HANDLE, wddmAllocation->peekSharedHandle());
    EXPECT_EQ(RESOURCE_HANDLE, wddmAllocation->resourceHandle);
    EXPECT_NE(0u, wddmAllocation->handle);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmAllocation->handle);
    EXPECT_NE(0u, wddmAllocation->getGpuAddress());
    EXPECT_EQ(wddmAllocation->gpuPtr, wddmAllocation->getGpuAddress());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(nullptr, wddmAllocation->getAlignedCpuPtr());
    EXPECT_NE(nullptr, wddmAllocation->gmm);

    EXPECT_EQ(4096u, wddmAllocation->gmm->gmmResourceInfo->getSizeAllocation());

    mm.freeGraphicsMemory(graphicsAllocation);
    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    status = getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);

    EXPECT_EQ(0u, status);
    EXPECT_EQ(1u, destroyWithResourceHandleCalled);
}

TEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenMapGpuVaWithCpuPtrDepensOnBitness) {
    void *pSysMem = (void *)0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    WddmMemoryManager mm(false, false, wddm, executionEnvironment);

    auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(ALLOCATION_HANDLE, false);
    auto wddmAllocation = (WddmAllocation *)graphicsAllocation;
    ASSERT_NE(nullptr, wddmAllocation);

    if (is32bit) {
        EXPECT_NE(wddm->mapGpuVirtualAddressResult.cpuPtrPassed, nullptr);
    } else {
        EXPECT_EQ(wddm->mapGpuVirtualAddressResult.cpuPtrPassed, nullptr);
    }

    mm.freeGraphicsMemory(graphicsAllocation);
}

TEST_F(Wddm20Tests, givenWddmCreatedWhenInitedThenMinAddressValid) {
    uintptr_t expected = windowsMinAddress;
    uintptr_t actual = wddm->getWddmMinAddress();
    EXPECT_EQ(expected, actual);
}

HWTEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceOnInit) {
    SYSTEM_INFO sysInfo = {};
    WddmMock::getSystemInfo(&sysInfo);

    D3DKMT_HANDLE adapterHandle = ADAPTER_HANDLE;
    D3DKMT_HANDLE deviceHandle = DEVICE_HANDLE;
    const HardwareInfo hwInfo = *platformDevices[0];
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initGmm(&hwInfo);
    BOOLEAN FtrL3IACoherency = hwInfo.pSkuTable->ftrL3IACoherency ? 1 : 0;
    uintptr_t maxAddr = hwInfo.capabilityTable.gpuAddressSpace == MemoryConstants::max48BitAddress
                            ? reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress) + 1
                            : 0;
    EXPECT_CALL(*gmmMem, configureDeviceAddressSpace(adapterHandle,
                                                     deviceHandle,
                                                     wddm->gdi->escape.mFunc,
                                                     maxAddr,
                                                     FtrL3IACoherency))
        .Times(1)
        .WillRepeatedly(::testing::Return(true));
    wddm->init();

    EXPECT_TRUE(wddm->isInitialized());
}

TEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceNoAdapter) {
    wddm->adapter = static_cast<D3DKMT_HANDLE>(0);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initGmm(*platformDevices);
    EXPECT_CALL(*gmmMem,
                configureDeviceAddressSpace(static_cast<D3DKMT_HANDLE>(0), ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    auto ret = wddm->configureDeviceAddressSpace();

    EXPECT_FALSE(ret);
}

TEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceNoDevice) {
    wddm->device = static_cast<D3DKMT_HANDLE>(0);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initGmm(*platformDevices);
    EXPECT_CALL(*gmmMem,
                configureDeviceAddressSpace(::testing::_, static_cast<D3DKMT_HANDLE>(0), ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    auto ret = wddm->configureDeviceAddressSpace();

    EXPECT_FALSE(ret);
}

TEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceNoEscFunc) {
    wddm->gdi->escape = static_cast<PFND3DKMT_ESCAPE>(nullptr);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initGmm(*platformDevices);
    EXPECT_CALL(*gmmMem, configureDeviceAddressSpace(::testing::_, ::testing::_, static_cast<PFND3DKMT_ESCAPE>(nullptr), ::testing::_,
                                                     ::testing::_))
        .Times(0);
    auto ret = wddm->configureDeviceAddressSpace();

    EXPECT_FALSE(ret);
}

TEST_F(Wddm20Tests, getMaxApplicationAddress) {
    uint64_t maxAddr = wddm->getMaxApplicationAddress();
    if (is32bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    }
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenUseNoRingFlushesKmdModeDebugFlagToFalseWhenCreateContextIsCalledThenNoRingFlushesKmdModeIsSetToFalse) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseNoRingFlushesKmdMode.set(false);
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    auto privateData = (CREATECONTEXT_PVTDATA *)createContextParams->pPrivateDriverData;
    EXPECT_FALSE(!!privateData->NoRingFlushes);
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenUseNoRingFlushesKmdModeDebugFlagToTrueWhenCreateContextIsCalledThenNoRingFlushesKmdModeIsSetToTrue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseNoRingFlushesKmdMode.set(true);
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    auto privateData = (CREATECONTEXT_PVTDATA *)createContextParams->pPrivateDriverData;
    EXPECT_TRUE(!!privateData->NoRingFlushes);
}

TEST_F(Wddm20WithMockGdiDllTests, whenCreateContextIsCalledThenDisableHwQueues) {
    EXPECT_FALSE(wddm->wddmInterface->hwQueuesSupported());
    EXPECT_EQ(0u, getCreateContextDataFcn()->Flags.HwQueueSupported);
}

TEST_F(Wddm20Tests, whenCreateHwQueueIsCalledThenAlwaysReturnFalse) {
    EXPECT_FALSE(wddm->wddmInterface->createHwQueue(wddm->preemptionMode, *osContextWin));
}

TEST_F(Wddm20Tests, whenWddmIsInitializedThenGdiDoesntHaveHwQueueDDIs) {
    EXPECT_EQ(nullptr, wddm->gdi->createHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->destroyHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->submitCommandToHwQueue.mFunc);
}

TEST(DebugFlagTest, givenDebugManagerWhenGetForUseNoRingFlushesKmdModeIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(DebugManager.flags.UseNoRingFlushesKmdMode.get());
}

TEST_F(Wddm20Tests, makeResidentMultipleHandles) {
    OsAgnosticMemoryManager mm(false, false, executionEnvironment);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr, MemoryPool::MemoryNull, 1u);
    allocation.handle = ALLOCATION_HANDLE;

    D3DKMT_HANDLE handles[2] = {0};

    handles[0] = allocation.handle;
    handles[1] = allocation.handle;

    gdi->getMakeResidentArg().NumAllocations = 0;
    gdi->getMakeResidentArg().AllocationList = nullptr;

    bool error = wddm->makeResident(handles, 2, false, nullptr);
    EXPECT_TRUE(error);

    EXPECT_EQ(2u, gdi->getMakeResidentArg().NumAllocations);
    EXPECT_EQ(handles, gdi->getMakeResidentArg().AllocationList);

    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20Tests, makeResidentMultipleHandlesWithReturnBytesToTrim) {
    OsAgnosticMemoryManager mm(false, false, executionEnvironment);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr, MemoryPool::MemoryNull, 1u);
    allocation.handle = ALLOCATION_HANDLE;

    D3DKMT_HANDLE handles[2] = {0};

    handles[0] = allocation.handle;
    handles[1] = allocation.handle;

    gdi->getMakeResidentArg().NumAllocations = 0;
    gdi->getMakeResidentArg().AllocationList = nullptr;
    gdi->getMakeResidentArg().NumBytesToTrim = 30;

    uint64_t bytesToTrim = 0;
    bool success = wddm->makeResident(handles, 2, false, &bytesToTrim);
    EXPECT_TRUE(success);

    EXPECT_EQ(gdi->getMakeResidentArg().NumBytesToTrim, bytesToTrim);

    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20Tests, makeNonResidentCallsEvict) {
    D3DKMT_HANDLE handle = (D3DKMT_HANDLE)0x1234;

    gdi->getEvictArg().AllocationList = nullptr;
    gdi->getEvictArg().Flags.Value = 0;
    gdi->getEvictArg().hDevice = 0;
    gdi->getEvictArg().NumAllocations = 0;
    gdi->getEvictArg().NumBytesToTrim = 20;

    uint64_t sizeToTrim = 10;
    wddm->evict(&handle, 1, sizeToTrim);

    EXPECT_EQ(1u, gdi->getEvictArg().NumAllocations);
    EXPECT_EQ(&handle, gdi->getEvictArg().AllocationList);
    EXPECT_EQ(wddm->getDevice(), gdi->getEvictArg().hDevice);
    EXPECT_EQ(0u, gdi->getEvictArg().NumBytesToTrim);
}

TEST_F(Wddm20Tests, givenDestroyAllocationWhenItIsCalledThenAllocationIsPassedToDestroyAllocation) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, 1u);
    allocation.getResidencyData().updateCompletionData(10, osContext.get()->getContextId());
    allocation.handle = ALLOCATION_HANDLE;

    *osContextWin->getResidencyController().getMonitoredFence().cpuAddress = 10;

    D3DKMT_HANDLE handle = (D3DKMT_HANDLE)0x1234;

    gdi->getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi->getWaitFromCpuArg().Flags.Value = 0;
    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi->getWaitFromCpuArg().ObjectCount = 0;
    gdi->getWaitFromCpuArg().ObjectHandleArray = nullptr;

    gdi->getDestroyArg().AllocationCount = 0;
    gdi->getDestroyArg().Flags.Value = 0;
    gdi->getDestroyArg().hDevice = (D3DKMT_HANDLE)0;
    gdi->getDestroyArg().hResource = (D3DKMT_HANDLE)0;
    gdi->getDestroyArg().phAllocationList = nullptr;

    wddm->destroyAllocation(&allocation, osContextWin);

    EXPECT_EQ(wddm->getDevice(), gdi->getDestroyArg().hDevice);
    EXPECT_EQ(1u, gdi->getDestroyArg().AllocationCount);
    EXPECT_NE(nullptr, gdi->getDestroyArg().phAllocationList);
}

TEST_F(Wddm20Tests, WhenLastFenceLessEqualThanMonitoredThenWaitFromCpuIsNotCalled) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, 1u);
    allocation.getResidencyData().updateCompletionData(10, osContext.get()->getContextId());
    allocation.handle = ALLOCATION_HANDLE;

    *osContextWin->getResidencyController().getMonitoredFence().cpuAddress = 10;

    gdi->getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi->getWaitFromCpuArg().Flags.Value = 0;
    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi->getWaitFromCpuArg().ObjectCount = 0;
    gdi->getWaitFromCpuArg().ObjectHandleArray = nullptr;

    auto status = wddm->waitFromCpu(10, osContextWin->getResidencyController().getMonitoredFence());

    EXPECT_TRUE(status);

    EXPECT_EQ(nullptr, gdi->getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)0, gdi->getWaitFromCpuArg().hDevice);
    EXPECT_EQ(0u, gdi->getWaitFromCpuArg().ObjectCount);
    EXPECT_EQ(nullptr, gdi->getWaitFromCpuArg().ObjectHandleArray);
}

TEST_F(Wddm20Tests, WhenLastFenceGreaterThanMonitoredThenWaitFromCpuIsCalled) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, 1u);
    allocation.getResidencyData().updateCompletionData(10, osContext.get()->getContextId());
    allocation.handle = ALLOCATION_HANDLE;

    *osContextWin->getResidencyController().getMonitoredFence().cpuAddress = 10;

    gdi->getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi->getWaitFromCpuArg().Flags.Value = 0;
    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi->getWaitFromCpuArg().ObjectCount = 0;
    gdi->getWaitFromCpuArg().ObjectHandleArray = nullptr;

    auto status = wddm->waitFromCpu(20, osContextWin->getResidencyController().getMonitoredFence());

    EXPECT_TRUE(status);

    EXPECT_NE(nullptr, gdi->getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)wddm->getDevice(), gdi->getWaitFromCpuArg().hDevice);
    EXPECT_EQ(1u, gdi->getWaitFromCpuArg().ObjectCount);
    EXPECT_NE(nullptr, gdi->getWaitFromCpuArg().ObjectHandleArray);
}

TEST_F(Wddm20Tests, createMonitoredFenceIsInitializedWithFenceValueZeroAndCurrentFenceValueIsSetToOne) {
    gdi->createSynchronizationObject2 = gdi->createSynchronizationObject2Mock;

    gdi->getCreateSynchronizationObject2Arg().Info.MonitoredFence.InitialFenceValue = 300;

    wddm->wddmInterface->createMonitoredFence(osContextWin->getResidencyController());

    EXPECT_EQ(0u, gdi->getCreateSynchronizationObject2Arg().Info.MonitoredFence.InitialFenceValue);
    EXPECT_EQ(1u, osContextWin->getResidencyController().getMonitoredFence().currentFenceValue);
}

NTSTATUS APIENTRY queryResourceInfoMock(D3DKMT_QUERYRESOURCEINFO *pData) {
    pData->NumAllocations = 0;
    return 0;
}

TEST_F(Wddm20Tests, givenOpenSharedHandleWhenZeroAllocationsThenReturnNull) {
    D3DKMT_HANDLE handle = 0;
    WddmAllocation *alloc = nullptr;

    gdi->queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(queryResourceInfoMock);
    auto ret = wddm->openSharedHandle(handle, alloc);

    EXPECT_EQ(false, ret);
}

TEST_F(Wddm20Tests, whenCreateAllocation64kFailsThenReturnFalse) {
    struct FailingCreateAllocation {
        static NTSTATUS APIENTRY mockCreateAllocation(D3DKMT_CREATEALLOCATION *param) {
            return STATUS_GRAPHICS_NO_VIDEO_MEMORY;
        };
    };
    gdi->createAllocation = FailingCreateAllocation::mockCreateAllocation;

    void *fakePtr = reinterpret_cast<void *>(0x123);
    auto gmm = std::make_unique<Gmm>(fakePtr, 100, false);
    WddmAllocation allocation(fakePtr, 100, nullptr, MemoryPool::MemoryNull, 1u);
    allocation.gmm = gmm.get();

    EXPECT_FALSE(wddm->createAllocation64k(&allocation));
}

TEST_F(Wddm20Tests, givenReadOnlyMemoryWhenCreateAllocationFailsWithNoVideoMemoryThenCorrectStatusIsReturned) {
    class MockCreateAllocation {
      public:
        static NTSTATUS APIENTRY mockCreateAllocation(D3DKMT_CREATEALLOCATION *param) {
            return STATUS_GRAPHICS_NO_VIDEO_MEMORY;
        };
    };
    gdi->createAllocation = MockCreateAllocation::mockCreateAllocation;

    OsHandleStorage handleStorage;
    OsHandle handle = {0};
    ResidencyData residency;

    handleStorage.fragmentCount = 1;
    handleStorage.fragmentStorageData[0].cpuPtr = (void *)0x1000;
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;
    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle;
    handleStorage.fragmentStorageData[0].residency = &residency;
    handleStorage.fragmentStorageData[0].osHandleStorage->gmm = GmmHelperFunctions::getGmm(nullptr, 0);

    NTSTATUS result = wddm->createAllocationsAndMapGpuVa(handleStorage);

    EXPECT_EQ(STATUS_GRAPHICS_NO_VIDEO_MEMORY, result);

    delete handleStorage.fragmentStorageData[0].osHandleStorage->gmm;
}

TEST_F(Wddm20Tests, whenContextIsInitializedThenApplyAdditionalContextFlagsIsCalled) {
    auto result = wddm->init();
    EXPECT_TRUE(result);
    EXPECT_EQ(1u, wddm->applyAdditionalContextFlagsResult.called);
}

TEST_F(Wddm20Tests, givenTrimCallbackRegistrationIsDisabledInDebugVariableWhenRegisteringCallbackThenReturnNullptr) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.DoNotRegisterTrimCallback.set(true);
    WddmResidencyController residencyController{*wddm, 0u};
    EXPECT_EQ(nullptr, wddm->registerTrimCallback([](D3DKMT_TRIMNOTIFICATION *) {}, residencyController));
}

TEST_F(Wddm20Tests, givenSuccessWhenRegisteringTrimCallbackThenReturnTrimCallbackHandle) {
    WddmResidencyController residencyController{*wddm, 0u};
    auto trimCallbackHandle = wddm->registerTrimCallback([](D3DKMT_TRIMNOTIFICATION *) {}, residencyController);
    EXPECT_NE(nullptr, trimCallbackHandle);
}

TEST_F(Wddm20Tests, givenCorrectArgumentsWhenUnregisteringTrimCallbackThenPassArgumentsToGdiCall) {
    PFND3DKMT_TRIMNOTIFICATIONCALLBACK callback = [](D3DKMT_TRIMNOTIFICATION *) {};
    auto trimCallbackHandle = reinterpret_cast<VOID *>(0x9876);

    wddm->unregisterTrimCallback(callback, trimCallbackHandle);
    EXPECT_EQ(callback, gdi->getUnregisterTrimNotificationArg().Callback);
    EXPECT_EQ(trimCallbackHandle, gdi->getUnregisterTrimNotificationArg().Handle);
}

TEST_F(Wddm20Tests, givenNullTrimCallbackHandleWhenUnregisteringTrimCallbackThenDoNotDoGdiCall) {
    PFND3DKMT_TRIMNOTIFICATIONCALLBACK callbackBefore = [](D3DKMT_TRIMNOTIFICATION *) {};
    auto trimCallbackHandleBefore = reinterpret_cast<VOID *>(0x9876);
    gdi->getUnregisterTrimNotificationArg().Callback = callbackBefore;
    gdi->getUnregisterTrimNotificationArg().Handle = trimCallbackHandleBefore;

    wddm->unregisterTrimCallback([](D3DKMT_TRIMNOTIFICATION *) {}, nullptr);
    EXPECT_EQ(callbackBefore, gdi->getUnregisterTrimNotificationArg().Callback);
    EXPECT_EQ(trimCallbackHandleBefore, gdi->getUnregisterTrimNotificationArg().Handle);
}
