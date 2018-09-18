/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/kernel_commands.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "test.h"

using namespace OCLRT;

using Gen9KernelTest = Test<DeviceFixture>;
GEN9TEST_F(Gen9KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_TRUE(retVal);
}
using Gen9KernelCommandsTest = testing::Test;
GEN9TEST_F(Gen9KernelCommandsTest, givenGen9PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(KernelCommandsHelper<FamilyType>::doBindingTablePrefetch());
}
