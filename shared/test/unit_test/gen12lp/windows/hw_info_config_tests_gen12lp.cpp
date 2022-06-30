/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"
#include "shared/test/unit_test/os_interface/windows/hw_info_config_win_tests.h"

using namespace NEO;

using HwInfoConfigTestWindowsGen12lp = HwInfoConfigTestWindows;

GEN12LPTEST_F(HwInfoConfigTestWindowsGen12lp, givenE2ECSetByKmdWhenConfiguringHwThenAdjustInternalImageFlag) {
    FeatureTable &localFeatureTable = outHwInfo.featureTable;

    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    localFeatureTable.flags.ftrE2ECompression = true;
    hwInfoConfig->configureHardwareCustom(&outHwInfo, nullptr);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedImages);

    localFeatureTable.flags.ftrE2ECompression = false;
    hwInfoConfig->configureHardwareCustom(&outHwInfo, nullptr);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
}

GEN12LPTEST_F(HwInfoConfigTestWindowsGen12lp, givenGen12LpProductWhenAdjustPlatformForProductFamilyCalledThenOverrideWithCorrectFamily) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    PLATFORM *testPlatform = &outHwInfo.platform;
    testPlatform->eDisplayCoreFamily = IGFX_GEN11_CORE;
    testPlatform->eRenderCoreFamily = IGFX_GEN11_CORE;
    hwInfoConfig->adjustPlatformForProductFamily(&outHwInfo);

    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eRenderCoreFamily);
    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eDisplayCoreFamily);
}

GEN12LPTEST_F(HwInfoConfigTestWindowsGen12lp, givenCompressionFtrEnabledWhenAskingForPageTableManagerThenReturnCorrectValue) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_FALSE(hwInfoConfig.isPageTableManagerSupported(outHwInfo));

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_TRUE(hwInfoConfig.isPageTableManagerSupported(outHwInfo));

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(hwInfoConfig.isPageTableManagerSupported(outHwInfo));

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(hwInfoConfig.isPageTableManagerSupported(outHwInfo));
}

GEN12LPTEST_F(HwInfoConfigTestWindowsGen12lp, givenGen12LpSkuWhenGettingCapabilityCoherencyFlagThenExpectValidValue) {
    auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    bool coherency = false;
    hwInfoConfig.setCapabilityCoherencyFlag(outHwInfo, coherency);
    const bool checkDone = SpecialUltHelperGen12lp::additionalCoherencyCheck(outHwInfo.platform.eProductFamily, coherency);
    if (checkDone) {
        EXPECT_FALSE(coherency);
        return;
    }

    if (SpecialUltHelperGen12lp::isAdditionalCapabilityCoherencyFlagSettingRequired(outHwInfo.platform.eProductFamily)) {
        outHwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A1, outHwInfo);
        hwInfoConfig.setCapabilityCoherencyFlag(outHwInfo, coherency);
        EXPECT_TRUE(coherency);
        outHwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, outHwInfo);
        hwInfoConfig.setCapabilityCoherencyFlag(outHwInfo, coherency);
        EXPECT_FALSE(coherency);
    } else {
        EXPECT_TRUE(coherency);
    }
}
