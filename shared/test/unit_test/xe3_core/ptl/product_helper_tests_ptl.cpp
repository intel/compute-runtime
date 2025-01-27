/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "platforms.h"
#include "wmtp_setup_ptl.inl"

using namespace NEO;

using PtlProductHelper = ProductHelperTest;

PTLTEST_F(PtlProductHelper, whenGettingPreferredAllocationMethodThenAllocateByKmdIsReturned) {
    for (auto i = 0; i < static_cast<int>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        EXPECT_TRUE(preferredAllocationMethod.has_value());
        EXPECT_EQ(GfxMemoryAllocationMethod::allocateByKmd, preferredAllocationMethod.value());
    }
}

PTLTEST_F(PtlProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::PTL_H_A0);
}

HWTEST_EXCLUDE_PRODUCT(CompilerProductHelperFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned, IGFX_PTL);
PTLTEST_F(PtlProductHelper, givenCompilerProductHelperWhenGetMidThreadPreemptionSupportThenCorrectValueIsSet) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrWalkerMTP = false;
    EXPECT_FALSE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
    hwInfo.featureTable.flags.ftrWalkerMTP = true;
    EXPECT_EQ(wmtpSupported, compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
}
