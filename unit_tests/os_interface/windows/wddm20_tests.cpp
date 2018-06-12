/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "unit_tests/os_interface/windows/wddm_fixture.h"

#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/os_library.h"
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
    Gmm *gmm = new Gmm;

    gmm->resourceParams.Type = RESOURCE_BUFFER;
    gmm->resourceParams.Format = GMM_FORMAT_GENERIC_8BIT;
    gmm->resourceParams.BaseWidth = (uint32_t)alignedSize;
    gmm->resourceParams.BaseHeight = 1;
    gmm->resourceParams.Depth = 1;
    gmm->resourceParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER;

    gmm->resourceParams.pExistingSysMem = reinterpret_cast<GMM_VOIDPTR64>(alignedPtr);
    gmm->resourceParams.ExistingSysMemSize = alignedSize;
    gmm->resourceParams.BaseAlignment = 0;

    gmm->resourceParams.Flags.Info.ExistingSysMem = 1;
    gmm->resourceParams.Flags.Info.Linear = 1;
    gmm->resourceParams.Flags.Info.Cacheable = 1;
    gmm->resourceParams.Flags.Gpu.Texture = 1;

    gmm->create();

    EXPECT_NE(gmm->gmmResourceInfo.get(), nullptr);

    return gmm;
}
} // namespace GmmHelperFunctions

using Wddm20Tests = WddmTest;
using Wddm20WithMockGdiDllTests = WddmTestWithMockGdiDll;
using Wddm20InstrumentationTest = WddmInstrumentationTest;

HWTEST_F(Wddm20Tests, givenMinWindowsAddressWhenWddmIsInitializedThenWddmUseThisAddress) {
    uintptr_t expectedAddress = 0x200000;
    EXPECT_EQ(expectedAddress, OCLRT::windowsMinAddress);

    bool status = wddm->init<FamilyType>();
    EXPECT_TRUE(status);
    EXPECT_TRUE(wddm->isInitialized());
    EXPECT_EQ(expectedAddress, wddm->getWddmMinAddress());
}

HWTEST_F(Wddm20Tests, creation) {
    bool error = wddm->init<FamilyType>();
    EXPECT_TRUE(error);
    EXPECT_TRUE(wddm->isInitialized());
}

HWTEST_F(Wddm20Tests, doubleCreation) {
    bool error = wddm->init<FamilyType>();
    EXPECT_EQ(1u, wddm->createContextResult.called);
    error |= wddm->init<FamilyType>();
    EXPECT_EQ(1u, wddm->createContextResult.called);

    EXPECT_TRUE(error);
    EXPECT_TRUE(wddm->isInitialized());
}

TEST_F(Wddm20Tests, givenNullPageTableManagerWhenUpdateAuxTableCalledThenReturnFalse) {
    wddm->resetPageTableManager(nullptr);
    EXPECT_EQ(nullptr, wddm->getPageTableManager());

    auto gmm = std::unique_ptr<Gmm>(Gmm::create(nullptr, 1, false));
    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(wddm->updateAuxTable(1234u, gmm.get(), true));
}

TEST(Wddm20EnumAdaptersTest, expectTrue) {
    HardwareInfo outHwInfo;

    const HardwareInfo hwInfo = *platformDevices[0];
    OsLibrary *mockGdiDll = setAdapterInfo(hwInfo.pPlatform, hwInfo.pSysInfo);

    bool success = Wddm::enumAdapters(0, outHwInfo);
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

    bool success = Wddm::enumAdapters(0, outHwInfo);
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
    bool ret = Wddm::enumAdapters(0, outHwInfo);
    EXPECT_FALSE(ret);
    EXPECT_EQ(nullptr, outHwInfo.pPlatform);
    EXPECT_EQ(nullptr, outHwInfo.pSkuTable);
    EXPECT_EQ(nullptr, outHwInfo.pSysInfo);
    EXPECT_EQ(nullptr, outHwInfo.pWaTable);
}

TEST(Wddm20EnumAdaptersTest, devIdExpectFalse) {
    HardwareInfo tempHwInfos;

    bool success = Wddm::enumAdapters(1, tempHwInfos);
    EXPECT_FALSE(success);
}

HWTEST_F(Wddm20Tests, context) {
    EXPECT_TRUE(wddm->getOsDeviceContext() == static_cast<D3DKMT_HANDLE>(0));
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    EXPECT_TRUE(wddm->createContext());

    auto context = wddm->getOsDeviceContext();
    EXPECT_TRUE(context != static_cast<D3DKMT_HANDLE>(0));

    EXPECT_TRUE(wddm->destroyContext(context));
}

HWTEST_F(Wddm20Tests, allocation) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());
    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    auto error = wddm->destroyAllocation(&allocation);
    EXPECT_TRUE(error);

    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

HWTEST_F(Wddm20WithMockGdiDllTests, givenAllocationSmallerUnderlyingThanAlignedSizeWhenCreatedThenWddmUseAligned) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    size_t underlyingSize = 0x2100;
    size_t alignedSize = 0x3000;

    size_t underlyingPages = underlyingSize / MemoryConstants::pageSize;
    size_t alignedPages = alignedSize / MemoryConstants::pageSize;

    WddmAllocation allocation(ptr, 0x2100, ptr, 0x3000, nullptr);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_NE(0, allocation.handle);

    bool ret = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.getAlignedSize(), allocation.is32BitAllocation, false, false);
    EXPECT_TRUE(ret);

    EXPECT_EQ(alignedPages, getLastCallMapGpuVaArgFcn()->SizeInPages);
    EXPECT_NE(underlyingPages, getLastCallMapGpuVaArgFcn()->SizeInPages);

    ret = wddm->destroyAllocation(&allocation);
    EXPECT_TRUE(ret);

    delete gmm;
}

HWTEST_F(Wddm20Tests, createAllocation32bit) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    uint64_t heap32baseAddress = 0x40000;
    uint64_t heap32Size = 0x40000;
    wddm->setHeap32(heap32baseAddress, heap32Size);

    void *alignedPtr = (void *)0x12000;
    size_t alignedSize = 0x2000;

    WddmAllocation allocation(alignedPtr, alignedSize, nullptr);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    allocation.is32BitAllocation = true; // mark 32 bit allocation

    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    bool ret = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.getAlignedSize(), allocation.is32BitAllocation, false, false);
    EXPECT_TRUE(ret);

    EXPECT_EQ(1u, wddm->mapGpuVirtualAddressResult.called);

    EXPECT_LE(heap32baseAddress, allocation.gpuPtr);
    EXPECT_GT(heap32baseAddress + heap32Size, allocation.gpuPtr);

    auto success = wddm->destroyAllocation(&allocation);
    EXPECT_TRUE(success);

    delete gmm;
}

HWTEST_F(Wddm20Tests, givenGraphicsAllocationWhenItIsMappedInHeap1ThenItHasGpuAddressWithingHeap1Limits) {
    wddm->init<FamilyType>();
    void *alignedPtr = (void *)0x12000;
    size_t alignedSize = 0x2000;
    WddmAllocation allocation(alignedPtr, alignedSize, nullptr);

    allocation.handle = ALLOCATION_HANDLE;
    allocation.gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    bool ret = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.getAlignedSize(), false, false, true);
    EXPECT_TRUE(ret);

    auto cannonizedHeapBase = Gmm::canonize(this->wddm->getGfxPartition().Heap32[1].Base);
    auto cannonizedHeapEnd = Gmm::canonize(this->wddm->getGfxPartition().Heap32[1].Limit);

    EXPECT_GE(allocation.gpuPtr, cannonizedHeapBase);
    EXPECT_LE(allocation.gpuPtr, cannonizedHeapEnd);
    delete allocation.gmm;
}

HWTEST_F(Wddm20WithMockGdiDllTests, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
    EXPECT_TRUE(wddm->init<FamilyType>());
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
    bool retVal = wddm->destroyAllocations(handles, 3, 0, 0);
    EXPECT_TRUE(retVal);

    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);
}

HWTEST_F(Wddm20Tests, mapAndFreeGpuVa) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    auto error = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.getUnderlyingBufferSize(), false, false, false);
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.gpuPtr != 0);

    error = wddm->freeGpuVirtualAddres(allocation.gpuPtr, allocation.getUnderlyingBufferSize());
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.gpuPtr == 0);

    error = wddm->destroyAllocation(&allocation);
    EXPECT_TRUE(error);
    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

HWTEST_F(Wddm20Tests, givenNullAllocationWhenCreateThenAllocateAndMap) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());
    OsAgnosticMemoryManager mm(false);

    WddmAllocation allocation(nullptr, 100, nullptr);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);

    bool ret = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.getAlignedSize(), allocation.is32BitAllocation, false, false);
    EXPECT_TRUE(ret);

    EXPECT_NE(0u, allocation.gpuPtr);
    EXPECT_EQ(allocation.gpuPtr, Gmm::canonize(allocation.gpuPtr));

    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

HWTEST_F(Wddm20Tests, makeResidentNonResident) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    auto error = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.getUnderlyingBufferSize(), false, false, false);
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.gpuPtr != 0);

    error = wddm->makeResident(&allocation.handle, 1, false, nullptr);
    EXPECT_TRUE(error);

    uint64_t sizeToTrim;
    error = wddm->evict(&allocation.handle, 1, sizeToTrim);
    EXPECT_TRUE(error);

    auto monitoredFence = wddm->getMonitoredFence();
    UINT64 fenceValue = 100;
    monitoredFence.cpuAddress = &fenceValue;
    monitoredFence.currentFenceValue = 101;

    error = wddm->destroyAllocation(&allocation);

    EXPECT_TRUE(error);
    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20Tests, GetCpuTime) {
    uint64_t time = 0;
    std::unique_ptr<OSTime> osTime(OSTime::create(nullptr).release());
    auto error = osTime->getCpuTime(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0, time);
}

TEST_F(Wddm20Tests, GivenNoOSInterfaceGetCpuGpuTimeReturnsError) {
    TimeStampData CPUGPUTime = {0};
    std::unique_ptr<OSTime> osTime(OSTime::create(nullptr).release());
    auto success = osTime->getCpuGpuTime(&CPUGPUTime);
    EXPECT_FALSE(success);
    EXPECT_EQ(0, CPUGPUTime.CPUTimeinNS);
    EXPECT_EQ(0, CPUGPUTime.GPUTimeStamp);
}

TEST_F(Wddm20Tests, GetCpuGpuTime) {
    TimeStampData CPUGPUTime01 = {0};
    TimeStampData CPUGPUTime02 = {0};
    std::unique_ptr<OSInterface> osInterface(new OSInterface());
    osInterface->get()->setWddm(wddm.get());
    std::unique_ptr<OSTime> osTime(OSTime::create(osInterface.get()).release());
    auto success = osTime->getCpuGpuTime(&CPUGPUTime01);
    EXPECT_TRUE(success);
    EXPECT_NE(0, CPUGPUTime01.CPUTimeinNS);
    EXPECT_NE(0, CPUGPUTime01.GPUTimeStamp);
    success = osTime->getCpuGpuTime(&CPUGPUTime02);
    EXPECT_TRUE(success);
    EXPECT_NE(0, CPUGPUTime02.CPUTimeinNS);
    EXPECT_NE(0, CPUGPUTime02.GPUTimeStamp);
    EXPECT_GT(CPUGPUTime02.GPUTimeStamp, CPUGPUTime01.GPUTimeStamp);
    EXPECT_GT(CPUGPUTime02.CPUTimeinNS, CPUGPUTime01.CPUTimeinNS);
}

HWTEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenGraphicsAllocationWithSharedPropertiesIsCreated) {
    void *pSysMem = (void *)0x1000;
    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    wddm->init<FamilyType>();
    WddmMemoryManager mm(false, wddm.release());

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

HWTEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenMapGpuVaWithCpuPtrDepensOnBitness) {
    void *pSysMem = (void *)0x1000;
    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    auto mockWddm = wddm.release();
    mockWddm->init<FamilyType>();
    WddmMemoryManager mm(false, mockWddm);

    auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(ALLOCATION_HANDLE, false);
    auto wddmAllocation = (WddmAllocation *)graphicsAllocation;
    ASSERT_NE(nullptr, wddmAllocation);

    if (is32bit) {
        EXPECT_NE(mockWddm->mapGpuVirtualAddressResult.cpuPtrPassed, nullptr);
    } else {
        EXPECT_EQ(mockWddm->mapGpuVirtualAddressResult.cpuPtrPassed, nullptr);
    }

    mm.freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(Wddm20Tests, givenWddmCreatedWhenNotInitedThenMinAddressZero) {
    uintptr_t expected = 0;
    uintptr_t actual = wddm->getWddmMinAddress();
    EXPECT_EQ(expected, actual);
}

HWTEST_F(Wddm20Tests, givenWddmCreatedWhenInitedThenMinAddressValid) {
    bool ret = wddm->init<FamilyType>();
    EXPECT_TRUE(ret);

    uintptr_t expected = windowsMinAddress;
    uintptr_t actual = wddm->getWddmMinAddress();
    EXPECT_EQ(expected, actual);
}

HWTEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceOnInit) {
    SYSTEM_INFO sysInfo = {};
    WddmMock::getSystemInfo(&sysInfo);
    uintptr_t maxAddr = reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress) + 1;

    D3DKMT_HANDLE adapterHandle = ADAPTER_HANDLE;
    D3DKMT_HANDLE deviceHandle = DEVICE_HANDLE;
    const HardwareInfo hwInfo = *platformDevices[0];
    BOOLEAN FtrL3IACoherency = hwInfo.pSkuTable->ftrL3IACoherency ? 1 : 0;

    EXPECT_CALL(*gmmMem, configureDeviceAddressSpace(adapterHandle,
                                                     deviceHandle,
                                                     wddm->gdi->escape.mFunc,
                                                     maxAddr,
                                                     0,
                                                     0,
                                                     FtrL3IACoherency,
                                                     0,
                                                     0))
        .Times(1)
        .WillRepeatedly(::testing::Return(true));
    wddm->init<FamilyType>();

    EXPECT_TRUE(wddm->isInitialized());
}

HWTEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceNoAdapter) {
    wddm->adapter = static_cast<D3DKMT_HANDLE>(0);
    EXPECT_CALL(*gmmMem, configureDeviceAddressSpace(static_cast<D3DKMT_HANDLE>(0),
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Return(false));
    auto ret = wddm->configureDeviceAddressSpace<FamilyType>();

    EXPECT_FALSE(ret);
}

HWTEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceNoDevice) {
    wddm->device = static_cast<D3DKMT_HANDLE>(0);
    EXPECT_CALL(*gmmMem, configureDeviceAddressSpace(::testing::_,
                                                     static_cast<D3DKMT_HANDLE>(0),
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Return(false));
    auto ret = wddm->configureDeviceAddressSpace<FamilyType>();

    EXPECT_FALSE(ret);
}

HWTEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceNoEscFunc) {
    wddm->gdi->escape = static_cast<PFND3DKMT_ESCAPE>(nullptr);
    EXPECT_CALL(*gmmMem, configureDeviceAddressSpace(::testing::_,
                                                     ::testing::_,
                                                     static_cast<PFND3DKMT_ESCAPE>(nullptr),
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_,
                                                     ::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Return(false));
    auto ret = wddm->configureDeviceAddressSpace<FamilyType>();

    EXPECT_FALSE(ret);
}

HWTEST_F(Wddm20Tests, getMaxApplicationAddress) {
    wddm->init<FamilyType>();
    EXPECT_TRUE(wddm->isInitialized());

    uint64_t maxAddr = wddm->getMaxApplicationAddress();
    if (is32bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    }
}

HWTEST_F(Wddm20Tests, dontCallCreateContextBeforeConfigureDeviceAddressSpace) {
    wddm->createContext();
    EXPECT_EQ(1u, wddm->createContextResult.called); // dont care about the result

    wddm->configureDeviceAddressSpace<FamilyType>();

    EXPECT_EQ(1u, wddm->configureDeviceAddressSpaceResult.called);
    EXPECT_FALSE(wddm->configureDeviceAddressSpaceResult.success);
}

HWTEST_F(Wddm20WithMockGdiDllTests, givenUseNoRingFlushesKmdModeDebugFlagToFalseWhenCreateContextIsCalledThenNoRingFlushesKmdModeIsSetToFalse) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseNoRingFlushesKmdMode.set(false);
    wddm->init<FamilyType>();
    auto createContextParams = this->getCreateContextDataFcn();
    auto privateData = (CREATECONTEXT_PVTDATA *)createContextParams->pPrivateDriverData;
    EXPECT_FALSE(!!privateData->NoRingFlushes);
}

HWTEST_F(Wddm20WithMockGdiDllTests, givenUseNoRingFlushesKmdModeDebugFlagToTrueWhenCreateContextIsCalledThenNoRingFlushesKmdModeIsSetToTrue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseNoRingFlushesKmdMode.set(true);
    wddm->init<FamilyType>();
    auto createContextParams = this->getCreateContextDataFcn();
    auto privateData = (CREATECONTEXT_PVTDATA *)createContextParams->pPrivateDriverData;
    EXPECT_TRUE(!!privateData->NoRingFlushes);
}

HWTEST_F(Wddm20WithMockGdiDllTests, whenCreateContextIsCalledThenDisableHwQueues) {
    wddm->init<FamilyType>();
    EXPECT_FALSE(wddm->hwQueuesSupported());
    EXPECT_EQ(0u, getCreateContextDataFcn()->Flags.HwQueueSupported);
}

TEST_F(Wddm20Tests, whenCreateHwQueueIsCalledThenAlwaysReturnFalse) {
    EXPECT_FALSE(wddm->createHwQueue());
}

HWTEST_F(Wddm20Tests, whenInitCalledThenDontCallToCreateHwQueue) {
    wddm->init<FamilyType>();
    EXPECT_EQ(0u, wddm->createHwQueueResult.called);
}

HWTEST_F(Wddm20Tests, whenWddmIsInitializedThenGdiDoesntHaveHwQueueDDIs) {
    wddm->init<FamilyType>();
    EXPECT_EQ(nullptr, wddm->gdi->createHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->destroyHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->submitCommandToHwQueue.mFunc);
}

HWTEST_F(Wddm20Tests, givenDebugManagerWhenGetForUseNoRingFlushesKmdModeIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(DebugManager.flags.UseNoRingFlushesKmdMode.get());
}

HWTEST_F(Wddm20Tests, makeResidentMultipleHandles) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
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

HWTEST_F(Wddm20Tests, makeResidentMultipleHandlesWithReturnBytesToTrim) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
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

HWTEST_F(Wddm20Tests, makeNonResidentCallsEvict) {
    wddm->init<FamilyType>();

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

HWTEST_F(Wddm20Tests, destroyAllocationWithLastFenceValueGreaterThanCurrentValueCallsWaitFromCpu) {
    wddm->init<FamilyType>();

    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.getResidencyData().lastFence = 20;
    allocation.handle = ALLOCATION_HANDLE;

    *wddm->getMonitoredFence().cpuAddress = 10;

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

    wddm->destroyAllocation(&allocation);

    EXPECT_NE(nullptr, gdi->getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ(wddm->getDevice(), gdi->getWaitFromCpuArg().hDevice);
    EXPECT_EQ(1u, gdi->getWaitFromCpuArg().ObjectCount);
    EXPECT_EQ(&wddm->getMonitoredFence().fenceHandle, gdi->getWaitFromCpuArg().ObjectHandleArray);

    EXPECT_EQ(wddm->getDevice(), gdi->getDestroyArg().hDevice);
    EXPECT_EQ(1u, gdi->getDestroyArg().AllocationCount);
    EXPECT_NE(nullptr, gdi->getDestroyArg().phAllocationList);
}

HWTEST_F(Wddm20Tests, destroyAllocationWithLastFenceValueLessEqualToCurrentValueDoesNotCallWaitFromCpu) {
    wddm->init<FamilyType>();

    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.getResidencyData().lastFence = 10;
    allocation.handle = ALLOCATION_HANDLE;

    *wddm->getMonitoredFence().cpuAddress = 10;

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

    wddm->destroyAllocation(&allocation);

    EXPECT_EQ(nullptr, gdi->getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)0, gdi->getWaitFromCpuArg().hDevice);
    EXPECT_EQ(0u, gdi->getWaitFromCpuArg().ObjectCount);
    EXPECT_EQ(nullptr, gdi->getWaitFromCpuArg().ObjectHandleArray);

    EXPECT_EQ(wddm->getDevice(), gdi->getDestroyArg().hDevice);
    EXPECT_EQ(1u, gdi->getDestroyArg().AllocationCount);
    EXPECT_NE(nullptr, gdi->getDestroyArg().phAllocationList);
}

HWTEST_F(Wddm20Tests, WhenLastFenceLessEqualThanMonitoredThenWaitFromCpuIsNotCalled) {
    wddm->init<FamilyType>();

    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.getResidencyData().lastFence = 10;
    allocation.handle = ALLOCATION_HANDLE;

    *wddm->getMonitoredFence().cpuAddress = 10;

    gdi->getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi->getWaitFromCpuArg().Flags.Value = 0;
    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi->getWaitFromCpuArg().ObjectCount = 0;
    gdi->getWaitFromCpuArg().ObjectHandleArray = nullptr;

    auto status = wddm->waitFromCpu(10);

    EXPECT_TRUE(status);

    EXPECT_EQ(nullptr, gdi->getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)0, gdi->getWaitFromCpuArg().hDevice);
    EXPECT_EQ(0u, gdi->getWaitFromCpuArg().ObjectCount);
    EXPECT_EQ(nullptr, gdi->getWaitFromCpuArg().ObjectHandleArray);
}

HWTEST_F(Wddm20Tests, WhenLastFenceGreaterThanMonitoredThenWaitFromCpuIsCalled) {
    wddm->init<FamilyType>();

    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.getResidencyData().lastFence = 10;
    allocation.handle = ALLOCATION_HANDLE;

    *wddm->getMonitoredFence().cpuAddress = 10;

    gdi->getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi->getWaitFromCpuArg().Flags.Value = 0;
    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi->getWaitFromCpuArg().ObjectCount = 0;
    gdi->getWaitFromCpuArg().ObjectHandleArray = nullptr;

    auto status = wddm->waitFromCpu(20);

    EXPECT_TRUE(status);

    EXPECT_NE(nullptr, gdi->getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)wddm->getDevice(), gdi->getWaitFromCpuArg().hDevice);
    EXPECT_EQ(1u, gdi->getWaitFromCpuArg().ObjectCount);
    EXPECT_NE(nullptr, gdi->getWaitFromCpuArg().ObjectHandleArray);
}

HWTEST_F(Wddm20Tests, createMonitoredFenceIsInitializedWithFenceValueZeroAndCurrentFenceValueIsSetToOne) {
    wddm->init<FamilyType>();

    gdi->createSynchronizationObject2 = gdi->createSynchronizationObject2Mock;

    gdi->getCreateSynchronizationObject2Arg().Info.MonitoredFence.InitialFenceValue = 300;

    wddm->createMonitoredFence();

    EXPECT_EQ(0u, gdi->getCreateSynchronizationObject2Arg().Info.MonitoredFence.InitialFenceValue);
    EXPECT_EQ(1u, wddm->getMonitoredFence().currentFenceValue);
}

NTSTATUS APIENTRY queryResourceInfoMock(D3DKMT_QUERYRESOURCEINFO *pData) {
    pData->NumAllocations = 0;
    return 0;
}

HWTEST_F(Wddm20Tests, givenOpenSharedHandleWhenZeroAllocationsThenReturnNull) {
    wddm->init<FamilyType>();

    D3DKMT_HANDLE handle = 0;
    WddmAllocation *alloc = nullptr;

    gdi->queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(queryResourceInfoMock);
    auto ret = wddm->openSharedHandle(handle, alloc);

    EXPECT_EQ(false, ret);
}

HWTEST_F(Wddm20Tests, givenReadOnlyMemoryWhenCreateAllocationFailsWithNoVideoMemoryThenCorrectStatusIsReturned) {
    class MockCreateAllocation {
      public:
        static NTSTATUS APIENTRY mockCreateAllocation(D3DKMT_CREATEALLOCATION *param) {
            return STATUS_GRAPHICS_NO_VIDEO_MEMORY;
        };
    };
    gdi->createAllocation = MockCreateAllocation::mockCreateAllocation;
    wddm->init<FamilyType>();

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

HWTEST_F(Wddm20Tests, whenGetOsDeviceContextIsCalledThenWddmOsDeviceContextIsReturned) {
    D3DKMT_HANDLE ctx = 0xc1;
    wddm->context = ctx;
    EXPECT_EQ(ctx, wddm->getOsDeviceContext());
}
