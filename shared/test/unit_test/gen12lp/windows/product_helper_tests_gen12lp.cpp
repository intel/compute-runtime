/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/windows/product_helper_win_tests.h"

using namespace NEO;

using Gen12lpProductHelperWindows = ProductHelperTestWindows;

GEN12LPTEST_F(Gen12lpProductHelperWindows, givenGen12LpSkuWhenGettingCapabilityCoherencyFlagThenExpectValidValue) {
    bool coherency = false;
    productHelper->setCapabilityCoherencyFlag(outHwInfo, coherency);
    const bool checkDone = SpecialUltHelperGen12lp::additionalCoherencyCheck(outHwInfo.platform.eProductFamily, coherency);
    if (checkDone) {
        EXPECT_FALSE(coherency);
        return;
    }

    if (SpecialUltHelperGen12lp::isAdditionalCapabilityCoherencyFlagSettingRequired(outHwInfo.platform.eProductFamily)) {
        outHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A1, outHwInfo);
        productHelper->setCapabilityCoherencyFlag(outHwInfo, coherency);
        EXPECT_TRUE(coherency);
        outHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, outHwInfo);
        productHelper->setCapabilityCoherencyFlag(outHwInfo, coherency);
        EXPECT_FALSE(coherency);
    } else {
        EXPECT_TRUE(coherency);
    }
}
