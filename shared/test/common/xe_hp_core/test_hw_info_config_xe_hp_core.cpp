/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"

#include "test.h"

using namespace NEO;

using XeHPMaxThreadsTest = Test<DeviceFixture>;

XEHPTEST_F(XeHPMaxThreadsTest, givenXEHPWithA0SteppingThenMaxThreadsForWorkgroupWAIsRequired) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
    auto isWARequired = hwInfoConfig.isMaxThreadsForWorkgroupWARequired(pDevice->getHardwareInfo());
    EXPECT_TRUE(isWARequired);
}

XEHPTEST_F(XeHPMaxThreadsTest, givenXEHPWithBSteppingThenMaxThreadsForWorkgroupWAIsNotRequired) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo);
    auto isWARequired = hwInfoConfig.isMaxThreadsForWorkgroupWARequired(pDevice->getHardwareInfo());
    EXPECT_FALSE(isWARequired);
}