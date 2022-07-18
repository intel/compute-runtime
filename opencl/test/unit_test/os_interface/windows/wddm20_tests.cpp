/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_engine_mapper.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_wddm_residency_logger.h"
#include "shared/test/common/os_interface/windows/ult_dxcore_factory.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"

#include "opencl/test/unit_test/os_interface/windows/mock_wddm_allocation.h"

#include "gtest/gtest.h"
#include "mock_gmm_memory.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

namespace NEO {
namespace SysCalls {
extern const wchar_t *currentLibraryPath;
}
extern uint32_t numRootDevicesToEnum;
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid);
} // namespace NEO

using namespace NEO;

namespace GmmHelperFunctions {
Gmm *getGmm(void *ptr, size_t size, GmmHelper *gmmHelper) {
    size_t alignedSize = alignSizeWholePage(ptr, size);
    void *alignedPtr = alignUp(ptr, 4096);

    Gmm *gmm = new Gmm(gmmHelper, alignedPtr, alignedSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
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

TEST_F(Wddm20Tests, GivenExisitingContextWhenInitializingWddmThenCreateContextResultCalledIsStillOne) {
    EXPECT_EQ(1u, wddm->createContextResult.called);
    wddm->init();
    EXPECT_EQ(1u, wddm->createContextResult.called);
}

TEST_F(Wddm20Tests, givenNullPageTableManagerAndCompressedResourceWhenMappingGpuVaThenDontUpdateAuxTable) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    void *fakePtr = reinterpret_cast<void *>(0x100);
    auto gmmHelper = getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(fakePtr)));
    WddmAllocation allocation(0, AllocationType::UNKNOWN, fakePtr, canonizedAddress, 0x2100, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());
    allocation.getHandleToModify(0u) = ALLOCATION_HANDLE;

    EXPECT_TRUE(wddm->mapGpuVirtualAddress(&allocation));
}

TEST(WddmDiscoverDevices, WhenNoHwDeviceIdIsProvidedToWddmThenWddmIsNotCreated) {
    struct MockWddm : public Wddm {
        MockWddm(std::unique_ptr<HwDeviceIdWddm> &&hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment) : Wddm(std::move(hwDeviceIdIn), rootDeviceEnvironment) {}
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

TEST(WddmDiscoverDevices, givenMultipleRootDevicesExposedWhenCreateMultipleRootDevicesFlagIsSetToLowerValueThenDiscoverOnlySpecifiedNumberOfDevices) {
    DebugManagerStateRestore restorer{};
    VariableBackup<uint32_t> backup{&numRootDevicesToEnum};
    numRootDevicesToEnum = 3u;
    uint32_t requestedNumRootDevices = 2u;

    DebugManager.flags.CreateMultipleRootDevices.set(requestedNumRootDevices);
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(requestedNumRootDevices, hwDeviceIds.size());
}

TEST(WddmDiscoverDevices, givenInvalidFirstAdapterWhenDiscoveringAdaptersThenReturnAllValidAdapters) {
    VariableBackup<uint32_t> backup{&numRootDevicesToEnum, 2u};
    VariableBackup<bool> backup2{&UltDXCoreAdapterList::firstInvalid, true};

    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
}

TEST(WddmDiscoverDevices, givenMultipleRootDevicesExposedWhenCreateMultipleRootDevicesFlagIsSetToGreaterValueThenDiscoverSpecifiedNumberOfDevices) {
    DebugManagerStateRestore restorer{};
    VariableBackup<uint32_t> backup{&numRootDevicesToEnum};
    numRootDevicesToEnum = 3u;
    uint32_t requestedNumRootDevices = 4u;

    DebugManager.flags.CreateMultipleRootDevices.set(requestedNumRootDevices);
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(requestedNumRootDevices, hwDeviceIds.size());
}

TEST(WddmDiscoverDevices, WhenAdapterDescriptionContainsVirtualRenderThenAdapterIsDiscovered) {
    VariableBackup<const char *> descriptionBackup(&UltDxCoreAdapter::description);
    descriptionBackup = "Virtual Render";
    ExecutionEnvironment executionEnvironment;

    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
}

TEST(Wddm20EnumAdaptersTest, WhenInitializingWddmThenHardwareInfoIsCorrectlyPopulated) {

    const HardwareInfo *hwInfo = defaultHwInfo.get();
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo->platform,
                                                         &hwInfo->gtSystemInfo,
                                                         hwInfo->capabilityTable.gpuAddressSpace));

    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = Wddm::createWddm(nullptr, rootDeviceEnvironment);
    bool success = wddm->init();

    EXPECT_TRUE(success);

    EXPECT_EQ(rootDeviceEnvironment.getHardwareInfo()->platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);
}

TEST(Wddm20EnumAdaptersTest, givenEmptyHardwareInfoWhenEnumAdapterIsCalledThenCapabilityTableIsSet) {
    const HardwareInfo *hwInfo = defaultHwInfo.get();
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo->platform,
                                                         &hwInfo->gtSystemInfo,
                                                         hwInfo->capabilityTable.gpuAddressSpace));

    ExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    auto wddm = Wddm::createWddm(nullptr, *rootDeviceEnvironment);
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
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.eProductFamily = IGFX_UNKNOWN;
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo.platform,
                                                         &hwInfo.gtSystemInfo,
                                                         hwInfo.capabilityTable.gpuAddressSpace));

    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = Wddm::createWddm(nullptr, rootDeviceEnvironment);
    auto ret = wddm->init();
    EXPECT_FALSE(ret);

    // reset mock gdi
    hwInfo = *defaultHwInfo;
    mockGdiDll.reset(setAdapterInfo(&hwInfo.platform,
                                    &hwInfo.gtSystemInfo,
                                    hwInfo.capabilityTable.gpuAddressSpace));
}

TEST_F(Wddm20Tests, whenInitializeWddmThenContextIsCreated) {
    auto context = osContext->getWddmContextHandle();
    EXPECT_TRUE(context != static_cast<D3DKMT_HANDLE>(0));
}

TEST_F(Wddm20Tests, whenCreatingContextWithPowerHintSuccessIsReturned) {
    auto newContext = osContext.get();
    newContext->setUmdPowerHintValue(1);
    EXPECT_EQ(1, newContext->getUmdPowerHintValue());
    wddm->createContext(*newContext);
    EXPECT_TRUE(wddm->createContext(*newContext));
}

TEST_F(Wddm20Tests, whenInitPrivateDataThenDefaultValuesAreSet) {
    auto newContext = osContext.get();
    CREATECONTEXT_PVTDATA PrivateData = initPrivateData(*newContext);
    EXPECT_FALSE(PrivateData.IsProtectedProcess);
    EXPECT_FALSE(PrivateData.IsDwm);
    EXPECT_TRUE(PrivateData.GpuVAContext);
    EXPECT_FALSE(PrivateData.IsMediaUsage);
}

TEST_F(Wddm20Tests, WhenCreatingAllocationAndDestroyingAllocationThenCorrectResultReturned) {
    OsAgnosticMemoryManager mm(*executionEnvironment);
    auto gmmHelper = getGmmHelper();
    auto ptr = mm.allocateSystemMemory(100, 0);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, AllocationType::UNKNOWN, ptr, canonizedAddress, 100, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), getGmmHelper());

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

    auto gmmHelper = getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, AllocationType::UNKNOWN, ptr, canonizedAddress, 0x2100, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize(), getGmmHelper());

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
    auto gmmHelper = getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(fakePtr)));
    WddmAllocation allocation(0, AllocationType::UNKNOWN, fakePtr, canonizedAddress, 100, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    std::unique_ptr<Gmm> gmm(GmmHelperFunctions::getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize(), getGmmHelper()));

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
    std::unique_ptr<Gmm> gmm(GmmHelperFunctions::getGmm(alignedPtr, alignedSize, getGmmHelper()));
    uint64_t gpuAddress = 0u;
    auto heapBase = wddm->getGfxPartition().Heap32[static_cast<uint32_t>(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)].Base;
    auto heapLimit = wddm->getGfxPartition().Heap32[static_cast<uint32_t>(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)].Limit;

    bool ret = wddm->mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, heapBase, heapLimit, 0u, gpuAddress);
    EXPECT_TRUE(ret);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto cannonizedHeapBase = gmmHelper->canonize(heapBase);
    auto cannonizedHeapEnd = gmmHelper->canonize(heapLimit);

    EXPECT_GE(gpuAddress, cannonizedHeapBase);
    EXPECT_LE(gpuAddress, cannonizedHeapEnd);
}

TEST_F(Wddm20WithMockGdiDllTests, GivenInvalidCpuAddressWhenCheckingForGpuHangThenFalseIsReturned) {
    osContext->getResidencyController().getMonitoredFence().cpuAddress = nullptr;
    EXPECT_FALSE(wddm->isGpuHangDetected(*osContext));
}

TEST_F(Wddm20WithMockGdiDllTests, GivenCpuValueDifferentThanGpuHangIndicationWhenCheckingForGpuHangThenFalseIsReturned) {
    constexpr auto cpuValue{777u};
    ASSERT_NE(NEO::Wddm::gpuHangIndication, cpuValue);

    *osContext->getResidencyController().getMonitoredFence().cpuAddress = cpuValue;
    EXPECT_FALSE(wddm->isGpuHangDetected(*osContext));
}

TEST_F(Wddm20WithMockGdiDllTests, GivenGpuHangIndicationWhenCheckingForGpuHangThenTrueIsReturned) {
    *osContext->getResidencyController().getMonitoredFence().cpuAddress = NEO::Wddm::gpuHangIndication;
    EXPECT_TRUE(wddm->isGpuHangDetected(*osContext));
}

TEST_F(Wddm20WithMockGdiDllTests, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
    OsHandleStorage storage;
    OsHandleWin osHandle1;
    OsHandleWin osHandle2;
    OsHandleWin osHandle3;

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

TEST_F(Wddm20Tests, WhenMappingAndFreeingGpuVaThenReturnIsCorrect) {
    OsAgnosticMemoryManager mm(*executionEnvironment);
    auto gmmHelper = getGmmHelper();
    auto ptr = mm.allocateSystemMemory(100, 0);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, AllocationType::UNKNOWN, ptr, canonizedAddress, 100, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), getGmmHelper());

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

    WddmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, 0, 100, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), getGmmHelper()));

    allocation.setDefaultGmm(gmm.get());
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);

    bool ret = wddm->mapGpuVirtualAddress(&allocation);
    EXPECT_TRUE(ret);

    EXPECT_NE(0u, allocation.getGpuAddress());

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    EXPECT_EQ(allocation.getGpuAddress(), gmmHelper->canonize(allocation.getGpuAddress()));

    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(WddmTestWithMockGdiDll, givenShareableAllocationWhenCreateThenCreateResourceFlagIsEnabled) {
    init();
    WddmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, true, 1u);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(nullptr, MemoryConstants::pageSize, getGmmHelper()));
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
    WddmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, true, 1u);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(nullptr, MemoryConstants::pageSize, getGmmHelper()));
    allocation.setDefaultGmm(gmm.get());
    auto status = memoryManager.createGpuAllocationsWithRetry(&allocation);
    EXPECT_TRUE(status);
    EXPECT_NE(0u, allocation.peekInternalHandle(&memoryManager));
}

TEST(WddmAllocationTest, whenAllocationIsShareableThenSharedHandleToModifyIsSharedHandleOfAllocation) {
    WddmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, true, 1u);
    auto sharedHandleToModify = allocation.getSharedHandleToModify();
    EXPECT_NE(nullptr, sharedHandleToModify);
    *sharedHandleToModify = 1234u;
    EXPECT_EQ(*sharedHandleToModify, allocation.peekInternalHandle(nullptr));
}

TEST(WddmAllocationTest, whenAllocationIsNotShareableThenItDoesntReturnSharedHandleToModify) {
    WddmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, false, 1u);
    auto sharedHandleToModify = allocation.getSharedHandleToModify();
    EXPECT_EQ(nullptr, sharedHandleToModify);
}

TEST_F(Wddm20Tests, WhenMakingResidentAndEvictingThenReturnIsCorrect) {
    OsAgnosticMemoryManager mm(*executionEnvironment);
    auto gmmHelper = getGmmHelper();
    auto ptr = mm.allocateSystemMemory(100, 0);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, AllocationType::UNKNOWN, ptr, canonizedAddress, 100, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), getGmmHelper());

    allocation.setDefaultGmm(gmm);
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(allocation.getDefaultHandle() != 0);

    auto error = wddm->mapGpuVirtualAddress(&allocation);
    EXPECT_TRUE(error);
    EXPECT_TRUE(allocation.getGpuAddress() != 0);

    error = wddm->makeResident(&allocation.getHandles()[0], allocation.getNumGmms(), false, nullptr, allocation.getAlignedSize());
    EXPECT_TRUE(error);

    uint64_t sizeToTrim;
    error = wddm->evict(&allocation.getHandles()[0], allocation.getNumGmms(), sizeToTrim, true);
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
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    MemoryManagerCreate<WddmMemoryManager> mm(false, false, *executionEnvironment);
    AllocationProperties properties(0, false, 4096u, AllocationType::SHARED_BUFFER, false, {});

    auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(ALLOCATION_HANDLE, properties, false, false);
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

TEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromMultipleSharedHandlesIsCalledThenNullptrIsReturned) {
    void *pSysMem = (void *)0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    MemoryManagerCreate<WddmMemoryManager> mm(false, false, *executionEnvironment);
    AllocationProperties properties(0, false, 4096u, AllocationType::SHARED_BUFFER, false, {});

    std::vector<osHandle> handles{ALLOCATION_HANDLE};
    auto graphicsAllocation = mm.createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false);
    ASSERT_EQ(nullptr, graphicsAllocation);
}

TEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenMapGpuVaWithCpuPtrDepensOnBitness) {
    void *pSysMem = (void *)0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);
    EXPECT_EQ(0u, status);

    MemoryManagerCreate<WddmMemoryManager> mm(false, false, *executionEnvironment);
    AllocationProperties properties(0, false, 4096, AllocationType::SHARED_BUFFER, false, {});

    auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(ALLOCATION_HANDLE, properties, false, false);
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

HWTEST_F(Wddm20InstrumentationTest, WhenConfiguringDeviceAddressSpaceThenTrueIsReturned) {
    SYSTEM_INFO sysInfo = {};
    WddmMock::getSystemInfo(&sysInfo);

    D3DKMT_HANDLE adapterHandle = ADAPTER_HANDLE;
    D3DKMT_HANDLE deviceHandle = DEVICE_HANDLE;
    const HardwareInfo hwInfo = *defaultHwInfo;
    BOOLEAN FtrL3IACoherency = hwInfo.featureTable.flags.ftrL3IACoherency ? 1 : 0;
    uintptr_t maxAddr = hwInfo.capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress
                            ? reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress) + 1
                            : 0;

    wddm->init();
    EXPECT_EQ(1u, gmmMem->configureDeviceAddressSpaceCalled);
    EXPECT_EQ(adapterHandle, gmmMem->configureDeviceAddressSpaceParamsPassed[0].hAdapter);
    EXPECT_EQ(deviceHandle, gmmMem->configureDeviceAddressSpaceParamsPassed[0].hDevice);
    EXPECT_EQ(wddm->getGdi()->escape.mFunc, gmmMem->configureDeviceAddressSpaceParamsPassed[0].pfnEscape);
    EXPECT_EQ(maxAddr, gmmMem->configureDeviceAddressSpaceParamsPassed[0].svmSize);
    EXPECT_EQ(FtrL3IACoherency, gmmMem->configureDeviceAddressSpaceParamsPassed[0].bdwL3Coherency);
}

TEST_F(Wddm20InstrumentationTest, GivenNoAdapterWhenConfiguringDeviceAddressSpaceThenFalseIsReturned) {
    auto gdi = std::make_unique<Gdi>();
    wddm->resetGdi(gdi.release());

    auto ret = wddm->configureDeviceAddressSpace();

    EXPECT_FALSE(ret);
    EXPECT_EQ(0u, gmmMem->configureDeviceAddressSpaceCalled);
}

TEST_F(Wddm20InstrumentationTest, GivenNoDeviceWhenConfiguringDeviceAddressSpaceThenFalseIsReturned) {
    wddm->device = static_cast<D3DKMT_HANDLE>(0);

    auto ret = wddm->configureDeviceAddressSpace();

    EXPECT_FALSE(ret);
    EXPECT_EQ(0u, gmmMem->configureDeviceAddressSpaceCalled);
}

TEST_F(Wddm20InstrumentationTest, GivenNoEscFuncWhenConfiguringDeviceAddressSpaceThenFalseIsReturned) {
    wddm->getGdi()->escape = static_cast<PFND3DKMT_ESCAPE>(nullptr);

    auto ret = wddm->configureDeviceAddressSpace();

    EXPECT_FALSE(ret);
    EXPECT_EQ(0u, gmmMem->configureDeviceAddressSpaceCalled);
}

TEST_F(Wddm20Tests, WhenGettingMaxApplicationAddressThen32Or64BitIsCorrectlyReturned) {
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

struct WddmContextSchedulingPriorityTests : public Wddm20WithMockGdiDllTestsWithoutWddmInit {
    void initContext(bool lowPriority) {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.release());
        wddm->init();
        wddm->wddmInterface.reset(wddmMockInterface);

        auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
        auto engine = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0];

        auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode);
        engineDescriptor.engineTypeUsage.second = lowPriority ? EngineUsage::LowPriority : EngineUsage::Regular;
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0u, engineDescriptor);
        osContext->ensureContextInitialized();
    }
};

TEST_F(WddmContextSchedulingPriorityTests, givenLowPriorityContextWhenInitializingThenCallSetPriority) {
    initContext(true);

    auto createContextParams = this->getSetContextSchedulingPriorityDataCallFcn();

    EXPECT_EQ(osContext->getWddmContextHandle(), createContextParams->hContext);
    EXPECT_EQ(1, createContextParams->Priority);
}

TEST_F(WddmContextSchedulingPriorityTests, givenLowPriorityContextWhenFailingDuringSetSchedulingPriorityThenThrow) {
    *this->getFailOnSetContextSchedulingPriorityCallFcn() = true;

    EXPECT_ANY_THROW(initContext(true));
}

TEST_F(WddmContextSchedulingPriorityTests, givenDebugFlagSetWhenInitializingLowPriorityContextThenSetPriorityValue) {
    DebugManagerStateRestore dbgRestore;

    constexpr int32_t newPriority = 3;

    DebugManager.flags.ForceWddmLowPriorityContextValue.set(newPriority);

    initContext(true);

    auto createContextParams = this->getSetContextSchedulingPriorityDataCallFcn();

    EXPECT_EQ(osContext->getWddmContextHandle(), createContextParams->hContext);
    EXPECT_EQ(newPriority, createContextParams->Priority);
}

TEST_F(WddmContextSchedulingPriorityTests, givenRegularContextWhenInitializingThenDontCallSetPriority) {
    initContext(false);

    auto createContextParams = this->getSetContextSchedulingPriorityDataCallFcn();

    EXPECT_EQ(0, createContextParams->hContext);
    EXPECT_EQ(0, createContextParams->Priority);
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenCreateContextCallWhenDriverHintsThenItPointsToOpenCL) {
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
    UINT expected = WddmEngineMapper::engineNodeMap(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0].first);
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

TEST_F(Wddm20Tests, GivenMultipleHandlesWhenMakingResidentThenAllocationListIsCorrect) {
    D3DKMT_HANDLE handles[2] = {ALLOCATION_HANDLE, ALLOCATION_HANDLE};
    gdi->getMakeResidentArg().NumAllocations = 0;
    gdi->getMakeResidentArg().AllocationList = nullptr;
    wddm->callBaseMakeResident = true;

    bool error = wddm->makeResident(handles, 2, false, nullptr, 0x1000);
    EXPECT_TRUE(error);

    EXPECT_EQ(2u, gdi->getMakeResidentArg().NumAllocations);
    EXPECT_EQ(handles, gdi->getMakeResidentArg().AllocationList);
}

TEST_F(Wddm20Tests, GivenMultipleHandlesWhenMakingResidentThenBytesToTrimIsCorrect) {
    D3DKMT_HANDLE handles[2] = {ALLOCATION_HANDLE, ALLOCATION_HANDLE};

    gdi->getMakeResidentArg().NumAllocations = 0;
    gdi->getMakeResidentArg().AllocationList = nullptr;
    gdi->getMakeResidentArg().NumBytesToTrim = 30;
    wddm->callBaseMakeResident = true;

    uint64_t bytesToTrim = 0;
    bool success = wddm->makeResident(handles, 2, false, &bytesToTrim, 0x1000);
    EXPECT_TRUE(success);

    EXPECT_EQ(gdi->getMakeResidentArg().NumBytesToTrim, bytesToTrim);
}

TEST_F(Wddm20Tests, WhenMakingNonResidentAndEvictNotNeededThenEvictIsCalledWithProperFlagSet) {
    D3DKMT_HANDLE handle = (D3DKMT_HANDLE)0x1234;

    gdi->getEvictArg().AllocationList = nullptr;
    gdi->getEvictArg().Flags.Value = 0;
    gdi->getEvictArg().hDevice = 0;
    gdi->getEvictArg().NumAllocations = 0;
    gdi->getEvictArg().NumBytesToTrim = 20;
    wddm->callBaseEvict = true;

    uint64_t sizeToTrim = 10;
    wddm->evict(&handle, 1, sizeToTrim, false);

    EXPECT_EQ(1u, gdi->getEvictArg().NumAllocations);
    EXPECT_EQ(&handle, gdi->getEvictArg().AllocationList);
    EXPECT_EQ(wddm->getDeviceHandle(), gdi->getEvictArg().hDevice);
    EXPECT_EQ(0u, gdi->getEvictArg().NumBytesToTrim);
    EXPECT_EQ(1u, gdi->getEvictArg().Flags.EvictOnlyIfNecessary);
}

TEST_F(Wddm20Tests, WhenMakingNonResidentAndEvictNeededThenEvictIsCalledWithProperFlagSet) {
    D3DKMT_HANDLE handle = (D3DKMT_HANDLE)0x1234;

    gdi->getEvictArg().AllocationList = nullptr;
    gdi->getEvictArg().Flags.Value = 0;
    gdi->getEvictArg().hDevice = 0;
    gdi->getEvictArg().NumAllocations = 0;
    gdi->getEvictArg().NumBytesToTrim = 20;
    wddm->callBaseEvict = true;

    uint64_t sizeToTrim = 10;
    wddm->evict(&handle, 1, sizeToTrim, true);

    EXPECT_EQ(1u, gdi->getEvictArg().NumAllocations);
    EXPECT_EQ(&handle, gdi->getEvictArg().AllocationList);
    EXPECT_EQ(wddm->getDeviceHandle(), gdi->getEvictArg().hDevice);
    EXPECT_EQ(0u, gdi->getEvictArg().NumBytesToTrim);
    EXPECT_EQ(0u, gdi->getEvictArg().Flags.EvictOnlyIfNecessary);
}

TEST_F(Wddm20Tests, givenDestroyAllocationWhenItIsCalledThenAllocationIsPassedToDestroyAllocation) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
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

    EXPECT_EQ(wddm->getDeviceHandle(), gdi->getDestroyArg().hDevice);
    EXPECT_EQ(1u, gdi->getDestroyArg().AllocationCount);
    EXPECT_NE(nullptr, gdi->getDestroyArg().phAllocationList);
}

TEST_F(Wddm20Tests, WhenLastFenceLessEqualThanMonitoredThenWaitFromCpuIsNotCalled) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
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
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
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
    EXPECT_EQ((D3DKMT_HANDLE)wddm->getDeviceHandle(), gdi->getWaitFromCpuArg().hDevice);
    EXPECT_EQ(1u, gdi->getWaitFromCpuArg().ObjectCount);
    EXPECT_NE(nullptr, gdi->getWaitFromCpuArg().ObjectHandleArray);
}

TEST_F(Wddm20Tests, WhenCreatingMonitoredFenceThenItIsInitializedWithFenceValueZeroAndCurrentFenceValueIsSetToOne) {
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
        static NTSTATUS APIENTRY mockCreateAllocation2(D3DKMT_CREATEALLOCATION *param) {
            return STATUS_GRAPHICS_NO_VIDEO_MEMORY;
        };
    };
    gdi->createAllocation2 = FailingCreateAllocation::mockCreateAllocation2;

    void *fakePtr = reinterpret_cast<void *>(0x123);
    auto gmmHelper = getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(fakePtr)));
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), fakePtr, 100, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, StorageInfo{}, true);
    WddmAllocation allocation(0, AllocationType::UNKNOWN, fakePtr, canonizedAddress, 100, nullptr, MemoryPool::MemoryNull, 0u, 1u);
    allocation.setDefaultGmm(gmm.get());

    EXPECT_FALSE(wddm->createAllocation64k(&allocation));
}

TEST_F(Wddm20Tests, givenReadOnlyMemoryWhenCreateAllocationFailsWithNoVideoMemoryThenCorrectStatusIsReturned) {
    class MockCreateAllocation {
      public:
        static NTSTATUS APIENTRY mockCreateAllocation2(D3DKMT_CREATEALLOCATION *param) {
            return STATUS_GRAPHICS_NO_VIDEO_MEMORY;
        };
    };
    gdi->createAllocation2 = MockCreateAllocation::mockCreateAllocation2;

    OsHandleStorage handleStorage;
    OsHandleWin handle;
    auto maxOsContextCount = 1u;
    ResidencyData residency(maxOsContextCount);

    handleStorage.fragmentCount = 1;
    handleStorage.fragmentStorageData[0].cpuPtr = (void *)0x1000;
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;
    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle;
    handleStorage.fragmentStorageData[0].residency = &residency;
    handle.gmm = GmmHelperFunctions::getGmm(nullptr, 0, getGmmHelper());

    NTSTATUS result = wddm->createAllocationsAndMapGpuVa(handleStorage);

    EXPECT_EQ(STATUS_GRAPHICS_NO_VIDEO_MEMORY, result);

    delete handle.gmm;
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
    EXPECT_EQ(1u, wddm->makeResidentResult.called);
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
    wddm->callBaseMakeResident = true;
    wddm->mockPagingFence = 0u;
    wddm->temporaryResources->makeResidentResource(ALLOCATION_HANDLE, 0x1000);
    UINT64 expectedCallNumber = NEO::wddmResidencyLoggingAvailable ? MockGdi::pagingFenceReturnValue + 1 : 0ull;
    EXPECT_EQ(1u, wddm->makeResidentResult.called);
    EXPECT_EQ(MockGdi::pagingFenceReturnValue + 1, wddm->mockPagingFence);
    EXPECT_EQ(expectedCallNumber, wddm->getPagingFenceAddressResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, givenAllocationWhenApplyBlockingMakeResidentAndMakeResidentCallFailsThenEvictTemporaryResourcesAndRetry) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    allocation.handle = 0x3;
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    mockWddm.makeResidentStatus = false;
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(1u, mockTemporaryResources->evictAllResourcesResult.called);
    EXPECT_EQ(allocation.handle, mockWddm.makeResidentResult.handlePack[0]);
    EXPECT_EQ(2u, mockWddm.makeResidentResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, whenApplyBlockingMakeResidentAndTemporaryResourcesAreEvictedSuccessfullyThenCallMakeResidentOneMoreTime) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    allocation.handle = 0x3;
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    mockWddm.makeResidentStatus = false;
    mockWddm.callBaseEvict = true;
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(allocation.handle);
    mockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(2u, mockTemporaryResources->evictAllResourcesResult.called);
    EXPECT_EQ(1u, mockWddm.evictResult.called);
    EXPECT_EQ(0u, gdi->getEvictArg().Flags.EvictOnlyIfNecessary);
    EXPECT_EQ(allocation.handle, mockWddm.makeResidentResult.handlePack[0]);
    EXPECT_EQ(3u, mockWddm.makeResidentResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, whenApplyBlockingMakeResidentAndMakeResidentStillFailsThenDontStoreTemporaryResource) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    allocation.handle = 0x2;
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    mockWddm.makeResidentStatus = false;
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(0x1);
    EXPECT_EQ(1u, mockTemporaryResources->resourceHandles.size());
    mockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(0u, mockTemporaryResources->resourceHandles.size());
    EXPECT_EQ(1u, mockWddm.evictResult.called);
    EXPECT_EQ(allocation.handle, mockWddm.makeResidentResult.handlePack[0]);
    EXPECT_EQ(3u, mockWddm.makeResidentResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, whenApplyBlockingMakeResidentAndMakeResidentPassesAfterEvictThenStoreTemporaryResource) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    allocation.handle = 0x2;
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    mockWddm.makeResidentResults = {false, true};
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(0x1);
    EXPECT_EQ(1u, mockTemporaryResources->resourceHandles.size());
    mockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(1u, mockTemporaryResources->resourceHandles.size());
    EXPECT_EQ(0x2, mockTemporaryResources->resourceHandles.back());
    EXPECT_EQ(1u, mockWddm.evictResult.called);
    EXPECT_EQ(allocation.handle, mockWddm.makeResidentResult.handlePack[0]);
    EXPECT_EQ(2u, mockWddm.makeResidentResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, whenApplyBlockingMakeResidentAndMakeResidentPassesThenStoreTemporaryResource) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    allocation.handle = 0x2;
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(0x1);
    mockWddm.temporaryResources->makeResidentResource(allocation.handle, 0x1000);
    EXPECT_EQ(2u, mockTemporaryResources->resourceHandles.size());
    EXPECT_EQ(0x2, mockTemporaryResources->resourceHandles.back());
    EXPECT_EQ(allocation.handle, mockWddm.makeResidentResult.handlePack[0]);
    EXPECT_EQ(1u, mockWddm.makeResidentResult.called);
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
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(allocation.handle);
    mockWddm.getTemporaryResourcesContainer()->evictAllResources();
    EXPECT_EQ(1u, mockTemporaryResources->evictAllResourcesResult.called);
    EXPECT_EQ(MemoryOperationsStatus::SUCCESS, mockTemporaryResources->evictAllResourcesResult.operationSuccess);
    EXPECT_EQ(1u, mockWddm.evictResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, givenThreeAllocationsWhenEvictingAllTemporaryResourcesThenCallEvictForEachAllocationAndCleanList) {
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    constexpr uint32_t numAllocations = 3u;
    for (auto i = 0u; i < numAllocations; i++) {
        mockTemporaryResources->resourceHandles.push_back(i);
    }
    mockWddm.getTemporaryResourcesContainer()->evictAllResources();
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(1u, mockWddm.evictResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, givenThreeAllocationsWhenEvictingAllTemporaryResourcesAndOneOfThemFailsThenReturnFail) {
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0].get());
    mockWddm.evictStatus = false;
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    constexpr uint32_t numAllocations = 3u;
    for (auto i = 0u; i < numAllocations; i++) {
        mockTemporaryResources->resourceHandles.push_back(i);
    }
    mockWddm.getTemporaryResourcesContainer()->evictAllResources();
    EXPECT_EQ(MemoryOperationsStatus::FAILED, mockTemporaryResources->evictAllResourcesResult.operationSuccess);
    EXPECT_EQ(1u, mockWddm.evictResult.called);
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
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    mockWddm.evictStatus = false;
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(ALLOCATION_HANDLE);
    mockWddm.getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE);
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(MemoryOperationsStatus::FAILED, mockTemporaryResources->evictResourceResult.operationSuccess);
    EXPECT_EQ(1u, mockWddm.evictResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, whenEvictingTemporaryResourceAndEvictSucceedThenReturnSuccess) {
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(ALLOCATION_HANDLE);
    mockWddm.getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE);
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(MemoryOperationsStatus::SUCCESS, mockTemporaryResources->evictResourceResult.operationSuccess);
    EXPECT_EQ(1u, mockWddm.evictResult.called);
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
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
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

TEST_F(WddmGfxPartitionTest, WhenInitializingGfxPartitionThenAllHeapsAreInitialized) {
    MockGfxPartition gfxPartition;

    for (auto heap : MockGfxPartition::allHeapNames) {
        ASSERT_FALSE(gfxPartition.heapInitialized(heap));
    }

    wddm->initGfxPartition(gfxPartition, 0, 1, false);

    for (auto heap : MockGfxPartition::allHeapNames) {
        if (!gfxPartition.heapInitialized(heap)) {
            EXPECT_TRUE(heap == HeapIndex::HEAP_SVM || heap == HeapIndex::HEAP_STANDARD2MB || heap == HeapIndex::HEAP_EXTENDED);
        } else {
            EXPECT_TRUE(gfxPartition.heapInitialized(heap));
        }
    }
}

TEST(WddmGfxPartitionTests, WhenInitializingGfxPartitionThen64KBHeapsAreUsed) {
    struct MockWddm : public Wddm {
        using Wddm::gfxPartition;

        MockWddm(RootDeviceEnvironment &rootDeviceEnvironment)
            : Wddm(std::unique_ptr<HwDeviceIdWddm>(OSInterface::discoverDevices(rootDeviceEnvironment.executionEnvironment)[0].release()->as<HwDeviceIdWddm>()), rootDeviceEnvironment) {}
    };

    MockExecutionEnvironment executionEnvironment;
    auto wddm = new MockWddm(*executionEnvironment.rootDeviceEnvironments[0]);

    uint32_t rootDeviceIndex = 3;
    size_t numRootDevices = 5;

    MockGfxPartition gfxPartition;
    wddm->init();
    wddm->initGfxPartition(gfxPartition, rootDeviceIndex, numRootDevices, false);

    auto heapStandard64KBSize = alignDown((wddm->gfxPartition.Standard64KB.Limit - wddm->gfxPartition.Standard64KB.Base + 1) / numRootDevices, GfxPartition::heapGranularity);
    EXPECT_EQ(heapStandard64KBSize, gfxPartition.getHeapSize(HeapIndex::HEAP_STANDARD64KB));
    EXPECT_EQ(wddm->gfxPartition.Standard64KB.Base + rootDeviceIndex * heapStandard64KBSize, gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD64KB));
}

TEST(WddmGfxPartitionTests, givenGfxPartitionWhenInitializedThenInternalFrontWindowHeapIsAllocatedAtInternalHeapFront) {
    MockExecutionEnvironment executionEnvironment;
    auto wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0]);

    uint32_t rootDeviceIndex = 0;
    size_t numRootDevices = 1;

    MockGfxPartition gfxPartition;
    wddm->init();
    wddm->initGfxPartition(gfxPartition, rootDeviceIndex, numRootDevices, false);

    EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL));
    EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY));

    auto frontWindowSize = GfxPartition::internalFrontWindowPoolSize;
    EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW), frontWindowSize);
    EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW), frontWindowSize);

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW));
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW));

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL) + frontWindowSize);
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY), gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY) + frontWindowSize);

    EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::HEAP_INTERNAL),
              gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL) + gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL) - 1);
    EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY),
              gfxPartition.getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY) + gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY) - 1);
}

TEST(WddmGfxPartitionTests, givenInternalFrontWindowHeapWhenAllocatingSmallOrBigChunkThenAddressFromFrontIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    auto wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0]);

    uint32_t rootDeviceIndex = 0;
    size_t numRootDevices = 1;

    MockGfxPartition gfxPartition;
    wddm->init();
    wddm->initGfxPartition(gfxPartition, rootDeviceIndex, numRootDevices, false);

    const size_t sizeSmall = MemoryConstants::pageSize64k;
    const size_t sizeBig = static_cast<size_t>(gfxPartition.getHeapSize(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW)) - MemoryConstants::pageSize64k;

    HeapIndex heaps[] = {HeapIndex::HEAP_INTERNAL_FRONT_WINDOW,
                         HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW};

    for (int i = 0; i < 2; i++) {
        size_t sizeToAlloc = sizeSmall;
        auto address = gfxPartition.heapAllocate(heaps[i], sizeToAlloc);

        EXPECT_EQ(gfxPartition.getHeapBase(heaps[i]), address);
        gfxPartition.heapFree(heaps[i], address, sizeToAlloc);

        sizeToAlloc = sizeBig;
        address = gfxPartition.heapAllocate(heaps[i], sizeToAlloc);

        EXPECT_EQ(gfxPartition.getHeapBase(heaps[i]), address);
        gfxPartition.heapFree(heaps[i], address, sizeToAlloc);
    }
}

TEST_F(Wddm20Tests, givenWddmWhenDiscoverDevicesAndFilterDeviceIdIsTheSameAsTheExistingDeviceThenReturnTheAdapter) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.FilterDeviceId.set("1234"); // Existing device Id
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
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
    wddm->featureTable->flags.ftrWddmHwQueues = 0;
    EXPECT_EQ(WddmVersion::WDDM_2_0, wddm->getWddmVersion());
}

TEST_F(WddmTest, WhenFeatureFlagHwQueueIsEnabledThenReturnWddm23Version) {
    wddm->featureTable->flags.ftrWddmHwQueues = 1;
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
    expectedDeviceCb.hDevice.KmtHandle = wddm->getDeviceHandle();
    expectedDeviceCb.hCsr = nullptr;
    expectedDeviceCb.PagingQueue = wddm->getPagingQueue();
    expectedDeviceCb.PagingFence = wddm->getPagingQueueSyncObject();

    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnAllocate = gdi->createAllocation_;
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

    EXPECT_EQ(expectedDeviceCb.Adapter.KmtHandle, gmmMemory->deviceCallbacks.Adapter.KmtHandle);
    EXPECT_EQ(expectedDeviceCb.hDevice.KmtHandle, gmmMemory->deviceCallbacks.hDevice.KmtHandle);
    EXPECT_EQ(expectedDeviceCb.hCsr, gmmMemory->deviceCallbacks.hCsr);
    EXPECT_EQ(expectedDeviceCb.PagingQueue, gmmMemory->deviceCallbacks.PagingQueue);
    EXPECT_EQ(expectedDeviceCb.PagingFence, gmmMemory->deviceCallbacks.PagingFence);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnAllocate, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnAllocate);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnDeallocate);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnMapGPUVA, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnMapGPUVA);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnMakeResident, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnMakeResident);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnEvict, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnEvict);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnReserveGPUVA, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnReserveGPUVA);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnUpdateGPUVA, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnUpdateGPUVA);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnWaitFromCpu, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnWaitFromCpu);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnLock, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnLock);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnUnLock, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnUnLock);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnEscape, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnEscape);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnFreeGPUVA, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnFreeGPUVA);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnNotifyAubCapture, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnNotifyAubCapture);
}

TEST_F(Wddm20WithMockGdiDllTests, whenSetDeviceInfoFailsThenDeviceIsNotConfigured) {

    auto mockGmmMemory = new MockGmmMemoryBase(getGmmClientContext());
    mockGmmMemory->setDeviceInfoResult = false;

    wddm->gmmMemory.reset(mockGmmMemory);

    wddm->init();
    EXPECT_EQ(0u, mockGmmMemory->configureDeviceAddressSpaceCalled);
}

HWTEST_F(Wddm20WithMockGdiDllTests, givenNonGen12LPPlatformWhenConfigureDeviceAddressSpaceThenDontObtainMinAddress) {
    if (defaultHwInfo->platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    auto gmmMemory = new MockGmmMemoryBase(getGmmClientContext());
    wddm->gmmMemory.reset(gmmMemory);

    wddm->init();

    EXPECT_EQ(NEO::windowsMinAddress, wddm->getWddmMinAddress());
    EXPECT_EQ(0u, gmmMemory->getInternalGpuVaRangeLimitCalled);
}

struct GdiWithMockedCloseFunc : public MockGdi {
    GdiWithMockedCloseFunc() : MockGdi() {
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
        HwDeviceIdWddm hwDeviceId{adapter, {}, osEnv.get(), std::make_unique<UmKmDataTranslator>()};
    }
    EXPECT_EQ(1u, GdiWithMockedCloseFunc::closeAdapterCalled);
    EXPECT_EQ(adapter, GdiWithMockedCloseFunc::closeAdapterCalledArgPassed);
}

TEST_F(WddmTest, WhenResidencyLoggingEnabledThenExpectLoggerCreated) {
    NEO::IoFunctions::mockFopenCalled = 0;
    NEO::IoFunctions::mockVfptrinfCalled = 0;
    NEO::IoFunctions::mockFcloseCalled = 0;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.WddmResidencyLogger.set(true);

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    wddm->residencyLogger.reset();
    if (NEO::wddmResidencyLoggingAvailable) {
        EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
        EXPECT_EQ(1u, NEO::IoFunctions::mockVfptrinfCalled);
        EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
    }
}

TEST_F(WddmTest, GivenResidencyLoggingEnabledWhenMakeResidentSuccessThenExpectSizeRapport) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::IoFunctions::mockFopenCalled = 0;
    NEO::IoFunctions::mockVfptrinfCalled = 0;
    NEO::IoFunctions::mockFcloseCalled = 0;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.WddmResidencyLogger.set(true);
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = ALLOCATION_HANDLE;
    uint64_t bytesToTrim = 0;
    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    //2 - one for open log, second for allocation size
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_TRUE(logger->makeResidentCall);
    EXPECT_EQ(MockGdi::pagingFenceReturnValue, logger->makeResidentPagingFence);
}

TEST_F(WddmTest, GivenResidencyLoggingEnabledWhenMakeResidentFailThenExpectTrimReport) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::IoFunctions::mockFopenCalled = 0;
    NEO::IoFunctions::mockVfptrinfCalled = 0;
    NEO::IoFunctions::mockFcloseCalled = 0;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.WddmResidencyLogger.set(true);
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = INVALID_HANDLE;
    uint64_t bytesToTrim = 0;

    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    //3 - one for open log, second for report allocations, 3rd for trim size
    EXPECT_EQ(3u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_FALSE(logger->makeResidentCall);
}

TEST_F(WddmTest, GivenResidencyLoggingEnabledWhenEnterWaitCalledThenExpectInternalFlagOn) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::IoFunctions::mockFopenCalled = 0;
    NEO::IoFunctions::mockVfptrinfCalled = 0;
    NEO::IoFunctions::mockFcloseCalled = 0;

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
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    NEO::IoFunctions::mockFopenCalled = 0;
    NEO::IoFunctions::mockVfptrinfCalled = 0;
    NEO::IoFunctions::mockFcloseCalled = 0;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.WddmResidencyLogger.set(true);
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = ALLOCATION_HANDLE;
    uint64_t bytesToTrim = 0;
    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    //2 - one for open log, second for allocation size
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_TRUE(logger->makeResidentCall);
    EXPECT_EQ(MockGdi::pagingFenceReturnValue, logger->makeResidentPagingFence);

    logger->enterWait = true;
    wddm->waitOnPagingFenceFromCpu();
    EXPECT_EQ(5u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_FALSE(logger->makeResidentCall);
    EXPECT_FALSE(logger->enterWait);
    EXPECT_EQ(MockGdi::pagingFenceReturnValue, logger->startWaitPagingFenceSave);
}

class MockRegistryReaderWithDriverStorePath : public SettingsReader {
  public:
    MockRegistryReaderWithDriverStorePath(const char *driverStorePathArg) : driverStorePath(driverStorePathArg){};
    std::string getSetting(const char *settingName, const std::string &value) override {
        std::string key(settingName);
        if (key == "DriverStorePathForComputeRuntime") {
            return driverStorePath;
        } else if (key == "OpenCLDriverName") {
            return driverStorePath;
        }
        return value;
    }

    bool getSetting(const char *settingName, bool defaultValue) override { return defaultValue; };
    int64_t getSetting(const char *settingName, int64_t defaultValue) override { return defaultValue; };
    int32_t getSetting(const char *settingName, int32_t defaultValue) override { return defaultValue; };
    const char *appSpecificLocation(const std::string &name) override { return name.c_str(); };

    const std::string driverStorePath;
};

TEST(DiscoverDevices, whenDriverInfoHasIncompatibleDriverStoreThenHwDeviceIdIsNotCreated) {
    VariableBackup<decltype(DriverInfoWindows::createRegistryReaderFunc)> createFuncBackup{&DriverInfoWindows::createRegistryReaderFunc};
    DriverInfoWindows::createRegistryReaderFunc = [](const std::string &) -> std::unique_ptr<SettingsReader> {
        return std::make_unique<MockRegistryReaderWithDriverStorePath>("driverStore\\0x8086");
    };
    VariableBackup<const wchar_t *> currentLibraryPathBackup(&SysCalls::currentLibraryPath);
    currentLibraryPathBackup = L"driverStore\\different_driverStore\\myLib.dll";
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_TRUE(hwDeviceIds.empty());
}

TEST(DiscoverDevices, givenDifferentCaseInLibPathAndInDriverStorePathWhenDiscoveringDeviceThenHwDeviceIdIsCreated) {
    VariableBackup<decltype(DriverInfoWindows::createRegistryReaderFunc)> createFuncBackup{&DriverInfoWindows::createRegistryReaderFunc};
    DriverInfoWindows::createRegistryReaderFunc = [](const std::string &) -> std::unique_ptr<SettingsReader> {
        return std::make_unique<MockRegistryReaderWithDriverStorePath>("\\SystemRoot\\driverStore\\0x8086");
    };
    VariableBackup<const wchar_t *> currentLibraryPathBackup(&SysCalls::currentLibraryPath);
    currentLibraryPathBackup = L"\\SyStEmrOOt\\driverstore\\0x8086\\myLib.dll";
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
}

TEST(DiscoverDevices, givenLibFromHostDriverStoreAndRegistryWithDriverStoreWhenDiscoveringDeviceThenHwDeviceIdIsCreated) {
    VariableBackup<decltype(DriverInfoWindows::createRegistryReaderFunc)> createFuncBackup{&DriverInfoWindows::createRegistryReaderFunc};
    DriverInfoWindows::createRegistryReaderFunc = [](const std::string &) -> std::unique_ptr<SettingsReader> {
        return std::make_unique<MockRegistryReaderWithDriverStorePath>("\\SystemRoot\\driverStore\\0x8086");
    };
    VariableBackup<const wchar_t *> currentLibraryPathBackup(&SysCalls::currentLibraryPath);
    currentLibraryPathBackup = L"\\SystemRoot\\hostdriverStore\\0x8086\\myLib.dll";
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
}

TEST(DiscoverDevices, givenLibFromDriverStoreAndRegistryWithHostDriverStoreWhenDiscoveringDeviceThenHwDeviceIdIsCreated) {
    VariableBackup<decltype(DriverInfoWindows::createRegistryReaderFunc)> createFuncBackup{&DriverInfoWindows::createRegistryReaderFunc};
    DriverInfoWindows::createRegistryReaderFunc = [](const std::string &) -> std::unique_ptr<SettingsReader> {
        return std::make_unique<MockRegistryReaderWithDriverStorePath>("\\SystemRoot\\driverStore\\0x8086");
    };
    VariableBackup<const wchar_t *> currentLibraryPathBackup(&SysCalls::currentLibraryPath);
    currentLibraryPathBackup = L"\\SystemRoot\\hostdriverStore\\0x8086\\myLib.dll";
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
}

TEST(VerifyAdapterType, whenAdapterDoesntSupportRenderThenDontCreateHwDeviceId) {
    auto gdi = std::make_unique<MockGdi>();
    auto osEnv = std::make_unique<OsEnvironmentWin>();
    osEnv->gdi.reset(gdi.release());

    LUID shadowAdapterLuid = {0xdd, 0xdd};
    auto hwDeviceId = createHwDeviceIdFromAdapterLuid(*osEnv, shadowAdapterLuid);
    EXPECT_EQ(nullptr, hwDeviceId.get());
}

TEST(VerifyAdapterType, whenAdapterSupportsRenderThenCreateHwDeviceId) {
    auto gdi = std::make_unique<MockGdi>();
    auto osEnv = std::make_unique<OsEnvironmentWin>();
    osEnv->gdi.reset(gdi.release());

    LUID adapterLuid = {0x12, 0x1234};
    auto hwDeviceId = createHwDeviceIdFromAdapterLuid(*osEnv, adapterLuid);
    EXPECT_NE(nullptr, hwDeviceId.get());
}

TEST_F(WddmTestWithMockGdiDll, givenInvalidInputwhenSettingAllocationPriorityThenFalseIsReturned) {
    init();
    EXPECT_FALSE(wddm->setAllocationPriority(nullptr, 0, DXGI_RESOURCE_PRIORITY_MAXIMUM));
    EXPECT_FALSE(wddm->setAllocationPriority(nullptr, 5, DXGI_RESOURCE_PRIORITY_MAXIMUM));
    {
        D3DKMT_HANDLE handles[] = {ALLOCATION_HANDLE, 0};
        EXPECT_FALSE(wddm->setAllocationPriority(handles, 2, DXGI_RESOURCE_PRIORITY_MAXIMUM));
    }
}

TEST_F(WddmTestWithMockGdiDll, givenValidInputwhenSettingAllocationPriorityThenTrueIsReturned) {
    init();
    D3DKMT_HANDLE handles[] = {ALLOCATION_HANDLE, ALLOCATION_HANDLE + 1};
    EXPECT_TRUE(wddm->setAllocationPriority(handles, 2, DXGI_RESOURCE_PRIORITY_MAXIMUM));
    EXPECT_EQ(DXGI_RESOURCE_PRIORITY_MAXIMUM, getLastPriorityFcn());

    EXPECT_TRUE(wddm->setAllocationPriority(handles, 2, DXGI_RESOURCE_PRIORITY_NORMAL));
    EXPECT_EQ(DXGI_RESOURCE_PRIORITY_NORMAL, getLastPriorityFcn());
}

TEST_F(WddmTestWithMockGdiDll, givenQueryAdapterInfoCallReturnsSuccesThenPciBusInfoIsValid) {
    ADAPTER_BDF queryAdapterBDF{};
    queryAdapterBDF.Bus = 1;
    queryAdapterBDF.Device = 2;
    queryAdapterBDF.Function = 3;
    setAdapterBDFFcn(queryAdapterBDF);

    EXPECT_TRUE(wddm->queryAdapterInfo());

    auto pciBusInfo = wddm->getPciBusInfo();

    EXPECT_EQ(pciBusInfo.pciDomain, 0u);
    EXPECT_EQ(pciBusInfo.pciBus, 1u);
    EXPECT_EQ(pciBusInfo.pciDevice, 2u);
    EXPECT_EQ(pciBusInfo.pciFunction, 3u);
}

TEST_F(WddmTestWithMockGdiDll, givenQueryAdapterInfoCallReturnsInvalidAdapterBDFThenPciBusInfoIsNotValid) {
    ADAPTER_BDF queryAdapterBDF{};
    queryAdapterBDF.Data = std::numeric_limits<uint32_t>::max();
    setAdapterBDFFcn(queryAdapterBDF);

    EXPECT_TRUE(wddm->queryAdapterInfo());

    auto pciBusInfo = wddm->getPciBusInfo();

    EXPECT_EQ(pciBusInfo.pciDomain, PhysicalDevicePciBusInfo::invalidValue);
    EXPECT_EQ(pciBusInfo.pciBus, PhysicalDevicePciBusInfo::invalidValue);
    EXPECT_EQ(pciBusInfo.pciDevice, PhysicalDevicePciBusInfo::invalidValue);
    EXPECT_EQ(pciBusInfo.pciFunction, PhysicalDevicePciBusInfo::invalidValue);
}
