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
#include "unit_tests/os_interface/windows/wddm_tests.h"

#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/os_library.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/windows/wddm_memory_manager.h"

#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "gtest/gtest.h"
#include "runtime/os_interface/os_time.h"

using namespace OCLRT;

HWTEST_F(WddmTestSingle, creation) {
    Wddm *wddm = Wddm::createWddm();

    bool error = wddm->init<FamilyType>();
    EXPECT_TRUE(error);
    EXPECT_TRUE(wddm->isInitialized());
    delete wddm;
}

HWTEST_F(WddmTest, doubleCreation) {
    auto wddmMock(new WddmMock());

    bool error = wddmMock->init<FamilyType>();
    EXPECT_EQ(1u, wddmMock->createContextResult.called);
    error |= wddmMock->init<FamilyType>();
    EXPECT_EQ(1u, wddmMock->createContextResult.called);

    EXPECT_TRUE(error);
    EXPECT_TRUE(wddmMock->isInitialized());

    delete wddmMock;
}

TEST_F(WddmTest, givenNullPageTableManagerWhenUpdateAuxTableCalledThenReturnFalse) {
    auto wddmMock = std::make_unique<WddmMock>();

    wddmMock->resetPageTableManager(nullptr);
    EXPECT_EQ(nullptr, wddmMock->getPageTableManager());

    auto gmm = std::unique_ptr<Gmm>(Gmm::create(nullptr, 1, false));
    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(wddmMock->updateAuxTable(1234u, gmm.get(), true));
}

TEST(WddmTestEnumAdapters, expectTrue) {
    ADAPTER_INFO adpaterInfo;
    const HardwareInfo hwInfo = *platformDevices[0];
    OsLibrary *mockGdiDll = setAdapterInfo(reinterpret_cast<const void *>(hwInfo.pPlatform), reinterpret_cast<const void *>(hwInfo.pSysInfo));

    bool success = Wddm::enumAdapters(0, &adpaterInfo);
    EXPECT_TRUE(success);

    const HardwareInfo *hwinfo = *platformDevices;

    EXPECT_EQ(adpaterInfo.GfxPlatform.eDisplayCoreFamily, hwinfo->pPlatform->eDisplayCoreFamily);
    delete mockGdiDll;
}

TEST(WddmTestEnumAdapters, devIdExpectFalse) {
    ADAPTER_INFO adpaterInfo;
    bool success = Wddm::enumAdapters(1, &adpaterInfo);
    EXPECT_FALSE(success);
}

TEST(WddmTestEnumAdapters, nullAdapterInfoExpectFalse) {
    bool success = Wddm::enumAdapters(0, nullptr);
    EXPECT_FALSE(success);
}

HWTEST_F(WddmTest, context) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());
    D3DKMT_HANDLE context = wddm->createContext();
    EXPECT_TRUE(context != static_cast<D3DKMT_HANDLE>(0));
    bool error = wddm->destroyContext(context);
    EXPECT_TRUE(error);
}

HWTEST_F(WddmTest, allocation) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());
    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
    Gmm *gmm;

    gmm = getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());
    ASSERT_NE(gmm, nullptr);

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    auto error = mockWddm->destroyAllocation(&allocation);
    EXPECT_TRUE(error);

    releaseGmm(gmm);
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

HWTEST_F(WddmTest, givenAllocationSmallerUnderlyingThanAlignedSizeWhenCreatedThenWddmUseAligned) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    void *ptr = reinterpret_cast<void *>(mockWddm->virtualAllocAddress + 0x1000);
    size_t underlyingSize = 0x2100;
    size_t alignedSize = 0x3000;

    size_t underlyingPages = underlyingSize / MemoryConstants::pageSize;
    size_t alignedPages = alignedSize / MemoryConstants::pageSize;

    WddmAllocation allocation(ptr, 0x2100, ptr, 0x3000, nullptr);
    Gmm *gmm = getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize());
    ASSERT_NE(nullptr, gmm);

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_NE(0, allocation.handle);

    EXPECT_EQ(alignedPages, getLastCallMapGpuVaArgFcn()->SizeInPages);
    EXPECT_NE(underlyingPages, getLastCallMapGpuVaArgFcn()->SizeInPages);

    auto ret = mockWddm->destroyAllocation(&allocation);
    EXPECT_TRUE(ret);

    releaseGmm(gmm);
}

HWTEST_F(WddmTest, createAllocation32bit) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    WddmMock *wddmMock = (WddmMock *)wddm;

    uint64_t heap32baseAddress = 0x40000;
    uint64_t heap32Size = 0x40000;
    wddmMock->setHeap32(heap32baseAddress, heap32Size);

    void *alignedPtr = (void *)0x12000;
    size_t alignedSize = 0x2000;

    WddmAllocation allocation(alignedPtr, alignedSize, nullptr);
    Gmm *gmm;

    gmm = getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());
    ASSERT_NE(gmm, nullptr);

    allocation.gmm = gmm;
    allocation.is32BitAllocation = true; // mark 32 bit allocation

    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    EXPECT_EQ(1u, wddmMock->mapGpuVirtualAddressResult.called);

    EXPECT_LE(heap32baseAddress, allocation.gpuPtr);
    EXPECT_GT(heap32baseAddress + heap32Size, allocation.gpuPtr);

    auto success = mockWddm->destroyAllocation(&allocation);
    EXPECT_TRUE(success);

    releaseGmm(gmm);
}

HWTEST_F(WddmTest, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
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

    getSizesFunction(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);
}

HWTEST_F(WddmTest, mapAndFreeGpuVa) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
    Gmm *gmm;

    gmm = getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());
    ASSERT_NE(gmm, nullptr);

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    auto error = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.getUnderlyingBufferSize(), false, false);
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.gpuPtr != 0);

    error = wddm->freeGpuVirtualAddres(allocation.gpuPtr, allocation.getUnderlyingBufferSize());
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.gpuPtr == 0);

    error = mockWddm->destroyAllocation(&allocation);
    EXPECT_TRUE(error);
    releaseGmm(gmm);
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

HWTEST_F(WddmTest, givenNullAllocationWhenCreateThenAllocateAndMap) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());
    OsAgnosticMemoryManager mm(false);

    WddmAllocation allocation(nullptr, 100, nullptr);
    Gmm *gmm;

    gmm = getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());
    ASSERT_NE(gmm, nullptr);

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.gpuPtr != 0);
    EXPECT_TRUE(allocation.gpuPtr == Gmm::canonize(allocation.gpuPtr));

    releaseGmm(gmm);
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

HWTEST_F(WddmTest, makeResidentNonResident) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
    Gmm *gmm;

    gmm = getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());
    ASSERT_NE(gmm, nullptr);

    allocation.gmm = gmm;
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.handle != 0);

    auto error = wddm->mapGpuVirtualAddress(&allocation, allocation.getAlignedCpuPtr(), allocation.getUnderlyingBufferSize(), false, false);
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

    error = mockWddm->destroyAllocation(&allocation);

    EXPECT_TRUE(error);
    releaseGmm(gmm);
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(WddmTest, GetCpuTime) {
    uint64_t time = 0;
    std::unique_ptr<OSTime> osTime(OSTime::create(nullptr).release());
    auto error = osTime->getCpuTime(&time);
    EXPECT_TRUE(error);
    EXPECT_NE(0, time);
}

TEST_F(WddmTest, GivenNoOSInterfaceGetCpuGpuTimeReturnsError) {
    TimeStampData CPUGPUTime = {0};
    std::unique_ptr<OSTime> osTime(OSTime::create(nullptr).release());
    auto success = osTime->getCpuGpuTime(&CPUGPUTime);
    EXPECT_FALSE(success);
    EXPECT_EQ(0, CPUGPUTime.CPUTimeinNS);
    EXPECT_EQ(0, CPUGPUTime.GPUTimeStamp);
}

TEST_F(WddmTest, GetCpuGpuTime) {
    TimeStampData CPUGPUTime01 = {0};
    TimeStampData CPUGPUTime02 = {0};
    std::unique_ptr<OSInterface> osInterface(new OSInterface());
    osInterface->get()->setWddm(wddm);
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

HWTEST_F(WddmTest, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenGraphicsAllocationWithSharedPropertiesIsCreated) {
    void *pSysMem = (void *)0x1000;
    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, 4096u, false));
    auto status = setSizesFunction(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    std::unique_ptr<WddmMock> mockWddm(new WddmMock());
    mockWddm->init<FamilyType>();
    WddmMemoryManager mm(false, mockWddm.release());

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

    status = getSizesFunction(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);

    EXPECT_EQ(0u, status);
    EXPECT_EQ(1u, destroyWithResourceHandleCalled);
}

HWTEST_F(WddmTest, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenMapGpuVaWithCpuPtrDepensOnBitness) {
    void *pSysMem = (void *)0x1000;
    std::unique_ptr<Gmm> gmm(Gmm::create(pSysMem, 4096u, false));
    auto status = setSizesFunction(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    auto mockWddm(new WddmMock());

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

HWTEST_F(WddmTest, givenWddmCreatedWhenNotInitedThenMinAddressZero) {
    Wddm *wddm = Wddm::createWddm();
    uintptr_t expected = 0;
    uintptr_t actual = wddm->getWddmMinAddress();
    EXPECT_EQ(expected, actual);

    delete wddm;
}

HWTEST_F(WddmTest, givenWddmCreatedWhenInitedThenMinAddressValid) {
    Wddm *wddm = Wddm::createWddm();
    bool ret = wddm->init<FamilyType>();
    EXPECT_TRUE(ret);

    uintptr_t expected = windowsMinAddress;
    uintptr_t actual = wddm->getWddmMinAddress();
    EXPECT_EQ(expected, actual);

    delete wddm;
}

HWTEST_F(WddmInstrumentationTest, configureDeviceAddressSpaceOnInit) {
    SYSTEM_INFO sysInfo = {};
    auto mockWddm(new WddmMock());
    auto gmmMem = static_cast<GmockGmmMemory *>(mockWddm->getGmmMemory());
    WddmMock::getSystemInfo(&sysInfo);
    uintptr_t maxAddr = reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress) + 1;

    D3DKMT_HANDLE adapterHandle = ADAPTER_HANDLE;
    D3DKMT_HANDLE deviceHandle = DEVICE_HANDLE;
    const HardwareInfo hwInfo = *platformDevices[0];
    BOOLEAN FtrL3IACoherency = hwInfo.pSkuTable->ftrL3IACoherency ? 1 : 0;

    EXPECT_CALL(*gmmMem, configureDeviceAddressSpace(adapterHandle,
                                                     deviceHandle,
                                                     mockWddm->gdi->escape.mFunc,
                                                     maxAddr,
                                                     0,
                                                     0,
                                                     FtrL3IACoherency,
                                                     0,
                                                     0))
        .Times(1)
        .WillRepeatedly(::testing::Return(true));
    mockWddm->init<FamilyType>();

    EXPECT_TRUE(mockWddm->isInitialized());
    delete mockWddm;
}

HWTEST_F(WddmInstrumentationTest, configureDeviceAddressSpaceNoAdapter) {
    auto mockWddm(new WddmMock());
    auto gmmMem = static_cast<GmockGmmMemory *>(mockWddm->getGmmMemory());

    mockWddm->adapter = static_cast<D3DKMT_HANDLE>(0);
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
    auto ret = mockWddm->configureDeviceAddressSpace<FamilyType>();

    EXPECT_FALSE(ret);
    delete mockWddm;
}

HWTEST_F(WddmInstrumentationTest, configureDeviceAddressSpaceNoDevice) {
    auto mockWddm(new WddmMock());
    auto gmmMem = static_cast<GmockGmmMemory *>(mockWddm->getGmmMemory());

    mockWddm->device = static_cast<D3DKMT_HANDLE>(0);
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
    auto ret = mockWddm->configureDeviceAddressSpace<FamilyType>();

    EXPECT_FALSE(ret);
    delete mockWddm;
}

HWTEST_F(WddmInstrumentationTest, configureDeviceAddressSpaceNoEscFunc) {
    auto mockWddm(new WddmMock());
    auto gmmMem = static_cast<GmockGmmMemory *>(mockWddm->getGmmMemory());

    mockWddm->gdi->escape = static_cast<PFND3DKMT_ESCAPE>(nullptr);
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
    auto ret = mockWddm->configureDeviceAddressSpace<FamilyType>();

    EXPECT_FALSE(ret);
    delete mockWddm;
}

HWTEST_F(WddmTest, getMaxApplicationAddress) {
    auto mockWddm(new WddmMock());

    mockWddm->init<FamilyType>();
    EXPECT_TRUE(mockWddm->isInitialized());

    uint64_t maxAddr = mockWddm->getMaxApplicationAddress();
    if (is32bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    }

    delete mockWddm;
}

HWTEST_F(WddmTest, dontCallCreateContextBeforeConfigureDeviceAddressSpace) {
    auto mockWddm(new WddmMock());
    mockWddm->createContext();
    EXPECT_EQ(1u, mockWddm->createContextResult.called); // dont care about the result

    mockWddm->configureDeviceAddressSpace<FamilyType>();

    EXPECT_EQ(1u, mockWddm->configureDeviceAddressSpaceResult.called);
    EXPECT_FALSE(mockWddm->configureDeviceAddressSpaceResult.success);

    delete mockWddm;
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionEnabledDebugFlagDontForceWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOn) {
    DebugManager.flags.ForcePreemptionMode.set(0); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    unsigned int expectedVal = 1u;
    createAndInitWddm<FamilyType>(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagDontForceWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(0); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm<FamilyType>(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionEnabledDebugFlagDontForceWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(0); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    unsigned int expectedVal = 0u;
    createAndInitWddm<FamilyType>(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagDontForceWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(0); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm<FamilyType>(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagForcePreemptionWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOn) {
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread)); // force preemption
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 1u;
    createAndInitWddm<FamilyType>(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
}

HWTEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagForcePreemptionWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread)); // force preemption
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm<FamilyType>(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
}

HWTEST_F(WddmWithMockGdiTest, makeResidentMultipleHandles) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
    allocation.handle = ALLOCATION_HANDLE;

    D3DKMT_HANDLE handles[2] = {0};

    handles[0] = allocation.handle;
    handles[1] = allocation.handle;

    gdi.getMakeResidentArg().NumAllocations = 0;
    gdi.getMakeResidentArg().AllocationList = nullptr;

    bool error = wddm->makeResident(handles, 2, false, nullptr);
    EXPECT_TRUE(error);

    EXPECT_EQ(2u, gdi.getMakeResidentArg().NumAllocations);
    EXPECT_EQ(handles, gdi.getMakeResidentArg().AllocationList);

    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

HWTEST_F(WddmWithMockGdiTest, makeResidentMultipleHandlesWithReturnBytesToTrim) {
    wddm->init<FamilyType>();
    ASSERT_TRUE(wddm->isInitialized());

    OsAgnosticMemoryManager mm(false);
    WddmAllocation allocation(mm.allocateSystemMemory(100, 0), 100, nullptr);
    allocation.handle = ALLOCATION_HANDLE;

    D3DKMT_HANDLE handles[2] = {0};

    handles[0] = allocation.handle;
    handles[1] = allocation.handle;

    gdi.getMakeResidentArg().NumAllocations = 0;
    gdi.getMakeResidentArg().AllocationList = nullptr;
    gdi.getMakeResidentArg().NumBytesToTrim = 30;

    uint64_t bytesToTrim = 0;
    bool success = wddm->makeResident(handles, 2, false, &bytesToTrim);
    EXPECT_TRUE(success);

    EXPECT_EQ(gdi.getMakeResidentArg().NumBytesToTrim, bytesToTrim);

    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

HWTEST_F(WddmWithMockGdiTest, makeNonResidentCallsEvict) {
    MockGdi gdi;
    WddmMock wddm(&gdi);

    wddm.init<FamilyType>();

    D3DKMT_HANDLE handle = (D3DKMT_HANDLE)0x1234;

    gdi.getEvictArg().AllocationList = nullptr;
    gdi.getEvictArg().Flags.Value = 0;
    gdi.getEvictArg().hDevice = 0;
    gdi.getEvictArg().NumAllocations = 0;
    gdi.getEvictArg().NumBytesToTrim = 20;

    uint64_t sizeToTrim = 10;
    wddm.evict(&handle, 1, sizeToTrim);

    EXPECT_EQ(1u, gdi.getEvictArg().NumAllocations);
    EXPECT_EQ(&handle, gdi.getEvictArg().AllocationList);
    EXPECT_EQ(wddm.getDevice(), gdi.getEvictArg().hDevice);
    EXPECT_EQ(0u, gdi.getEvictArg().NumBytesToTrim);
}

HWTEST_F(WddmWithMockGdiTest, destroyAllocationWithLastFenceValueGreaterThanCurrentValueCallsWaitFromCpu) {
    MockGdi gdi;
    WddmMock wddm(&gdi);

    wddm.init<FamilyType>();

    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.getResidencyData().lastFence = 20;
    allocation.handle = ALLOCATION_HANDLE;

    *wddm.getMonitoredFence().cpuAddress = 10;

    D3DKMT_HANDLE handle = (D3DKMT_HANDLE)0x1234;

    gdi.getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi.getWaitFromCpuArg().Flags.Value = 0;
    gdi.getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi.getWaitFromCpuArg().ObjectCount = 0;
    gdi.getWaitFromCpuArg().ObjectHandleArray = nullptr;

    gdi.getDestroyArg().AllocationCount = 0;
    gdi.getDestroyArg().Flags.Value = 0;
    gdi.getDestroyArg().hDevice = (D3DKMT_HANDLE)0;
    gdi.getDestroyArg().hResource = (D3DKMT_HANDLE)0;
    gdi.getDestroyArg().phAllocationList = nullptr;

    wddm.destroyAllocation(&allocation);

    EXPECT_NE(nullptr, gdi.getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ(wddm.getDevice(), gdi.getWaitFromCpuArg().hDevice);
    EXPECT_EQ(1u, gdi.getWaitFromCpuArg().ObjectCount);
    EXPECT_EQ(&wddm.getMonitoredFence().fenceHandle, gdi.getWaitFromCpuArg().ObjectHandleArray);

    EXPECT_EQ(wddm.getDevice(), gdi.getDestroyArg().hDevice);
    EXPECT_EQ(1u, gdi.getDestroyArg().AllocationCount);
    EXPECT_NE(nullptr, gdi.getDestroyArg().phAllocationList);
}

HWTEST_F(WddmWithMockGdiTest, destroyAllocationWithLastFenceValueLessEqualToCurrentValueDoesNotCallWaitFromCpu) {
    MockGdi gdi;
    WddmMock wddm(&gdi);

    wddm.init<FamilyType>();

    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.getResidencyData().lastFence = 10;
    allocation.handle = ALLOCATION_HANDLE;

    *wddm.getMonitoredFence().cpuAddress = 10;

    D3DKMT_HANDLE handle = (D3DKMT_HANDLE)0x1234;

    gdi.getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi.getWaitFromCpuArg().Flags.Value = 0;
    gdi.getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi.getWaitFromCpuArg().ObjectCount = 0;
    gdi.getWaitFromCpuArg().ObjectHandleArray = nullptr;

    gdi.getDestroyArg().AllocationCount = 0;
    gdi.getDestroyArg().Flags.Value = 0;
    gdi.getDestroyArg().hDevice = (D3DKMT_HANDLE)0;
    gdi.getDestroyArg().hResource = (D3DKMT_HANDLE)0;
    gdi.getDestroyArg().phAllocationList = nullptr;

    wddm.destroyAllocation(&allocation);

    EXPECT_EQ(nullptr, gdi.getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)0, gdi.getWaitFromCpuArg().hDevice);
    EXPECT_EQ(0u, gdi.getWaitFromCpuArg().ObjectCount);
    EXPECT_EQ(nullptr, gdi.getWaitFromCpuArg().ObjectHandleArray);

    EXPECT_EQ(wddm.getDevice(), gdi.getDestroyArg().hDevice);
    EXPECT_EQ(1u, gdi.getDestroyArg().AllocationCount);
    EXPECT_NE(nullptr, gdi.getDestroyArg().phAllocationList);
}

HWTEST_F(WddmWithMockGdiTest, WhenLastFenceLessEqualThanMonitoredThenWaitFromCpuIsNotCalled) {
    MockGdi gdi;
    WddmMock wddm(&gdi);

    wddm.init<FamilyType>();

    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.getResidencyData().lastFence = 10;
    allocation.handle = ALLOCATION_HANDLE;

    *wddm.getMonitoredFence().cpuAddress = 10;

    gdi.getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi.getWaitFromCpuArg().Flags.Value = 0;
    gdi.getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi.getWaitFromCpuArg().ObjectCount = 0;
    gdi.getWaitFromCpuArg().ObjectHandleArray = nullptr;

    auto status = wddm.waitFromCpu(10);

    EXPECT_TRUE(status);

    EXPECT_EQ(nullptr, gdi.getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)0, gdi.getWaitFromCpuArg().hDevice);
    EXPECT_EQ(0u, gdi.getWaitFromCpuArg().ObjectCount);
    EXPECT_EQ(nullptr, gdi.getWaitFromCpuArg().ObjectHandleArray);
}

HWTEST_F(WddmWithMockGdiTest, WhenLastFenceGreaterThanMonitoredThenWaitFromCpuIsCalled) {
    MockGdi gdi;
    WddmMock wddm(&gdi);

    wddm.init<FamilyType>();

    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.getResidencyData().lastFence = 10;
    allocation.handle = ALLOCATION_HANDLE;

    *wddm.getMonitoredFence().cpuAddress = 10;

    gdi.getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi.getWaitFromCpuArg().Flags.Value = 0;
    gdi.getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi.getWaitFromCpuArg().ObjectCount = 0;
    gdi.getWaitFromCpuArg().ObjectHandleArray = nullptr;

    auto status = wddm.waitFromCpu(20);

    EXPECT_TRUE(status);

    EXPECT_NE(nullptr, gdi.getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)wddm.getDevice(), gdi.getWaitFromCpuArg().hDevice);
    EXPECT_EQ(1u, gdi.getWaitFromCpuArg().ObjectCount);
    EXPECT_NE(nullptr, gdi.getWaitFromCpuArg().ObjectHandleArray);
}

HWTEST_F(WddmWithMockGdiTest, createMonitoredFenceIsInitializedWithFenceValueZeroAndCurrentFenceValueIsSetToOne) {
    MockGdi gdi;
    WddmMock wddm(&gdi);

    wddm.init<FamilyType>();

    gdi.createSynchronizationObject2 = gdi.createSynchronizationObject2Mock;

    gdi.getCreateSynchronizationObject2Arg().Info.MonitoredFence.InitialFenceValue = 300;

    wddm.createMonitoredFence();

    EXPECT_EQ(0u, gdi.getCreateSynchronizationObject2Arg().Info.MonitoredFence.InitialFenceValue);
    EXPECT_EQ(1u, wddm.getMonitoredFence().currentFenceValue);
}

NTSTATUS APIENTRY queryResourceInfoMock(D3DKMT_QUERYRESOURCEINFO *pData) {
    pData->NumAllocations = 0;
    return 0;
}

HWTEST_F(WddmWithMockGdiTest, givenOpenSharedHandleWhenZeroAllocationsThenReturnNull) {
    MockGdi gdi;
    WddmMock wddm(&gdi);

    wddm.init<FamilyType>();

    D3DKMT_HANDLE handle = 0;
    WddmAllocation *alloc = nullptr;

    gdi.queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(queryResourceInfoMock);
    auto ret = wddm.openSharedHandle(handle, alloc);

    EXPECT_EQ(false, ret);
}

using WddmReserveAddressTest = WddmTest;

HWTEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsSuccessfulThenReturnReserveAddress) {
    std::unique_ptr<WddmMockReserveAddress> wddmMockPtr(new WddmMockReserveAddress());
    WddmMockReserveAddress *wddmMock = wddmMockPtr.get();
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddmMock->init<FamilyType>();
    EXPECT_TRUE(ret);

    wddmMock->returnGood = 1;
    uintptr_t expectedReserve = wddmMock->virtualAllocAddress;
    ret = wddmMock->reserveValidAddressRange(size, reserve);
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddmMock->releaseReservedAddress(reserve);
}

HWTEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsNullThenReturnNull) {
    std::unique_ptr<WddmMockReserveAddress> wddmMockPtr(new WddmMockReserveAddress());
    WddmMockReserveAddress *wddmMock = wddmMockPtr.get();
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddmMock->init<FamilyType>();
    EXPECT_TRUE(ret);
    uintptr_t expectedReserve = 0;
    ret = wddmMock->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}

HWTEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondSuccessfulThenReturnSecond) {
    std::unique_ptr<WddmMockReserveAddress> wddmMockPtr(new WddmMockReserveAddress());
    WddmMockReserveAddress *wddmMock = wddmMockPtr.get();
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddmMock->init<FamilyType>();
    EXPECT_TRUE(ret);

    wddmMock->returnInvalidCount = 1;
    uintptr_t expectedReserve = wddmMock->virtualAllocAddress;

    ret = wddmMock->reserveValidAddressRange(size, reserve);
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddmMock->releaseReservedAddress(reserve);
}

HWTEST_F(WddmReserveAddressTest, givenWddmWhenSecondIsInvalidThirdSuccessfulThenReturnThird) {
    std::unique_ptr<WddmMockReserveAddress> wddmMockPtr(new WddmMockReserveAddress());
    WddmMockReserveAddress *wddmMock = wddmMockPtr.get();
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddmMock->init<FamilyType>();
    EXPECT_TRUE(ret);

    wddmMock->returnInvalidCount = 2;
    uintptr_t expectedReserve = wddmMock->virtualAllocAddress;

    ret = wddmMock->reserveValidAddressRange(size, reserve);
    EXPECT_TRUE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
    wddmMock->releaseReservedAddress(reserve);
}

HWTEST_F(WddmReserveAddressTest, givenWddmWhenFirstIsInvalidSecondNullThenReturnSecondNull) {
    std::unique_ptr<WddmMockReserveAddress> wddmMockPtr(new WddmMockReserveAddress());
    WddmMockReserveAddress *wddmMock = wddmMockPtr.get();
    size_t size = 0x1000;
    void *reserve = nullptr;

    bool ret = wddmMock->init<FamilyType>();
    EXPECT_TRUE(ret);

    wddmMock->returnInvalidCount = 2;
    wddmMock->returnNullCount = 1;
    uintptr_t expectedReserve = 0;

    ret = wddmMock->reserveValidAddressRange(size, reserve);
    EXPECT_FALSE(ret);
    EXPECT_EQ(expectedReserve, reinterpret_cast<uintptr_t>(reserve));
}
