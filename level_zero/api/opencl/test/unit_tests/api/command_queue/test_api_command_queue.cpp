/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(CreateCommandQueueTests, givenNullContextWhenCreateCommandQueueThenReturnsCLInvalidContext) {
    cl_int errcode = CL_SUCCESS;
    auto cmdQueue = clCreateCommandQueue(nullptr, nullptr, 0, &errcode);
    EXPECT_EQ(nullptr, cmdQueue);
    EXPECT_EQ(CL_INVALID_CONTEXT, errcode);
}

TEST(CreateCommandQueueTests, givenNullContextWhenCreateCommandQueueWithPropertiesThenReturnsCLInvalidContext) {
    cl_int errcode = CL_SUCCESS;
    auto cmdQueue = clCreateCommandQueueWithProperties(nullptr, nullptr, nullptr, &errcode);
    EXPECT_EQ(nullptr, cmdQueue);
    EXPECT_EQ(CL_INVALID_CONTEXT, errcode);
}

TEST(RetainReleaseCommandQueueTests, givenNullCommandQueueWhenRetainCommandQueueThenReturnsCLInvalidCommandQueue) {
    auto retVal = clRetainCommandQueue(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST(RetainReleaseCommandQueueTests, givenNullCommandQueueWhenReleaseCommandQueueThenReturnsCLInvalidCommandQueue) {
    auto retVal = clReleaseCommandQueue(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST(GetCommandQueueInfoTests, givenNullCommandQueueWhenGetCommandQueueInfoThenReturnsCLInvalidCommandQueue) {
    auto retVal = clGetCommandQueueInfo(nullptr, CL_QUEUE_CONTEXT, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST(FlushTests, givenNullCommandQueueWhenFlushThenReturnsCLInvalidCommandQueue) {
    auto retVal = clFlush(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST(FinishTests, givenNullCommandQueueWhenFinishThenReturnsCLInvalidCommandQueue) {
    auto retVal = clFinish(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
