/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

using namespace NEO;

TEST(KernelWithCacheFlushTests, givenDeviceWhichDoesntRequireCacheFlushWhenCheckIfKernelRequireFlushThenReturnedFalse) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device.get());
    MockCommandQueue queue;
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(queue);
    EXPECT_FALSE(flushRequired);
}
