/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(IcllpHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_ICELAKE_LP != productFamily) {
        return;
    }
    GT_SYSTEM_INFO gInfo = {0};
    FeatureTable fTable;
    std::string strConfig = "1x8x8";
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 8u);
    EXPECT_EQ(gInfo.EUCount, 63u);

    strConfig = "1x4x8";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 4u);
    EXPECT_EQ(gInfo.EUCount, 31u);

    strConfig = "1x6x8";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 6u);
    EXPECT_EQ(gInfo.EUCount, 47u);

    strConfig = "1x1x8";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 1u);
    EXPECT_EQ(gInfo.EUCount, 8u);

    strConfig = "default";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig);
    EXPECT_EQ(gInfo.SliceCount, 1u);
    EXPECT_EQ(gInfo.SubSliceCount, 8u);
    EXPECT_EQ(gInfo.EUCount, 63u);

    strConfig = "erroneous";
    gInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&gInfo, &fTable, false, strConfig));
    EXPECT_EQ(gInfo.SliceCount, 0u);
    EXPECT_EQ(gInfo.SubSliceCount, 0u);
    EXPECT_EQ(gInfo.EUCount, 0u);
}
