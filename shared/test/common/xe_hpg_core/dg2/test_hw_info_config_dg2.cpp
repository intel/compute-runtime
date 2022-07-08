/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using TestDg2HwInfoConfig = Test<DeviceFixture>;

HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTestWindows, givenHardwareInfoWhenCallingIsAdditionalStateBaseAddressWARequiredThenFalseIsReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTestWindows, givenHardwareInfoWhenCallingIsMaxThreadsForWorkgroupWARequiredThenFalseIsReturned, IGFX_DG2);

DG2TEST_F(TestDg2HwInfoConfig, givenDG2WithCSteppingThenAdditionalStateBaseAddressWAIsNotRequired) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_C, *hwInfo);
    auto isWARequired = hwInfoConfig.isAdditionalStateBaseAddressWARequired(pDevice->getHardwareInfo());
    EXPECT_FALSE(isWARequired);
}

DG2TEST_F(TestDg2HwInfoConfig, givenDG2WithBSteppingThenAdditionalStateBaseAddressWAIsRequired) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo);
    auto isWARequired = hwInfoConfig.isAdditionalStateBaseAddressWARequired(pDevice->getHardwareInfo());
    EXPECT_TRUE(isWARequired);
}

DG2TEST_F(TestDg2HwInfoConfig, givenDG2WithA0SteppingThenMaxThreadsForWorkgroupWAIsRequired) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
    for (const auto &devId : dg2G10DeviceIds) {
        hwInfo->platform.usDeviceID = devId;
        auto isWARequired = hwInfoConfig.isMaxThreadsForWorkgroupWARequired(pDevice->getHardwareInfo());
        EXPECT_TRUE(isWARequired);
    }
}

DG2TEST_F(TestDg2HwInfoConfig, givenDG2WithBSteppingThenMaxThreadsForWorkgroupWAIsNotRequired) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A1, *hwInfo);
    auto isWARequired = hwInfoConfig.isMaxThreadsForWorkgroupWARequired(pDevice->getHardwareInfo());
    EXPECT_FALSE(isWARequired);
}

DG2TEST_F(TestDg2HwInfoConfig, givenSteppingWhenAskingForLocalMemoryAccessModeThenDisallowOnA0) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessDisallowed, hwInfoConfig.getLocalMemoryAccessMode(hwInfo));

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::Default, hwInfoConfig.getLocalMemoryAccessMode(hwInfo));
}

DG2TEST_F(TestDg2HwInfoConfig, givenDG2HwInfoConfigWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    auto hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isDirectSubmissionSupported(hwInfo));
}
