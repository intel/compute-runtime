/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

using Gen8KernelTest = Test<ClDeviceFixture>;
GEN8TEST_F(Gen8KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsFalse) {
    MockKernelWithInternals mockKernel(*pClDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_FALSE(retVal);
}
