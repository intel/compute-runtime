/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/kernel_commands.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

using Gen11KernelTest = Test<DeviceFixture>;
GEN11TEST_F(Gen11KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_TRUE(retVal);
}

using Gen11KernelCommandsTest = testing::Test;
GEN11TEST_F(Gen11KernelCommandsTest, givenGen11PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsFalse) {
    EXPECT_FALSE(KernelCommandsHelper<FamilyType>::doBindingTablePrefetch());
}
