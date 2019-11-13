/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hardware_commands_helper.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

using Gen12LpKernelTest = Test<DeviceFixture>;
GEN12LPTEST_F(Gen12LpKernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_FALSE(retVal);
}
using Gen12LpHardwareCommandsTest = testing::Test;
GEN12LPTEST_F(Gen12LpHardwareCommandsTest, givenGen12LpPlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_FALSE(HardwareCommandsHelper<FamilyType>::doBindingTablePrefetch());
}
