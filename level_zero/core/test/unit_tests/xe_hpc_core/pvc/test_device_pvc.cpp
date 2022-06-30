/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_imp.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/xe_hpc_core/xe_hpc_core_test_l0_fixtures.h"

namespace L0 {
namespace ult {

using DeviceTestPvcXt = Test<DeviceFixtureXeHpcTests>;

PVCTEST_F(DeviceTestPvcXt, whenCallingGetMemoryPropertiesWithNonNullPtrAndRevisionIsNotBaseDieA0OnPvcXtThenMaxClockRateReturnedIsZero) {
    auto &device = driverHandle->devices[0];
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usDeviceID = PVC_XT_IDS.front();
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo); // not BD A0

    checkIfCallingGetMemoryPropertiesWithNonNullPtrThenMaxClockRateReturnZero(hwInfo);
}

using DeviceTestPvc = Test<DeviceFixtureXeHpcTests>;
PVCTEST_F(DeviceTestPvc, givenPvcAStepWhenCreatingMultiTileDeviceThenExpectImplicitScalingDisabled) {
    auto hwInfo = *NEO::defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);

    DebugManager.flags.CreateMultipleSubDevices.set(2);
    VariableBackup<bool> apiSupportBackup(&NEO::ImplicitScaling::apiSupport, true);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    std::unique_ptr<DriverHandleImp> driverHandle(new DriverHandleImp);

    auto neoDevice = std::unique_ptr<NEO::Device>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    auto device = Device::create(driverHandle.get(), neoDevice.release(), false, &returnValue);
    ASSERT_NE(nullptr, device);

    EXPECT_FALSE(device->isImplicitScalingCapable());

    static_cast<DeviceImp *>(device)->releaseResources();
    delete device;
}

PVCTEST_F(DeviceTestPvc, whenCallingGetMemoryPropertiesWithNonNullPtrThenPropertiesAreReturned) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    ze_device_memory_properties_t memProperties = {};
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    EXPECT_EQ(memProperties.maxClockRate, 3200u);
    EXPECT_EQ(memProperties.maxBusWidth, this->neoDevice->getDeviceInfo().addressBits);
    EXPECT_EQ(memProperties.totalSize, this->neoDevice->getDeviceInfo().globalMemSize);
}

PVCTEST_F(DeviceTestPvc, GivenPvcWhenGettingPhysicalEuSimdWidthThenReturn16) {
    ze_device_properties_t properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&properties);
    EXPECT_EQ(16u, properties.physicalEUSimdWidth);
}

} // namespace ult
} // namespace L0
