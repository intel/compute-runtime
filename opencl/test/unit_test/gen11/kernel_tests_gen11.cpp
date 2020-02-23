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

using Gen11KernelTest = Test<DeviceFixture>;
GEN11TEST_F(Gen11KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pClDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_TRUE(retVal);
}

using Gen11HardwareCommandsTest = testing::Test;
GEN11TEST_F(Gen11HardwareCommandsTest, givenGen11PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsFalse) {
    EXPECT_FALSE(HardwareCommandsHelper<FamilyType>::doBindingTablePrefetch());
}
