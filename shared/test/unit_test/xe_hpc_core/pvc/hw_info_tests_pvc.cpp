/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using PvcConfigHwInfoTests = ::testing::Test;

PVCTEST_F(PvcConfigHwInfoTests, givenPvcDeviceIdsAndRevisionsWhenCheckingConfigsThenReturnCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;

    for (auto &deviceId : PVC_XL_IDS) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(PVC::isXl(hwInfo));
        EXPECT_FALSE(PVC::isXt(hwInfo));

        hwInfo.platform.usRevId = 0x0;
        EXPECT_TRUE(PVC::isXlA0(hwInfo));
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x1;
        EXPECT_TRUE(PVC::isXlA0(hwInfo));
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x6;
        EXPECT_FALSE(PVC::isXlA0(hwInfo));
        EXPECT_FALSE(PVC::isAtMostXtA0(hwInfo));
    }

    for (auto &deviceId : PVC_XT_IDS) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_FALSE(PVC::isXl(hwInfo));
        EXPECT_TRUE(PVC::isXt(hwInfo));

        hwInfo.platform.usRevId = 0x0;
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x1;
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x3;
        EXPECT_TRUE(PVC::isAtMostXtA0(hwInfo));

        hwInfo.platform.usRevId = 0x5;
        EXPECT_FALSE(PVC::isAtMostXtA0(hwInfo));
    }
}
