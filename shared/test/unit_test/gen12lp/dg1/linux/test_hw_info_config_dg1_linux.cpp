/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Dg1HwInfoConfigLinux = ::testing::Test;

DG1TEST_F(Dg1HwInfoConfigLinux, whenConfigureHwInfoThenBlitterSupportIsDisabled) {
    auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    auto hardwareInfo = *defaultHwInfo;

    hardwareInfo.capabilityTable.blitterOperationsSupported = false;
    hwInfoConfig.configureHardwareCustom(&hardwareInfo, nullptr);

    EXPECT_FALSE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

DG1TEST_F(Dg1HwInfoConfigLinux, givenDg1WhenObtainingBlitterPreferenceThenReturnFalse) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    const auto &hardwareInfo = DG1::hwInfo;

    EXPECT_TRUE(hwInfoConfig.obtainBlitterPreference(hardwareInfo));
}