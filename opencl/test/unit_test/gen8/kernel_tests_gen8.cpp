/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

using namespace NEO;

using Gen8KernelTest = Test<DeviceFixture>;
GEN8TEST_F(Gen8KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsFalse) {
    MockKernelWithInternals mockKernel(*pClDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_FALSE(retVal);
}
using Gen8HardwareCommandsTest = testing::Test;
GEN8TEST_F(Gen8HardwareCommandsTest, givenGen8PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(HardwareCommandsHelper<FamilyType>::doBindingTablePrefetch());
}
