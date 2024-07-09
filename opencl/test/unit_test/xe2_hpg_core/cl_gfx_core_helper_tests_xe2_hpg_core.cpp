/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using ClGfxCoreHelperTestsXe2HpgCore = Test<ClDeviceFixture>;

XE2_HPG_CORETEST_F(ClGfxCoreHelperTestsXe2HpgCore, givenXe2HpgCoreThenAuxTranslationIsNotRequired) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    KernelInfo kernelInfo{};

    EXPECT_FALSE(clGfxCoreHelper.requiresAuxResolves(kernelInfo));
}
XE2_HPG_CORETEST_F(ClGfxCoreHelperTestsXe2HpgCore, WhenCheckingPreferenceForBlitterForLocalToLocalTransfersThenReturnFalse) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    EXPECT_FALSE(clGfxCoreHelper.preferBlitterForLocalToLocalTransfers());
}
XE2_HPG_CORETEST_F(ClGfxCoreHelperTestsXe2HpgCore, WhenCheckingIsLimitationForPreemptionNeededThenReturnTrue) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    EXPECT_TRUE(clGfxCoreHelper.isLimitationForPreemptionNeeded());
}
