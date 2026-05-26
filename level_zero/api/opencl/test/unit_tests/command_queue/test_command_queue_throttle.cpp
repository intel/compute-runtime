/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/queue_throttle.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

namespace NEO {
namespace LEO {
namespace ult {

using CommandQueueThrottleTests = Test<OclFixture>;

TEST_F(CommandQueueThrottleTests, givenNoThrottlePropertyWhenCreatingCommandQueueThenL0CmdListHasMediumThrottle) {
    auto &clDevices = platform->getDevices();
    ASSERT_FALSE(clDevices.empty());

    cl_device_id clDeviceId = clDevices[0].get();
    cl_int errcode = CL_SUCCESS;
    cl_context clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, clContext);

    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, 0, 0};
    cl_command_queue queue = clCreateCommandQueueWithProperties(clContext, clDeviceId, props, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    auto leoQueue = castToObject<CommandQueue>(queue);
    ASSERT_NE(nullptr, leoQueue);
    auto l0CmdList = leoQueue->getL0Object();
    ASSERT_NE(nullptr, l0CmdList);
    auto whiteBoxCmdList = L0::ult::CommandList::whiteboxCast(l0CmdList);
    EXPECT_EQ(NEO::QueueThrottle::MEDIUM, whiteBoxCmdList->queueThrottle);

    clReleaseCommandQueue(queue);
    clReleaseContext(clContext);
}

TEST_F(CommandQueueThrottleTests, givenLowThrottlePropertyWhenCreatingCommandQueueThenL0CmdListHasLowThrottle) {
    auto &clDevices = platform->getDevices();
    ASSERT_FALSE(clDevices.empty());

    cl_device_id clDeviceId = clDevices[0].get();
    cl_int errcode = CL_SUCCESS;
    cl_context clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, clContext);

    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, 0, CL_QUEUE_THROTTLE_KHR, CL_QUEUE_THROTTLE_LOW_KHR, 0};
    cl_command_queue queue = clCreateCommandQueueWithProperties(clContext, clDeviceId, props, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    auto leoQueue = castToObject<CommandQueue>(queue);
    ASSERT_NE(nullptr, leoQueue);
    auto l0CmdList = leoQueue->getL0Object();
    ASSERT_NE(nullptr, l0CmdList);
    auto whiteBoxCmdList = L0::ult::CommandList::whiteboxCast(l0CmdList);
    EXPECT_EQ(NEO::QueueThrottle::LOW, whiteBoxCmdList->queueThrottle);

    clReleaseCommandQueue(queue);
    clReleaseContext(clContext);
}

TEST_F(CommandQueueThrottleTests, givenHighThrottlePropertyWhenCreatingCommandQueueThenL0CmdListHasHighThrottle) {
    auto &clDevices = platform->getDevices();
    ASSERT_FALSE(clDevices.empty());

    cl_device_id clDeviceId = clDevices[0].get();
    cl_int errcode = CL_SUCCESS;
    cl_context clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, clContext);

    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, 0, CL_QUEUE_THROTTLE_KHR, CL_QUEUE_THROTTLE_HIGH_KHR, 0};
    cl_command_queue queue = clCreateCommandQueueWithProperties(clContext, clDeviceId, props, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    auto leoQueue = castToObject<CommandQueue>(queue);
    ASSERT_NE(nullptr, leoQueue);
    auto l0CmdList = leoQueue->getL0Object();
    ASSERT_NE(nullptr, l0CmdList);
    auto whiteBoxCmdList = L0::ult::CommandList::whiteboxCast(l0CmdList);
    EXPECT_EQ(NEO::QueueThrottle::HIGH, whiteBoxCmdList->queueThrottle);

    clReleaseCommandQueue(queue);
    clReleaseContext(clContext);
}

TEST_F(CommandQueueThrottleTests, givenMedThrottlePropertyWhenCreatingCommandQueueThenL0CmdListHasMediumThrottle) {
    auto &clDevices = platform->getDevices();
    ASSERT_FALSE(clDevices.empty());

    cl_device_id clDeviceId = clDevices[0].get();
    cl_int errcode = CL_SUCCESS;
    cl_context clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, clContext);

    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, 0, CL_QUEUE_THROTTLE_KHR, CL_QUEUE_THROTTLE_MED_KHR, 0};
    cl_command_queue queue = clCreateCommandQueueWithProperties(clContext, clDeviceId, props, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    auto leoQueue = castToObject<CommandQueue>(queue);
    ASSERT_NE(nullptr, leoQueue);
    auto l0CmdList = leoQueue->getL0Object();
    ASSERT_NE(nullptr, l0CmdList);
    auto whiteBoxCmdList = L0::ult::CommandList::whiteboxCast(l0CmdList);
    EXPECT_EQ(NEO::QueueThrottle::MEDIUM, whiteBoxCmdList->queueThrottle);

    clReleaseCommandQueue(queue);
    clReleaseContext(clContext);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
