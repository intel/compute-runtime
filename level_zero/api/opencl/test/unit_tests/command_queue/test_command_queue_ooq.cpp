/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

using CommandQueueOoqTests = Test<OclFixture>;

TEST_F(CommandQueueOoqTests, givenOutOfOrderPropertyWhenCreatingCommandQueueThenL0CmdListIsNotInOrder) {
    auto &clDevices = platform->getDevices();
    ASSERT_FALSE(clDevices.empty());

    cl_device_id clDeviceId = clDevices[0].get();
    cl_int errcode = CL_SUCCESS;
    cl_context clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, clContext);

    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    cl_command_queue queue = clCreateCommandQueueWithProperties(clContext, clDeviceId, props, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    auto leoQueue = castToObject<CommandQueue>(queue);
    ASSERT_NE(nullptr, leoQueue);
    EXPECT_FALSE(leoQueue->getL0Object()->isInOrderExecutionEnabled());
    EXPECT_TRUE(leoQueue->isOutOfOrder());

    clReleaseCommandQueue(queue);
    clReleaseContext(clContext);
}

TEST_F(CommandQueueOoqTests, givenDefaultPropertiesWhenCreatingCommandQueueThenL0CmdListIsInOrder) {
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
    EXPECT_TRUE(leoQueue->getL0Object()->isInOrderExecutionEnabled());
    EXPECT_FALSE(leoQueue->isOutOfOrder());

    clReleaseCommandQueue(queue);
    clReleaseContext(clContext);
}

TEST_F(CommandQueueOoqTests, givenProfilingWithOutOfOrderPropertyWhenCreatingCommandQueueThenL0CmdListIsNotInOrder) {
    auto &clDevices = platform->getDevices();
    ASSERT_FALSE(clDevices.empty());

    cl_device_id clDeviceId = clDevices[0].get();
    cl_int errcode = CL_SUCCESS;
    cl_context clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, clContext);

    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE, 0};
    cl_command_queue queue = clCreateCommandQueueWithProperties(clContext, clDeviceId, props, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, queue);

    auto leoQueue = castToObject<CommandQueue>(queue);
    ASSERT_NE(nullptr, leoQueue);
    EXPECT_TRUE(leoQueue->isOutOfOrder());
    EXPECT_TRUE(leoQueue->isProfilingEnabled());

    clReleaseCommandQueue(queue);
    clReleaseContext(clContext);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
