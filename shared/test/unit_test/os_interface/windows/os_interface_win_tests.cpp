/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/os_interface_win_tests.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"

namespace NEO {
extern GMM_INIT_IN_ARGS passedInputArgs;
extern GT_SYSTEM_INFO passedGtSystemInfo;
extern SKU_FEATURE_TABLE passedFtrTable;
extern WA_TABLE passedWaTable;
extern bool copyInputArgs;
} // namespace NEO

TEST_F(OsInterfaceTest, GivenWindowsWhenOsSupportFor64KBpagesIsBeingQueriedThenTrueIsReturned) {
    EXPECT_TRUE(OSInterface::are64kbPagesEnabled());
}

TEST_F(OsInterfaceTest, GivenWindowsWhenCreateEentIsCalledThenValidEventHandleIsReturned) {
    auto ev = NEO::SysCalls::createEvent(NULL, TRUE, FALSE, "DUMMY_EVENT_NAME");
    EXPECT_NE(nullptr, ev);
    auto ret = NEO::SysCalls::closeHandle(ev);
    EXPECT_EQ(TRUE, ret);
}

TEST(OsContextTest, givenWddmWhenCreateOsContextAfterInitWddmThenOsContextIsInitializedTrimCallbackIsRegisteredMemoryOperationsHandlerCreated) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMock(rootDeviceEnvironment);
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
    wddm->init();
    EXPECT_EQ(0u, wddm->registerTrimCallbackResult.called);
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto osContext = std::make_unique<OsContextWin>(*wddm, 0, 0u,
                                                    EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment)[0], preemptionMode));
    osContext->ensureContextInitialized(false);
    EXPECT_EQ(osContext->getWddm(), wddm);
    EXPECT_EQ(1u, wddm->registerTrimCallbackResult.called);
}

TEST_F(OsInterfaceTest, GivenWindowsOsWhenCheckForNewResourceImplicitFlushSupportThenReturnTrue) {
    EXPECT_TRUE(OSInterface::newResourceImplicitFlush);
}

TEST_F(OsInterfaceTest, GivenWindowsOsWhenCheckForGpuIdleImplicitFlushSupportThenReturnFalse) {
    EXPECT_FALSE(OSInterface::gpuIdleImplicitFlush);
}

TEST_F(OsInterfaceTest, GivenDefaultOsInterfaceThenLocalMemoryEnabled) {
    EXPECT_TRUE(OSInterface::osEnableLocalMemory);
}

TEST(OsInterfaceSimpleTest, GivenOsInterfaceWhenCallingGetAggregatedProcessCountThenCallReturnsZero) {
    OSInterface osInterface;
    EXPECT_EQ(0u, osInterface.getAggregatedProcessCount());
}

TEST_F(OsInterfaceTest, whenOsInterfaceSetupGmmInputArgsThenArgsAreSet) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto wddm = new WddmMock(rootDeviceEnvironment);
    EXPECT_EQ(nullptr, rootDeviceEnvironment.osInterface.get());
    wddm->init();
    EXPECT_NE(nullptr, rootDeviceEnvironment.osInterface.get());

    constexpr auto expectedCoreFamily = IGFX_GEN12_CORE;

    wddm->gfxPlatform->eRenderCoreFamily = expectedCoreFamily;
    wddm->gfxPlatform->eDisplayCoreFamily = expectedCoreFamily;
    wddm->deviceRegistryPath = "registryPath";
    auto expectedRegistryPath = wddm->deviceRegistryPath.c_str();
    auto &adapterBDF = wddm->adapterBDF;
    uint32_t bus = 0x12;
    adapterBDF.Bus = bus;
    uint32_t device = 0x34;
    adapterBDF.Device = device;
    uint32_t function = 0x56;
    adapterBDF.Function = function;

    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedGtSystemInfo)> passedGtSystemInfoBackup(&passedGtSystemInfo);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    auto gmmHelper = std::make_unique<GmmHelper>(rootDeviceEnvironment);

    EXPECT_EQ(0, memcmp(&wddm->adapterBDF, &passedInputArgs.stAdapterBDF, sizeof(ADAPTER_BDF)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
    EXPECT_STREQ(expectedRegistryPath, passedInputArgs.DeviceRegistryPath);
    EXPECT_EQ(expectedCoreFamily, passedInputArgs.Platform.eRenderCoreFamily);
    EXPECT_EQ(expectedCoreFamily, passedInputArgs.Platform.eDisplayCoreFamily);
    EXPECT_EQ(wddm->gfxFeatureTable.get(), passedInputArgs.pSkuTable);
    EXPECT_EQ(wddm->gfxWorkaroundTable.get(), passedInputArgs.pWaTable);
    EXPECT_EQ(&rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo, passedInputArgs.pGtSysInfo);
}

TEST_F(OsInterfaceTest, givenEnableFtrTile64OptimizationDebugKeyWhenSetThenProperValueIsPassedToGmmlib) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto wddm = new WddmMock(rootDeviceEnvironment);
    EXPECT_EQ(nullptr, rootDeviceEnvironment.osInterface.get());
    wddm->init();
    EXPECT_NE(nullptr, rootDeviceEnvironment.osInterface.get());

    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedGtSystemInfo)> passedGtSystemInfoBackup(&passedGtSystemInfo);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);
    DebugManagerStateRestore restorer;
    {
        wddm->gfxFeatureTable->FtrTile64Optimization = 1;
        auto gmmHelper = std::make_unique<GmmHelper>(rootDeviceEnvironment);
        EXPECT_EQ(0u, passedFtrTable.FtrTile64Optimization);
    }
    {
        debugManager.flags.EnableFtrTile64Optimization.set(-1);
        wddm->gfxFeatureTable->FtrTile64Optimization = 1;
        auto gmmHelper = std::make_unique<GmmHelper>(rootDeviceEnvironment);
        EXPECT_EQ(1u, passedFtrTable.FtrTile64Optimization);
    }
    {
        debugManager.flags.EnableFtrTile64Optimization.set(-1);
        wddm->gfxFeatureTable->FtrTile64Optimization = 0;
        auto gmmHelper = std::make_unique<GmmHelper>(rootDeviceEnvironment);
        EXPECT_EQ(0u, passedFtrTable.FtrTile64Optimization);
    }

    {
        debugManager.flags.EnableFtrTile64Optimization.set(0);
        wddm->gfxFeatureTable->FtrTile64Optimization = 1;
        auto gmmHelper = std::make_unique<GmmHelper>(rootDeviceEnvironment);
        EXPECT_EQ(0u, passedFtrTable.FtrTile64Optimization);
    }

    {
        debugManager.flags.EnableFtrTile64Optimization.set(1);
        wddm->gfxFeatureTable->FtrTile64Optimization = 0;
        auto gmmHelper = std::make_unique<GmmHelper>(rootDeviceEnvironment);
        EXPECT_EQ(1u, passedFtrTable.FtrTile64Optimization);
    }
}

TEST_F(OsInterfaceTest, whenGetThresholdForStagingCalledThenReturnNoThreshold) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto wddm = new WddmMock(rootDeviceEnvironment);
    EXPECT_EQ(nullptr, rootDeviceEnvironment.osInterface.get());
    wddm->init();
    EXPECT_NE(nullptr, rootDeviceEnvironment.osInterface.get());
    EXPECT_TRUE(rootDeviceEnvironment.osInterface->isSizeWithinThresholdForStaging(MemoryConstants::gigaByte, false));
    EXPECT_TRUE(rootDeviceEnvironment.osInterface->isSizeWithinThresholdForStaging(MemoryConstants::gigaByte, true));
}