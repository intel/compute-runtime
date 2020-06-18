/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/test/unit_test/helpers/hw_helper_tests.h"

using HwHelperTestGen12Lp = HwHelperTest;

TGLLPTEST_F(HwHelperTestGen12Lp, GivenTGLLPWhenConfigureHardwareCustomThenMTPIsNotSet) {
    HwInfoConfig *hwInfoConfig = HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    OSInterface osIface;
    hardwareInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    PreemptionHelper::adjustDefaultPreemptionMode(hardwareInfo.capabilityTable, true, true, true);

    hwInfoConfig->configureHardwareCustom(&hardwareInfo, &osIface);
    EXPECT_FALSE(hardwareInfo.featureTable.ftrGpGpuMidThreadLevelPreempt);
}
