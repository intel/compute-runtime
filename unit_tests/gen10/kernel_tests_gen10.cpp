/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/kernel_commands.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "test.h"

using namespace OCLRT;

using Gen10KernelTest = Test<DeviceFixture>;
GEN10TEST_F(Gen10KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_TRUE(retVal);
}
using Gen10KernelCommandsTest = testing::Test;
GEN10TEST_F(Gen10KernelCommandsTest, givenGen10PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(KernelCommandsHelper<FamilyType>::doBindingTablePrefetch());
}
