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

using namespace OCLRT;

using Gen8KernelTest = Test<DeviceFixture>;
GEN8TEST_F(Gen8KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsFalse) {
    MockKernelWithInternals mockKernel(*pDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_FALSE(retVal);
}
using Gen8KernelCommandsTest = testing::Test;
GEN8TEST_F(Gen8KernelCommandsTest, givenGen8PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(KernelCommandsHelper<FamilyType>::doBindingTablePrefetch());
}
