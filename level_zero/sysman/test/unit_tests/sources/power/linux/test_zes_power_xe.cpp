/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/power/linux/mock_sysfs_power_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

TEST_F(SysmanDevicePowerFixtureXe, GivenKmdInterfaceWhenGettingSysFsFilenamesForPowerForXeVersionThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(pLinuxSysmanImp->getSysmanProductHelper());
    EXPECT_STREQ("energy1_input", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardEnergyCounterNode, 0, false).c_str());
    EXPECT_STREQ("energy2_input", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageEnergyCounterNode, 0, false).c_str());
    EXPECT_STREQ("power1_rated_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageDefaultPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_max", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimit, 0, false).c_str());
    EXPECT_STREQ("power1_max_interval", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageSustainedPowerLimitInterval, 0, false).c_str());
}

TEST_F(SysmanDevicePowerFixtureXe, GivenComponentCountZeroWhenEnumeratingPowerDomainsWhenOnlySysfsNodesExistThenValidCountIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 2u);
}

} // namespace ult
} // namespace Sysman
} // namespace L0