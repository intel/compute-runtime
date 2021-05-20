/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/gen12lp/special_ult_helper_gen12lp.h"
#include "opencl/test/unit_test/os_interface/windows/hw_info_config_win_tests.h"

using namespace NEO;

using HwInfoConfigTestWindowsGen12lp = HwInfoConfigTestWindows;

GEN12LPTEST_F(HwInfoConfigTestWindowsGen12lp, givenE2ECSetByKmdWhenConfiguringHwThenAdjustInternalImageFlag) {
    FeatureTable &localFeatureTable = outHwInfo.featureTable;

    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    localFeatureTable.ftrE2ECompression = true;
    hwInfoConfig->configureHardwareCustom(&outHwInfo, nullptr);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedImages);

    localFeatureTable.ftrE2ECompression = false;
    hwInfoConfig->configureHardwareCustom(&outHwInfo, nullptr);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
}

GEN12LPTEST_F(HwInfoConfigTestWindows, givenGen12LpProductWhenAdjustPlatformForProductFamilyCalledThenOverrideWithCorrectFamily) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    PLATFORM *testPlatform = &outHwInfo.platform;
    testPlatform->eDisplayCoreFamily = IGFX_GEN11_CORE;
    testPlatform->eRenderCoreFamily = IGFX_GEN11_CORE;
    hwInfoConfig->adjustPlatformForProductFamily(&outHwInfo);

    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eRenderCoreFamily);
    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eDisplayCoreFamily);
}
