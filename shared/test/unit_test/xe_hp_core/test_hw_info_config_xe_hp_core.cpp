/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "platforms.h"

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
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    for (auto revision : {REVISION_A0, REVISION_A1, REVISION_B}) {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(revision, hwInfo);
        if (revision < REVISION_B) {
            EXPECT_FALSE(hwInfoConfig.allowStatelessCompression(hwInfo));
        } else {
            EXPECT_TRUE(hwInfoConfig.allowStatelessCompression(hwInfo));
        }
    }
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenXeHpCoreHwInfoConfigWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    auto hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_FALSE(hwInfoConfig.isDirectSubmissionSupported(hwInfo));
    }

    {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A1, hwInfo);
        EXPECT_FALSE(hwInfoConfig.isDirectSubmissionSupported(hwInfo));
    }

    {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
        EXPECT_TRUE(hwInfoConfig.isDirectSubmissionSupported(hwInfo));
    }
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenHwInfoConfigWhenCreateMultipleSubDevicesThenDontAllowStatelessCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_FALSE(hwInfoConfig.allowStatelessCompression(hwInfo));
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenHwInfoConfigWhenCreateMultipleSubDevicesAndEnableMultitileCompressionThenAllowStatelessCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(4);
    DebugManager.flags.EnableMultiTileCompression.set(1);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(hwInfoConfig.allowStatelessCompression(hwInfo));
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenHwInfoConfigWhenCompressedBuffersAreDisabledThenDontAllowStatelessCompression) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    EXPECT_FALSE(hwInfoConfig.allowStatelessCompression(hwInfo));
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenSteppingWhenAskingForLocalMemoryAccessModeThenDisallowOnA0) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessDisallowed, hwInfoConfig.getLocalMemoryAccessMode(hwInfo));

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::Default, hwInfoConfig.getLocalMemoryAccessMode(hwInfo));
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenXEHPWhenHeapInLocalMemIsCalledThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;
    auto hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);
        EXPECT_FALSE(hwInfoConfig.heapInLocalMem(hwInfo));
    }
    {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
        EXPECT_TRUE(hwInfoConfig.heapInLocalMem(hwInfo));
    }
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenXeHpCoreWhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    const auto &hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_TRUE(hwInfoConfig.isBlitterForImagesSupported());
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenHwInfoConfigWhenIsImplicitScalingSupportedThenExpectTrue) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isImplicitScalingSupported(*defaultHwInfo));
}

XEHPTEST_F(TestXeHPHwInfoConfig, givenHwInfoConfigWhenGetProductConfigThenCorrectMatchIsFound) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_EQ(hwInfoConfig.getProductConfigFromHwInfo(hwInfo), AOT::XEHP_SDV);
}