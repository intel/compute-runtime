/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hp_core/hw_cmds_xe_hp_sdv.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using HwInfoConfigTestXeHpSdv = ::testing::Test;

XEHPTEST_F(HwInfoConfigTestXeHpSdv, givenHwInfoConfigWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isEvictionIfNecessaryFlagSupported());
}

XEHPTEST_F(HwInfoConfigTestXeHpSdv, givenHwInfoConfigWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_FALSE(hwInfoConfig.getScmPropertyThreadArbitrationSupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyCoherencySupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyZPassAsyncSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyPixelAsyncSupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyLargeGrfSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyDevicePreemptionSupport());

    EXPECT_TRUE(hwInfoConfig.getSbaPropertyGlobalAtomicsSupport());
    EXPECT_TRUE(hwInfoConfig.getSbaPropertyStatelessMocsSupport());

    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyScratchSizeSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyStateSipSupport());
    EXPECT_FALSE(hwInfoConfig.getPreemptionDbgPropertyCsrSurfaceSupport());
}
