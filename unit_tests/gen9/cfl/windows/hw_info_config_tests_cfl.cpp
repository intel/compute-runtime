/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/os_interface.h"
#include "unit_tests/os_interface/windows/hw_info_config_win_tests.h"

using namespace NEO;
using namespace std;

using HwInfoConfigTestWindowsCfl = HwInfoConfigTestWindows;

CFLTEST_F(HwInfoConfigTestWindowsCfl, whenCallAdjustPlatformThenDoNothing) {
    EXPECT_EQ(IGFX_COFFEELAKE, productFamily);
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    hwInfoConfig->adjustPlatformForProductFamily(&testHwInfo);

    int ret = memcmp(outHwInfo.pPlatform, testHwInfo.pPlatform, sizeof(PLATFORM));
    EXPECT_EQ(0, ret);
}
