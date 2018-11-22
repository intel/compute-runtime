/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace OCLRT;

TEST(CnlHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_CANNONLAKE != productFamily) {
        return;
    }
    GT_SYSTEM_INFO gInfo;
    FeatureTable fTable;
    std::string strConfig = "1x2x8";
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 2u);
    EXPECT_EQ(gInfo.EUCount, 15u);

    strConfig = "1x3x8";
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 3u);
    EXPECT_EQ(gInfo.EUCount, 23u);

    strConfig = "2x4x8";
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 2u);
    EXPECT_EQ(gInfo.SubSliceCount, 8u);
    EXPECT_EQ(gInfo.EUCount, 31u);

    strConfig = "2x5x8";
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 2u);
    EXPECT_EQ(gInfo.SubSliceCount, 10u);
    EXPECT_EQ(gInfo.EUCount, 39u);

    strConfig = "4x9x8";
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 4u);
    EXPECT_EQ(gInfo.SubSliceCount, 36u);
    EXPECT_EQ(gInfo.EUCount, 71u);

    strConfig = "default";
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 2u);
    EXPECT_EQ(gInfo.SubSliceCount, 10u);
    EXPECT_EQ(gInfo.EUCount, 39u);

    strConfig = "erroneous";
    gInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig));
    EXPECT_EQ(gInfo.SliceCount, 0u);
    EXPECT_EQ(gInfo.SubSliceCount, 0u);
    EXPECT_EQ(gInfo.EUCount, 0u);
}
