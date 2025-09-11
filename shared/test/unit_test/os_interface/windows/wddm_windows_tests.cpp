/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_callbacks.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/os_interface/windows/dxgi_wrapper.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm_engine_mapper.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/source/utilities/debug_settings_reader.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
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
extern size_t setProcessPowerThrottlingStateCalled;
extern SysCalls::ProcessPowerThrottlingState setProcessPowerThrottlingStateLastValue;
extern size_t setThreadPriorityCalled;
extern SysCalls::ThreadPriority setThreadPriorityLastValue;
extern MEMORY_BASIC_INFORMATION virtualQueryMemoryBasicInformation;
extern size_t closeHandleCalled;
extern BOOL (*sysCallsDuplicateHandle)(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
extern HANDLE (*sysCallsOpenProcess)(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
} // namespace SysCalls
extern uint32_t numRootDevicesToEnum;
extern bool gCreateAllocation2FailOnReadOnlyAllocation;
extern bool gCreateAllocation2ReadOnlyFlagWasPassed;
extern uint32_t gCreateAllocation2NumCalled;
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid, uint32_t adapterNodeOrdinalIn);
} // namespace NEO

namespace GmmHelperFunctionsWindows {
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
} // namespace GmmHelperFunctionsWindows

using namespace NEO;

using Wddm20Tests = WddmTest;
using Wddm20WithMockGdiDllTestsWithoutWddmInit = WddmTestWithMockGdiDll;
using Wddm20InstrumentationTest = WddmInstrumentationTest;
using WddmGfxPartitionTest = Wddm20Tests;

struct Wddm20WithMockGdiDllTests : public Wddm20WithMockGdiDllTestsWithoutWddmInit {
    using Wddm20WithMockGdiDllTestsWithoutWddmInit::TearDown;
    void SetUp() override {
        Wddm20WithMockGdiDllTestsWithoutWddmInit::SetUp();
        init();
    }
};

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

    EXPECT_EQ(outHwInfo.capabilityTable.clVersionSupport, hwInfo->capabilityTable.clVersionSupport);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify, hwInfo->capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds, hwInfo->capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.enableQuickKmdSleep, hwInfo->capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(outHwInfo.capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds, hwInfo->capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
}

HWTEST_F(Wddm20InstrumentationTest, WhenConfiguringDeviceAddressSpaceThenTrueIsReturned) {
    SYSTEM_INFO sysInfo = {};
    WddmMock::getSystemInfo(&sysInfo);

    D3DKMT_HANDLE adapterHandle = ADAPTER_HANDLE;
    D3DKMT_HANDLE deviceHandle = DEVICE_HANDLE;
    const HardwareInfo hwInfo = *defaultHwInfo;
    BOOLEAN ftrL3IACoherency = hwInfo.featureTable.flags.ftrL3IACoherency ? 1 : 0;
    uintptr_t maxAddr = hwInfo.capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress
                            ? reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress) + 1
                            : 0;

    wddm->init();
    EXPECT_EQ(1u, gmmMem->configureDeviceAddressSpaceCalled);
    EXPECT_EQ(adapterHandle, gmmMem->configureDeviceAddressSpaceParamsPassed[0].hAdapter);
    EXPECT_EQ(deviceHandle, gmmMem->configureDeviceAddressSpaceParamsPassed[0].hDevice);
    EXPECT_EQ(wddm->getGdi()->escape.mFunc, gmmMem->configureDeviceAddressSpaceParamsPassed[0].pfnEscape);
    EXPECT_EQ(maxAddr, gmmMem->configureDeviceAddressSpaceParamsPassed[0].svmSize);
    EXPECT_EQ(ftrL3IACoherency, gmmMem->configureDeviceAddressSpaceParamsPassed[0].bdwL3Coherency);
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

TEST_F(Wddm20Tests, givenGraphicsAllocationWhenItIsMappedInHeap0ThenItHasGpuAddressWithinHeapInternalLimits) {
    void *alignedPtr = (void *)0x12000;
    size_t alignedSize = 0x2000;
    std::unique_ptr<Gmm> gmm(GmmHelperFunctionsWindows::getGmm(alignedPtr, alignedSize, getGmmHelper()));
    uint64_t gpuAddress = 0u;
    auto heapBase = wddm->getGfxPartition().Heap32[static_cast<uint32_t>(HeapIndex::heapInternalDeviceMemory)].Base;
    auto heapLimit = wddm->getGfxPartition().Heap32[static_cast<uint32_t>(HeapIndex::heapInternalDeviceMemory)].Limit;

    bool ret = wddm->mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, heapBase, heapLimit, 0u, gpuAddress, AllocationType::unknown);
    EXPECT_TRUE(ret);

    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    auto cannonizedHeapBase = gmmHelper->canonize(heapBase);
    auto cannonizedHeapEnd = gmmHelper->canonize(heapLimit);

    EXPECT_GE(gpuAddress, cannonizedHeapBase);
    EXPECT_LE(gpuAddress, cannonizedHeapEnd);
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
    EXPECT_EQ(heapStandard64KBSize, gfxPartition.getHeapSize(HeapIndex::heapStandard64KB));
    EXPECT_EQ(wddm->gfxPartition.Standard64KB.Base + rootDeviceIndex * heapStandard64KBSize, gfxPartition.getHeapBase(HeapIndex::heapStandard64KB));
}

TEST_F(Wddm20WithMockGdiDllTests, givenDefaultScenarioWhenSetDeviceInfoSucceedsThenDeviceCallbacksWithoutNotifyAubCaptureArePassedToGmmMemory) {
    GMM_DEVICE_CALLBACKS_INT expectedDeviceCb{};
    wddm->init();
    auto gdi = wddm->getGdi();
    auto gmmMemory = static_cast<MockGmmMemoryBase *>(wddm->getGmmMemory());

    expectedDeviceCb.Adapter.KmtHandle = wddm->getAdapter();
    expectedDeviceCb.hDevice.KmtHandle = wddm->getDeviceHandle();
    expectedDeviceCb.hCsr = nullptr;
    expectedDeviceCb.PagingQueue = wddm->getPagingQueue();
    expectedDeviceCb.PagingFence = wddm->getPagingQueueSyncObject();

    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnAllocate = gdi->createAllocation;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate = gdi->destroyAllocation;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate2 = gdi->destroyAllocation2;
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
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnNotifyAubCapture = nullptr;

    EXPECT_EQ(expectedDeviceCb.Adapter.KmtHandle, gmmMemory->deviceCallbacks.Adapter.KmtHandle);
    EXPECT_EQ(expectedDeviceCb.hDevice.KmtHandle, gmmMemory->deviceCallbacks.hDevice.KmtHandle);
    EXPECT_EQ(expectedDeviceCb.hCsr, gmmMemory->deviceCallbacks.hCsr);
    EXPECT_EQ(expectedDeviceCb.PagingQueue, gmmMemory->deviceCallbacks.PagingQueue);
    EXPECT_EQ(expectedDeviceCb.PagingFence, gmmMemory->deviceCallbacks.PagingFence);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnAllocate, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnAllocate);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnDeallocate);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate2, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnDeallocate2);
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

TEST_F(Wddm20WithMockGdiDllTests, givenHwWithAubCaptureWhenSetDeviceInfoSucceedsThenNotifyAubCaptureDeviceCallbackIsPassedToGmmMemory) {

    DebugManagerStateRestore restorer{};
    debugManager.flags.SetCommandStreamReceiver.set(3);
    GMM_DEVICE_CALLBACKS_INT expectedDeviceCb{};
    wddm->init();
    auto gdi = wddm->getGdi();
    auto gmmMemory = static_cast<MockGmmMemoryBase *>(wddm->getGmmMemory());

    expectedDeviceCb.Adapter.KmtHandle = wddm->getAdapter();
    expectedDeviceCb.hDevice.KmtHandle = wddm->getDeviceHandle();
    expectedDeviceCb.hCsr = nullptr;
    expectedDeviceCb.PagingQueue = wddm->getPagingQueue();
    expectedDeviceCb.PagingFence = wddm->getPagingQueueSyncObject();

    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnAllocate = gdi->createAllocation;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate = gdi->destroyAllocation;
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate2 = gdi->destroyAllocation2;
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
    expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnNotifyAubCapture = notifyAubCaptureFuncFactory[defaultHwInfo->platform.eRenderCoreFamily];

    EXPECT_EQ(expectedDeviceCb.Adapter.KmtHandle, gmmMemory->deviceCallbacks.Adapter.KmtHandle);
    EXPECT_EQ(expectedDeviceCb.hDevice.KmtHandle, gmmMemory->deviceCallbacks.hDevice.KmtHandle);
    EXPECT_EQ(expectedDeviceCb.hCsr, gmmMemory->deviceCallbacks.hCsr);
    EXPECT_EQ(expectedDeviceCb.PagingQueue, gmmMemory->deviceCallbacks.PagingQueue);
    EXPECT_EQ(expectedDeviceCb.PagingFence, gmmMemory->deviceCallbacks.PagingFence);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnAllocate, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnAllocate);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnDeallocate);
    EXPECT_EQ(expectedDeviceCb.DevCbPtrs.KmtCbPtrs.pfnDeallocate2, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnDeallocate2);
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
    EXPECT_NE(nullptr, gmmMemory->deviceCallbacks.DevCbPtrs.KmtCbPtrs.pfnNotifyAubCapture);
}

class MockGmmMemoryWindows : public MockGmmMemoryBase {
  public:
    using MockGmmMemoryBase::MockGmmMemoryBase;
    bool setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) override {
        for (int i = 0; i < 3; i++) {
            segmentId[i] = deviceInfo->MsSegId[i];
        }
        adapterLocalMemory = deviceInfo->AdapterLocalMemory;
        adapterCpuVisibleMemory = deviceInfo->AdapterCpuVisibleLocalMemory;
        return MockGmmMemoryBase::setDeviceInfo(deviceInfo);
    }

    uint64_t adapterLocalMemory = 0;
    uint64_t adapterCpuVisibleMemory = 0;
    uint8_t segmentId[3]{};
};

TEST_F(Wddm20WithMockGdiDllTests, whenInitWddmThenAdapterInfoCapsArePassedToGmmLibViaSetDeviceInfo) {
    uint8_t expectedSegmentId[3] = {0x12, 0x34, 0x56};
    uint64_t expectedAdapterLocalMemory = 0x123467800u;
    uint64_t expectedAdapterCpuVisibleMemory = 0x123467A0u;

    wddm->segmentId[0] = 0u;
    wddm->segmentId[1] = 0u;
    wddm->segmentId[2] = 0u;
    wddm->lmemBarSize = 0u;
    wddm->dedicatedVideoMemory = 0u;

    wddm->gmmMemory = std::make_unique<MockGmmMemoryWindows>(getGmmClientContext());
    auto gmmMemory = static_cast<MockGmmMemoryWindows *>(wddm->getGmmMemory());
    wddm->init();

    EXPECT_EQ(1u, gmmMemory->setDeviceInfoCalled);
    EXPECT_EQ(expectedSegmentId[0], gmmMemory->segmentId[0]);
    EXPECT_EQ(expectedSegmentId[1], gmmMemory->segmentId[1]);
    EXPECT_EQ(expectedSegmentId[2], gmmMemory->segmentId[2]);
    EXPECT_EQ(expectedAdapterLocalMemory, gmmMemory->adapterLocalMemory);
    EXPECT_EQ(expectedAdapterCpuVisibleMemory, gmmMemory->adapterCpuVisibleMemory);
}

class MockRegistryReaderWithDriverStorePath : public SettingsReader {
  public:
    MockRegistryReaderWithDriverStorePath(const char *driverStorePathArg) : driverStorePath(driverStorePathArg){};
    std::string getSetting(const char *settingName, const std::string &value, DebugVarPrefix &type) override { return ""; };

    std::string getSetting(const char *settingName, const std::string &value) override {
        std::string key(settingName);
        if (key == "DriverStorePathForComputeRuntime") {
            return driverStorePath;
        } else if (key == "OpenCLDriverName") {
            return driverStorePath;
        }
        return value;
    }

    bool getSetting(const char *settingName, bool defaultValue, DebugVarPrefix &type) override { return defaultValue; };
    bool getSetting(const char *settingName, bool defaultValue) override { return defaultValue; };
    int64_t getSetting(const char *settingName, int64_t defaultValue, DebugVarPrefix &type) override { return defaultValue; };
    int64_t getSetting(const char *settingName, int64_t defaultValue) override { return defaultValue; };
    int32_t getSetting(const char *settingName, int32_t defaultValue, DebugVarPrefix &type) override { return defaultValue; };
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

TEST(WddmDiscoverDevices, WhenMultipleRootDevicesAreAvailableThenAllAreDiscovered) {
    VariableBackup<uint32_t> backup{&numRootDevicesToEnum};
    numRootDevicesToEnum = 3u;
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(numRootDevicesToEnum, hwDeviceIds.size());
}

TEST_F(WddmGfxPartitionTest, WhenInitializingGfxPartitionThenAllHeapsAreInitialized) {
    MockGfxPartition gfxPartition;

    for (auto heap : MockGfxPartition::allHeapNames) {
        ASSERT_FALSE(gfxPartition.heapInitialized(heap));
    }

    wddm->initGfxPartition(gfxPartition, 0, 1, false);

    for (auto heap : MockGfxPartition::allHeapNames) {
        if (!gfxPartition.heapInitialized(heap)) {
            EXPECT_TRUE(heap == HeapIndex::heapSvm || heap == HeapIndex::heapStandard2MB || heap == HeapIndex::heapExtended);
        } else {
            EXPECT_TRUE(gfxPartition.heapInitialized(heap));
        }
    }
}

TEST_F(WddmTestWithMockGdiDll, givenSetProcessPowerThrottlingStateDefaultWhenInitWddmThenPowerThrottlingStateIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetProcessPowerThrottlingState.set(-1);
    SysCalls::setProcessPowerThrottlingStateCalled = 0u;
    SysCalls::setProcessPowerThrottlingStateLastValue = SysCalls::ProcessPowerThrottlingState::Eco;
    wddm->init();
    EXPECT_EQ(0u, SysCalls::setProcessPowerThrottlingStateCalled);
}

TEST_F(WddmTestWithMockGdiDll, givenSetProcessPowerThrottlingState0WhenInitWddmThenPowerThrottlingStateIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetProcessPowerThrottlingState.set(0);
    SysCalls::setProcessPowerThrottlingStateCalled = 0u;
    SysCalls::setProcessPowerThrottlingStateLastValue = SysCalls::ProcessPowerThrottlingState::Eco;
    wddm->init();
    EXPECT_EQ(0u, SysCalls::setProcessPowerThrottlingStateCalled);
}

TEST_F(WddmTestWithMockGdiDll, givenSetProcessPowerThrottlingState1WhenInitWddmThenPowerThrottlingStateIsSetToEco) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetProcessPowerThrottlingState.set(1);
    SysCalls::setProcessPowerThrottlingStateCalled = 0u;
    SysCalls::setProcessPowerThrottlingStateLastValue = SysCalls::ProcessPowerThrottlingState::High;
    wddm->init();
    EXPECT_EQ(1u, SysCalls::setProcessPowerThrottlingStateCalled);
    EXPECT_EQ(SysCalls::ProcessPowerThrottlingState::Eco, SysCalls::setProcessPowerThrottlingStateLastValue);
}

TEST_F(WddmTestWithMockGdiDll, givenSetProcessPowerThrottlingState2WhenInitWddmThenPowerThrottlingStateIsSetToHigh) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetProcessPowerThrottlingState.set(2);
    SysCalls::setProcessPowerThrottlingStateCalled = 0u;
    SysCalls::setProcessPowerThrottlingStateLastValue = SysCalls::ProcessPowerThrottlingState::Eco;
    wddm->init();
    EXPECT_EQ(1u, SysCalls::setProcessPowerThrottlingStateCalled);
    EXPECT_EQ(SysCalls::ProcessPowerThrottlingState::High, SysCalls::setProcessPowerThrottlingStateLastValue);
}

TEST_F(WddmTestWithMockGdiDll, givenSetProcessPowerThrottlingStateUnsupportedWhenInitWddmThenPowerThrottlingStateIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetProcessPowerThrottlingState.set(3);
    SysCalls::setProcessPowerThrottlingStateCalled = 0u;
    SysCalls::setProcessPowerThrottlingStateLastValue = SysCalls::ProcessPowerThrottlingState::Eco;
    wddm->init();
    EXPECT_EQ(0u, SysCalls::setProcessPowerThrottlingStateCalled);
}

TEST_F(WddmTestWithMockGdiDll, givenSetThreadPriorityStateDefaultWhenInitWddmThenThreadPriorityIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetThreadPriority.set(-1);
    SysCalls::setThreadPriorityCalled = 0u;
    SysCalls::setThreadPriorityLastValue = SysCalls::ThreadPriority::Normal;
    wddm->init();
    EXPECT_EQ(0u, SysCalls::setThreadPriorityCalled);
}

TEST_F(WddmTestWithMockGdiDll, givenSetThreadPriorityStateDisabledWhenInitWddmThenThreadPriorityIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetThreadPriority.set(0);
    SysCalls::setThreadPriorityCalled = 0u;
    SysCalls::setThreadPriorityLastValue = SysCalls::ThreadPriority::Normal;
    wddm->init();
    EXPECT_EQ(0u, SysCalls::setThreadPriorityCalled);
}

TEST_F(WddmTestWithMockGdiDll, givenSetThreadPriorityStateEnabledWhenInitWddmThenThreadPriorityIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SetThreadPriority.set(1);
    SysCalls::setThreadPriorityCalled = 0u;
    SysCalls::setThreadPriorityLastValue = SysCalls::ThreadPriority::Normal;
    wddm->init();
    EXPECT_EQ(1u, SysCalls::setThreadPriorityCalled);
    EXPECT_EQ(SysCalls::ThreadPriority::AboveNormal, SysCalls::setThreadPriorityLastValue);
}

TEST_F(WddmTestWithMockGdiDll, whenGettingReadOnlyFlagThenReturnTrueOnlyForPageMisaligedCpuPointer) {
    void *alignedPtr = reinterpret_cast<void *>(MemoryConstants::pageSize);
    EXPECT_FALSE(wddm->getReadOnlyFlagValue(alignedPtr));

    void *misalignedPtr = reinterpret_cast<void *>(MemoryConstants::pageSize + MemoryConstants::cacheLineSize);
    EXPECT_TRUE(wddm->getReadOnlyFlagValue(misalignedPtr));

    EXPECT_FALSE(wddm->getReadOnlyFlagValue(nullptr));
}

TEST_F(WddmTestWithMockGdiDll, whenGettingReadOnlyFlagFallbackSupportThenTrueIsReturned) {
    EXPECT_TRUE(wddm->isReadOnlyFlagFallbackSupported());
}

TEST_F(WddmTestWithMockGdiDll, givenOsHandleDataWithoutParentProcessWhenGettingSharedHandleThenReturnOriginalHandle) {
    uint64_t originalHandle = 0x12345678;
    MemoryManager::OsHandleData osHandleData(originalHandle);

    HANDLE sharedHandle = wddm->getSharedHandle(osHandleData);

    EXPECT_EQ(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(originalHandle)), sharedHandle);
}

TEST_F(WddmTestWithMockGdiDll, givenOsHandleDataWithParentProcessWhenGettingSharedHandleThenDuplicateHandleFromParentProcess) {
    uint64_t originalHandle = 0x12345678;
    uint32_t parentProcessId = 1234;
    MemoryManager::OsHandleData osHandleData(originalHandle);
    osHandleData.parentProcessId = parentProcessId;

    HANDLE mockDuplicatedHandle = reinterpret_cast<HANDLE>(0x8888);

    // Mock openProcess to return a valid handle
    SysCalls::sysCallsOpenProcess = [](DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) -> HANDLE {
        EXPECT_EQ(static_cast<DWORD>(PROCESS_DUP_HANDLE), dwDesiredAccess);
        EXPECT_EQ(FALSE, bInheritHandle);
        EXPECT_EQ(1234u, dwProcessId);
        return reinterpret_cast<HANDLE>(0x9999);
    };

    // Mock duplicateHandle to succeed
    SysCalls::sysCallsDuplicateHandle = [](HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions) -> BOOL {
        EXPECT_EQ(reinterpret_cast<HANDLE>(0x9999), hSourceProcessHandle);
        EXPECT_EQ(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x12345678)), hSourceHandle);
        EXPECT_EQ(GetCurrentProcess(), hTargetProcessHandle);
        EXPECT_EQ(GENERIC_READ | GENERIC_WRITE, dwDesiredAccess);
        EXPECT_EQ(FALSE, bInheritHandle);
        EXPECT_EQ(0u, dwOptions);
        *lpTargetHandle = reinterpret_cast<HANDLE>(0x8888);
        return TRUE;
    };

    size_t closeHandleCallsBefore = SysCalls::closeHandleCalled;

    HANDLE sharedHandle = wddm->getSharedHandle(osHandleData);

    EXPECT_EQ(mockDuplicatedHandle, sharedHandle);
    EXPECT_EQ(closeHandleCallsBefore + 1, SysCalls::closeHandleCalled); // Parent process handle should be closed

    // Cleanup
    SysCalls::sysCallsOpenProcess = nullptr;
    SysCalls::sysCallsDuplicateHandle = nullptr;
}

TEST_F(WddmTestWithMockGdiDll, givenOsHandleDataWithParentProcessWhenOpenProcessFailsThenReturnOriginalHandle) {
    uint64_t originalHandle = 0x12345678;
    uint32_t parentProcessId = 1234;
    MemoryManager::OsHandleData osHandleData(originalHandle);
    osHandleData.parentProcessId = parentProcessId;

    // Mock openProcess to fail
    SysCalls::sysCallsOpenProcess = [](DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) -> HANDLE {
        return nullptr;
    };

    HANDLE sharedHandle = wddm->getSharedHandle(osHandleData);

    EXPECT_EQ(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(originalHandle)), sharedHandle);

    // Cleanup
    SysCalls::sysCallsOpenProcess = nullptr;
}

TEST_F(WddmTestWithMockGdiDll, givenOsHandleDataWithParentProcessWhenDuplicateHandleFailsThenReturnOriginalHandle) {
    uint64_t originalHandle = 0x12345678;
    uint32_t parentProcessId = 1234;
    MemoryManager::OsHandleData osHandleData(originalHandle);
    osHandleData.parentProcessId = parentProcessId;

    // Mock openProcess to succeed
    SysCalls::sysCallsOpenProcess = [](DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) -> HANDLE {
        return reinterpret_cast<HANDLE>(0x9999);
    };

    // Mock duplicateHandle to fail
    SysCalls::sysCallsDuplicateHandle = [](HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions) -> BOOL {
        return FALSE;
    };

    size_t closeHandleCallsBefore = SysCalls::closeHandleCalled;

    HANDLE sharedHandle = wddm->getSharedHandle(osHandleData);

    EXPECT_EQ(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(originalHandle)), sharedHandle);
    EXPECT_EQ(closeHandleCallsBefore + 1, SysCalls::closeHandleCalled); // Parent process handle should still be closed

    // Cleanup
    SysCalls::sysCallsOpenProcess = nullptr;
    SysCalls::sysCallsDuplicateHandle = nullptr;
}
