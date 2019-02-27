/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/command_queue/enqueue_common.h"

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clEnqueueMarkerTests;

TEST_F(clEnqueueMarkerTests, GivenNullCommandQueueWhenEnqueingMarkerThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueMarker(
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueMarkerTests, GivenValidCommandQueueWhenEnqueingMarkerThenSuccessIsReturned) {
    auto retVal = clEnqueueMarker(
        pCommandQueue,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

class CommandWithoutKernelTypesTests : public testing::TestWithParam<unsigned int /*commandTypes*/> {
};

TEST_P(CommandWithoutKernelTypesTests, commandWithoutKernelTypes) {
    unsigned int commandType = GetParam();
    EXPECT_TRUE(isCommandWithoutKernel(commandType));
};

TEST_P(CommandWithoutKernelTypesTests, commandZeroType) {
    EXPECT_FALSE(isCommandWithoutKernel(0));
};

static unsigned int commandWithoutKernelTypes[] = {
    CL_COMMAND_BARRIER,
    CL_COMMAND_MARKER,
    CL_COMMAND_MIGRATE_MEM_OBJECTS,
    CL_COMMAND_SVM_MAP,
    CL_COMMAND_SVM_UNMAP,
    CL_COMMAND_SVM_FREE};

INSTANTIATE_TEST_CASE_P(
    commandWithoutKernelTypes,
    CommandWithoutKernelTypesTests,
    testing::ValuesIn(commandWithoutKernelTypes));
