/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/os_interface/windows/dxgi_wrapper.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm_engine_mapper.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_wddm_residency_logger.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/mocks/windows/mock_gmm_memory_base.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/os_interface/windows/ult_dxcore_factory.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
namespace SysCalls {
extern const wchar_t *currentLibraryPath;
}
extern uint32_t numRootDevicesToEnum;
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid, uint32_t adapterNodeOrdinalIn);
} // namespace NEO

using namespace NEO;

namespace GmmHelperFunctions {
Gmm *getGmm(void *ptr, size_t size, GmmHelper *gmmHelper) {
    size_t alignedSize = alignSizeWholePage(ptr, size);
    void *alignedPtr = alignUp(ptr, 4096);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm *gmm = new Gmm(gmmHelper, alignedPtr, alignedSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    void *fakePtr = reinterpret_cast<void *>(0x100);
    auto gmmHelper = getGmmHelper();
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(fakePtr)));
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, fakePtr, canonizedAddress, 0x2100, nullptr, MemoryPool::memoryNull, 0u, 1u);
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

TEST(WddmDiscoverDevices, givenMultipleRootDevicesExposedWhenCreateMultipleRootDevicesFlagIsSetToLowerValueThenDiscoverOnlySpecifiedNumberOfDevices) {
    DebugManagerStateRestore restorer{};
    VariableBackup<uint32_t> backup{&numRootDevicesToEnum};
    numRootDevicesToEnum = 3u;
    uint32_t requestedNumRootDevices = 2u;

    debugManager.flags.CreateMultipleRootDevices.set(requestedNumRootDevices);
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

    debugManager.flags.CreateMultipleRootDevices.set(requestedNumRootDevices);
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
    CREATECONTEXT_PVTDATA privateData = initPrivateData(*newContext);
    EXPECT_FALSE(privateData.IsProtectedProcess);
    EXPECT_FALSE(privateData.IsDwm);
    EXPECT_TRUE(privateData.GpuVAContext);
    EXPECT_FALSE(privateData.IsMediaUsage);
}

TEST_F(Wddm20Tests, WhenCreatingAllocationAndDestroyingAllocationThenCorrectResultReturned) {
    OsAgnosticMemoryManager mm(*executionEnvironment);
    auto gmmHelper = getGmmHelper();
    auto ptr = mm.allocateSystemMemory(100, 0);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, ptr, canonizedAddress, 100, nullptr, MemoryPool::memoryNull, 0u, 1u);
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
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, ptr, canonizedAddress, 0x2100, nullptr, MemoryPool::memoryNull, 0u, 1u);
    Gmm *gmm = GmmHelperFunctions::getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize(), getGmmHelper());

    allocation.setDefaultGmm(gmm);
    auto status = wddm->createAllocation(&allocation);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_NE(0u, allocation.getDefaultHandle());

    bool ret = wddm->mapGpuVirtualAddress(&allocation);
    EXPECT_TRUE(ret);

    EXPECT_EQ(alignedPages, getLastCallMapGpuVaArgFcn()->SizeInPages);
    EXPECT_NE(underlyingPages, getLastCallMapGpuVaArgFcn()->SizeInPages);

    ret = wddm->destroyAllocation(&allocation, osContext.get());
    EXPECT_TRUE(ret);

    delete gmm;
}

TEST_F(Wddm20WithMockGdiDllTests, givenReserveCallWhenItIsCalledWithProperParametersThenAddressInRangeIsReturend) {
    auto sizeAlignedTo64Kb = 64 * MemoryConstants::kiloByte;
    D3DGPU_VIRTUAL_ADDRESS reservationAddress;
    auto status = wddm->reserveGpuVirtualAddress(0ull, wddm->getGfxPartition().Heap32[0].Base,
                                                 wddm->getGfxPartition().Heap32[0].Limit,
                                                 sizeAlignedTo64Kb, &reservationAddress);

    EXPECT_EQ(STATUS_SUCCESS, status);
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

TEST_F(Wddm20WithMockGdiDllTests, givenReserveCallWhenItIsCalledWithInvalidBaseThenFailureIsReturned) {
    auto sizeAlignedTo64Kb = 64 * MemoryConstants::kiloByte;
    D3DGPU_VIRTUAL_ADDRESS reservationAddress;
    auto status = wddm->reserveGpuVirtualAddress(0x1234, wddm->getGfxPartition().Heap32[0].Base,
                                                 wddm->getGfxPartition().Heap32[0].Limit,
                                                 sizeAlignedTo64Kb, &reservationAddress);
    EXPECT_NE(STATUS_SUCCESS, status);
}

TEST_F(Wddm20WithMockGdiDllTests, givenReserveCallWhenItIsCalledWithAttemptBaseAddressWithValidBaseThenAddressInRangeIsReturned) {
    auto sizeAlignedTo64Kb = 64 * MemoryConstants::kiloByte;
    D3DGPU_VIRTUAL_ADDRESS previousReservationAddress;
    D3DGPU_VIRTUAL_ADDRESS reservationAddress;
    NTSTATUS status;
    status = wddm->reserveGpuVirtualAddress(0ull, wddm->getGfxPartition().Heap32[0].Base,
                                            wddm->getGfxPartition().Heap32[0].Limit,
                                            sizeAlignedTo64Kb, &previousReservationAddress);

    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_TRUE(wddm->freeGpuVirtualAddress(previousReservationAddress, sizeAlignedTo64Kb));

    status = wddm->reserveGpuVirtualAddress(previousReservationAddress, wddm->getGfxPartition().Heap32[0].Base,
                                            wddm->getGfxPartition().Heap32[0].Limit,
                                            sizeAlignedTo64Kb, &reservationAddress);

    EXPECT_EQ(STATUS_SUCCESS, status);
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
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, fakePtr, canonizedAddress, 100, nullptr, MemoryPool::memoryNull, 0u, 1u);
    std::unique_ptr<Gmm> gmm(GmmHelperFunctions::getGmm(allocation.getAlignedCpuPtr(), allocation.getAlignedSize(), getGmmHelper()));

    allocation.setDefaultGmm(gmm.get());
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);

    auto mockResourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockResourceInfo->overrideReturnedSize(allocation.getAlignedSize() + (2 * MemoryConstants::pageSize));

    wddm->mapGpuVirtualAddress(&allocation);

    uint64_t expectedSizeInPages = static_cast<uint64_t>(mockResourceInfo->getSizeAllocation() / MemoryConstants::pageSize);
    EXPECT_EQ(expectedSizeInPages, getLastCallMapGpuVaArgFcn()->SizeInPages);
    wddm->destroyAllocation(&allocation, nullptr);
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

TEST_F(Wddm20WithMockGdiDllTests, whencreateMonitoredFenceForDirectSubmissionThenCreateFence) {
    MonitoredFence fence{};
    wddm->getWddmInterface()->createFenceForDirectSubmission(fence, *osContext);
    EXPECT_EQ(wddmMockInterface->createMonitoredFenceCalled, 1u);
    EXPECT_NE(osContext->getHwQueue().progressFenceHandle, fence.fenceHandle);
    wddm->getWddmInterface()->destroyMonitorFence(fence);
}

TEST_F(Wddm20WithMockGdiDllTests, GivenGpuHangIndicationWhenCheckingForGpuHangThenTrueIsReturned) {
    auto fenceCpuAddress = osContext->getResidencyController().getMonitoredFence().cpuAddress;
    VariableBackup<volatile uint64_t> backupWddmMonitorFence(fenceCpuAddress);

    *fenceCpuAddress = NEO::Wddm::gpuHangIndication;
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

TEST_F(Wddm20WithMockGdiDllTests, GivenSetAssumeNotInUseSetToFalseWhenDestroyAllocationsThenAssumeNotInUseNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetAssumeNotInUse.set(false);

    OsHandleStorage storage;
    OsHandleWin osHandle1;

    osHandle1.handle = ALLOCATION_HANDLE;

    storage.fragmentStorageData[0].osHandleStorage = &osHandle1;
    storage.fragmentStorageData[0].freeTheFragment = true;

    D3DKMT_HANDLE handles[1] = {ALLOCATION_HANDLE};
    bool retVal = wddm->destroyAllocations(handles, 1, 0);
    EXPECT_TRUE(retVal);

    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);
}

TEST_F(Wddm20Tests, WhenMappingAndFreeingGpuVaThenReturnIsCorrect) {
    OsAgnosticMemoryManager mm(*executionEnvironment);
    auto gmmHelper = getGmmHelper();
    auto ptr = mm.allocateSystemMemory(100, 0);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, ptr, canonizedAddress, 100, nullptr, MemoryPool::memoryNull, 0u, 1u);
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
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 100, nullptr, MemoryPool::memoryNull, 0u, 1u);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), getGmmHelper()));

    allocation.setDefaultGmm(gmm.get());
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);

    bool ret = wddm->mapGpuVirtualAddress(&allocation);
    EXPECT_TRUE(ret);

    EXPECT_NE(0u, allocation.getGpuAddress());

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    EXPECT_EQ(allocation.getGpuAddress(), gmmHelper->canonize(allocation.getGpuAddress()));
    wddm->destroyAllocation(&allocation, nullptr);
}

TEST_F(WddmTestWithMockGdiDll, givenShareableAllocationWhenCreateThenCreateResourceFlagIsEnabled) {
    init();
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::memoryNull, true, 1u);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(nullptr, MemoryConstants::pageSize, getGmmHelper()));
    allocation.setDefaultGmm(gmm.get());
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);
    auto passedCreateAllocation = getMockAllocationFcn();
    EXPECT_EQ(1u, passedCreateAllocation->Flags.CreateShared);
    EXPECT_EQ(1u, passedCreateAllocation->Flags.CreateResource);
    wddm->destroyAllocation(&allocation, nullptr);
}

TEST_F(WddmTestWithMockGdiDll, givenCompressedResourceWhenItIsCreatedThenItMayContainNonZeroContent) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.AllowNotZeroForCompressedOnWddm.set(1);
    init();
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::memoryNull, true, 1u);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(nullptr, MemoryConstants::pageSize, getGmmHelper()));
    gmm->setCompressionEnabled(true);
    allocation.setDefaultGmm(gmm.get());
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);
    auto passedCreateAllocation = getMockAllocationFcn();
    EXPECT_EQ(1u, passedCreateAllocation->Flags.AllowNotZeroed);
    wddm->destroyAllocation(&allocation, nullptr);
}

TEST_F(WddmTestWithMockGdiDll, givenNonCompressedResourceWhenItIsCreatedThenItCannotContainNonZeroContent) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.AllowNotZeroForCompressedOnWddm.set(1);
    init();
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::memoryNull, true, 1u);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(nullptr, MemoryConstants::pageSize, getGmmHelper()));
    allocation.setDefaultGmm(gmm.get());
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);
    auto passedCreateAllocation = getMockAllocationFcn();
    EXPECT_EQ(0u, passedCreateAllocation->Flags.AllowNotZeroed);
    wddm->destroyAllocation(&allocation, nullptr);
}

TEST_F(WddmTestWithMockGdiDll, givenCompressedResourceWhenItIsCreatedThenItCannotContainNonZeroContent) {
    init();
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::memoryNull, true, 1u);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(nullptr, MemoryConstants::pageSize, getGmmHelper()));
    gmm->setCompressionEnabled(true);
    allocation.setDefaultGmm(gmm.get());
    auto status = wddm->createAllocation(&allocation);
    EXPECT_EQ(STATUS_SUCCESS, status);
    auto passedCreateAllocation = getMockAllocationFcn();
    EXPECT_EQ(0u, passedCreateAllocation->Flags.AllowNotZeroed);
    wddm->destroyAllocation(&allocation, nullptr);
}

TEST_F(WddmTestWithMockGdiDll, givenShareableAllocationWhenCreateThenSharedHandleAndResourceHandleAreSet) {
    init();
    struct MockWddmMemoryManager : public WddmMemoryManager {
        using WddmMemoryManager::createGpuAllocationsWithRetry;
        using WddmMemoryManager::WddmMemoryManager;
    };
    MemoryManagerCreate<MockWddmMemoryManager> memoryManager(false, false, *executionEnvironment);
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::memoryNull, true, 1u);
    auto gmm = std::unique_ptr<Gmm>(GmmHelperFunctions::getGmm(nullptr, MemoryConstants::pageSize, getGmmHelper()));
    allocation.setDefaultGmm(gmm.get());
    auto status = memoryManager.createGpuAllocationsWithRetry(&allocation);
    EXPECT_TRUE(status);
    uint64_t handle = 0;
    allocation.peekInternalHandle(&memoryManager, handle);
    EXPECT_NE(0u, handle);
}

TEST_F(WddmTestWithMockGdiDll, whenCreateDeviceFailsThenGmmIsNotInitialized) {
    VariableBackup backupFailDevice(&failCreateDevice, true);
    wddm->rootDeviceEnvironment.gmmHelper.reset();
    EXPECT_FALSE(wddm->init());
    EXPECT_EQ(nullptr, wddm->rootDeviceEnvironment.gmmHelper.get());
}

TEST_F(WddmTestWithMockGdiDll, whenCreatePagingQueueFailsThenGmmIsNotInitialized) {
    VariableBackup backupFailDevice(&failCreatePagingQueue, true);
    wddm->rootDeviceEnvironment.gmmHelper.reset();
    EXPECT_FALSE(wddm->init());
    EXPECT_EQ(nullptr, wddm->rootDeviceEnvironment.gmmHelper.get());
}

TEST_F(Wddm20Tests, WhenMakingResidentAndEvictingThenReturnIsCorrect) {
    OsAgnosticMemoryManager mm(*executionEnvironment);
    auto gmmHelper = getGmmHelper();
    auto ptr = mm.allocateSystemMemory(100, 0);
    auto canonizedAddress = gmmHelper->canonize(castToUint64(const_cast<void *>(ptr)));
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, ptr, canonizedAddress, 100, nullptr, MemoryPool::memoryNull, 0u, 1u);
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

    error = wddm->destroyAllocation(&allocation, osContext.get());

    EXPECT_TRUE(error);
    delete gmm;
    mm.freeSystemMemory(allocation.getUnderlyingBuffer());
}

TEST_F(Wddm20WithMockGdiDllTests, givenSharedHandlesWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenGraphicsAllocationsWithSharedPropertiesAreCreated) {
    void *pSysMem = (void *)0x1000;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    std::unique_ptr<Gmm> gmm2(new Gmm(getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_IMAGE, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get(), gmm2->gmmResourceInfo.get()};
    auto status = setSizesFcn(gmmPtrArray, 2u, 1024u, 1u);
    EXPECT_EQ(0u, static_cast<uint32_t>(status));

    MemoryManagerCreate<WddmMemoryManager> mm(false, false, *executionEnvironment);
    AllocationProperties properties(0, false, 4096u, AllocationType::sharedImage, false, {});

    for (uint32_t i = 0; i < 3; i++) {
        WddmMemoryManager::OsHandleData osHandleData{ALLOCATION_HANDLE, i};

        auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
        auto wddmAllocation = (WddmAllocation *)graphicsAllocation;
        ASSERT_NE(nullptr, wddmAllocation);

        EXPECT_EQ(ALLOCATION_HANDLE, wddmAllocation->peekSharedHandle());
        EXPECT_EQ(RESOURCE_HANDLE, wddmAllocation->getResourceHandle());
        EXPECT_NE(0u, wddmAllocation->getDefaultHandle());
        EXPECT_EQ(ALLOCATION_HANDLE, wddmAllocation->getDefaultHandle());
        EXPECT_NE(0u, wddmAllocation->getGpuAddress());
        EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
        EXPECT_EQ(nullptr, wddmAllocation->getAlignedCpuPtr());
        EXPECT_NE(nullptr, wddmAllocation->getDefaultGmm());

        EXPECT_EQ(4096u, wddmAllocation->getDefaultGmm()->gmmResourceInfo->getSizeAllocation());

        if (i % 2)
            EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_IMAGE, reinterpret_cast<MockGmmResourceInfo *>(wddmAllocation->getDefaultGmm()->gmmResourceInfo.get())->getResourceUsage());
        else
            EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER, reinterpret_cast<MockGmmResourceInfo *>(wddmAllocation->getDefaultGmm()->gmmResourceInfo.get())->getResourceUsage());

        mm.freeGraphicsMemory(graphicsAllocation);
        auto destroyWithResourceHandleCalled = 0u;
        D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

        status = getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

        EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
        EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);

        EXPECT_EQ(0u, static_cast<uint32_t>(status));
        EXPECT_EQ(1u, destroyWithResourceHandleCalled);
    }
}

TEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledWithMapPointerThenGraphicsAllocationWithSharedPropertiesIsCreated) {
    void *pSysMem = (void *)0x1000;

    size_t sizeAlignedTo64Kb = 64 * MemoryConstants::kiloByte;
    void *reservedAddress;
    EXPECT_TRUE(wddm->reserveValidAddressRange(sizeAlignedTo64Kb, reservedAddress));
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, sizeAlignedTo64Kb, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    auto status = setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);
    EXPECT_EQ(0u, static_cast<uint32_t>(status));

    MemoryManagerCreate<WddmMemoryManager> mm(false, false, *executionEnvironment);
    AllocationProperties properties(0, false, sizeAlignedTo64Kb, AllocationType::sharedBuffer, false, {});
    WddmMemoryManager::OsHandleData osHandleData{ALLOCATION_HANDLE};

    auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, reservedAddress);
    auto wddmAllocation = (WddmAllocation *)graphicsAllocation;
    ASSERT_NE(nullptr, wddmAllocation);

    EXPECT_EQ(ALLOCATION_HANDLE, wddmAllocation->peekSharedHandle());
    EXPECT_EQ(RESOURCE_HANDLE, wddmAllocation->getResourceHandle());
    EXPECT_NE(0u, wddmAllocation->getDefaultHandle());
    EXPECT_EQ(ALLOCATION_HANDLE, wddmAllocation->getDefaultHandle());
    EXPECT_EQ(reservedAddress, reinterpret_cast<void *>(wddmAllocation->getGpuAddress()));
    EXPECT_EQ(sizeAlignedTo64Kb, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(nullptr, wddmAllocation->getAlignedCpuPtr());
    EXPECT_NE(nullptr, wddmAllocation->getDefaultGmm());

    EXPECT_EQ(sizeAlignedTo64Kb, wddmAllocation->getDefaultGmm()->gmmResourceInfo->getSizeAllocation());
    mm.freeGraphicsMemory(graphicsAllocation);
    wddm->releaseReservedAddress(nullptr);
    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    status = getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);

    EXPECT_EQ(0u, static_cast<uint32_t>(status));
    EXPECT_EQ(1u, destroyWithResourceHandleCalled);
}

TEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromMultipleSharedHandlesIsCalledThenNullptrIsReturned) {
    void *pSysMem = (void *)0x1000;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    auto status = setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);
    EXPECT_EQ(0u, static_cast<uint32_t>(status));

    MemoryManagerCreate<WddmMemoryManager> mm(false, false, *executionEnvironment);
    AllocationProperties properties(0, false, 4096u, AllocationType::sharedBuffer, false, {});

    std::vector<osHandle> handles{ALLOCATION_HANDLE};
    auto graphicsAllocation = mm.createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_EQ(nullptr, graphicsAllocation);
}

TEST_F(Wddm20WithMockGdiDllTests, givenSharedHandleWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenMapGpuVaWithCpuPtrDepensOnBitness) {
    void *pSysMem = (void *)0x1000;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    void *gmmPtrArray[]{gmm->gmmResourceInfo.get()};
    auto status = setSizesFcn(gmmPtrArray, 1u, 1024u, 1u);
    EXPECT_EQ(0u, static_cast<uint32_t>(status));

    MemoryManagerCreate<WddmMemoryManager> mm(false, false, *executionEnvironment);
    AllocationProperties properties(0, false, 4096, AllocationType::sharedBuffer, false, {});
    WddmMemoryManager::OsHandleData osHandleData{ALLOCATION_HANDLE};

    auto graphicsAllocation = mm.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
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
    debugManager.flags.UseNoRingFlushesKmdMode.set(false);
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

        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        auto engine = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0];

        auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode);
        engineDescriptor.engineTypeUsage.second = lowPriority ? EngineUsage::lowPriority : EngineUsage::regular;
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, engineDescriptor);
        osContext->ensureContextInitialized(false);
    }
};

TEST_F(WddmContextSchedulingPriorityTests, givenLowPriorityContextWhenInitializingThenCallSetPriority) {
    initContext(true);

    auto createContextParams = this->getSetContextSchedulingPriorityDataCallFcn();

    EXPECT_EQ(osContext->getWddmContextHandle(), createContextParams->hContext);
    EXPECT_EQ(-7, createContextParams->Priority);
}

TEST_F(WddmContextSchedulingPriorityTests, givenLowPriorityContextWhenFailingDuringSetSchedulingPriorityThenThrow) {
    *this->getFailOnSetContextSchedulingPriorityCallFcn() = true;

    EXPECT_ANY_THROW(initContext(true));
}

TEST_F(WddmContextSchedulingPriorityTests, givenDebugFlagSetWhenInitializingLowPriorityContextThenSetPriorityValue) {
    DebugManagerStateRestore dbgRestore;

    constexpr int32_t newPriority = 3;

    debugManager.flags.ForceWddmLowPriorityContextValue.set(newPriority);

    initContext(true);

    auto createContextParams = this->getSetContextSchedulingPriorityDataCallFcn();

    EXPECT_EQ(osContext->getWddmContextHandle(), createContextParams->hContext);
    EXPECT_EQ(newPriority, createContextParams->Priority);
}

TEST_F(WddmContextSchedulingPriorityTests, givenRegularContextWhenInitializingThenDontCallSetPriority) {
    initContext(false);

    auto createContextParams = this->getSetContextSchedulingPriorityDataCallFcn();

    EXPECT_EQ(0u, createContextParams->hContext);
    EXPECT_EQ(0, createContextParams->Priority);
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenCreateContextCallWhenDriverHintsThenItPointsToOpenCL) {
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    EXPECT_EQ(D3DKMT_CLIENTHINT_OPENCL, createContextParams->ClientHint);
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenUseNoRingFlushesKmdModeDebugFlagToTrueWhenCreateContextIsCalledThenNoRingFlushesKmdModeIsSetToTrue) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseNoRingFlushesKmdMode.set(true);
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    auto privateData = (CREATECONTEXT_PVTDATA *)createContextParams->pPrivateDriverData;
    EXPECT_TRUE(!!privateData->NoRingFlushes);
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, whenCreateContextIsCalledThenDummyPageBackingEnabledFlagIsDisabled) {
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    auto privateData = (CREATECONTEXT_PVTDATA *)createContextParams->pPrivateDriverData;
    EXPECT_FALSE(!!privateData->DummyPageBackingEnabled);
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenDummyPageBackingEnabledFlagSetToTrueWhenCreateContextIsCalledThenFlagIsEnabled) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.DummyPageBackingEnabled.set(true);
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    auto privateData = (CREATECONTEXT_PVTDATA *)createContextParams->pPrivateDriverData;
    EXPECT_TRUE(!!privateData->DummyPageBackingEnabled);
}

TEST_F(Wddm20WithMockGdiDllTestsWithoutWddmInit, givenEngineTypeWhenCreatingContextThenPassCorrectNodeOrdinal) {
    init();
    auto createContextParams = this->getCreateContextDataFcn();
    auto &gfxCoreHelper = this->rootDeviceEnvironment->getHelper<GfxCoreHelper>();
    UINT expected = WddmEngineMapper::engineNodeMap(gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0].first);
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
    EXPECT_TRUE(debugManager.flags.UseNoRingFlushesKmdMode.get());
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
    DebugManagerStateRestore restorer{};
    debugManager.flags.PlaformSupportEvictIfNecessaryFlag.set(1);

    auto &productHelper = rootDeviceEnvironment->getHelper<ProductHelper>();
    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);

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
    EXPECT_EQ(static_cast<uint32_t>(allocation.getHandles().size()), gdi->getDestroyArg().AllocationCount);
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

    auto status = wddm->waitFromCpu(10, osContext->getResidencyController().getMonitoredFence(), true);

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

    auto status = wddm->waitFromCpu(20, osContext->getResidencyController().getMonitoredFence(), true);

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
    MockMemoryManager::OsHandleData osHandleData{static_cast<uint64_t>(0ull)};
    WddmAllocation *alloc = nullptr;

    gdi->queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(queryResourceInfoMock);
    auto ret = wddm->openSharedHandle(osHandleData, alloc);

    EXPECT_EQ(false, ret);
}

TEST_F(Wddm20Tests, givenOpenNTHandleWhenZeroAllocationsThenReturnNull) {
    MockMemoryManager::OsHandleData osHandleData{static_cast<uint64_t>(0ull)};
    WddmAllocation *alloc = nullptr;

    auto ret = wddm->openNTHandle(osHandleData, alloc);

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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), fakePtr, 100, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, fakePtr, canonizedAddress, 100, nullptr, MemoryPool::memoryNull, 0u, 1u);
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
    debugManager.flags.DoNotRegisterTrimCallback.set(true);
    WddmResidencyController residencyController{*wddm, 0u};
    EXPECT_EQ(nullptr, wddm->registerTrimCallback([](D3DKMT_TRIMNOTIFICATION *) {}, residencyController));
}

using WddmLockWithMakeResidentTests = Wddm20Tests;

TEST_F(WddmLockWithMakeResidentTests, givenAllocationThatDoesntNeedMakeResidentBeforeLockWhenLockThenDontStoreItOrCallMakeResident) {
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    wddm->lockResource(ALLOCATION_HANDLE, false, 0x1000);
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(0u, wddm->makeResidentResult.called);
    wddm->unlockResource(ALLOCATION_HANDLE, false);
}

TEST_F(WddmLockWithMakeResidentTests, givenAllocationThatNeedsMakeResidentBeforeLockWhenLockThenCallBlockingMakeResident) {
    wddm->lockResource(ALLOCATION_HANDLE, true, 0x1000);
    EXPECT_EQ(1u, wddm->makeResidentResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, givenAllocationAndForcePagingFenceTrueWhenApplyBlockingMakeResidentThenWaitOnPagingFenceFromCpuIsCalledWithIsKmdWaitNeededAsFalse) {
    auto allocHandle = ALLOCATION_HANDLE;
    const bool forcePagingFence = true;
    wddm->temporaryResources->makeResidentResources(&allocHandle, 1u, 0x1000, forcePagingFence);
    EXPECT_EQ(1u, mockTemporaryResources->acquireLockResult.called);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&mockTemporaryResources->resourcesLock), mockTemporaryResources->acquireLockResult.uint64ParamPassed);
    EXPECT_NE(forcePagingFence, wddm->waitOnPagingFenceFromCpuResult.isKmdWaitNeededPassed);
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
    EXPECT_TRUE(mockWddm.makeResidentResult.cantTrimFurther);
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
    EXPECT_EQ(0x2u, mockTemporaryResources->resourceHandles.back());
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
    EXPECT_EQ(0x2u, mockTemporaryResources->resourceHandles.back());
    EXPECT_EQ(allocation.handle, mockWddm.makeResidentResult.handlePack[0]);
    EXPECT_EQ(1u, mockWddm.makeResidentResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, givenNoTemporaryResourcesWhenEvictingAllTemporaryResourcesThenEvictionIsNotApplied) {
    wddm->getTemporaryResourcesContainer()->evictAllResources();
    EXPECT_EQ(MemoryOperationsStatus::memoryNotFound, mockTemporaryResources->evictAllResourcesResult.operationSuccess);
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
    EXPECT_EQ(MemoryOperationsStatus::success, mockTemporaryResources->evictAllResourcesResult.operationSuccess);
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
    EXPECT_EQ(MemoryOperationsStatus::failed, mockTemporaryResources->evictAllResourcesResult.operationSuccess);
    EXPECT_EQ(1u, mockWddm.evictResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, givenNoTemporaryResourcesWhenEvictingTemporaryResourceThenEvictionIsNotApplied) {
    wddm->getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE);
    EXPECT_EQ(MemoryOperationsStatus::memoryNotFound, mockTemporaryResources->evictResourceResult.operationSuccess);
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
    EXPECT_EQ(MemoryOperationsStatus::memoryNotFound, mockTemporaryResources->evictResourceResult.operationSuccess);
}

TEST_F(WddmLockWithMakeResidentTests, whenEvictingTemporaryResourceAndEvictFailsThenReturnFail) {
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    mockWddm.evictStatus = false;
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(ALLOCATION_HANDLE);
    mockWddm.getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE);
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(MemoryOperationsStatus::failed, mockTemporaryResources->evictResourceResult.operationSuccess);
    EXPECT_EQ(1u, mockWddm.evictResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, whenEvictingTemporaryResourceAndEvictSucceedThenReturnSuccess) {
    WddmMock mockWddm(*executionEnvironment->rootDeviceEnvironments[0]);
    auto mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(mockWddm.temporaryResources.get());
    mockTemporaryResources->resourceHandles.push_back(ALLOCATION_HANDLE);
    mockWddm.getTemporaryResourcesContainer()->evictResource(ALLOCATION_HANDLE);
    EXPECT_TRUE(mockTemporaryResources->resourceHandles.empty());
    EXPECT_EQ(MemoryOperationsStatus::success, mockTemporaryResources->evictResourceResult.operationSuccess);
    EXPECT_EQ(1u, mockWddm.evictResult.called);
}

TEST_F(WddmLockWithMakeResidentTests, whenEvictingTemporaryResourceThenOtherResourcesRemainOnTheList) {
    mockTemporaryResources->resourceHandles.push_back(0x1);
    mockTemporaryResources->resourceHandles.push_back(0x2);
    mockTemporaryResources->resourceHandles.push_back(0x3);

    wddm->getTemporaryResourcesContainer()->evictResource(0x2);

    EXPECT_EQ(2u, mockTemporaryResources->resourceHandles.size());
    EXPECT_EQ(0x1u, mockTemporaryResources->resourceHandles.front());
    EXPECT_EQ(0x3u, mockTemporaryResources->resourceHandles.back());
}

TEST_F(WddmLockWithMakeResidentTests, whenAlllocationNeedsBlockingMakeResidentBeforeLockThenLockWithBlockingMakeResident) {
    WddmMemoryManager memoryManager(*executionEnvironment);
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    allocation.setMakeResidentBeforeLockRequired(false);
    memoryManager.lockResource(&allocation);
    EXPECT_EQ(1u, wddm->lockResult.called);
    EXPECT_EQ(rootDeviceEnvironment->getHelper<GfxCoreHelper>().makeResidentBeforeLockNeeded(false), wddm->lockResult.uint64ParamPassed);
    memoryManager.unlockResource(&allocation);

    allocation.setMakeResidentBeforeLockRequired(true);
    memoryManager.lockResource(&allocation);
    EXPECT_EQ(2u, wddm->lockResult.called);
    EXPECT_EQ(1u, wddm->lockResult.uint64ParamPassed);
    memoryManager.unlockResource(&allocation);
}
using WddmGfxPartitionTest = Wddm20Tests;

TEST(WddmGfxPartitionTests, givenGfxPartitionWhenInitializedThenInternalFrontWindowHeapIsAllocatedAtInternalHeapFront) {
    MockExecutionEnvironment executionEnvironment;
    auto wddm = new WddmMock(*executionEnvironment.rootDeviceEnvironments[0]);

    uint32_t rootDeviceIndex = 0;
    size_t numRootDevices = 1;

    MockGfxPartition gfxPartition;
    wddm->init();
    wddm->initGfxPartition(gfxPartition, rootDeviceIndex, numRootDevices, false);

    EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::heapInternalFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternal));
    EXPECT_EQ(gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceMemory));

    auto frontWindowSize = GfxPartition::internalFrontWindowPoolSize;
    EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::heapInternalFrontWindow), frontWindowSize);
    EXPECT_EQ(gfxPartition.getHeapSize(HeapIndex::heapInternalDeviceFrontWindow), frontWindowSize);

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternalFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternalFrontWindow));
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternalDeviceFrontWindow), gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceFrontWindow));

    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternal), gfxPartition.getHeapBase(HeapIndex::heapInternal) + frontWindowSize);
    EXPECT_EQ(gfxPartition.getHeapMinimalAddress(HeapIndex::heapInternalDeviceMemory), gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceMemory) + frontWindowSize);

    EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::heapInternal),
              gfxPartition.getHeapBase(HeapIndex::heapInternal) + gfxPartition.getHeapSize(HeapIndex::heapInternal) - 1);
    EXPECT_EQ(gfxPartition.getHeapLimit(HeapIndex::heapInternalDeviceMemory),
              gfxPartition.getHeapBase(HeapIndex::heapInternalDeviceMemory) + gfxPartition.getHeapSize(HeapIndex::heapInternalDeviceMemory) - 1);
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
    const size_t sizeBig = static_cast<size_t>(gfxPartition.getHeapSize(HeapIndex::heapInternalFrontWindow)) - MemoryConstants::pageSize64k;

    HeapIndex heaps[] = {HeapIndex::heapInternalFrontWindow,
                         HeapIndex::heapInternalDeviceFrontWindow};

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
    debugManager.flags.FilterDeviceId.set("1234"); // Existing device Id
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
}

TEST_F(WddmTest, WhenFeatureFlagHwQueueIsDisabledThenReturnWddm20Version) {
    wddm->featureTable->flags.ftrWddmHwQueues = 0;
    EXPECT_EQ(WddmVersion::wddm20, wddm->getWddmVersion());
}

TEST_F(WddmTest, WhenFeatureFlagHwQueueIsEnabledThenReturnWddm23Version) {
    wddm->featureTable->flags.ftrWddmHwQueues = 1;
    EXPECT_EQ(WddmVersion::wddm23, wddm->getWddmVersion());
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
        HwDeviceIdWddm hwDeviceId{adapter, {}, 1u, osEnv.get(), std::make_unique<UmKmDataTranslator>()};
    }
    EXPECT_EQ(1u, GdiWithMockedCloseFunc::closeAdapterCalled);
    EXPECT_EQ(adapter, GdiWithMockedCloseFunc::closeAdapterCalledArgPassed);
}

namespace MockWddmResidencyLoggerFunctions {
std::string recordedFileName(256, 0);

FILE *testFopen(const char *filename, const char *mode) {
    MockWddmResidencyLoggerFunctions::recordedFileName.assign(filename);
    return NEO::IoFunctions::mockFopen(filename, mode);
}
} // namespace MockWddmResidencyLoggerFunctions

struct WddmResidencyLoggerTest : public WddmTest {
    void SetUp() override {
        MockWddmResidencyLoggerFunctions::recordedFileName.clear();

        WddmTest::SetUp();

        NEO::IoFunctions::mockFopenCalled = 0;
        NEO::IoFunctions::mockVfptrinfCalled = 0;
        NEO::IoFunctions::mockFcloseCalled = 0;

        debugManager.flags.WddmResidencyLogger.set(true);

        mockFopenBackup = std::make_unique<VariableBackup<NEO::IoFunctions::fopenFuncPtr>>(&NEO::IoFunctions::fopenPtr);
        NEO::IoFunctions::fopenPtr = &MockWddmResidencyLoggerFunctions::testFopen;
    }

    DebugManagerStateRestore dbgRestore;
    std::unique_ptr<VariableBackup<NEO::IoFunctions::fopenFuncPtr>> mockFopenBackup;
};

TEST_F(WddmResidencyLoggerTest, WhenResidencyLoggingEnabledThenExpectLoggerCreated) {
    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    wddm->residencyLogger.reset();
    if (NEO::wddmResidencyLoggingAvailable) {
        EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);
        EXPECT_EQ(1u, NEO::IoFunctions::mockVfptrinfCalled);
        EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
    }
}

TEST_F(WddmResidencyLoggerTest, GivenResidencyLoggingEnabledWhenMakeResidentSuccessThenExpectSizeRapport) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = ALLOCATION_HANDLE;
    uint64_t bytesToTrim = 0;
    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    // 2 - one for open log, second for allocation size
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_TRUE(logger->makeResidentCall);
    EXPECT_EQ(MockGdi::pagingFenceReturnValue, logger->makeResidentPagingFence);
}

TEST_F(WddmResidencyLoggerTest, GivenResidencyLoggingEnabledWhenMakeResidentFailThenExpectTrimReport) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = INVALID_HANDLE;
    uint64_t bytesToTrim = 0;

    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    // 3 - one for open log, second for report allocations, 3rd for trim size
    EXPECT_EQ(3u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_FALSE(logger->makeResidentCall);
}

TEST_F(WddmTest, GivenInvalidHandleAndCantTrimFurtherSetToTrueWhenCallingMakeResidentThenFalseIsReturned) {
    wddm->callBaseMakeResident = true;

    D3DKMT_HANDLE handle = INVALID_HANDLE;
    uint64_t bytesToTrim = 4 * 4096;

    bool retVal = wddm->makeResident(&handle, 1, true, &bytesToTrim, 0x1000);

    EXPECT_FALSE(retVal);
}

TEST_F(WddmResidencyLoggerTest, GivenResidencyLoggingEnabledWhenEnterWaitCalledThenExpectInternalFlagOn) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    wddm->callBaseCreatePagingLogger = false;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());
    logger->enteredWait();
    EXPECT_TRUE(logger->enterWait);
}

TEST_F(WddmResidencyLoggerTest, GivenResidencyLoggingEnabledWhenMakeResidentAndWaitPagingThenExpectFlagsOff) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = ALLOCATION_HANDLE;
    uint64_t bytesToTrim = 0;
    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    // 2 - one for open log, second for allocation size
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_TRUE(logger->makeResidentCall);
    EXPECT_EQ(MockGdi::pagingFenceReturnValue, logger->makeResidentPagingFence);

    logger->enterWait = true;
    wddm->waitOnPagingFenceFromCpu(false);
    EXPECT_EQ(5u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_FALSE(logger->makeResidentCall);
    EXPECT_FALSE(logger->enterWait);
    EXPECT_EQ(MockGdi::pagingFenceReturnValue, logger->startWaitPagingFenceSave);
}

TEST_F(WddmResidencyLoggerTest, GivenResidencyLoggingEnabledWhenMakeResidentAndWaitPagingOnGpuThenExpectFlagsOff) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = ALLOCATION_HANDLE;
    uint64_t bytesToTrim = 0;
    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    // 2 - one for open log, second for allocation size
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_TRUE(logger->makeResidentCall);
    EXPECT_EQ(MockGdi::pagingFenceReturnValue, logger->makeResidentPagingFence);

    logger->enterWait = true;
    wddm->waitOnGPU(osContext->getWddmContextHandle());
    EXPECT_EQ(5u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_FALSE(logger->makeResidentCall);
    EXPECT_FALSE(logger->enterWait);
    EXPECT_EQ(MockGdi::pagingFenceReturnValue, logger->startWaitPagingFenceSave);
}

TEST_F(WddmResidencyLoggerTest, GivenResidencyLoggingEnabledWhenMakeResidentRequiresTrimToBudgetAndWaitPagingOnGpuThenExpectProperLoggingCount) {
    if (!NEO::wddmResidencyLoggingAvailable) {
        GTEST_SKIP();
    }
    wddm->callBaseCreatePagingLogger = false;
    wddm->callBaseMakeResident = true;

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());
    auto logger = static_cast<MockWddmResidencyLogger *>(wddm->residencyLogger.get());

    D3DKMT_HANDLE handle = INVALID_HANDLE;
    uint64_t bytesToTrim = 0;
    wddm->makeResident(&handle, 1, false, &bytesToTrim, 0x1000);

    // 3 - one for open log, second for allocation size, third reporting on trim to budget
    EXPECT_EQ(3u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_FALSE(logger->makeResidentCall);

    logger->enterWait = true;
    wddm->waitOnGPU(osContext->getWddmContextHandle());
    // additional 4 wait logs - 3 default and 1 extra for trim to budget delta time
    EXPECT_EQ(7u, NEO::IoFunctions::mockVfptrinfCalled);
    EXPECT_FALSE(logger->makeResidentCall);
    EXPECT_FALSE(logger->enterWait);
}

TEST_F(WddmResidencyLoggerTest, givenResidencyLoggingEnabledWhenDefaultDirectorySelectedThenDoNotUseDirectoryName) {
    std::string defaultDirectory("unk");
    std::string filenameLead("pagingfence_device-0x");

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());

    EXPECT_EQ(std::string::npos, MockWddmResidencyLoggerFunctions::recordedFileName.find(defaultDirectory));
    EXPECT_EQ(0u, MockWddmResidencyLoggerFunctions::recordedFileName.find(filenameLead));
}

TEST_F(WddmResidencyLoggerTest, givenResidencyLoggingEnabledWhenNonDefaultDirectorySelectedWithoutTrailingBackslashThenUseDirectoryNameWithAddedSlash) {
    std::string nonDefaultDirectory("c:\\temp\\logs");
    std::string filenameLead("pagingfence_device-0x");

    debugManager.flags.WddmResidencyLoggerOutputDirectory.set(nonDefaultDirectory);

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());

    EXPECT_EQ(0u, MockWddmResidencyLoggerFunctions::recordedFileName.find(nonDefaultDirectory));

    auto backslashPos = nonDefaultDirectory.length();
    auto pos = MockWddmResidencyLoggerFunctions::recordedFileName.find('\\', backslashPos);
    EXPECT_EQ(backslashPos, pos);

    auto filenameLeadPos = backslashPos + 1;
    EXPECT_EQ(filenameLeadPos, MockWddmResidencyLoggerFunctions::recordedFileName.find(filenameLead));
}

TEST_F(WddmResidencyLoggerTest, givenResidencyLoggingEnabledWhenNonDefaultDirectorySelectedWithTrailingBackslashThenUseDirectoryNameWithoutAddingSlash) {
    std::string nonDefaultDirectory("c:\\temp\\logs\\");
    std::string filenameLead("pagingfence_device-0x");

    debugManager.flags.WddmResidencyLoggerOutputDirectory.set(nonDefaultDirectory);

    wddm->createPagingFenceLogger();
    EXPECT_NE(nullptr, wddm->residencyLogger.get());

    EXPECT_EQ(0u, MockWddmResidencyLoggerFunctions::recordedFileName.find(nonDefaultDirectory));

    auto filenameLeadPos = nonDefaultDirectory.length();
    EXPECT_EQ(filenameLeadPos, MockWddmResidencyLoggerFunctions::recordedFileName.find(filenameLead));
}

TEST(VerifyAdapterType, whenAdapterDoesntSupportRenderThenDontCreateHwDeviceId) {
    auto gdi = std::make_unique<MockGdi>();
    auto osEnv = std::make_unique<OsEnvironmentWin>();
    osEnv->gdi.reset(gdi.release());

    LUID shadowAdapterLuid = {0xdd, 0xdd};
    auto hwDeviceId = createHwDeviceIdFromAdapterLuid(*osEnv, shadowAdapterLuid, 1u);
    EXPECT_EQ(nullptr, hwDeviceId.get());
}

TEST(VerifyAdapterType, whenAdapterSupportsRenderThenCreateHwDeviceId) {
    auto gdi = std::make_unique<MockGdi>();
    auto osEnv = std::make_unique<OsEnvironmentWin>();
    osEnv->gdi.reset(gdi.release());

    LUID adapterLuid = {0x12, 0x1234};
    auto hwDeviceId = createHwDeviceIdFromAdapterLuid(*osEnv, adapterLuid, 1u);
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
    EXPECT_EQ(static_cast<uint32_t>(DXGI_RESOURCE_PRIORITY_MAXIMUM), getLastPriorityFcn());

    EXPECT_TRUE(wddm->setAllocationPriority(handles, 2, DXGI_RESOURCE_PRIORITY_NORMAL));
    EXPECT_EQ(static_cast<uint32_t>(DXGI_RESOURCE_PRIORITY_NORMAL), getLastPriorityFcn());
}

TEST_F(WddmTestWithMockGdiDll, givenQueryAdapterInfoCallReturnsSuccessThenPciBusInfoIsValid) {
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

TEST_F(WddmTestWithMockGdiDll, givenForceDeviceIdWhenQueryAdapterInfoThenProperDeviceID) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.ForceDeviceId.set("0x1234");

    EXPECT_TRUE(wddm->queryAdapterInfo());
    uint16_t expectedDeviceId = 0x1234u;
    EXPECT_EQ(expectedDeviceId, wddm->gfxPlatform->usDeviceID);
}

TEST_F(WddmTestWithMockGdiDll, givenNoMaxDualSubSlicesSupportedWhenQueryAdapterInfoThenMaxDualSubSliceIsNotSet) {
    HardwareInfo hwInfo = *defaultHwInfo.get();
    uint32_t maxSS = 8u;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = maxSS;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 0u;

    mockGdiDll.reset(setAdapterInfo(&hwInfo.platform, &hwInfo.gtSystemInfo, hwInfo.capabilityTable.gpuAddressSpace));
    EXPECT_TRUE(wddm->queryAdapterInfo());
    EXPECT_EQ(0u, wddm->getGtSysInfo()->MaxDualSubSlicesSupported);
}

TEST_F(WddmTestWithMockGdiDll, givenNonZeroMaxDualSubSlicesSupportedWhenQueryAdapterInfoThenNothingChanged) {
    HardwareInfo hwInfo = *defaultHwInfo.get();
    uint32_t maxSS = 8u;
    uint32_t expectedMaxDSS = 6u;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = maxSS;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = expectedMaxDSS;

    mockGdiDll.reset(setAdapterInfo(&hwInfo.platform, &hwInfo.gtSystemInfo, hwInfo.capabilityTable.gpuAddressSpace));
    EXPECT_TRUE(wddm->queryAdapterInfo());
    EXPECT_EQ(expectedMaxDSS, wddm->getGtSysInfo()->MaxDualSubSlicesSupported);
    EXPECT_NE(maxSS / 2, wddm->getGtSysInfo()->MaxDualSubSlicesSupported);
}

struct WddmPagingFenceTest : public WddmTest {
    void SetUp() override {
        debugManager.flags.WddmPagingFenceCpuWaitDelayTime.set(WddmPagingFenceTest::defaultTestDelay);
        WddmTest::SetUp();
    }

    DebugManagerStateRestore dbgRestore;
    static constexpr int64_t defaultTestDelay = 10;
};

TEST_F(WddmPagingFenceTest, givenPagingFenceDelayNonZeroWhenCurrentPagingFenceValueNotGreaterThanPagingFenceObjectThenNoPagingFenceDelayCalled) {
    EXPECT_EQ(WddmPagingFenceTest::defaultTestDelay, wddm->pagingFenceDelayTime);
    wddm->mockPagingFence = 0u;
    wddm->currentPagingFenceValue = 1u;

    wddm->waitOnPagingFenceFromCpu(true);
    EXPECT_EQ(0u, wddm->delayPagingFenceFromCpuResult.called);
}

TEST_F(WddmPagingFenceTest, givenPagingFenceDelayNonZeroWhenCurrentPagingFenceValueGreaterThanPagingFenceObjectThenPagingFenceDelayCalled) {
    EXPECT_EQ(WddmPagingFenceTest::defaultTestDelay, wddm->pagingFenceDelayTime);
    wddm->mockPagingFence = 0u;
    wddm->currentPagingFenceValue = 2u;

    wddm->waitOnPagingFenceFromCpu(true);
    EXPECT_EQ(1u, wddm->delayPagingFenceFromCpuResult.called);
}

TEST_F(WddmPagingFenceTest, givenPagingFenceDelayZeroWhenCurrentPagingFenceValueGreaterThanPagingFenceObjectThenNoPagingFenceDelayCalled) {
    EXPECT_EQ(WddmPagingFenceTest::defaultTestDelay, wddm->pagingFenceDelayTime);
    wddm->mockPagingFence = 0u;
    wddm->currentPagingFenceValue = 3u;
    wddm->pagingFenceDelayTime = 0;

    wddm->waitOnPagingFenceFromCpu(true);
    EXPECT_EQ(0u, wddm->delayPagingFenceFromCpuResult.called);
}

uint64_t waitForSynchronizationObjectFromCpuCounter = 0u;
uint64_t lastPassedPagingFence = 0u;
NTSTATUS __stdcall waitForSynchronizationObjectFromCpuNoOpMock(const D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *waitStruct) {
    lastPassedPagingFence = *waitStruct->FenceValueArray;
    waitForSynchronizationObjectFromCpuCounter++;
    return STATUS_SUCCESS;
}
TEST_F(WddmPagingFenceTest, givenPagingFenceDelayZeroWhenCurrentPagingFenceValueGreaterAndNeededKmdWaitThenWaitForSynchronizationObjectFromCpuCalled) {
    EXPECT_EQ(WddmPagingFenceTest::defaultTestDelay, wddm->pagingFenceDelayTime);
    wddm->mockPagingFence = 0u;
    wddm->currentPagingFenceValue = 3u;
    wddm->getGdi()->waitForSynchronizationObjectFromCpu = &waitForSynchronizationObjectFromCpuNoOpMock;
    waitForSynchronizationObjectFromCpuCounter = 0u;
    wddm->waitOnPagingFenceFromCpu(true);
    EXPECT_EQ(lastPassedPagingFence, wddm->currentPagingFenceValue);
    EXPECT_EQ(1u, waitForSynchronizationObjectFromCpuCounter);
}

TEST_F(WddmPagingFenceTest, givenPagingFenceDelayZeroWhenCurrentPagingFenceValueGreaterAndNotNeededKmdWaitThenWaitForSynchronizationObjectFromCpuIsNotCalled) {
    EXPECT_EQ(WddmPagingFenceTest::defaultTestDelay, wddm->pagingFenceDelayTime);
    wddm->mockPagingFence = 0u;
    wddm->currentPagingFenceValue = 3u;
    wddm->getGdi()->waitForSynchronizationObjectFromCpu = &waitForSynchronizationObjectFromCpuNoOpMock;
    waitForSynchronizationObjectFromCpuCounter = 0u;
    wddm->waitOnPagingFenceFromCpu(false);

    EXPECT_EQ(0u, waitForSynchronizationObjectFromCpuCounter);
}
