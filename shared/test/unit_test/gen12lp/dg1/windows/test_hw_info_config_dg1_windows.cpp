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

using Dg1HwInfoConfigWindows = ::testing::Test;

DG1TEST_F(Dg1HwInfoConfigWindows, whenConfigureHwInfoThenBlitterSupportIsEnabled) {
    auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    auto hardwareInfo = *defaultHwInfo;

    hardwareInfo.capabilityTable.blitterOperationsSupported = false;
    hwInfoConfig.configureHardwareCustom(&hardwareInfo, nullptr);

    EXPECT_TRUE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

DG1TEST_F(Dg1HwInfoConfigWindows, givenDg1WhenObtainingBlitterPreferenceThenReturnTrue) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    const auto &hardwareInfo = DG1::hwInfo;

    EXPECT_TRUE(hwInfoConfig.obtainBlitterPreference(hardwareInfo));
}