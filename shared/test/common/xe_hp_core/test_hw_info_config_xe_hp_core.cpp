/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

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

using TestXeHPHwInfoConfig = Test<DeviceFixture>;

XEHPTEST_F(TestXeHPHwInfoConfig, givenHwInfoConfigWhenRevisionIsAtLeastBThenAllowStatelessCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(1);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;

    for (auto revision : {REVISION_A0, REVISION_A1, REVISION_B}) {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(revision, hwInfo);
        if (revision < REVISION_B) {
            EXPECT_FALSE(hwInfoConfig.allowStatelessCompression(hwInfo));
        } else {
            EXPECT_TRUE(hwInfoConfig.allowStatelessCompression(hwInfo));
        }
    }
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenHwInfoConfigWhenCreateMultipleSubDevicesThenDontAllowStatelessCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_FALSE(hwInfoConfig.allowStatelessCompression(hwInfo));
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenHwInfoConfigWhenCreateMultipleSubDevicesAndEnableMultitileCompressionThenAllowStatelessCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    DebugManager.flags.EnableMultiTileCompression.set(1);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(hwInfoConfig.allowStatelessCompression(hwInfo));
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenSteppingWhenAskingForLocalMemoryAccessModeThenDisallowOnA0) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessDisallowed, hwInfoConfig.getLocalMemoryAccessMode(hwInfo));

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::Default, hwInfoConfig.getLocalMemoryAccessMode(hwInfo));
}
