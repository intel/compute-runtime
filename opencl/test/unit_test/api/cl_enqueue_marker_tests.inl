/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/command_queue/enqueue_common.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClEnqueueMarkerTests = ApiTests;

TEST_F(ClEnqueueMarkerTests, GivenNullCommandQueueWhenEnqueingMarkerThenInvalidCommandQueueErrorIsReturned) {
    auto retVal = clEnqueueMarker(
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClEnqueueMarkerTests, GivenValidCommandQueueWhenEnqueingMarkerThenSuccessIsReturned) {
    auto retVal = clEnqueueMarker(
        pCommandQueue,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueMarkerTests, GivenQueueIncapableWhenEnqueingMarkerThenInvalidOperationReturned) {
    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_MARKER_INTEL);
    auto retVal = clEnqueueMarker(
        pCommandQueue,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

class CommandWithoutKernelTypesTests : public testing::TestWithParam<unsigned int /*commandTypes*/> {
};

TEST_P(CommandWithoutKernelTypesTests, GivenCommandTypeWhenCheckingIsCommandWithoutKernelThenTrueIsReturned) {
    unsigned int commandType = GetParam();
    EXPECT_TRUE(isCommandWithoutKernel(commandType));
};

TEST_F(CommandWithoutKernelTypesTests, GivenZeroWhenCheckingIsCommandWithoutKernelThenFalseIsReturned) {
    EXPECT_FALSE(isCommandWithoutKernel(0));
};

static unsigned int commandWithoutKernelTypes[] = {
    CL_COMMAND_BARRIER,
    CL_COMMAND_MARKER,
    CL_COMMAND_MIGRATE_MEM_OBJECTS,
    CL_COMMAND_SVM_MAP,
    CL_COMMAND_SVM_MIGRATE_MEM,
    CL_COMMAND_SVM_UNMAP,
    CL_COMMAND_SVM_FREE};

INSTANTIATE_TEST_SUITE_P(
    commandWithoutKernelTypes,
    CommandWithoutKernelTypesTests,
    testing::ValuesIn(commandWithoutKernelTypes));
