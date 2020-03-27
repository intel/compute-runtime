/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_interface.h"

#include "opencl/test/unit_test/os_interface/windows/hw_info_config_win_tests.h"

using namespace NEO;

using HwInfoConfigTestWindowsTgllp = HwInfoConfigTestWindows;

TGLLPTEST_F(HwInfoConfigTestWindowsTgllp, whenCallAdjustPlatformThenSetGen12LpFamily) {
    EXPECT_EQ(IGFX_TIGERLAKE_LP, productFamily);

    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    PLATFORM *testPlatform = &outHwInfo.platform;
    testPlatform->eDisplayCoreFamily = IGFX_GEN11_CORE;
    testPlatform->eRenderCoreFamily = IGFX_GEN11_CORE;
    hwInfoConfig->adjustPlatformForProductFamily(&outHwInfo);

    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eRenderCoreFamily);
    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eDisplayCoreFamily);
}
