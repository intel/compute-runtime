/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adlp.h"
#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using AdlpHwInfoLinux = ::testing::Test;
ADLPTEST_F(AdlpHwInfoLinux, givenAdlpConfigWhenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    ADLP::setupHardwareInfoBase(&hwInfo, false, nullptr);

    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
}

ADLPTEST_F(AdlpHwInfoLinux, givenSliceCountZeroWhenSetupHardwareInfoThenNotZeroValuesSetInGtSystemInfo) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo = {0};

    AdlpHwConfig::setupHardwareInfo(&hwInfo, false, nullptr);

    EXPECT_NE(0u, hwInfo.gtSystemInfo.SliceCount);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.EUCount);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.MaxEuPerSubSlice);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.MaxSlicesSupported);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_TRUE(hwInfo.gtSystemInfo.CCSInfo.IsValid);
    EXPECT_NE(0u, hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
}
