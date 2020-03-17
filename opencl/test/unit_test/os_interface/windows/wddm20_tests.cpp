/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_engine_mapper.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_gfx_partition.h"
#include "opencl/test/unit_test/mocks/mock_gmm_resource_info.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_wddm_residency_logger.h"
#include "opencl/test/unit_test/mocks/mock_wddm_residency_logger_functions.h"
#include "opencl/test/unit_test/os_interface/windows/mock_wddm_allocation.h"
#include "opencl/test/unit_test/os_interface/windows/ult_dxgi_factory.h"
#include "opencl/test/unit_test/os_interface/windows/wddm_fixture.h"

#include "gtest/gtest.h"

#include <functional>
#include <memory>

namespace NEO {
namespace SysCalls {
extern const wchar_t *currentLibraryPath;
}
extern uint32_t numRootDevicesToEnum;
} // namespace NEO

using namespace NEO;

namespace GmmHelperFunctions {
Gmm *getGmm(void *ptr, size_t size) {
    size_t alignedSize = alignSizeWholePage(ptr, size);
    void *alignedPtr = alignUp(ptr, 4096);

    Gmm *gmm = new Gmm(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), alignedPtr, alignedSize, false);
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
    EXPECT_EQ(expectedAddress, NEO::windowsMinAddress);
    EXPECT_EQ(expectedAddress, wddm->getWddmMinAddress());
}

TEST_F(Wddm20Tests, doubleCreation) {
    EXPECT_EQ(1u, wddm->createContextResult.called);
    wddm->init();
    EXPECT_EQ(1u, wddm->createContextResult.called);
}

TEST_F(Wddm20Tests, givenNullPageTableManagerAndRenderCompressedResourceWhenMappingGpuVaThenDontUpdateAuxTable) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(rootDeviceEnvironemnt->getGmmClientContext(), nullptr, 1, false));
    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    void *fakePtr = reinterpret_cast<void *>(0x100);
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, fakePtr, 0x2100, nullptr, MemoryPool::MemoryNull);
    allocation.setDefaultGmm(gmm.get());
    allocation.getHandleToModify(0u) = ALLOCATION_HANDLE;

    EXPECT_TRUE(wddm->mapGpuVirtualAddress(&allocation));
}

TEST(WddmDiscoverDevices, WhenNoHwDeviceIdIsProvidedToWddmThenWddmIsNotCreated) {
    struct MockWddm : public Wddm {
        MockWddm(std::unique_ptr<HwDeviceId> hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment) : Wddm(std::move(hwDeviceIdIn), rootDeviceEnvironment) {}
    };

    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    EXPECT_THROW(auto wddm = std::make_unique<MockWddm>(nullptr, rootDeviceEnvironment), std::exception);
}

TEST(WddmDiscoverDevices, WhenMultipleRootDevicesAreAvailableThenAllAreDiscovered) {
    VariableBackup<uint32_t> backup{&numRootDevicesToEnum};
    numRootDevicesToEnum = 3u;
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(numRootDevicesToEnum, hwDeviceIds.size());
}

TEST(WddmDiscoverDevices, WhenAdapterDescriptionContainsVirtualRenderThenAdapterIsDiscovered) {
    VariableBackup<const wchar_t *> descriptionBackup(&UltIDXGIAdapter1::description);
    descriptionBackup = L"Virtual Render";
    ExecutionEnvironment executionEnvironment;

    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
}

TEST(Wddm20EnumAdaptersTest, expectTrue) {

    const HardwareInfo *hwInfo = platformDevices[0];
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo->platform,
                                                         &hwInfo->gtSystemInfo,
                                                         hwInfo->capabilityTable.gpuAddressSpace));

    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    std::unique_ptr<Wddm> wddm(Wddm::createWddm(nullptr, rootDeviceEnvironment));
    bool success = wddm->init();

    EXPECT_TRUE(success);

    EXPECT_EQ(rootDeviceEnvironment.getHardwareInfo()->platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);
}

TEST(Wddm20EnumAdaptersTest, givenEmptyHardwareInfoWhenEnumAdapterIsCalledThenCapabilityTableIsSet) {
    const HardwareInfo *hwInfo = platformDevices[0];
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo->platform,
                                                         &hwInfo->gtSystemInfo,
                                                         hwInfo->capabilityTable.gpuAddressSpace));

    ExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    std::unique_ptr<Wddm> wddm(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    bool success = wddm->init();
    HardwareInfo outHwInfo = *rootDeviceEnvironment->getHardwareInfo();
    EXPECT_TRUE(success);

    EXPECT_EQ(outHwInfo.platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);

    EXPECT_EQ(outHwInfo.capabilityTable.defaultProfilingTimerResolution, hwInfo->capabilityTable.defaultProfilingTimerResolution);
    EXPECT_EQ(outHwInfo.capabilityTable.clVersionSupport, hwInfo->capabilityTable.clVersionSupport);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify, hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep, hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds, hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
}

TEST(Wddm20EnumAdaptersTest, givenUnknownPlatformWhenEnumAdapterIsCalledThenFalseIsReturnedAndOutputIsEmpty) {
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.platform.eProductFamily = IGFX_UNKNOWN;
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo.platform,
                                                         &hwInfo.gtSystemInfo,
                                                         hwInfo.capabilityTable.gpuAddressSpace));

    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    std::unique_ptr<Wddm> wddm(Wddm::createWddm(nullptr, rootDeviceEnvironment));
    auto ret = wddm->init();
    EXPECT_FALSE(ret);

    // reset mock gdi
    hwInfo = *platformDevices[0];
    mockGdiDll.reset(setAdapterInfo(&hwInfo.platform,
                                    &hwInfo.gtSystemInfo,
                                    hwInfo.capabilityTable.gpuAddressSpace));
}

TEST_F(Wddm20Tests, whenInitializeWddmThenContextIsCreated) {
    auto context = osContext->getWddmContextHandle();
    EXPECT_TRUE(context != static_cast<D3DKMT_HANDLE>(0));
}

TEST_F(Wddm20Tests, allocation) {
    OsAgnosticMemoryManager mm(*executionEnvironment);
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, mm.allocateSystemMemory(100, 0), 100, nullptr, MemoryPool::MemoryNull);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.setDefaultGmm(gmm);
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.getDefaultHandle() != 0);

    auto error = wddm->destroyAllocation(&allocation, osContext.get());
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

    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, ptr, 0x2100, nullptr, MemoryPool::MemoryNull);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize());

    allocation.setDefaultGmm(gmm);
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_NE(0, allocation.getDefaultHandle());

    bool ret = wddm->mapGpuVirtualAddress(&allocation);
    EXPECT_TRUE(ret);

    EXPECT_EQ(alignedPages, getLastCallMapGpuVaArgFcn()->SizeInPages);
    EXPECT_NE(underlyingPages, getLastCallMapGpuVaArgFcn()->SizeInPages);

    ret = wddm->destroyAllocation(&allocation, osContext.get());
    EXPECT_TRUE(ret);

    delete gmm;
}
TEST_F(Wddm20WithMockGdiDllTests, givenReserveCallWhenItIsCalledWithProperParamtersThenAddressInRangeIsReturend) {
    auto sizeAlignedTo64Kb = 64 * KB;

    auto reservationAddress = wddm->reserveGpuVirtualAddress(wddm->getGfxPartition().Heap32[0].Base,
                                                             wddm->getGfxPartition().Heap32[0].Limit,
                                                             sizeAlignedTo64Kb);

    EXPECT_GE(reservationAddress, wddm->getGfxPartition().Heap32[0].Base);
    auto programmedReserved = getLastCallReserveGpuVaArgFcn();
    EXPECT_EQ(0llu, programmedReserved->BaseAddress);
    EXPECT_EQ(wddm->getGfxPartition().Heap32[0].Base, programmedReserved->MinimumAddress);
    EXPECT_EQ(wddm->getGfxPartition().Heap32[0].Limit, programmedReserved->MaximumAddress);
    EXPECT_EQ(sizeAlignedTo64Kb, programmedReserved->Size);

    auto pagingQueue = wddm->getPagingQueue();
    EXPECT_NE(0llu, pagingQueue);
    EXPECT_EQ(pagingQueue, programmedReserved->hPagingQueue);
}

TEST_F(Wddm20WithMockGdiDllTests, givenWddmAllocationWhenMappingGpuVaThenUseGmmSize) {
    void *fakePtr = reinterpret_cast<void *>(0x123);
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, fakePtr, 100, nullptr, MemoryPool::MemoryNull);
    std::unique_ptr<Gmm> gmm(GmmHelperFunctions::getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize()));

    allocation.setDefaultGmm(gmm.get());
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);

    auto mockResourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockResourceInfo->overrideReturnedSize(allocation.getAlignedSize() + (2 * MemoryConstants::pageSize));

    wddm->mapGpuVirtualAddress(&allocation);

    uint64_t expectedSizeInPages = static_cast<uint64_t>(mockResourceInfo->getSizeAllocation() / MemoryConstants::pageSize);
    EXPECT_EQ(expectedSizeInPages, getLastCallMapGpuVaArgFcn()->SizeInPages);
}

TEST_F(Wddm20Tests, givenGraphicsAllocationWhenItIsMappedInHeap0ThenItHasGpuAddressWithinHeapInternalLimits) {
    void *alignedPtr = (void *)0x12000;
    size_t alignedSize = 0x2000;
    std::unique_ptr<Gmm> gmm(GmmHelperFunctions::getGmm(alignedPtr, alignedSize));
    uint64_t gpuAddress = 0u;
    auto heapBase = wddm->getGfxPartition().Heap32[static_cast<uint32_t>(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)].Base;
    auto heapLimit = wddm->getGfxPartition().Heap32[static_cast<uint32_t>(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)].Limit;

    bool ret = wddm->mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, heapBase, heapLimit, 0u, gpuAddress);
    EXPECT_TRUE(ret);
    auto cannonizedHeapBase = GmmHelper::canonize(heapBase);
    auto cannonizedHeapEnd = GmmHelper::canonize(heapLimit);

    EXPECT_GE(gpuAddress, cannonizedHeapBase);
    EXPECT_LE(gpuAddress, cannonizedHeapEnd);
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
    OsAgnosticMemoryManager mm(*executionEnvironment);
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, mm.allocateSystemMemory(100, 0), 100, nullptr, MemoryPool::MemoryNull);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.setDefaultGmm(gmm);
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.getDefaultHandle() != 0);

    auto error = wddm->mapGpuVirtualAddress(&allocation);
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.getGpuAddress() != 0);

    error = wddm->freeGpuVirtualAddress(allocation.getGpuAddressToModify(), allocation.getUnderlyingBufferSize());
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.getGpuAddress() == 0);

    error = wddm->destroyAllocation(&allocation, osContext.get());
    EXPECT_TRUE(error);
    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20Tests, givenNullAllocationWhenCreateThenAllocateAndMap) {
    OsAgnosticMemoryManager mm(*executionEnvironment);

    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 100, nullptr, MemoryPool::MemoryNull);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.setDefaultGmm(gmm);
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);

    bool ret = wddm->mapGpuVirtualAddress(&allocation);
    EXPECT_TRUE(ret);

    EXPECT_NE(0u, allocation.getGpuAddress());
    EXPECT_EQ(allocation.getGpuAddress(), GmmHelper::canonize(allocation.getGpuAddress()));

    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(WddmTestWithMockGdiDll, givenShareableAllocationWhenCreateThenCreateResourceFlagIsEnabled) {
    init();
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, true);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(nullptr, MemoryConstants::pageSize));
    allocation.setDefaultGmm(gmm.get());
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);
    auto passedCreateAllocation = getMockAllocationFcn();
    EXPECT_EQ(TRUE, passedCreateAllocation->Flags.CreateShared);
    EXPECT_EQ(TRUE, passedCreateAllocation->Flags.CreateResource);
}

TEST_F(WddmTestWithMockGdiDll, givenShareableAllocationWhenCreateThenSharedHandleAndResourceHandleAreSet) {
    init();
    struct MockWddmMemoryManager : public WddmMemoryManager {
        using WddmMemoryManager::createGpuAllocationsWithRetry;
        using WddmMemoryManager::WddmMemoryManager;
    };
    MemoryManagerCreate<MockWddmMemoryManager> memoryManager(false, false, *executionEnvironment);
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, true);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(nullptr, MemoryConstants::pageSize));
    allocation.setDefaultGmm(gmm.get());
    auto status = memoryManager.createGpuAllocationsWithRetry(&allocation);
    EXPECT_TRUE(status);
    EXPECT_NE(0u, allocation.resourceHandle);
    EXPECT_NE(0u, allocation.peekSharedHandle());
}

TEST(WddmAllocationTest, whenAllocationIsShareableThenSharedHandleToModifyIsSharedHandleOfAllocation) {
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, true);
    auto sharedHandleToModify = allocation.getSharedHandleToModify();
    EXPECT_NE(nullptr, sharedHandleToModify);
    *sharedHandleToModify = 1234u;
    EXPECT_EQ(*sharedHandleToModify, allocation.peekSharedHandle());
}

TEST(WddmAllocationTest, whenAllocationIsNotShareableThenItDoesntReturnSharedHandleToModify) {
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, false);
    auto sharedHandleToModify = allocation.getSharedHandleToModify();
    EXPECT_EQ(nullptr, sharedHandleToModify);
}

TEST_F(Wddm20Tests, makeResidentNonResident) {
    OsAgnosticMemoryManager mm(*executionEnvironment);
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, mm.allocateSystemMemory(100, 0), 100, nullptr, MemoryPool::MemoryNull);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize());

    allocation.setDefaultGmm(gmm);
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.getDefaultHandle() != 0);

    auto error = wddm->mapGpuVirtualAddress(&allocation);
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.getGpuAddress() != 0);

    error = wddm->makeResident(allocation.getHandles().data(), allocation.getNumHandles(), false, nullptr, allocation.getAlignedSize());
    EXPECT_TRUE(error);

    uint64_t sizeToTrim;
    error = wddm->evict(allocation.getHandles().data(), allocation.getNumHandles(), sizeToTrim);
    EXPECT_TRUE(error);

    auto monitoredFence = osContext->getResidencyController().getMonitoredFence();
    UINT64 fenceValue = 100;
    monitoredFence.cpuAddress = &fenceValue;
    monitoredFence.currentFenceValue = 101;

    error = wddm->destroyAllocation(&allocation, osContext.get());

    EXPECT_TRUE(error);
    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenGraphicsAllocationWithSharedPropertiesIsCreated) {
    void *pSysMem = (void *)0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    MemoryManagerCreate<WddmMemoryManager> mm(false, false, *executionEnvironment);
    AllocationProperties properties(0, false, 4096u, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);

    auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(ALLOCATION_HANDLE, properties, false);
    auto wddmAllocation = (WddmAllocation *)graphicsAllocation;
    ASSERT_NE(nullptr, wddmAllocation);

    EXPECT_EQ(ALLOCATION_HANDLE, wddmAllocation->peekSharedHandle());
    EXPECT_EQ(RESOURCE_HANDLE, wddmAllocation->resourceHandle);
    EXPECT_NE(0u, wddmAllocation->getDefaultHandle());
    EXPECT_EQ(ALLOCATION_HANDLE, wddmAllocation->getDefaultHandle());
    EXPECT_NE(0u, wddmAllocation->getGpuAddress());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(nullptr, wddmAllocation->getAlignedCpuPtr());
    EXPECT_NE(nullptr, wddmAllocation->getDefaultGmm());

    EXPECT_EQ(4096u, wddmAllocation->getDefaultGmm()->gmmResourceInfo->getSizeAllocation());

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
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    MemoryManagerCreate<WddmMemoryManager> mm(false, false, *executionEnvironment);
    AllocationProperties properties(0, false, 4096, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);

    auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(ALLOCATION_HANDLE, properties, false);
    auto wddmAllocation = (WddmAllocation *)graphicsAllocation;
    ASSERT_NE(nullptr, wddmAllocation);

    if (is32bit && executionEnvironment->rootDeviceEnvironments[graphicsAllocation->getRootDeviceIndex()]->isFullRangeSvm()) {
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
    BOOLEAN FtrL3IACoherency = hwInfo.featureTable.ftrL3IACoherency ? 1 : 0;
    uintptr_t maxAddr = hwInfo.capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress
                            ? reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress) + 1
                            : 0;
    EXPECT_CALL(*gmmMem, configureDeviceAddressSpace(adapterHandle,
                                                     deviceHandle,
                                                     wddm->getGdi()->escape.mFunc,
                                                     maxAddr,
                                                     FtrL3IACoherency))
        .Times(1)
        .WillRepeatedly(::testing::Return(true));
    wddm->init();
}

TEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceNoAdapter) {
    auto gdi = std::make_unique<Gdi>();
    wddm->resetGdi(gdi.release());
    EXPECT_CALL(*gmmMem,
                configureDeviceAddressSpace(static_cast<D3DKMT_HANDLE>(0), ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    auto ret = wddm->configureDeviceAddressSpace();

    EXPECT_FALSE(ret);
}

TEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceNoDevice) {
    wddm->device = static_cast<D3DKMT_HANDLE>(0);
    EXPECT_CALL(*gmmMem,
                configureDeviceAddressSpace(::testing::_, static_cast<D3DKMT_HANDLE>(0), ::testing::_, ::testing::_, ::testing::_))
        .Times(0);
    auto ret = wddm->configureDeviceAddressSpace();

    EXPECT_FALSE(ret);
}

TEST_F(Wddm20InstrumentationTest, configureDeviceAddressSpaceNoEscFunc) {
    wddm->getGdi()->escape = static_cast<PFND3DKMT_ESCAPE>(nullptr);
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

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenCreateContextCallWhenDriverHintsItPointsToOpenCL) {
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    EXPECT_EQ(D3DKMT_CLIENTHINT_OPENCL, createContextParams->ClientHint);
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenUseNoRingFlushesKmdModeDebugFlagToTrueWhenCreateContextIsCalledThenNoRingFlushesKmdModeIsSetToTrue) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseNoRingFlushesKmdMode.set(true);
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    auto privateData = (CREATECONTEXT_PVTDATA *)createContextParams->pPrivateDriverData;
    EXPECT_TRUE(!!privateData->NoRingFlushes);
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenEngineTypeWhenCreatingContextThenPassCorrectNodeOrdinal) {
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    UINT expected = WddmEngineMapper::engineNodeMap(HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[0]);
    EXPECT_EQ(expected, createContextParams->NodeOrdinal);
}

TEST_F(Wddm20WithMockGdiDllTests, whenCreateContextIsCalledThenDisableHwQueues) {
    EXPECT_FALSE(wddm->wddmInterface->hwQueuesSupported());
    EXPECT_EQ(0u, getCreateContextDataFcn()->Flags.HwQueueSupported);
}

TEST_F(Wddm20WithMockGdiDllTests, givenDestructionOsContextWinWhenCallingDestroyMonitorFenceThenDoCallGdiDestroy) {
    auto fenceHandle = osContext->getResidencyController().getMonitoredFence().fenceHandle;

    osContext.reset(nullptr);
    EXPECT_EQ(1u, wddmMockInterface->destroyMonitorFenceCalled);
    EXPECT_EQ(fenceHandle, getDestroySynchronizationObjectDataFcn()->hSyncObject);
}

TEST_F(Wddm20Tests, whenCreateHwQueueIsCalledThenAlwaysReturnFalse) {
    EXPECT_FALSE(wddm->wddmInterface->createHwQueue(*osContext.get()));
}

TEST_F(Wddm20Tests, whenWddmIsInitializedThenGdiDoesntHaveHwQueueDDIs) {
    EXPECT_EQ(nullptr, wddm->getGdi()->createHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->getGdi()->destroyHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->getGdi()->submitCommandToHwQueue.mFunc);
}

TEST(DebugFlagTest, givenDebugManagerWhenGetForUseNoRingFlushesKmdModeIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(DebugManager.flags.UseNoRingFlushesKmdMode.get());
}

TEST_F(Wddm20Tests, makeResidentMultipleHandles) {
    D3DKMT_HANDLE handles[2] = {ALLOCATION_HANDLE, ALLOCATION_HANDLE};
    gdi->getMakeResidentArg().NumAllocations = 0;
    gdi->getMakeResidentArg().AllocationList = nullptr;

    bool error = wddm->makeResident(handles, 2, false, nullptr, 0x1000);
    EXPECT_TRUE(error);

    EXPECT_EQ(2u, gdi->getMakeResidentArg().NumAllocations);
    EXPECT_EQ(handles, gdi->getMakeResidentArg().AllocationList);
}

TEST_F(Wddm20Tests, makeResidentMultipleHandlesWithReturnBytesToTrim) {
    D3DKMT_HANDLE handles[2] = {ALLOCATION_HANDLE, ALLOCATION_HANDLE};

    gdi->getMakeResidentArg().NumAllocations = 0;
    gdi->getMakeResidentArg().AllocationList = nullptr;
    gdi->getMakeResidentArg().NumBytesToTrim = 30;

    uint64_t bytesToTrim = 0;
    bool success = wddm->makeResident(handles, 2, false, &bytesToTrim, 0x1000);
    EXPECT_TRUE(success);

    EXPECT_EQ(gdi->getMakeResidentArg().NumBytesToTrim, bytesToTrim);
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
    MockWddmAllocation allocation;
    allocation.getResidencyData().updateCompletionData(10, osContext->getContextId());
    allocation.handle = ALLOCATION_HANDLE;

    *osContext->getResidencyController().getMonitoredFence().cpuAddress = 10;

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

    wddm->destroyAllocation(&allocation, osContext.get());

    EXPECT_EQ(wddm->getDevice(), gdi->getDestroyArg().hDevice);
    EXPECT_EQ(1u, gdi->getDestroyArg().AllocationCount);
    EXPECT_NE(nullptr, gdi->getDestroyArg().phAllocationList);
}

TEST_F(Wddm20Tests, WhenLastFenceLessEqualThanMonitoredThenWaitFromCpuIsNotCalled) {
    MockWddmAllocation allocation;
    allocation.getResidencyData().updateCompletionData(10, osContext->getContextId());
    allocation.handle = ALLOCATION_HANDLE;

    *osContext->getResidencyController().getMonitoredFence().cpuAddress = 10;

    gdi->getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi->getWaitFromCpuArg().Flags.Value = 0;
    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi->getWaitFromCpuArg().ObjectCount = 0;
    gdi->getWaitFromCpuArg().ObjectHandleArray = nullptr;

    auto status = wddm->waitFromCpu(10, osContext->getResidencyController().getMonitoredFence());

    EXPECT_TRUE(status);

    EXPECT_EQ(nullptr, gdi->getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)0, gdi->getWaitFromCpuArg().hDevice);
    EXPECT_EQ(0u, gdi->getWaitFromCpuArg().ObjectCount);
    EXPECT_EQ(nullptr, gdi->getWaitFromCpuArg().ObjectHandleArray);
}

TEST_F(Wddm20Tests, WhenLastFenceGreaterThanMonitoredThenWaitFromCpuIsCalled) {
    MockWddmAllocation allocation;
    allocation.getResidencyData().updateCompletionData(10, osContext->getContextId());
    allocation.handle = ALLOCATION_HANDLE;

    *osContext->getResidencyController().getMonitoredFence().cpuAddress = 10;

    gdi->getWaitFromCpuArg().FenceValueArray = nullptr;
    gdi->getWaitFromCpuArg().Flags.Value = 0;
    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;
    gdi->getWaitFromCpuArg().ObjectCount = 0;
    gdi->getWaitFromCpuArg().ObjectHandleArray = nullptr;

    auto status = wddm->waitFromCpu(20, osContext->getResidencyController().getMonitoredFence());

    EXPECT_TRUE(status);

    EXPECT_NE(nullptr, gdi->getWaitFromCpuArg().FenceValueArray);
    EXPECT_EQ((D3DKMT_HANDLE)wddm->getDevice(), gdi->getWaitFromCpuArg().hDevice);
    EXPECT_EQ(1u, gdi->getWaitFromCpuArg().ObjectCount);
    EXPECT_NE(nullptr, gdi->getWaitFromCpuArg().ObjectHandleArray);
}

TEST_F(Wddm20Tests, createMonitoredFenceIsInitializedWithFenceValueZeroAndCurrentFenceValueIsSetToOne) {
    gdi->createSynchronizationObject2 = gdi->createSynchronizationObject2Mock;

    gdi->getCreateSynchronizationObject2Arg().Info.MonitoredFence.InitialFenceValue = 300;

    wddm->wddmInterface->createMonitoredFence(*osContext);

    EXPECT_EQ(0u, gdi->getCreateSynchronizationObject2Arg().Info.MonitoredFence.InitialFenceValue);
    EXPECT_EQ(1u, osContext->getResidencyController().getMonitoredFence().currentFenceValue);
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
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironemnt->getGmmClientContext(), fakePtr, 100, false);
    WddmAllocation allocation(0, GraphicsAllocation::AllocationType::UNKNOWN, fakePtr, 100, nullptr, MemoryPool::MemoryNull);
    allocation.setDefaultGmm(gmm.get());

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

using WddmLockWithMakeResidentTests = Wddm20Tests;

TEST_F(WddmLockWithMakeResidentTests, givenAllocationThatDoesntNeedMakeResidentBeforeLockWhenLockThenDontStoreItOrCallMakeResident) {
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    wddm->lockResource(ALLOCATION_HANDLE, false, 0x1000);
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    wddm->unlockResource(ALLOCATION_HANDLE);
}
TEST_F(WddmLockWithMakeResidentTests, givenAllocationThatNeedsMakeResidentBeforeLockWhenLockThenCallBlockingMakeResident) {
    wddm->lockResource(ALLOCATION_HANDLE, true, 0x1000);
    EXPECT_EQ(1u, mockTemporaryResources->makeResidentResult.called);
}
TEST_F(WddmLockWithMakeResidentTests, givenAllocationWhenApplyBlockingMakeResidentThenAcquireUniqueLock) {
    wddm->temporaryResources->makeResidentResource(ALLOCATION_HANDLE, 0x1000);
    EXPECT_EQ(1u, mockTemporaryResources->acquireLockResult.called);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&mockTemporaryResources->resourcesLock), mockTemporaryResources->acquireLockResult.uint64ParamPassed);
}
TEST_F(WddmLockWithMakeResidentTests, givenAllocationWhenApplyBlockingMakeResidentThenCallMakeResidentAndStoreAllocation) {
    wddm->temporaryResources->makeResidentResource(ALLOCATION_HANDLE, 0x1000);
    EXPECT_EQ(1u, wddm->makeResidentResult.called);
    EXPECT_EQ(ALLOCATION_HANDLE, mockTemporaryResources->resourceHandles.back());
}
TEST_F(WddmLockWithMakeResidentTests, givenAllocationWhenApplyBlockingMakeResidentThenWaitForCurrentPagingFenceValue) {
    wddm->mockPagingFence = 0u;
    wddm->currentPagingFenceValue = 3u;
    wddm->temporaryResources->makeResidentResource(ALLOCATION_HANDLE, 0x1000);
    EXPECT_EQ(1u, wddm->makeResidentResult.called);
    EXPECT_EQ(3u, wddm->mockPagingFence);
    EXPECT_EQ(3u, wddm->getPagingFenceAddressResult.called);
}
TEST_F(WddmLockWithMakeResidentTests, givenAllocationWhenApplyBlockingMakeResidentAndMakeResidentCallFailsThenEvictTemporaryResourcesAndRetry) {
    MockWddmAllocation allocation;
    allocation.handle = 0x3;
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    EXPECT_CALL(gmockWddm, makeResident(&allocation.handle, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillRepeatedly(::testing::Return(false));
    gmockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(1u, mockTemporaryResources->evictAllResourcesResult.called);
}
TEST_F(WddmLockWithMakeResidentTests, whenApplyBlockingMakeResidentAndTemporaryResourcesAreEvictedSuccessfullyThenCallMakeResidentOneMoreTime) {
    MockWddmAllocation allocation;
    allocation.handle = 0x3;
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(allocation.handle);
    EXPECT_CALL(gmockWddm, evict(::testing::_, ::testing::_, ::testing::_)).Times(1).WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(gmockWddm, makeResident(&allocation.handle, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(3).WillRepeatedly(::testing::Return(false));
    gmockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(2u, mockTemporaryResources->evictAllResourcesResult.called);
}
TEST_F(WddmLockWithMakeResidentTests, whenApplyBlockingMakeResidentAndMakeResidentStillFailsThenDontStoreTemporaryResource) {
    MockWddmAllocation allocation;
    allocation.handle = 0x2;
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(0x1);
    EXPECT_CALL(gmockWddm, evict(::testing::_, ::testing::_, ::testing::_)).Times(1).WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(gmockWddm, makeResident(&allocation.handle, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(3).WillRepeatedly(::testing::Return(false));
    EXPECT_EQ(1u, mockTemporaryResources->resourceHandles.size());
    gmockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(0u, mockTemporaryResources->resourceHandles.size());
}
TEST_F(WddmLockWithMakeResidentTests, whenApplyBlockingMakeResidentAndMakeResidentPassesAfterEvictThenStoreTemporaryResource) {
    MockWddmAllocation allocation;
    allocation.handle = 0x2;
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(0x1);
    EXPECT_CALL(gmockWddm, evict(::testing::_, ::testing::_, ::testing::_)).Times(1).WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(gmockWddm, makeResident(&allocation.handle, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Return(false)).WillOnce(::testing::Return(true));
    EXPECT_EQ(1u, mockTemporaryResources->resourceHandles.size());
    gmockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(1u, mockTemporaryResources->resourceHandles.size());
    EXPECT_EQ(0x2, mockTemporaryResources->resourceHandles.back());
}
TEST_F(WddmLockWithMakeResidentTests, whenApplyBlockingMakeResidentAndMakeResidentPassesThenStoreTemporaryResource) {
    MockWddmAllocation allocation;
    allocation.handle = 0x2;
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(0x1);
    EXPECT_CALL(gmockWddm, makeResident(&allocation.handle, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(1).WillOnce(::testing::Return(true));
    gmockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(2u, mockTemporaryResources->resourceHandles.size());
    EXPECT_EQ(0x2, mockTemporaryResources->resourceHandles.back());
}
TEST_F(WddmLockWithMakeResidentTests, givenNoTemporaryResourcesWhenEvictingAllTemporaryResourcesThenEvictionIsNotApplied) {
    wddm->getTemporaryResourcesContainer()->evictAllResources();
    EXPECT_EQ(MemoryOperationsStatus::MEMORY_NOT_FOUND, mockTemporaryResources->evictAllResourcesResult.operationSuccess);
}
TEST_F(WddmLockWithMakeResidentTests, whenEvictingAllTemporaryResourcesThenAcquireTemporaryResourcesLock) {
    wddm->getTemporaryResourcesContainer()->evictAllResources();
    EXPECT_EQ(1u, mockTemporaryResources->acquireLockResult.called);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&mockTemporaryResources->resourcesLock), mockTemporaryResources->acquireLockResult.uint64ParamPassed);
}
TEST_F(WddmLockWithMakeResidentTests, whenEvictingAllTemporaryResourcesAndAllEvictionsSucceedThenReturnSuccess) {
    MockWddmAllocation allocation;
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(allocation.handle);
    EXPECT_CALL(gmockWddm, evict(::testing::_, ::testing::_, ::testing::_)).Times(1).WillOnce(::testing::Return(true));
    gmockWddm.getTemporaryResourcesContainer()->evictAllResources();
    EXPECT_EQ(1u, mockTemporaryResources->evictAllResourcesResult.called);
    EXPECT_EQ(MemoryOperationsStatus::SUCCESS, mockTemporaryResources->evictAllResourcesResult.operationSuccess);
}
TEST_F(WddmLockWithMakeResidentTests, givenThreeAllocationsWhenEvictingAllTemporaryResourcesThenCallEvictForEachAllocationAndCleanList) {
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    constexpr uint32_t numAllocations = 3u;
    for (auto i = 0u; i < numAllocations; i++) {
        mockTemporaryResources->resourceHandles.push_back(i);
    }
    EXPECT_CALL(gmockWddm, evict(::testing::_, ::testing::_, ::testing::_)).Times(1).WillRepeatedly(::testing::Return(true));
    gmockWddm.getTemporaryResourcesContainer()->evictAllResources();
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
}
TEST_F(WddmLockWithMakeResidentTests, givenThreeAllocationsWhenEvictingAllTemporaryResourcesAndOneOfThemFailsThenReturnFail) {
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    constexpr uint32_t numAllocations = 3u;
    for (auto i = 0u; i < numAllocations; i++) {
        mockTemporaryResources->resourceHandles.push_back(i);
    }
    EXPECT_CALL(gmockWddm, evict(::testing::_, ::testing::_, ::testing::_)).Times(1).WillOnce(::testing::Return(false));
    gmockWddm.getTemporaryResourcesContainer()->evictAllResources();
    EXPECT_EQ(MemoryOperationsStatus::FAILED, mockTemporaryResources->evictAllResourcesResult.operationSuccess);
}
TEST_F(WddmLockWithMakeResidentTests, givenNoTemporaryResourcesWhenEvictingTemporaryResourceThenEvictionIsNotApplied) {
    wddm->getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE);
    EXPECT_EQ(MemoryOperationsStatus::MEMORY_NOT_FOUND, mockTemporaryResources->evictResourceResult.operationSuccess);
}
TEST_F(WddmLockWithMakeResidentTests, whenEvictingTemporaryResourceThenAcquireTemporaryResourcesLock) {
    wddm->getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE);
    EXPECT_EQ(1u, mockTemporaryResources->acquireLockResult.called);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&mockTemporaryResources->resourcesLock), mockTemporaryResources->acquireLockResult.uint64ParamPassed);
}
TEST_F(WddmLockWithMakeResidentTests, whenEvictingNonExistingTemporaryResourceThenEvictIsNotAppliedAndTemporaryResourcesAreRestored) {
    mockTemporaryResources->resourceHandles.push_back(ALLOCATION_HANDLE);
    EXPECT_FALSE(mockTemporaryResources->resourceHandles.empty());
    wddm->getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE + 1);
    EXPECT_FALSE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(MemoryOperationsStatus::MEMORY_NOT_FOUND, mockTemporaryResources->evictResourceResult.operationSuccess);
}
TEST_F(WddmLockWithMakeResidentTests, whenEvictingTemporaryResourceAndEvictFailsThenReturnFail) {
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(ALLOCATION_HANDLE);
    EXPECT_CALL(gmockWddm, evict(::testing::_, ::testing::_, ::testing::_)).Times(1).WillOnce(::testing::Return(false));
    gmockWddm.getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE);
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(MemoryOperationsStatus::FAILED, mockTemporaryResources->evictResourceResult.operationSuccess);
}
TEST_F(WddmLockWithMakeResidentTests, whenEvictingTemporaryResourceAndEvictSucceedThenReturnSuccess) {
    GmockWddm gmockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    auto mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(gmockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(ALLOCATION_HANDLE);
    EXPECT_CALL(gmockWddm, evict(::testing::_, ::testing::_, ::testing::_)).Times(1).WillOnce(::testing::Return(true));
    gmockWddm.getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE);
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(MemoryOperationsStatus::SUCCESS, mockTemporaryResources->evictResourceResult.operationSuccess);
}
TEST_F(WddmLockWithMakeResidentTests, whenEvictingTemporaryResourceThenOtherResourcesRemainOnTheList) {
    mockTemporaryResources->resourceHandles.push_back(0x1);
    mockTemporaryResources->resourceHandles.push_back(0x2);
    mockTemporaryResources->resourceHandles.push_back(0x3);

    wddm->getTemporaryResourcesContainer()->evictResource(0x2);

    EXPECT_EQ(2u, mockTemporaryResources->resourceHandles.size());
    EXPECT_EQ(0x1, mockTemporaryResources->resourceHandles.front());
    EXPECT_EQ(0x3, mockTemporaryResources->resourceHandles.back());
}

TEST_F(WddmLockWithMakeResidentTests, whenAlllocationNeedsBlockingMakeResidentBeforeLockThenLockWithBlockingMakeResident) {
    WddmMemoryManager memoryManager(*executionEnvironment);
    MockWddmAllocation allocation;
    allocation.needsMakeResidentBeforeLock = false;
    memoryManager.lockResource(&allocation);
    EXPECT_EQ(1u, wddm->lockResult.called);
    EXPECT_EQ(0u, wddm->lockResult.uint64ParamPassed);
    memoryManager.unlockResource(&allocation);

    allocation.needsMakeResidentBeforeLock = true;
    memoryManager.lockResource(&allocation);
    EXPECT_EQ(2u, wddm->lockResult.called);
    EXPECT_EQ(1u, wddm->lockResult.uint64ParamPassed);
    memoryManager.unlockResource(&allocation);
}
using WddmGfxPartitionTest = Wddm20Tests;

TEST_F(WddmGfxPartitionTest, initGfxPartition) {
    MockGfxPartition gfxPartition;

    for (auto heap : MockGfxPartition::allHeapNames) {
        ASSERT_FALSE(gfxPartition.heapInitialized(heap));
    }

    wddm->initGfxPartition(gfxPartition, 0, 1);

    for (auto heap : MockGfxPartition::allHeapNames) {
        if (!gfxPartition.heapInitialized(heap)) {
            EXPECT_TRUE(heap == HeapIndex::HEAP_SVM || heap == HeapIndex::HEAP_EXTENDED);
        } else {
            EXPECT_TRUE(gfxPartition.heapInitialized(heap));
        }
    }
}

TEST(WddmGfxPartitionTests, initGfxPartitionHeapStandard64KBSplit) {
    struct MockWddm : public Wddm {
        using Wddm::gfxPartition;

        MockWddm(RootDeviceEnvironment &rootDeviceEnvironment) : Wddm(std::move(OSInterface::discoverDevices(rootDeviceEnvironment.executionEnvironment)[0]), rootDeviceEnvironment) {}
    };

    MockWddm wddm(*platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0].get());

    uint32_t rootDeviceIndex = 3;
    size_t numRootDevices = 5;

    MockGfxPartition gfxPartition;
    wddm.init();
    wddm.initGfxPartition(gfxPartition, rootDeviceIndex, numRootDevices);

    auto heapStandard64KBSize = alignDown((wddm.gfxPartition.Standard64KB.Limit - wddm.gfxPartition.Standard64KB.Base + 1) / numRootDevices, GfxPartition::heapGranularity);
    EXPECT_EQ(heapStandard64KBSize, gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD64KB));
    EXPECT_EQ(wddm.gfxPartition.Standard64KB.Base + rootDeviceIndex * heapStandard64KBSize, gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD64KB));
}

TEST_F(Wddm20Tests, givenWddmWhenDiscoverDevicesAndForceDeviceIdIsTheSameAsTheExistingDeviceThenReturnTheAdapter) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.ForceDeviceId.set("1234"); // Existing device Id
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
}

TEST_F(WddmTest, WhenFeatureFlagHwQueueIsDisabledThenReturnWddm20Version) {
    wddm->featureTable->ftrWddmHwQueues = 0;
    EXPECT_EQ(WddmVersion::WDDM_2_0, wddm->getWddmVersion());
}

TEST_F(WddmTest, WhenFeatureFlagHwQueueIsEnabledThenReturnWddm23Version) {
    wddm->featureTable->ftrWddmHwQueues = 1;
    EXPECT_EQ(WddmVersion::WDDM_2_3, wddm->getWddmVersion());
}

TEST_F(Wddm20WithMockGdiDllTests, GivenCreationSucceedWhenCreatingSeparateMonitorFenceThenReturnFilledStructure) {
    MonitoredFence monitorFence = {0};

    bool ret = wddmMockInterface->createMonitoredFence(monitorFence);
    EXPECT_TRUE(ret);

    EXPECT_EQ(4u, monitorFence.fenceHandle);
    EXPECT_EQ(getMonitorFenceCpuFenceAddressFcn(), monitorFence.cpuAddress);
}

TEST_F(Wddm20WithMockGdiDllTests, GivenCreationFailWhenCreatingSeparateMonitorFenceThenReturnNotFilledStructure) {
    MonitoredFence monitorFence = {0};

    *getCreateSynchronizationObject2FailCallFcn() = true;
    bool ret = wddmMockInterface->createMonitoredFence(monitorFence);
    EXPECT_FALSE(ret);

    EXPECT_EQ(0u, monitorFence.fenceHandle);
    void *retAddress = reinterpret_cast<void *>(0);
    EXPECT_EQ(retAddress, monitorFence.cpuAddress);
}

TEST_F(Wddm20WithMockGdiDllTests, WhenDestroyingSeparateMonitorFenceThenExpectGdiCalled) {
    MonitoredFence monitorFence = {0};
    monitorFence.fenceHandle = 10u;

    wddmMockInterface->destroyMonitorFence(monitorFence);

    EXPECT_EQ(monitorFence.fenceHandle, getDestroySynchronizationObjectDataFcn()->hSyncObject);
}
namespace NEO {
long __stdcall notifyAubCapture(void *csrHandle, uint64_t gfxAddress, size_t gfxSize, bool allocate);
}

TEST_F(Wddm20WithMockGdiDllTests, whenSetDeviceInfoSucceedsThenDeviceCallbacksArePassedToGmmMemory) {
    GMM_DEVICE_CALLBACKS_INT expectedDeviceCb{};
    wddm->init();
    auto gdi = wddm->getGdi();
    auto gmmMemory = static_cast<MockGmmMemory *>(wddm->getGmmMemory());

    expectedDeviceCb.Adapter.KmtHandle = wddm->getAdapter();
    expectedDeviceCb.hDevice.KmtHandle = wddm->getDevice();
    expectedDeviceCb.hCsr = nullptr;
    expectedDeviceCb.PagingQueue = wddm->getPagingQueue();
    expectedDeviceCb.PagingFence = wddm->getPagingQueueSyncObject();

    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnAllocate = gdi->createAllocation;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate = gdi->destroyAllocation;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnMapGPUVA = gdi->mapGpuVirtualAddress;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnMakeResident = gdi->makeResident;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnEvict = gdi->evict;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnReserveGPUVA = gdi->reserveGpuVirtualAddress;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnUpdateGPUVA = gdi->updateGpuVirtualAddress;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnWaitFromCpu = gdi->waitForSynchronizationObjectFromCpu;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnLock = gdi->lock2;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnUnLock = gdi->unlock2;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnEscape = gdi->escape;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnFreeGPUVA = gdi->freeGpuVirtualAddress;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnNotifyAubCapture = notifyAubCapture;

    EXPECT_TRUE(memcmp(&expectedDeviceCb, &gmmMemory->deviceCallbacks, sizeof(GMM_DEVICE_CALLBACKS_INT)) == 0);
    EXPECT_TRUE(memcmp(&expectedDeviceCb.Adapter, &gmmMemory->deviceCallbacks.Adapter, sizeof(GMM_HANDLE_EXT)) == 0);
    EXPECT_TRUE(memcmp(&expectedDeviceCb.hDevice, &gmmMemory->deviceCallbacks.hDevice, sizeof(GMM_HANDLE_EXT)) == 0);
    EXPECT_TRUE(memcmp(&expectedDeviceCb.DevCbPtrs.KmtCbPtrs, &gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs, sizeof(GMM_DEVICE_CB_PTRS::KmtCbPtrs)) == 0);
}

TEST_F(Wddm20WithMockGdiDllTests, whenSetDeviceInfoFailsThenDeviceIsNotConfigured) {

    auto gmockGmmMemory = new ::testing::NiceMock<GmockGmmMemory>(rootDeviceEnvironment->getGmmClientContext());
    ON_CALL(*gmockGmmMemory, setDeviceInfo(::testing::_))
        .WillByDefault(::testing::Return(false));
    EXPECT_CALL(*gmockGmmMemory, configureDeviceAddressSpace(::testing::_,
                                                             ::testing::_,
                                                             ::testing::_,
                                                             ::testing::_,
                                                             ::testing::_))
        .Times(0);

    wddm->gmmMemory.reset(gmockGmmMemory);

    wddm->init();
}

HWTEST_F(Wddm20WithMockGdiDllTests, givenNonGen12LPPlatformWhenConfigureDeviceAddressSpaceThenDontObtainMinAddress) {
    if (platformDevices[0]->platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    auto gmmMemory = new ::testing::NiceMock<GmockGmmMemory>(rootDeviceEnvironment->getGmmClientContext());
    wddm->gmmMemory.reset(gmmMemory);
    ON_CALL(*gmmMemory, configureDeviceAddressSpace(::testing::_,
                                                    ::testing::_,
                                                    ::testing::_,
                                                    ::testing::_,
                                                    ::testing::_))
        .WillByDefault(::testing::Return(true));

    EXPECT_CALL(*gmmMemory,
                getInternalGpuVaRangeLimit())
        .Times(0);

    wddm->init();

    EXPECT_EQ(NEO::windowsMinAddress, wddm->getWddmMinAddress());
}

struct GdiWithMockedCloseFunc : public Gdi {
    GdiWithMockedCloseFunc() : Gdi() {
        closeAdapter = mockCloseAdapter;
        GdiWithMockedCloseFunc::closeAdapterCalled = 0u;
        GdiWithMockedCloseFunc::closeAdapterCalledArgPassed = 0u;
    }
    static NTSTATUS __stdcall mockCloseAdapter(IN CONST D3DKMT_CLOSEADAPTER *adapter) {
        closeAdapterCalled++;
        closeAdapterCalledArgPassed = adapter->hAdapter;
        return STATUS_SUCCESS;
    }
    static uint32_t closeAdapterCalled;
    static D3DKMT_HANDLE closeAdapterCalledArgPassed;
};

uint32_t GdiWithMockedCloseFunc::closeAdapterCalled;
D3DKMT_HANDLE GdiWithMockedCloseFunc::closeAdapterCalledArgPassed;
TEST(HwDeviceId, whenHwDeviceIdIsDestroyedThenAdapterIsClosed) {
    auto gdi = std::make_unique<GdiWithMockedCloseFunc>();
    auto osEnv = std::make_unique<OsEnvironmentWin>();
    osEnv->gdi.reset(gdi.release());

    D3DKMT_HANDLE adapter = 0x1234;
    {
        HwDeviceId hwDeviceId{adapter, {}, osEnv.get()};
    }
    EXPECT_EQ(1u, GdiWithMockedCloseFunc::closeAdapterCalled);
    EXPECT_EQ(adapter, GdiWithMockedCloseFunc::closeAdapterCalledArgPassed);
}

TEST_F(WddmTest, WhenResidencyLoggingEnabledThenExpectLoggerCreated) {
    NEO::ResLog::mockFopenCalled = 0;
    NEO::ResLog::mockVfptrinfCalled = 0;
    NEO::ResLog::mockFcloseCalled = 0;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.WddmResidencyLogger.set(true);

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    wddm->residencyLogger.reset();
    if (NEO::residencyLoggingAvailable) {
        EXPECT_EQ(1u, NEO::ResLog::mockFopenCalled);
        EXPECT_EQ(1u, NEO::ResLog::mockVfptrinfCalled);
        EXPECT_EQ(1u, NEO::ResLog::mockFcloseCalled);
    }
}

TEST_F(WddmTest, GivenResidencyLoggingEnabledWhenMakeResidentSuccessThenExpectSizeRapport) {
    if (!NEO::residencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::ResLog::mockFopenCalled = 0;
    NEO::ResLog::mockVfptrinfCalled = 0;
    NEO::ResLog::mockFcloseCalled = 0;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.WddmResidencyLogger.set(true);
    wddm->callBaseCreatePagingLogger = false;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = 0x10;
    uint64_t bytesToTrim = 0;
    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    //2 - one for open log, second for allocation size
    EXPECT_EQ(2u, NEO::ResLog::mockVfptrinfCalled);
    EXPECT_TRUE(logger->makeResidentCall);
}

TEST_F(WddmTest, GivenResidencyLoggingEnabledWhenMakeResidentFailThenExpectTrimReport) {
    if (!NEO::residencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::ResLog::mockFopenCalled = 0;
    NEO::ResLog::mockVfptrinfCalled = 0;
    NEO::ResLog::mockFcloseCalled = 0;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.WddmResidencyLogger.set(true);
    wddm->callBaseCreatePagingLogger = false;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = static_cast<D3DKMT_HANDLE>(-1);
    uint64_t bytesToTrim = 0;

    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    //3 - one for open log, second for report allocations, 3rd for trim size
    EXPECT_EQ(3u, NEO::ResLog::mockVfptrinfCalled);
    EXPECT_FALSE(logger->makeResidentCall);
}

TEST_F(WddmTest, GivenResidencyLoggingEnabledWhenEnterWaitCalledThenExpectInternalFlagOn) {
    if (!NEO::residencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::ResLog::mockFopenCalled = 0;
    NEO::ResLog::mockVfptrinfCalled = 0;
    NEO::ResLog::mockFcloseCalled = 0;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.WddmResidencyLogger.set(true);
    wddm->callBaseCreatePagingLogger = false;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());
    logger->enteredWait();
    EXPECT_TRUE(logger->enterWait);
}

TEST_F(WddmTest, GivenResidencyLoggingEnabledWhenMakeResidentAndWaitPagingThenExpectFlagsOff) {
    if (!NEO::residencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::ResLog::mockFopenCalled = 0;
    NEO::ResLog::mockVfptrinfCalled = 0;
    NEO::ResLog::mockFcloseCalled = 0;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.WddmResidencyLogger.set(true);
    wddm->callBaseCreatePagingLogger = false;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = 0x10;
    uint64_t bytesToTrim = 0;
    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    //2 - one for open log, second for allocation size
    EXPECT_EQ(2u, NEO::ResLog::mockVfptrinfCalled);
    EXPECT_TRUE(logger->makeResidentCall);

    logger->enterWait = true;
    wddm->waitOnPagingFenceFromCpu();
    EXPECT_EQ(4u, NEO::ResLog::mockVfptrinfCalled);
    EXPECT_FALSE(logger->makeResidentCall);
    EXPECT_FALSE(logger->enterWait);
}

TEST(DiscoverDevices, whenDriverInfoHasIncompatibleDriverStoreThenHwDeviceIdIsNotCreated) {

    class MockRegistryReader : public SettingsReader {
      public:
        std::string getSetting(const char *settingName, const std::string &value) override {
            std::string key(settingName);
            if (key == "DriverStorePathForComputeRuntime") {
                return driverStorePath;
            }
            return value;
        }

        bool getSetting(const char *settingName, bool defaultValue) override { return defaultValue; };
        int32_t getSetting(const char *settingName, int32_t defaultValue) override { return defaultValue; };
        const char *appSpecificLocation(const std::string &name) override { return name.c_str(); };

        std::string driverStorePath = "driverStore\\0x8086";
    };
    VariableBackup<decltype(DriverInfoWindows::createRegistryReaderFunc)> createFuncBackup{&DriverInfoWindows::createRegistryReaderFunc};
    DriverInfoWindows::createRegistryReaderFunc = [](const std::string &) -> std::unique_ptr<SettingsReader> {
        return std::make_unique<MockRegistryReader>();
    };
    VariableBackup<const wchar_t *> currentLibraryPathBackup(&SysCalls::currentLibraryPath);
    currentLibraryPathBackup = L"driverStore\\different_driverStore\\myLib.dll";
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_TRUE(hwDeviceIds.empty());
}
