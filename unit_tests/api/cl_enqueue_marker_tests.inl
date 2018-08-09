/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_queue/enqueue_common.h"

using namespace OCLRT;

typedef api_tests clEnqueueMarkerTests;

TEST_F(clEnqueueMarkerTests, NullCommandQueue_returnsError) {
    auto retVal = clEnqueueMarker(
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueMarkerTests, returnsSuccess) {
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
