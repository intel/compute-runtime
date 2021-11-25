/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/hw_helper_tests.h"

using HwHelperTestGen12Lp = HwHelperTest;

DG1TEST_F(HwHelperTestGen12Lp, GivenDG1WhenConfigureHardwareCustomThenMTPIsNotSet) {
    HwInfoConfig *hwInfoConfig = HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    OSInterface osIface;
    hardwareInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    PreemptionHelper::adjustDefaultPreemptionMode(hardwareInfo.capabilityTable, true, true, true);

    hwInfoConfig->configureHardwareCustom(&hardwareInfo, &osIface);
    EXPECT_FALSE(hardwareInfo.featureTable.flags.ftrGpGpuMidThreadLevelPreempt);
}

DG1TEST_F(HwHelperTestGen12Lp, GivenDG1WhenConfigureHardwareCustomThenKmdNotifyIsEnabled) {
    HwInfoConfig *hwInfoConfig = HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    OSInterface osIface;
    hwInfoConfig->configureHardwareCustom(&hardwareInfo, &osIface);
    EXPECT_TRUE(hardwareInfo.capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(300ll, hardwareInfo.capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
}
