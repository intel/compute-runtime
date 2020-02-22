/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"

#include "fixtures/context_fixture.h"
#include "fixtures/device_fixture.h"
#include "mocks/mock_command_queue.h"
#include "mocks/mock_context.h"
#include "mocks/mock_graphics_allocation.h"
#include "mocks/mock_kernel.h"
#include "mocks/mock_program.h"

using namespace NEO;

TEST(KernelWithCacheFlushTests, givenDeviceWhichDoesntRequireCacheFlushWhenCheckIfKernelRequireFlushThenReturnedFalse) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    MockContext mockContext(device.get());
    MockCommandQueue queue;
    bool flushRequired = mockKernel->mockKernel->Kernel::requiresCacheFlushCommand(queue);
    EXPECT_FALSE(flushRequired);
}
