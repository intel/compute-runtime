/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

using namespace NEO;

using XeHPUsDeviceIdTest = Test<ClDeviceFixture>;

HWTEST_EXCLUDE_PRODUCT(SubDeviceTests, givenCCSEngineWhenCallingGetDefaultEngineWithWaThenTheSameEngineIsReturned, IGFX_XE_HP_SDV);

XEHPTEST_F(XeHPUsDeviceIdTest, givenRevisionAWhenCreatingEngineWithSubdevicesThenEngineTypeIsSetToCCS) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    auto executionEnvironment = new MockExecutionEnvironment;
    MockDevice device(executionEnvironment, 0);
    EXPECT_EQ(0u, device.engines.size());
    device.createSubDevices();
    EXPECT_EQ(2u, device.getNumAvailableDevices());

    auto hwInfo = device.getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    hwInfo->platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
    device.createEngines();
    auto engines = device.getEngines();
    for (auto engine : engines) {
        EXPECT_EQ(aub_stream::ENGINE_CCS, engine.osContext->getEngineType());
    }
}

XEHPTEST_F(XeHPUsDeviceIdTest, givenRevisionBWhenCreatingEngineWithSubdevicesThenEngineTypeIsSetToCCS) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    auto executionEnvironment = new MockExecutionEnvironment;
    MockDevice device(executionEnvironment, 0);
    EXPECT_EQ(0u, device.engines.size());
    device.createSubDevices();
    EXPECT_EQ(2u, device.getNumAvailableDevices());

    auto hwInfo = device.getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    hwInfo->platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);
    device.createEngines();
    auto engines = device.getEngines();
    for (auto engine : engines) {
        EXPECT_EQ(aub_stream::ENGINE_CCS, engine.osContext->getEngineType());
    }
}
