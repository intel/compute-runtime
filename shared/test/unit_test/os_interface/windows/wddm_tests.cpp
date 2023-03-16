/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid, uint32_t nodeMask);

using WddmTests = WddmTestWithMockGdiDll;

TEST_F(WddmTests, whenCreatingAllocation64kThenDoNotCreateResource) {
    init();

    D3DKMT_HANDLE handle;
    Gmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 20, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, true, {}, true);

    EXPECT_TRUE(wddm->createAllocation(&gmm, handle));
    auto gdiParam = getMockAllocationFcn();
    EXPECT_EQ(FALSE, gdiParam->Flags.CreateResource);
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
    EXPECT_TRUE(wddm->isDriverAvailable());
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
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::Online);
    wddm->init();
    OsContextWin osContext(*wddm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::Internal}));
    osContext.ensureContextInitialized();
    EXPECT_FALSE(osContext.isDebuggableContext());
}
TEST_F(WddmTests, WhenCreatingContextWithContextCreateDisabledFlagEnabledThenContextHandleIsNull) {
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEO_L0_SYSMAN_NO_CONTEXT_MODE", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    init();
    auto newContext = osContext.get();
    EXPECT_TRUE(wddm->createContext(*newContext));
    EXPECT_EQ(0u, newContext->getWddmContextHandle());
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
    HwDeviceIdWddm *hwDeviceId = new HwDeviceIdWddm(0, {0, 0}, 1u, executionEnvironment->osEnvironment.get(), nullptr);
    wddm->hwDeviceId.reset(hwDeviceId);

    auto luid = wddm->getAdapterLuid();
    EXPECT_TRUE(luid.HighPart == 0 && luid.LowPart == 0);
}

TEST_F(WddmTests, GivenDebugFlagDisablesEvictIfNecessarySupportThenFlagIsFalse) {
    DebugManagerStateRestore restorer{};
    DebugManager.flags.PlaformSupportEvictIfNecessaryFlag.set(0);
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_FALSE(wddm->platformSupportsEvictIfNecessary);
}

TEST_F(WddmTests, GivenDebugFlagEnablesEvictIfNecessarySupportThenFlagIsTrue) {
    DebugManagerStateRestore restorer{};
    DebugManager.flags.PlaformSupportEvictIfNecessaryFlag.set(1);

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_TRUE(wddm->platformSupportsEvictIfNecessary);
}

TEST_F(WddmTests, givenDebugFlagForceEvictOnlyIfNecessaryAllValuesThenForceSettingIsSetCorrectly) {
    DebugManagerStateRestore restorer{};
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_EQ(-1, wddm->forceEvictOnlyIfNecessary);

    DebugManager.flags.ForceEvictOnlyIfNecessaryFlag.set(0);
    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_EQ(0, wddm->forceEvictOnlyIfNecessary);

    DebugManager.flags.ForceEvictOnlyIfNecessaryFlag.set(1);
    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_EQ(1, wddm->forceEvictOnlyIfNecessary);
}

TEST_F(WddmTests, GivengtSystemInfoSliceInfoHasEnabledSlicesAtHigherIndicesThenExpectTopologyMapCreateAndReturnTrue) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo.get()->gtSystemInfo.MaxSlicesSupported = 2;
    defaultHwInfo.get()->gtSystemInfo.IsDynamicallyPopulated = true;
    defaultHwInfo.get()->gtSystemInfo.SliceCount = 1; // Only one slice enabled

    for (auto &sliceInfo : defaultHwInfo->gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }

    defaultHwInfo.get()->gtSystemInfo.SliceInfo[3].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[3].DualSubSliceEnabledCount = 1;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[3].DSSInfo[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[3].DSSInfo[0].SubSlice[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[3].DSSInfo[0].SubSlice[0].EuEnabledCount = 4;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[3].DSSInfo[0].SubSlice[1].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[3].DSSInfo[0].SubSlice[1].EuEnabledCount = 4;

    const HardwareInfo *hwInfo = defaultHwInfo.get();
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo->platform,
                                                         &hwInfo->gtSystemInfo,
                                                         hwInfo->capabilityTable.gpuAddressSpace));

    wddm->rootDeviceEnvironment.executionEnvironment.setDebuggingMode(NEO::DebuggingMode::Online);
    EXPECT_TRUE(wddm->init());
    const auto &topologyMap = wddm->getTopologyMap();
    EXPECT_EQ(topologyMap.size(), 1u);
}

TEST_F(WddmTests, GivenProperTopologyDataAndDebugFlagsEnabledWhenInitializingWddmThenExpectTopologyMapCreateAndReturnTrue) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo.get()->gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
    defaultHwInfo.get()->gtSystemInfo.IsDynamicallyPopulated = true;
    defaultHwInfo.get()->gtSystemInfo.SliceCount = 1; // Only one slice enabled

    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DualSubSliceEnabledCount = 1;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[0].EuEnabledCount = 4;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[1].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[1].EuEnabledCount = 4;

    const HardwareInfo *hwInfo = defaultHwInfo.get();
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo->platform,
                                                         &hwInfo->gtSystemInfo,
                                                         hwInfo->capabilityTable.gpuAddressSpace));

    wddm->rootDeviceEnvironment.executionEnvironment.setDebuggingMode(NEO::DebuggingMode::Online);
    EXPECT_TRUE(wddm->init());
    const auto &topologyMap = wddm->getTopologyMap();
    EXPECT_EQ(topologyMap.size(), 1u);
}

TEST_F(WddmTests, GivenNoSubsliceEnabledAndDebugFlagsEnabledWhenInitializingWddmThenExpectTopologyMapNotCreateAndReturnFalse) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo.get()->gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
    defaultHwInfo.get()->gtSystemInfo.SliceCount = 1; // Only one slice enabled
    defaultHwInfo.get()->gtSystemInfo.IsDynamicallyPopulated = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DualSubSliceEnabledCount = 1;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[0].Enabled = false;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[1].Enabled = false;

    const HardwareInfo *hwInfo = defaultHwInfo.get();
    std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo->platform,
                                                         &hwInfo->gtSystemInfo,
                                                         hwInfo->capabilityTable.gpuAddressSpace));

    wddm->rootDeviceEnvironment.executionEnvironment.setDebuggingMode(NEO::DebuggingMode::Online);
    EXPECT_FALSE(wddm->init());
    const auto &topologyMap = wddm->getTopologyMap();
    EXPECT_TRUE(topologyMap.empty());
}

TEST_F(WddmTests, GivenProperTopologyDataWhenQueryingTopologyThenExpectTrue) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo.get()->gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    defaultHwInfo.get()->gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
    defaultHwInfo.get()->gtSystemInfo.IsDynamicallyPopulated = true;
    defaultHwInfo.get()->gtSystemInfo.SliceCount = 1; // Only one slice enabled

    for (auto &sliceInfo : defaultHwInfo->gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }

    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DualSubSliceEnabledCount = 2;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].Enabled = false; // DSS[0] disabled
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[1].Enabled = true;  // DSS[1] enabled
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[1].SubSlice[0].Enabled = false;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[1].SubSlice[1].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[1].SubSlice[1].EuEnabledCount = 4;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[2].Enabled = false; // DSS[2] disabled
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[3].Enabled = true;  // DSS[3] enabled
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[3].SubSlice[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[3].SubSlice[1].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[3].SubSlice[1].EuEnabledCount = 4;

    wddm->rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
    EXPECT_TRUE(wddm->buildTopologyMapping());
    const auto &topologyMap = wddm->getTopologyMap();
    EXPECT_EQ(topologyMap.size(), 1u);
    EXPECT_EQ(topologyMap.at(0).sliceIndices.size(), 1u);
    EXPECT_EQ(topologyMap.at(0).sliceIndices[0], 0);
    EXPECT_EQ(topologyMap.at(0).subsliceIndices.size(), 3u);
    EXPECT_EQ(topologyMap.at(0).subsliceIndices[0], 3);
    EXPECT_EQ(topologyMap.at(0).subsliceIndices[1], 6);
    EXPECT_EQ(topologyMap.at(0).subsliceIndices[2], 7);
}

TEST_F(WddmTests, GivenMoreThanOneEnabledSliceWhenQueryingTopologyThenExpectTrueAndNoSubSliceIndicesInTopology) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo.get()->gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    defaultHwInfo.get()->gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
    defaultHwInfo.get()->gtSystemInfo.SliceCount = 2;
    defaultHwInfo.get()->gtSystemInfo.IsDynamicallyPopulated = true;

    for (auto &sliceInfo : defaultHwInfo->gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }

    uint32_t index = 1;
    for (uint32_t enabledSliceCount = 0; enabledSliceCount < defaultHwInfo.get()->gtSystemInfo.SliceCount; enabledSliceCount++) {
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[index].Enabled = true;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[index].DualSubSliceEnabledCount = 1;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[index].DSSInfo[0].Enabled = true;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[index].DSSInfo[0].SubSlice[0].Enabled = true;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[index].DSSInfo[0].SubSlice[0].EuEnabledCount = 4;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[index].DSSInfo[0].SubSlice[1].Enabled = true;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[index].DSSInfo[0].SubSlice[1].EuEnabledCount = 4;
        index++;
    }

    wddm->rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
    EXPECT_TRUE(wddm->buildTopologyMapping());
    const auto &topologyMap = wddm->getTopologyMap();
    EXPECT_EQ(topologyMap.size(), 1u);
    EXPECT_EQ(topologyMap.at(0).sliceIndices.size(), 2u);
    EXPECT_EQ(topologyMap.at(0).sliceIndices[0], 1);
    EXPECT_EQ(topologyMap.at(0).sliceIndices[1], 2);
    EXPECT_TRUE(topologyMap.at(0).subsliceIndices.empty());
}

TEST_F(WddmTests, GivenNoSubsliceEnabledWhenQueryingTopologyThenExpectFalse) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo.get()->gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    defaultHwInfo.get()->gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
    defaultHwInfo.get()->gtSystemInfo.SliceCount = 1;
    defaultHwInfo.get()->gtSystemInfo.IsDynamicallyPopulated = true;

    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DualSubSliceEnabledCount = 1;
    // Lets say, DSS 0 is disabled and dss 1 is enabled, thus overall DSS enable count is 1
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].Enabled = false;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[1].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[0].Enabled = false;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[1].Enabled = false;

    wddm->rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
    EXPECT_FALSE(wddm->buildTopologyMapping());
}

TEST_F(WddmTests, GivenNoEuThreadsEnabledWhenQueryingTopologyThenExpectFalse) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo.get()->gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    defaultHwInfo.get()->gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
    defaultHwInfo.get()->gtSystemInfo.SliceCount = 1;
    defaultHwInfo.get()->gtSystemInfo.IsDynamicallyPopulated = true;

    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DualSubSliceEnabledCount = 1;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[0].Enabled = true;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[0].EuEnabledCount = 0;
    defaultHwInfo.get()->gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[1].Enabled = false;
    wddm->rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
    EXPECT_FALSE(wddm->buildTopologyMapping());
}

TEST_F(WddmTests, GivenNoSliceEnabledWhenQueryingTopologyThenExpectFalse) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo.get()->gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    for (auto &sliceInfo : defaultHwInfo->gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }
    wddm->rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
    EXPECT_FALSE(wddm->buildTopologyMapping());
}

TEST_F(WddmTests, GivenPlatformSupportsEvictIfNecessaryWhenAdjustingEvictNeededTrueThenExpectTrue) {
    wddm->platformSupportsEvictIfNecessary = true;
    bool value = wddm->adjustEvictNeededParameter(true);
    EXPECT_TRUE(value);
}

TEST_F(WddmTests, GivenWddmWhenAdditionalAdapterInfoOptionIsSetThenCorrectValueIsReturned) {
    wddm->additionalAdapterInfoOptions = 13u;
    EXPECT_EQ(13u, wddm->getAdditionalAdapterInfoOptions());
}

TEST_F(WddmTests, GivenPlatformNotSupportEvictIfNecessaryWhenAdjustingEvictNeededTrueThenExpectTrue) {
    wddm->platformSupportsEvictIfNecessary = false;
    bool value = wddm->adjustEvictNeededParameter(true);
    EXPECT_TRUE(value);
}

TEST_F(WddmTests, GivenPlatformSupportsEvictIfNecessaryWhenAdjustingEvictNeededFalseThenExpectFalse) {
    wddm->platformSupportsEvictIfNecessary = true;
    bool value = wddm->adjustEvictNeededParameter(false);
    EXPECT_FALSE(value);
}

TEST_F(WddmTests, GivenForceEvictOnlyIfNecessarySetToNotUseTheEvictFlagWhenAdjustingEvictNeededAlwaysIsFalseThenExpectTrue) {
    wddm->platformSupportsEvictIfNecessary = true;
    wddm->forceEvictOnlyIfNecessary = 0;
    bool value = wddm->adjustEvictNeededParameter(false);
    EXPECT_TRUE(value);
}

TEST_F(WddmTests, GivenForceEvictOnlyIfNecessarySetToUseEvictFlagWhenAdjustingEvictNeededAlwaysIsTrueThenExpectFalse) {
    wddm->forceEvictOnlyIfNecessary = 1;
    bool value = wddm->adjustEvictNeededParameter(true);
    EXPECT_FALSE(value);
}

TEST_F(WddmTests, GivenPlatformNotSupportEvictIfNecessaryWhenAdjustingEvictNeededFalseThenExpectTrue) {
    wddm->platformSupportsEvictIfNecessary = false;
    bool value = wddm->adjustEvictNeededParameter(false);
    EXPECT_TRUE(value);
}

uint64_t waitForSynchronizationObjectFromCpuCounter = 0u;

NTSTATUS __stdcall waitForSynchronizationObjectFromCpuNoOpMock(const D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *waitStruct) {
    waitForSynchronizationObjectFromCpuCounter++;
    return STATUS_SUCCESS;
}

class WddmSkipResourceCleanupMock : public WddmMock {
  public:
    using NEO::DriverModel::skipResourceCleanupVar;
};

struct WddmSkipResourceCleanupFixtureWithMockGdiDll : public GdiDllFixture, public MockExecutionEnvironmentGmmFixture {
    void setUp() {
        MockExecutionEnvironmentGmmFixture::setUp();
        GdiDllFixture::setUp();
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
        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        auto engine = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode));
        osContext->ensureContextInitialized();
    }

    void tearDown() {
        osContext.reset(nullptr);
        GdiDllFixture::tearDown();
        MockExecutionEnvironmentGmmFixture::tearDown();
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
    wddm->getGdi()->waitForSynchronizationObjectFromCpu = &waitForSynchronizationObjectFromCpuNoOpMock;
    MonitoredFence monitoredFence = {};
    EXPECT_TRUE(wddm->waitFromCpu(0, monitoredFence));
    EXPECT_EQ(0u, waitForSynchronizationObjectFromCpuCounter);
}

TEST_F(WddmSkipResourceCleanupFixtureTests, givenWaitForSynchronizationObjectFromCpuWhenSkipResourceCleanupIsFalseThenSuccessIsReturnedAndGdiFunctionIsCalled) {
    VariableBackup<uint64_t> varBackup(&waitForSynchronizationObjectFromCpuCounter);
    init();
    wddm->skipResourceCleanupVar = false;
    EXPECT_FALSE(wddm->skipResourceCleanup());
    wddm->getGdi()->waitForSynchronizationObjectFromCpu = &waitForSynchronizationObjectFromCpuNoOpMock;
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
    wddm->getGdi()->waitForSynchronizationObjectFromCpu = &waitForSynchronizationObjectFromCpuNoOpMock;
    uint64_t fenceValue = 1u;
    MonitoredFence monitoredFence = {};
    monitoredFence.lastSubmittedFence = 0u;
    monitoredFence.cpuAddress = &fenceValue;
    EXPECT_TRUE(wddm->waitFromCpu(1u, monitoredFence));
    EXPECT_EQ(0u, waitForSynchronizationObjectFromCpuCounter);
}

TEST(WddmAllocationTest, whenAllocationIsShareableThenSharedHandleToModifyIsSharedHandleOfAllocation) {
    WddmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, true, 1u);
    auto sharedHandleToModify = allocation.getSharedHandleToModify();
    EXPECT_NE(nullptr, sharedHandleToModify);
    *sharedHandleToModify = 1234u;
    uint64_t handle = 0;
    int ret = allocation.peekInternalHandle(nullptr, handle);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(*sharedHandleToModify, handle);
}

TEST(WddmAllocationTest, whenAllocationIsNotShareableThenItDoesntReturnSharedHandleToModifyAndCantPeekInternalHandle) {
    WddmAllocation allocation(0, AllocationType::UNKNOWN, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::MemoryNull, false, 1u);
    auto sharedHandleToModify = allocation.getSharedHandleToModify();
    EXPECT_EQ(nullptr, sharedHandleToModify);
    uint64_t handle = 0;
    int ret = allocation.peekInternalHandle(nullptr, handle);
    EXPECT_NE(ret, 0);
}
} // namespace NEO
