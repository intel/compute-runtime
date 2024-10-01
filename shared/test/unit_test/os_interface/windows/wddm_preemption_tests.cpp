/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

class WddmPreemptionTests : public Test<WddmFixtureWithMockGdiDll> {
  public:
    void SetUp() override {
        WddmFixtureWithMockGdiDll::setUp();
        const HardwareInfo hwInfo = *defaultHwInfo;
        memcpy(&hwInfoTest, &hwInfo, sizeof(hwInfoTest));
        dbgRestorer = new DebugManagerStateRestore();
    }

    void TearDown() override {
        delete dbgRestorer;
        WddmFixtureWithMockGdiDll::tearDown();
    }

    void createAndInitWddm(unsigned int forceReturnPreemptionRegKeyValue) {
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[0].get()));
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
        wddm->enablePreemptionRegValue = forceReturnPreemptionRegKeyValue;
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfoTest);
        wddm->init();
        hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
        ASSERT_NE(nullptr, hwInfo);
        auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        auto engine = gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0];
        osContext = std::make_unique<OsContextWin>(*wddm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode));
        osContext->ensureContextInitialized(false);
    }

    DebugManagerStateRestore *dbgRestorer = nullptr;
    HardwareInfo hwInfoTest;
    const HardwareInfo *hwInfo = nullptr;
};

TEST_F(WddmPreemptionTests, givenDevicePreemptionEnabledDebugFlagDontForceWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOn) {
    debugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    unsigned int expectedVal = 1u;
    createAndInitWddm(1u);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagDontForceWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOff) {
    debugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionEnabledDebugFlagDontForceWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    debugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    unsigned int expectedVal = 0u;
    createAndInitWddm(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagDontForceWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    debugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagForcePreemptionWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOn) {
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread)); // force preemption
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 1u;
    createAndInitWddm(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagForcePreemptionWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread)); // force preemption
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, whenFlagToOverridePreemptionSurfaceSizeIsSetThenSurfaceSizeIsChanged) {
    DebugManagerStateRestore restore;
    uint32_t expectedValue = 123;
    debugManager.flags.OverridePreemptionSurfaceSizeInMb.set(expectedValue);
    createAndInitWddm(0u);
    EXPECT_EQ(expectedValue, hwInfo->gtSystemInfo.CsrSizeInMb);
}

TEST_F(WddmPreemptionTests, whenFlagToOverridePreemptionSurfaceSizeIsNotSetThenSurfaceSizeIsNotChanged) {
    createAndInitWddm(0u);
    EXPECT_EQ(wddm->getGtSysInfo()->CsrSizeInMb, hwInfo->gtSystemInfo.CsrSizeInMb);
}
