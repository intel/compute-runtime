/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

using Gen9KernelTest = Test<DeviceFixture>;
GEN9TEST_F(Gen9KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_TRUE(retVal);
}
using Gen9HardwareCommandsTest = testing::Test;
GEN9TEST_F(Gen9HardwareCommandsTest, givenGen9PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(HardwareCommandsHelper<FamilyType>::doBindingTablePrefetch());
}
