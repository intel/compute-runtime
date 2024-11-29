/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm_client_context_base.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid, uint32_t nodeMask);

using WddmTests = WddmTestWithMockGdiDll;

TEST_F(WddmTests, whenCreatingAllocation64kThenDoNotCreateResource) {
    init();

    D3DKMT_HANDLE handle;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    Gmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 20, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);

    EXPECT_TRUE(wddm->createAllocation(&gmm, handle));
    auto gdiParam = getMockAllocationFcn();
    EXPECT_EQ(0u, gdiParam->Flags.CreateResource);
    EXPECT_TRUE(wddm->destroyAllocations(&handle, 1, 0));
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

TEST_F(WddmTests, whenCreatingContextThenUmdTypeInPrivateDataIsSetToOpenCL) {
    init();
    auto newContext = osContext.get();
    wddm->createContext(*newContext);
    auto data = getCreateContextDataFcn();
    auto umdType = reinterpret_cast<CREATECONTEXT_PVTDATA *>(data->pPrivateDriverData)->UmdContextType;
    EXPECT_EQ(umdType, UMD_OCL);
}

TEST_F(WddmTests, whenftrEuDebugIsFalseThenDebuggingEnabledReturnsFalse) {
    init();
    EXPECT_FALSE(wddm->isDebugAttachAvailable());
}

TEST_F(WddmTests, whenProgramDebugIsEnabledAndCreatingContextWithInternalEngineThenDebuggableContextReturnsFalse) {
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    wddm->init();
    OsContextWin osContext(*wddm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::internal}));
    osContext.ensureContextInitialized(false);
    EXPECT_FALSE(osContext.isDebuggableContext());
}

TEST_F(WddmTests, WhenCallingInitializeContextWithContextCreateDisabledFlagEnabledThenContextHandleIsNull) {
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEO_L0_SYSMAN_NO_CONTEXT_MODE", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    init();
    auto newContext = osContext.get();
    EXPECT_TRUE(newContext->ensureContextInitialized(false));
    EXPECT_EQ(0u, newContext->getWddmContextHandle());
}

TEST_F(WddmTests, WhenCallingReInitializeContextWithContextCreateDisabledFlagEnabledThenContextHandleIsNull) {
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEO_L0_SYSMAN_NO_CONTEXT_MODE", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);
    init();
    auto newContext = osContext.get();
    newContext->reInitializeContext();
    EXPECT_EQ(0u, newContext->getWddmContextHandle());
}

TEST_F(WddmTests, givenFailedAilInitializationResultWhenInitializingWddmThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    MockRootDeviceEnvironment mockRootDeviceEnvironment(executionEnvironment);
    mockRootDeviceEnvironment.ailInitializationResult = false;

    auto wddm = Wddm::createWddm(nullptr, mockRootDeviceEnvironment);
    auto res = wddm->init();
    EXPECT_FALSE(res);
}

TEST(WddmNewRsourceTest, whenSetNewResourcesBoundToPageTableThenSetInContextFromProperRootDeviceEnvironment) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(2);
    WddmMock *mockWddm;
    for (int i = 0; i < 2; ++i) {
        *executionEnvironment.rootDeviceEnvironments[i]->getMutableHardwareInfo() = *defaultHwInfo;
        auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[i]));
        auto wddmMockInterface = new WddmMockInterface20(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        mockWddm = wddm;
        executionEnvironment.rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        executionEnvironment.rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        executionEnvironment.rootDeviceEnvironments[i]->initHelpers();
    }
    executionEnvironment.initializeMemoryManager();
    auto csr1 = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 0, 1));
    auto csr2 = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1, 1));
    EngineDescriptor engineDesc({aub_stream::ENGINE_CCS, EngineUsage::regular}, 1, PreemptionMode::Disabled, false);
    executionEnvironment.memoryManager->createAndRegisterOsContext(csr1.get(), engineDesc);
    executionEnvironment.memoryManager->createAndRegisterOsContext(csr2.get(), engineDesc);

    auto engines = executionEnvironment.memoryManager->getRegisteredEngines(0);
    for (const auto &engine : engines) {
        EXPECT_EQ(engine.osContext->peekTlbFlushCounter(), 0u);
    }
    engines = executionEnvironment.memoryManager->getRegisteredEngines(1);
    for (const auto &engine : engines) {
        EXPECT_EQ(engine.osContext->peekTlbFlushCounter(), 0u);
    }

    mockWddm->setNewResourceBoundToPageTable();

    engines = executionEnvironment.memoryManager->getRegisteredEngines(0);
    for (const auto &engine : engines) {
        EXPECT_EQ(engine.osContext->peekTlbFlushCounter(), 0u);
    }
    engines = executionEnvironment.memoryManager->getRegisteredEngines(1);
    for (const auto &engine : engines) {
        EXPECT_EQ(engine.osContext->peekTlbFlushCounter(), executionEnvironment.rootDeviceEnvironments[1]->getProductHelper().isTlbFlushRequired());
    }

    executionEnvironment.memoryManager->unregisterEngineForCsr(csr1.get());
    executionEnvironment.memoryManager->unregisterEngineForCsr(csr2.get());
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
    debugManager.flags.PlaformSupportEvictIfNecessaryFlag.set(0);
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_FALSE(wddm->platformSupportsEvictIfNecessary);
}

TEST_F(WddmTests, GivenDebugFlagEnablesEvictIfNecessarySupportThenFlagIsTrue) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PlaformSupportEvictIfNecessaryFlag.set(1);

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_TRUE(wddm->platformSupportsEvictIfNecessary);
}

TEST_F(WddmTests, givenDebugFlagForceEvictOnlyIfNecessaryAllValuesThenForceSettingIsSetCorrectly) {
    DebugManagerStateRestore restorer{};
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_EQ(-1, wddm->forceEvictOnlyIfNecessary);

    debugManager.flags.ForceEvictOnlyIfNecessaryFlag.set(0);
    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_EQ(0, wddm->forceEvictOnlyIfNecessary);

    debugManager.flags.ForceEvictOnlyIfNecessaryFlag.set(1);
    wddm->setPlatformSupportEvictIfNecessaryFlag(productHelper);
    EXPECT_EQ(1, wddm->forceEvictOnlyIfNecessary);
}

TEST_F(WddmTests, GivengtSystemInfoSliceInfoHasEnabledSlicesAtHigherIndicesThenExpectTopologyMapCreateAndReturnTrue) {
    {
        auto hwInfo = *defaultHwInfo;
        hwInfo.gtSystemInfo.MaxSlicesSupported = 2;
        hwInfo.gtSystemInfo.SliceCount = 1; // Only one slice enabled
        hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;

        for (auto &sliceInfo : hwInfo.gtSystemInfo.SliceInfo) {
            sliceInfo.Enabled = false;
        }

        hwInfo.gtSystemInfo.SliceInfo[3].Enabled = true;
        hwInfo.gtSystemInfo.SliceInfo[3].DualSubSliceEnabledCount = 1;
        hwInfo.gtSystemInfo.SliceInfo[3].DSSInfo[0].Enabled = true;
        hwInfo.gtSystemInfo.SliceInfo[3].DSSInfo[0].SubSlice[0].Enabled = true;
        hwInfo.gtSystemInfo.SliceInfo[3].DSSInfo[0].SubSlice[0].EuEnabledCount = 4;
        hwInfo.gtSystemInfo.SliceInfo[3].DSSInfo[0].SubSlice[1].Enabled = true;
        hwInfo.gtSystemInfo.SliceInfo[3].DSSInfo[0].SubSlice[1].EuEnabledCount = 4;

        std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo.platform,
                                                             &hwInfo.gtSystemInfo,
                                                             hwInfo.capabilityTable.gpuAddressSpace));
    }

    EXPECT_TRUE(wddm->init());
    const auto &topologyMap = wddm->getTopologyMap();
    EXPECT_EQ(topologyMap.size(), 1u);
    {
        auto hwInfo = *defaultHwInfo;
        std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo.platform,
                                                             &hwInfo.gtSystemInfo,
                                                             hwInfo.capabilityTable.gpuAddressSpace));
    }
}

TEST_F(WddmTests, GivenProperTopologyDataWhenInitializingWddmThenExpectTopologyMapCreateAndReturnTrue) {
    {
        auto hwInfo = *defaultHwInfo;
        hwInfo.gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
        hwInfo.gtSystemInfo.SliceCount = 1; // Only one slice enabled
        hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;

        hwInfo.gtSystemInfo.SliceInfo[0].Enabled = true;
        hwInfo.gtSystemInfo.SliceInfo[0].DualSubSliceEnabledCount = 1;
        hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[0].Enabled = true;
        hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[0].Enabled = true;
        hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[0].EuEnabledCount = 4;
        hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[1].Enabled = true;
        hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[0].SubSlice[1].EuEnabledCount = 4;

        std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo.platform,
                                                             &hwInfo.gtSystemInfo,
                                                             hwInfo.capabilityTable.gpuAddressSpace));
    }
    EXPECT_TRUE(wddm->init());
    const auto &topologyMap = wddm->getTopologyMap();
    EXPECT_EQ(topologyMap.size(), 1u);
    {
        auto hwInfo = *defaultHwInfo;
        std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo.platform,
                                                             &hwInfo.gtSystemInfo,
                                                             hwInfo.capabilityTable.gpuAddressSpace));
    }
}

TEST_F(WddmTests, GivenNoSubsliceEnabledWhenInitializingWddmThenExpectIntializationFailureAndTopologyMapNotCreated) {
    {
        auto hwInfo = *defaultHwInfo;
        hwInfo.gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
        hwInfo.gtSystemInfo.SliceCount = 1; // Only one slice enabled
        hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;

        for (auto slice = 0; slice < GT_MAX_SLICE; slice++) {
            hwInfo.gtSystemInfo.SliceInfo[slice].DualSubSliceEnabledCount = 1;
            hwInfo.gtSystemInfo.SliceInfo[slice].DSSInfo[0].Enabled = true;
            hwInfo.gtSystemInfo.SliceInfo[slice].DSSInfo[0].SubSlice[0].Enabled = false;
            hwInfo.gtSystemInfo.SliceInfo[slice].DSSInfo[0].SubSlice[1].Enabled = false;
            hwInfo.gtSystemInfo.SliceInfo[slice].SubSliceInfo[0].Enabled = false;
        }
        std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo.platform,
                                                             &hwInfo.gtSystemInfo,
                                                             hwInfo.capabilityTable.gpuAddressSpace));
    }
    EXPECT_FALSE(wddm->init());
    const auto &topologyMap = wddm->getTopologyMap();
    EXPECT_TRUE(topologyMap.empty());
    {
        auto hwInfo = *defaultHwInfo;
        std::unique_ptr<OsLibrary> mockGdiDll(setAdapterInfo(&hwInfo.platform,
                                                             &hwInfo.gtSystemInfo,
                                                             hwInfo.capabilityTable.gpuAddressSpace));
    }
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

    for (auto slice = 0; slice < GT_MAX_SLICE; slice++) {
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DualSubSliceEnabledCount = 1;
        // Lets say, DSS 0 is disabled and dss 1 is enabled, thus overall DSS enable count is 1
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DSSInfo[0].Enabled = false;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DSSInfo[1].Enabled = true;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DSSInfo[0].SubSlice[0].Enabled = false;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DSSInfo[0].SubSlice[1].Enabled = false;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].SubSliceInfo[0].Enabled = false;
    }
    wddm->rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
    EXPECT_FALSE(wddm->buildTopologyMapping());
}

TEST_F(WddmTests, GivenNoEuThreadsEnabledWhenQueryingTopologyThenExpectFalse) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo.get()->gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    defaultHwInfo.get()->gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
    defaultHwInfo.get()->gtSystemInfo.SliceCount = 1;
    defaultHwInfo.get()->gtSystemInfo.IsDynamicallyPopulated = true;

    for (auto slice = 0; slice < GT_MAX_SLICE; slice++) {
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DualSubSliceEnabledCount = 1;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DSSInfo[0].Enabled = true;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DSSInfo[0].SubSlice[0].Enabled = true;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DSSInfo[0].SubSlice[0].EuEnabledCount = 0;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].DSSInfo[0].SubSlice[1].Enabled = false;
        defaultHwInfo.get()->gtSystemInfo.SliceInfo[slice].SubSliceInfo[0].Enabled = false;
    }
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

TEST_F(WddmTests, GivenOnlySubsliceEnabledWhenQueryingTopologyThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo.get();
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    hwInfo.gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    hwInfo.gtSystemInfo.SliceCount = 1; // Only one slice enabled

    for (auto &sliceInfo : hwInfo.gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }

    hwInfo.gtSystemInfo.SliceInfo[0].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[0].DualSubSliceEnabledCount = 0;
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceEnabledCount = 2;
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[0].Enabled = false; // SS[0] disabled
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[1].Enabled = true;  // SS[1] enabled
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[1].EuEnabledCount = 4;
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[2].Enabled = false; // SS[2] disabled
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[3].Enabled = true;  // SS[3] enabled
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[3].EuEnabledCount = 4;

    wddm->rootDeviceEnvironment.setHwInfoAndInitHelpers(&hwInfo);
    EXPECT_TRUE(wddm->buildTopologyMapping());
    const auto &topologyMap = wddm->getTopologyMap();
    EXPECT_EQ(topologyMap.size(), 1u);
    EXPECT_EQ(topologyMap.at(0).sliceIndices.size(), 1u);
    EXPECT_EQ(topologyMap.at(0).sliceIndices[0], 0);
    EXPECT_EQ(topologyMap.at(0).subsliceIndices.size(), 2u);
    EXPECT_EQ(topologyMap.at(0).subsliceIndices[0], 1);
    EXPECT_EQ(topologyMap.at(0).subsliceIndices[1], 3);
}

TEST_F(WddmTests, GivenBothSublicesAndDualSubslicesEnabledWhenQueryingTopologyThenOnlyDSSInfoCounted) {
    HardwareInfo hwInfo = *defaultHwInfo.get();
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    hwInfo.gtSystemInfo.MaxSlicesSupported = GT_MAX_SLICE;
    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    hwInfo.gtSystemInfo.SliceCount = 1; // Only one slice enabled

    for (auto &sliceInfo : hwInfo.gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
    }

    hwInfo.gtSystemInfo.SliceInfo[0].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[0].DualSubSliceEnabledCount = 2;
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[0].Enabled = false; // DSS[0] disabled
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[1].Enabled = true;  // DSS[1] enabled
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[1].SubSlice[0].Enabled = false;
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[1].SubSlice[1].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[1].SubSlice[1].EuEnabledCount = 4;
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[2].Enabled = false; // DSS[2] disabled
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[3].Enabled = true;  // DSS[3] enabled
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[3].SubSlice[0].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[3].SubSlice[1].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[0].DSSInfo[3].SubSlice[1].EuEnabledCount = 4;

    hwInfo.gtSystemInfo.SliceInfo[0].DualSubSliceEnabledCount = 2;
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceEnabledCount = 2;
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[0].Enabled = false; // SS[0] disabled
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[1].Enabled = true;  // SS[1] enabled
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[1].EuEnabledCount = 4;
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[2].Enabled = true; // SS[2] enabled
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[3].Enabled = true; // SS[3] enabled
    hwInfo.gtSystemInfo.SliceInfo[0].SubSliceInfo[3].EuEnabledCount = 4;

    wddm->rootDeviceEnvironment.setHwInfoAndInitHelpers(&hwInfo);
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

TEST_F(WddmTests, whenCallingEvictWithNoAllocationsThenDontCallGdi) {
    uint64_t sizeToTrim = 0;
    wddm->callBaseEvict = true;
    EXPECT_TRUE(wddm->evict(nullptr, 0, sizeToTrim, false));
    bool value = wddm->adjustEvictNeededParameter(false);
    EXPECT_TRUE(value);
}

TEST_F(WddmTests, GivenWddmWhenMapGpuVaCalledThenGmmClientCallsMapGpuVa) {
    wddm->callBaseDestroyAllocations = false;
    wddm->pagingQueue = PAGINGQUEUE_HANDLE;
    auto memoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize}));
    wddm->mapGpuVirtualAddress(allocation);
    EXPECT_GT(reinterpret_cast<MockGmmClientContextBase *>(allocation->getDefaultGmm()->getGmmHelper()->getClientContext())->mapGpuVirtualAddressCalled, 0u);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmTests, givenCheckDeviceStateSetToTrueAndForceExecutionStateWhenSubmitThenProperValueIsReturned) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableDebugBreak.set(false);

    wddm->checkDeviceState = true;
    uint64_t pagingFenceValue = 0u;
    VariableBackup<volatile uint64_t *> pagingFenceBackup(&wddm->pagingFenceAddress, &pagingFenceValue);
    auto executionState = D3DKMT_DEVICEEXECUTION_ERROR_OUTOFMEMORY;
    setMockDeviceExecutionStateFcn(executionState);
    ::testing::internal::CaptureStderr();
    WddmSubmitArguments submitArguments{};
    EXPECT_FALSE(wddm->submit(0, 0, nullptr, submitArguments));
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string("Device execution error, out of memory " + std::to_string(executionState) + "\n"), output);

    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ACTIVE);
    ::testing::internal::CaptureStderr();

    COMMAND_BUFFER_HEADER commandBufferHeader{};
    MonitoredFence monitoredFence{};
    submitArguments.monitorFence = &monitoredFence;
    EXPECT_TRUE(wddm->submit(0, 0, &commandBufferHeader, submitArguments));
    output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);
}

TEST_F(WddmTests, givenCheckDeviceStateSetToTrueWhenCallGetDeviceStateAndForceExecutionStateThenProperMessageIsVisible) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableDebugBreak.set(false);

    wddm->checkDeviceState = true;
    auto executionState = D3DKMT_DEVICEEXECUTION_ERROR_OUTOFMEMORY;
    setMockDeviceExecutionStateFcn(executionState);

    ::testing::internal::CaptureStderr();
    EXPECT_FALSE(wddm->getDeviceState());
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string("Device execution error, out of memory " + std::to_string(executionState) + "\n"), output);

    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ACTIVE);

    ::testing::internal::CaptureStderr();
    EXPECT_TRUE(wddm->getDeviceState());
    output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);
}

TEST_F(WddmTests, givenCheckDeviceStateSetToTrueWhenCallGetDeviceStateReturnsFailThenNoMessageIsVisible) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableDebugBreak.set(false);

    wddm->checkDeviceState = true;
    auto executionState = D3DKMT_DEVICEEXECUTION_ERROR_OUTOFMEMORY;
    setMockDeviceExecutionStateFcn(executionState);
    setMockGetDeviceStateReturnValueFcn(STATUS_SUCCESS + 1, true);

    ::testing::internal::CaptureStderr();
    EXPECT_FALSE(wddm->getDeviceState());
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);

    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ACTIVE);
    setMockGetDeviceStateReturnValueFcn(STATUS_SUCCESS, true);

    ::testing::internal::CaptureStderr();
    EXPECT_TRUE(wddm->getDeviceState());
    output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);
}

TEST_F(WddmTests, givenCheckDeviceStateSetToFalseWhenCallGetDeviceStateAndForceExecutionStateThenNoMessageIsVisible) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableDebugBreak.set(false);

    wddm->checkDeviceState = false;
    auto executionState = D3DKMT_DEVICEEXECUTION_ERROR_OUTOFMEMORY;
    setMockDeviceExecutionStateFcn(executionState);

    ::testing::internal::CaptureStderr();
    EXPECT_TRUE(wddm->getDeviceState());
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);

    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ACTIVE);

    ::testing::internal::CaptureStderr();
    EXPECT_TRUE(wddm->getDeviceState());
    output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);
}

TEST_F(WddmTests, givenDebugFlagSetWhenFailedOnSubmissionThenCheckDeviceState) {
    DebugManagerStateRestore restorer;

    constexpr uint32_t submitId = 0;
    constexpr uint32_t deviceStateId = 1;

    std::vector<uint32_t> operations;

    class MyMockWddm : public WddmMock {
      public:
        MyMockWddm(RootDeviceEnvironment &rootDeviceEnvironment, std::vector<uint32_t> &operations, uint32_t deviceStateId)
            : WddmMock(rootDeviceEnvironment), operations(operations), deviceStateId(deviceStateId) {
        }

        bool getDeviceState() override {
            operations.push_back(deviceStateId);

            return true;
        }

        std::vector<uint32_t> &operations;
        const uint32_t deviceStateId;
    };

    class MyWddmMockInterface20 : public WddmMockInterface20 {
      public:
        MyWddmMockInterface20(Wddm &wddm, std::vector<uint32_t> &operations, uint32_t submitId) : WddmMockInterface20(wddm), operations(operations), submitId(submitId) {
        }

        bool submit(uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments) override {
            operations.push_back(submitId);

            return submitRetVal;
        }

        std::vector<uint32_t> &operations;
        const uint32_t submitId;
        bool submitRetVal = true;
    };

    MyMockWddm myMockWddm(*rootDeviceEnvironment, operations, deviceStateId);
    auto wddmMockInterface = new MyWddmMockInterface20(myMockWddm, operations, submitId);

    myMockWddm.init();

    myMockWddm.wddmInterface.reset(wddmMockInterface);

    COMMAND_BUFFER_HEADER commandBufferHeader{};
    MonitoredFence monitoredFence{};
    WddmSubmitArguments submitArguments{};
    submitArguments.monitorFence = &monitoredFence;

    EXPECT_TRUE(myMockWddm.submit(0, 0, &commandBufferHeader, submitArguments));

    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(deviceStateId, operations[0]);
    EXPECT_EQ(submitId, operations[1]);

    wddmMockInterface->submitRetVal = false;

    EXPECT_FALSE(myMockWddm.submit(0, 0, &commandBufferHeader, submitArguments));

    ASSERT_EQ(4u, operations.size());
    EXPECT_EQ(deviceStateId, operations[2]);
    EXPECT_EQ(submitId, operations[3]);

    debugManager.flags.EnableDeviceStateVerificationAfterFailedSubmission.set(1);

    EXPECT_FALSE(myMockWddm.submit(0, 0, &commandBufferHeader, submitArguments));

    ASSERT_EQ(7u, operations.size());
    EXPECT_EQ(deviceStateId, operations[4]);
    EXPECT_EQ(submitId, operations[5]);
    EXPECT_EQ(deviceStateId, operations[6]);

    wddmMockInterface->submitRetVal = true;

    EXPECT_TRUE(myMockWddm.submit(0, 0, &commandBufferHeader, submitArguments));

    ASSERT_EQ(9u, operations.size());
    EXPECT_EQ(deviceStateId, operations[7]);
    EXPECT_EQ(submitId, operations[8]);
}

TEST_F(WddmTests, givenCheckDeviceStateSetToTrueWhenCallGetDeviceStateReturnsPageFaultThenProperMessageIsVisible) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableDebugBreak.set(false);

    wddm->checkDeviceState = true;
    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ERROR_DMAPAGEFAULT);

    ::testing::internal::CaptureStderr();
    EXPECT_FALSE(wddm->getDeviceState());
    std::string output = testing::internal::GetCapturedStderr();
    std::string expected = "Device execution error, page fault\nfaulted gpuva 0xabc000, pipeline stage 0, bind table entry 2, flags 0x1, error code(is device) 1, error code 10\n";
    EXPECT_EQ(expected, output);

    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ACTIVE);

    ::testing::internal::CaptureStderr();
    EXPECT_TRUE(wddm->getDeviceState());
    output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);
}

TEST_F(WddmTests, givenCheckDeviceStateSetToTrueWhenCallGetDeviceStateReturnsFailureThenBasicMessageDisplayed) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableDebugBreak.set(false);

    wddm->checkDeviceState = true;
    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ERROR_DMAPAGEFAULT);
    setMockGetDeviceStateReturnValueFcn(STATUS_SUCCESS + 1, false);

    ::testing::internal::CaptureStderr();
    EXPECT_FALSE(wddm->getDeviceState());
    std::string output = testing::internal::GetCapturedStderr();
    std::string expected = "Device execution error, page fault\n";
    EXPECT_EQ(expected, output);

    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ACTIVE);
    setMockGetDeviceStateReturnValueFcn(STATUS_SUCCESS, false);

    ::testing::internal::CaptureStderr();
    EXPECT_TRUE(wddm->getDeviceState());
    output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);
}

TEST_F(WddmTests, givenCheckDeviceStateSetToTrueWhenCallGetDeviceStateReturnsOtherNonActiveStateThenGenericMessageDisplayed) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableDebugBreak.set(false);

    wddm->checkDeviceState = true;
    auto executionState = D3DKMT_DEVICEEXECUTION_RESET;
    setMockDeviceExecutionStateFcn(executionState);

    ::testing::internal::CaptureStderr();
    EXPECT_FALSE(wddm->getDeviceState());
    std::string output = testing::internal::GetCapturedStderr();
    std::string expected = std::string("Device execution error " + std::to_string(executionState) + "\n");
    EXPECT_EQ(expected, output);

    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ACTIVE);

    ::testing::internal::CaptureStderr();
    EXPECT_TRUE(wddm->getDeviceState());
    output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string(""), output);
}

TEST_F(WddmTests, givenGetDeviceExecutionStatusWhenGdiCallFailsThenReturnFalse) {
    setMockGetDeviceStateReturnValueFcn(STATUS_SUCCESS + 1, true);
    setMockDeviceExecutionStateFcn(D3DKMT_DEVICEEXECUTION_ACTIVE);

    auto status = wddm->getDeviceExecutionState(D3DKMT_DEVICESTATE_EXECUTION, nullptr);
    EXPECT_FALSE(status);

    setMockGetDeviceStateReturnValueFcn(STATUS_SUCCESS, true);

    status = wddm->getDeviceExecutionState(D3DKMT_DEVICESTATE_EXECUTION, nullptr);
    EXPECT_TRUE(status);
}

TEST_F(WddmTests, givenGetDeviceExecutionStatusWhenUnsupportedStatusProvidedThenReturnsFalse) {
    auto status = wddm->getDeviceExecutionState(D3DKMT_DEVICESTATE_PRESENT, nullptr);
    EXPECT_FALSE(status);
}

TEST_F(WddmTests, givenGetDeviceExecutionStatusWhenGettingPageFaultStatusReturnsFailThenReturnFalse) {
    setMockGetDeviceStateReturnValueFcn(STATUS_SUCCESS + 1, false);
    auto status = wddm->getDeviceExecutionState(D3DKMT_DEVICESTATE_PAGE_FAULT, nullptr);
    EXPECT_FALSE(status);

    setMockGetDeviceStateReturnValueFcn(STATUS_SUCCESS, false);
    status = wddm->getDeviceExecutionState(D3DKMT_DEVICESTATE_PAGE_FAULT, nullptr);
    EXPECT_TRUE(status);
}

TEST(WddmConstructorTest, givenEnableDeviceStateVerificationSetTrueWhenCreateWddmThenCheckDeviceStateIsTrue) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableDeviceStateVerification.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    auto mockWddm = std::make_unique<WddmMock>(*rootDeviceEnvironment);
    EXPECT_TRUE(mockWddm->checkDeviceState);
}

TEST(WddmConstructorTest, givenEnableDeviceStateVerificationSetFalseWhenCreateWddmThenCheckDeviceStateIsFalse) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableDeviceStateVerification.set(0);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    auto mockWddm = std::make_unique<WddmMock>(*rootDeviceEnvironment);
    EXPECT_FALSE(mockWddm->checkDeviceState);
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
        osContext->ensureContextInitialized(false);
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
    EXPECT_TRUE(wddm->waitFromCpu(0, monitoredFence, true));
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
    EXPECT_TRUE(wddm->waitFromCpu(1u, monitoredFence, true));
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
    EXPECT_TRUE(wddm->waitFromCpu(1u, monitoredFence, true));
    EXPECT_EQ(0u, waitForSynchronizationObjectFromCpuCounter);
}

TEST(WddmAllocationTest, whenAllocationIsShareableThenSharedHandleToModifyIsSharedHandleOfAllocation) {
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::memoryNull, true, 1u);
    auto sharedHandleToModify = allocation.getSharedHandleToModify();
    EXPECT_NE(nullptr, sharedHandleToModify);
    *sharedHandleToModify = 1234u;
    uint64_t handle = 0;
    int ret = allocation.peekInternalHandle(nullptr, handle);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(*sharedHandleToModify, handle);
}

TEST(WddmAllocationTest, whenAllocationIsNotShareableThenItDoesntReturnSharedHandleToModifyAndCantPeekInternalHandle) {
    WddmAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, MemoryConstants::pageSize, nullptr, MemoryPool::memoryNull, false, 1u);
    auto sharedHandleToModify = allocation.getSharedHandleToModify();
    EXPECT_EQ(nullptr, sharedHandleToModify);
    uint64_t handle = 0;
    int ret = allocation.peekInternalHandle(nullptr, handle);
    EXPECT_NE(ret, 0);
}

TEST_F(WddmTests, whenInitializeFailureThenInitOsInterfaceWddmFails) {

    auto hwDeviceId = std::make_unique<HwDeviceIdWddm>(0, LUID{0, 0}, 1u, executionEnvironment->osEnvironment.get(), nullptr);

    rootDeviceEnvironment->osInterface.reset();
    auto productHelper = std::make_unique<MockProductHelper>();
    productHelper->configureHwInfoWddmResult = 1; // failure
    rootDeviceEnvironment->productHelper = std::move(productHelper);

    EXPECT_FALSE(rootDeviceEnvironment->initOsInterface(std::move(hwDeviceId), 0u));
}
} // namespace NEO
