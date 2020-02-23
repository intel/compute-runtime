/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_interface.h"
#include "opencl/test/unit_test/os_interface/windows/hw_info_config_win_tests.h"

using namespace NEO;
using namespace std;

using HwInfoConfigTestWindowsBxt = HwInfoConfigTestWindows;

BXTTEST_F(HwInfoConfigTestWindowsBxt, whenCallAdjustPlatformThenDoNothing) {
    EXPECT_EQ(IGFX_BROXTON, productFamily);
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    outHwInfo = pInHwInfo;
    hwInfoConfig->adjustPlatformForProductFamily(&outHwInfo);

    int ret = memcmp(&outHwInfo.platform, &pInHwInfo.platform, sizeof(PLATFORM));
    EXPECT_EQ(0, ret);
}
