/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/power/linux/mock_sysfs_power.h"

namespace L0 {
namespace ult {

constexpr uint32_t powerHandleComponentCount = 1u;
using SysmanDevicePowerFixtureHelper = SysmanDevicePowerFixture;

TEST_F(SysmanDevicePowerFixtureHelper, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
    }
}

using SysmanDevicePowerMultiDeviceFixtureHelper = SysmanDevicePowerMultiDeviceFixture;

TEST_F(SysmanDevicePowerMultiDeviceFixtureHelper, GivenValidDeviceHandlesAndHwmonInterfaceExistThenSuccessIsReturned) {
    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        PublicLinuxPowerImp *pPowerImp = new PublicLinuxPowerImp(pOsSysman, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.subdeviceId);
        EXPECT_TRUE(pPowerImp->isPowerModuleSupported());
        delete pPowerImp;
    }
}

} // namespace ult
} // namespace L0