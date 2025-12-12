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

using ClGfxCoreHelperTestsXe3pCore = Test<ClDeviceFixture>;

XE3P_CORETEST_F(ClGfxCoreHelperTestsXe3pCore, givenXe3pCoreThenAuxTranslationIsNotRequired) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    KernelInfo kernelInfo{};

    EXPECT_FALSE(clGfxCoreHelper.requiresAuxResolves(kernelInfo));
}
