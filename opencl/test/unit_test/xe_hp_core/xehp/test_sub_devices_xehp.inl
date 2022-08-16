/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

using XeHPUsDeviceIdTest = Test<ClDeviceFixture>;

HWTEST_EXCLUDE_PRODUCT(SubDeviceTests, givenCCSEngineWhenCallingGetDefaultEngineWithWaThenTheSameEngineIsReturned, IGFX_XE_HP_SDV);

XEHPTEST_F(XeHPUsDeviceIdTest, givenRevisionAWhenCreatingEngineWithSubdevicesThenEngineTypeIsSetToCCS) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    auto executionEnvironment = new MockExecutionEnvironment;
    MockDevice device(executionEnvironment, 0);
    EXPECT_EQ(0u, device.allEngines.size());
    device.createSubDevices();
    device.createEngines();
    EXPECT_EQ(2u, device.getNumGenericSubDevices());

    auto hwInfo = device.getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
    device.createEngines();
    auto engines = device.getAllEngines();
    for (auto engine : engines) {
        EXPECT_EQ(aub_stream::ENGINE_CCS, engine.osContext->getEngineType());
    }
}

XEHPTEST_F(XeHPUsDeviceIdTest, givenRevisionBWhenCreatingEngineWithSubdevicesThenEngineTypeIsSetToCCS) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    auto executionEnvironment = new MockExecutionEnvironment;
    MockDevice device(executionEnvironment, 0);
    EXPECT_EQ(0u, device.allEngines.size());
    device.createSubDevices();
    device.createEngines();
    EXPECT_EQ(2u, device.getNumGenericSubDevices());

    auto hwInfo = device.getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo);
    device.createEngines();
    auto engines = device.getAllEngines();
    for (auto engine : engines) {
        EXPECT_EQ(aub_stream::ENGINE_CCS, engine.osContext->getEngineType());
    }
}
