/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace OCLRT;

TEST(SklHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_SKYLAKE != productFamily) {
        return;
    }
    GT_SYSTEM_INFO gInfo = {0};
    FeatureTable fTable;
    std::string strConfig = "1x3x8";
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 3u);
    EXPECT_EQ(gInfo.EUCount, 23u);

    strConfig = "2x3x8";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 2u);
    EXPECT_EQ(gInfo.SubSliceCount, 6u);
    EXPECT_EQ(gInfo.EUCount, 47u);

    strConfig = "3x3x8";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 3u);
    EXPECT_EQ(gInfo.SubSliceCount, 9u);
    EXPECT_EQ(gInfo.EUCount, 71u);

    strConfig = "1x2x6";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 2u);
    EXPECT_EQ(gInfo.EUCount, 11u);

    strConfig = "1x3x6";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 3u);
    EXPECT_EQ(gInfo.EUCount, 17u);

    strConfig = "default";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 3u);
    EXPECT_EQ(gInfo.EUCount, 23u);

    strConfig = "erroneous";
    gInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig));
    EXPECT_EQ(gInfo.SliceCount, 0u);
    EXPECT_EQ(gInfo.SubSliceCount, 0u);
    EXPECT_EQ(gInfo.EUCount, 0u);
}
