/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

using namespace NEO;

using Gen12LpKernelTest = Test<DeviceFixture>;
GEN12LPTEST_F(Gen12LpKernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pClDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_FALSE(retVal);
}
using Gen12LpHardwareCommandsTest = testing::Test;
GEN12LPTEST_F(Gen12LpHardwareCommandsTest, givenGen12LpPlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_FALSE(HardwareCommandsHelper<FamilyType>::doBindingTablePrefetch());
}
