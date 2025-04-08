/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

using namespace NEO;

using Gen12lpProductHelper = ProductHelperTest;

GEN12LPTEST_F(Gen12lpProductHelper, givenGen12LpProductWhenAdjustPlatformForProductFamilyCalledThenOverrideWithCorrectFamily) {

    PLATFORM *testPlatform = &outHwInfo.platform;
    testPlatform->eDisplayCoreFamily = IGFX_GEN11_CORE;
    testPlatform->eRenderCoreFamily = IGFX_GEN11_CORE;
    productHelper->adjustPlatformForProductFamily(&outHwInfo);

    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eRenderCoreFamily);
    EXPECT_EQ(IGFX_GEN12LP_CORE, testPlatform->eDisplayCoreFamily);
}

GEN12LPTEST_F(Gen12lpProductHelper, givenCompressionFtrEnabledWhenAskingForPageTableManagerThenReturnCorrectValue) {

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_FALSE(productHelper->isPageTableManagerSupported(outHwInfo));

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_TRUE(productHelper->isPageTableManagerSupported(outHwInfo));

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(productHelper->isPageTableManagerSupported(outHwInfo));

    outHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    outHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(productHelper->isPageTableManagerSupported(outHwInfo));
}

GEN12LPTEST_F(Gen12lpProductHelper, givenProductHelperThenCompressionIsForbidden) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isCompressionForbidden(hwInfo));
}
