/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"
#include "shared/test/unit_test/os_interface/windows/hw_info_config_win_tests.h"

using namespace NEO;

using ProductHelperTestWindowsGen12lp = HwInfoConfigTestWindows;

GEN12LPTEST_F(ProductHelperTestWindowsGen12lp, givenE2ECSetByKmdWhenConfiguringHwThenAdjustInternalImageFlag) {
    FeatureTable &localFeatureTable = outHwInfo.featureTable;

    auto &helper = getHelper<ProductHelper>();

    localFeatureTable.flags.ftrE2ECompression = true;
    helper.configureHardwareCustom(&outHwInfo, nullptr);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedImages);

    localFeatureTable.flags.ftrE2ECompression = false;
    helper.configureHardwareCustom(&outHwInfo, nullptr);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
}

GEN12LPTEST_F(ProductHelperTestWindowsGen12lp, givenGen12LpProductWhenAdjustPlatformForProductFamilyCalledThenOverrideWithCorrectFamily) {
    auto &helper = getHelper<ProductHelper>();

    PLATFORM *testPlatform = &outHwInfo.platform;
    testPlatform->eDisplayCoreFamily = IGFX_GEN11_CORE;
    testPlatform->eRenderCoreFamily = IGFX_GEN11_CORE;
    helper.adjustPlatformForProductFamily(&outHwInfo);

    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eRenderCoreFamily);
    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eDisplayCoreFamily);
}

GEN12LPTEST_F(ProductHelperTestWindowsGen12lp, givenCompressionFtrEnabledWhenAskingForPageTableManagerThenReturnCorrectValue) {
    auto &helper = getHelper<ProductHelper>();

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_FALSE(helper.isPageTableManagerSupported(outHwInfo));

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_TRUE(helper.isPageTableManagerSupported(outHwInfo));

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(helper.isPageTableManagerSupported(outHwInfo));

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(helper.isPageTableManagerSupported(outHwInfo));
}

GEN12LPTEST_F(ProductHelperTestWindowsGen12lp, givenGen12LpSkuWhenGettingCapabilityCoherencyFlagThenExpectValidValue) {
    auto &helper = getHelper<ProductHelper>();
    bool coherency = false;
    helper.setCapabilityCoherencyFlag(outHwInfo, coherency);
    const bool checkDone = SpecialUltHelperGen12lp::additionalCoherencyCheck(outHwInfo.platform.eProductFamily, coherency);
    if (checkDone) {
        EXPECT_FALSE(coherency);
        return;
    }

    if (SpecialUltHelperGen12lp::isAdditionalCapabilityCoherencyFlagSettingRequired(outHwInfo.platform.eProductFamily)) {
        outHwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A1, outHwInfo);
        helper.setCapabilityCoherencyFlag(outHwInfo, coherency);
        EXPECT_TRUE(coherency);
        outHwInfo.platform.usRevId = helper.getHwRevIdFromStepping(REVISION_A0, outHwInfo);
        helper.setCapabilityCoherencyFlag(outHwInfo, coherency);
        EXPECT_FALSE(coherency);
    } else {
        EXPECT_TRUE(coherency);
    }
}
