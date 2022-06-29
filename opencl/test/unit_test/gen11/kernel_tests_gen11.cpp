/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

using Gen11KernelTest = Test<ClDeviceFixture>;
GEN11TEST_F(Gen11KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pClDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_TRUE(retVal);
}

GEN11TEST_F(Gen11KernelTest, givenBuiltinKernelWhenCanTransformImagesIsCalledThenReturnsFalse) {
    MockKernelWithInternals mockKernel(*pClDevice);
    mockKernel.mockKernel->isBuiltIn = true;
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_FALSE(retVal);
}

GEN11TEST_F(Gen11KernelTest, GivenKernelWhenNotRunningOnGen12lpThenWaDisableRccRhwoOptimizationIsNotRequired) {
    MockKernelWithInternals kernel(*pClDevice);
    EXPECT_FALSE(kernel.mockKernel->requiresWaDisableRccRhwoOptimization());
}
