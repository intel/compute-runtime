/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/preemption.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/helpers/hw_helper.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"

using namespace NEO;

class WddmPreemptionTests : public Test<WddmFixtureWithMockGdiDll> {
  public:
    void SetUp() override {
        WddmFixtureWithMockGdiDll::SetUp();
        const HardwareInfo hwInfo = *platformDevices[0];
        memcpy(&hwInfoTest, &hwInfo, sizeof(hwInfoTest));
        dbgRestorer = new DebugManagerStateRestore();
        wddm->featureTable->ftrGpGpuMidThreadLevelPreempt = true;
    }

    void TearDown() override {
        delete dbgRestorer;
        WddmFixtureWithMockGdiDll::TearDown();
    }

    void createAndInitWddm(unsigned int forceReturnPreemptionRegKeyValue) {
        wddm = static_cast<WddmMock *>(Wddm::createWddm(*executionEnvironment->rootDeviceEnvironments[0].get()));
        executionEnvironment->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->osInterface->get()->setWddm(wddm);
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = executionEnvironment->osInterface.get();
        auto regReader = new RegistryReaderMock();
        wddm->registryReader.reset(regReader);
        regReader->forceRetValue = forceReturnPreemptionRegKeyValue;
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfoTest);
        wddm->init(hwInfoTest);
        osContext = std::make_unique<OsContextWin>(*wddm, 0u, 1, HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode, false);
    }

    DebugManagerStateRestore *dbgRestorer = nullptr;
    HardwareInfo hwInfoTest;
};

TEST_F(WddmPreemptionTests, givenDevicePreemptionEnabledDebugFlagDontForceWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOn) {
    DebugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    unsigned int expectedVal = 1u;
    createAndInitWddm(1u);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagDontForceWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionEnabledDebugFlagDontForceWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    unsigned int expectedVal = 0u;
    createAndInitWddm(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagDontForceWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(-1); // dont force
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagForcePreemptionWhenPreemptionRegKeySetThenSetGpuTimeoutFlagOn) {
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread)); // force preemption
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 1u;
    createAndInitWddm(1u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(WddmPreemptionTests, givenDevicePreemptionDisabledDebugFlagForcePreemptionWhenPreemptionRegKeyNotSetThenSetGpuTimeoutFlagOff) {
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread)); // force preemption
    hwInfoTest.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    unsigned int expectedVal = 0u;
    createAndInitWddm(0u);
    EXPECT_EQ(expectedVal, getMockCreateDeviceParamsFcn().Flags.DisableGpuTimeout);
    EXPECT_EQ(expectedVal, getCreateContextDataFcn()->Flags.DisableGpuTimeout);
}
