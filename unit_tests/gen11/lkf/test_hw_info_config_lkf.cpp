/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(LkfHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_LAKEFIELD != productFamily) {
        return;
    }
    GT_SYSTEM_INFO gInfo = {0};
    FeatureTable fTable;
    std::string strConfig = "1x8x8";
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 8u);
    EXPECT_EQ(gInfo.EUCount, 64u);

    strConfig = "default";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 8u);
    EXPECT_EQ(gInfo.EUCount, 64u);

    strConfig = "erroneous";
    gInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig));
    EXPECT_EQ(gInfo.SliceCount, 0u);
    EXPECT_EQ(gInfo.SubSliceCount, 0u);
    EXPECT_EQ(gInfo.EUCount, 0u);
}
