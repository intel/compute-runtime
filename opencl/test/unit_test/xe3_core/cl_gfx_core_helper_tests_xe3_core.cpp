/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
using namespace NEO;

using ClGfxCoreHelperTestsXe3Core = Test<ClDeviceFixture>;

XE3_CORETEST_F(ClGfxCoreHelperTestsXe3Core, givenXe3CoreThenAuxTranslationIsNotRequired) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    KernelInfo kernelInfo{};

    EXPECT_FALSE(clGfxCoreHelper.requiresAuxResolves(kernelInfo));
}

XE3_CORETEST_F(ClGfxCoreHelperTestsXe3Core, WhenCheckingPreferenceForBlitterForLocalToLocalTransfersThenReturnTrue) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    EXPECT_FALSE(clGfxCoreHelper.preferBlitterForLocalToLocalTransfers());
}
