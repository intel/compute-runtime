/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

using WddmTests = WddmTestWithMockGdiDll;

TEST_F(WddmTests, whenCreatingAllocation64kThenDoNotCreateResource) {
    init();

    D3DKMT_HANDLE handle;
    Gmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 20, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, true, {}, true);

    EXPECT_TRUE(wddm->createAllocation(&gmm, handle));
    auto gdiParam = getMockAllocationFcn();
    EXPECT_EQ(FALSE, gdiParam->Flags.CreateResource);
}

TEST_F(WddmTests, whenInitializingWddmThenSetMinAddressToCorrectValue) {
    constexpr static uintptr_t mockedInternalGpuVaRange = 0x9876u;
    auto gmmMemory = new MockGmmMemoryBase(wddm->rootDeviceEnvironment.getGmmClientContext());
    gmmMemory->overrideInternalGpuVaRangeLimit(mockedInternalGpuVaRange);
    wddm->gmmMemory.reset(gmmMemory);

    ASSERT_EQ(0u, wddm->getWddmMinAddress());
    wddm->init();

    const bool obtainFromGmm = defaultHwInfo->platform.eRenderCoreFamily == IGFX_GEN12LP_CORE;
    const auto expectedMinAddress = obtainFromGmm ? mockedInternalGpuVaRange : windowsMinAddress;
    ASSERT_EQ(expectedMinAddress, wddm->getWddmMinAddress());
}

TEST_F(WddmTests, whenInitializingWddmThenSetTimestampFrequencyToCorrectValue) {
    EXPECT_EQ(0u, wddm->timestampFrequency);
    init();
    EXPECT_EQ(1u, wddm->timestampFrequency);
}

TEST_F(WddmTests, givenWddmWhenPassesCorrectHandleToVerifySharedHandleThenReturnTrue) {
    init();
    D3DKMT_HANDLE handle = 1u;
    EXPECT_TRUE(wddm->verifySharedHandle(handle));
}

TEST_F(WddmTests, givenWddmWhenPassesIncorrectHandleToVerifySharedHandleThenReturnFalse) {
    init();
    D3DKMT_HANDLE handle = 0u;
    EXPECT_FALSE(wddm->verifySharedHandle(handle));
}

TEST_F(WddmTests, givenWddmWhenPassesCorrectHandleToVerifyNTHandleThenReturnTrue) {
    init();
    uint32_t temp = 0;
    HANDLE handle = &temp;
    EXPECT_TRUE(wddm->verifyNTHandle(handle));
}

TEST_F(WddmTests, givenWddmWhenPassesIncorrectHandleToVerifyNTHandleThenReturnFalse) {
    init();
    HANDLE handle = nullptr;
    EXPECT_FALSE(wddm->verifyNTHandle(handle));
}

TEST_F(WddmTests, whenCheckedIfResourcesCleanupCanBeSkippedThenReturnsFalse) {
    init();
    EXPECT_FALSE(wddm->skipResourceCleanup());
    EXPECT_TRUE(wddm->isDriverAvaliable());
}

TEST_F(WddmTests, whenCheckedIfDebugAttachAvailableThenReturnsFalse) {
    init();
    EXPECT_FALSE(wddm->isDebugAttachAvailable());
}

TEST_F(WddmTests, whenCreatingContextWithPowerHintSuccessIsReturned) {
    init();
    auto newContext = osContext.get();
    newContext->setUmdPowerHintValue(1);
    EXPECT_EQ(1, newContext->getUmdPowerHintValue());
    wddm->createContext(*newContext);
    EXPECT_TRUE(wddm->createContext(*newContext));
}

TEST_F(WddmTests, whenftrEuDebugIsFalseThenDebuggingEnabledReturnsFalse) {
    init();
    EXPECT_FALSE(wddm->isDebugAttachAvailable());
}

TEST_F(WddmTests, whenProgramDebugIsEnabledAndCreatingContextWithInternalEngineThenDebuggableContextReturnsFalse) {
    executionEnvironment->setDebuggingEnabled();
    wddm->init();
    OsContextWin osContext(*wddm, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::Internal}));
    osContext.ensureContextInitialized();
    EXPECT_FALSE(osContext.isDebuggableContext());
}

TEST(WddmPciSpeedInfoTest, WhenGetPciSpeedInfoIsCalledThenUnknownIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = Wddm::createWddm(nullptr, rootDeviceEnvironment);
    wddm->init();
    auto speedInfo = wddm->getPciSpeedInfo();

    EXPECT_EQ(-1, speedInfo.genVersion);
    EXPECT_EQ(-1, speedInfo.width);
    EXPECT_EQ(-1, speedInfo.maxBandwidth);
}

TEST_F(WddmTests, whenGetAdapterLuidThenLuidIsReturned) {
    HwDeviceIdWddm *hwDeviceId = new HwDeviceIdWddm(0, {0, 0}, executionEnvironment->osEnvironment.get(), nullptr);
    wddm->hwDeviceId.reset(hwDeviceId);

    auto luid = wddm->getAdapterLuid();
    EXPECT_TRUE(luid.HighPart == 0 && luid.LowPart == 0);
}

uint64_t waitForSynchronizationObjectFromCpuCounter = 0u;

NTSTATUS __stdcall waitForSynchronizationObjectFromCpuNoOp(const D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *waitStruct) {
    waitForSynchronizationObjectFromCpuCounter++;
    return STATUS_SUCCESS;
}

class WddmSkipResourceCleanupMock : public WddmMock {
  public:
    using NEO::DriverModel::skipResourceCleanupVar;
};

struct WddmSkipResourceCleanupFixtureWithMockGdiDll : public GdiDllFixture, public MockExecutionEnvironmentGmmFixture {
    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::SetUp();
        GdiDllFixture::SetUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        wddm = static_cast<WddmSkipResourceCleanupMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        wddmMockInterface = new WddmMockInterface20(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = rootDeviceEnvironment->osInterface.get();
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.release());
        wddm->init();
        wddm->wddmInterface.reset(wddmMockInterface);

        auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
        auto engine = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0u, EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode));
        osContext->ensureContextInitialized();
    }

    void TearDown() override {
        osContext.reset(nullptr);
        GdiDllFixture::TearDown();
        MockExecutionEnvironmentGmmFixture::TearDown();
    }

    WddmSkipResourceCleanupMock *wddm = nullptr;
    OSInterface *osInterface;
    std::unique_ptr<OsContextWin> osContext;
    WddmMockInterface20 *wddmMockInterface = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

using WddmSkipResourceCleanupFixtureTests = Test<WddmSkipResourceCleanupFixtureWithMockGdiDll>;

TEST_F(WddmSkipResourceCleanupFixtureTests, givenWaitForSynchronizationObjectFromCpuWhenSkipResourceCleanupIsTrueThenSuccessIsReturnedAndGdiFunctionIsNotCalled) {
    VariableBackup<uint64_t> varBackup(&waitForSynchronizationObjectFromCpuCounter);
    init();
    wddm->skipResourceCleanupVar = true;
    EXPECT_TRUE(wddm->skipResourceCleanup());
    wddm->getGdi()->waitForSynchronizationObjectFromCpu = &waitForSynchronizationObjectFromCpuNoOp;
    MonitoredFence monitoredFence = {};
    EXPECT_TRUE(wddm->waitFromCpu(0, monitoredFence));
    EXPECT_EQ(0u, waitForSynchronizationObjectFromCpuCounter);
}

TEST_F(WddmSkipResourceCleanupFixtureTests, givenWaitForSynchronizationObjectFromCpuWhenSkipResourceCleanupIsFalseThenSuccessIsReturnedAndGdiFunctionIsCalled) {
    VariableBackup<uint64_t> varBackup(&waitForSynchronizationObjectFromCpuCounter);
    init();
    wddm->skipResourceCleanupVar = false;
    EXPECT_FALSE(wddm->skipResourceCleanup());
    wddm->getGdi()->waitForSynchronizationObjectFromCpu = &waitForSynchronizationObjectFromCpuNoOp;
    uint64_t fenceValue = 0u;
    D3DKMT_HANDLE fenceHandle = 1u;
    MonitoredFence monitoredFence = {};
    monitoredFence.lastSubmittedFence = 1u;
    monitoredFence.cpuAddress = &fenceValue;
    monitoredFence.fenceHandle = fenceHandle;
    EXPECT_TRUE(wddm->waitFromCpu(1u, monitoredFence));
    EXPECT_EQ(1u, waitForSynchronizationObjectFromCpuCounter);
}

TEST_F(WddmSkipResourceCleanupFixtureTests, givenWaitForSynchronizationObjectFromCpuWhenSkipResourceCleanupIsFalseAndFenceWasNotUpdatedThenSuccessIsReturnedAndGdiFunctionIsNotCalled) {
    VariableBackup<uint64_t> varBackup(&waitForSynchronizationObjectFromCpuCounter);
    init();
    wddm->skipResourceCleanupVar = false;
    EXPECT_FALSE(wddm->skipResourceCleanup());
    wddm->getGdi()->waitForSynchronizationObjectFromCpu = &waitForSynchronizationObjectFromCpuNoOp;
    uint64_t fenceValue = 1u;
    MonitoredFence monitoredFence = {};
    monitoredFence.lastSubmittedFence = 0u;
    monitoredFence.cpuAddress = &fenceValue;
    EXPECT_TRUE(wddm->waitFromCpu(1u, monitoredFence));
    EXPECT_EQ(0u, waitForSynchronizationObjectFromCpuCounter);
}

} // namespace NEO
