/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "test.h"

using namespace NEO;

using Dg2ConfigHwInfoTests = ::testing::Test;

DG2TEST_F(Dg2ConfigHwInfoTests, givenDg2ConfigWhenSetupHardwearInfoMultiTileThenGtSystemInfoIsSetCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    DG2_CONFIG::setupHardwareInfoMultiTile(&hwInfo, false, false);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
