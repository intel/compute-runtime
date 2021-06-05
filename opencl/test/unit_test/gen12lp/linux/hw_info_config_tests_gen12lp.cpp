/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/test/unit_test/os_interface/linux/hw_info_config_linux_tests.h"

using namespace NEO;

using HwInfoConfigTestWindowsGen12lp = HwInfoConfigTestLinux;

GEN12LPTEST_F(HwInfoConfigTestWindowsGen12lp, givenGen12LpProductWhenAdjustPlatformForProductFamilyCalledThenOverrideWithCorrectFamily) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    PLATFORM *testPlatform = &outHwInfo.platform;
    testPlatform->eDisplayCoreFamily = IGFX_GEN11_CORE;
    testPlatform->eRenderCoreFamily = IGFX_GEN11_CORE;
    hwInfoConfig->adjustPlatformForProductFamily(&outHwInfo);

    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eRenderCoreFamily);
    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eDisplayCoreFamily);
}
