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
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "test.h"

using namespace OCLRT;

typedef api_tests clCreateCommandQueueTest;

namespace ULT {

TEST_F(clCreateCommandQueueTest, returnsSuccess) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;

    cmdQ = clCreateCommandQueue(pContext, devices[0], properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateCommandQueueTest, NullContext_returnsError) {
    clCreateCommandQueue(nullptr, devices[0], 0, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreateCommandQueueTest, NullDevice_returnsError) {
    clCreateCommandQueue(pContext, nullptr, 0, &retVal);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreateCommandQueueTest, InvalidProp_returnsError) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0xf0000;

    cmdQ = clCreateCommandQueue(pContext, devices[0], properties, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateCommandQueueTest, givenOoqParametersWhenQueueIsCreatedThenQueueIsSucesfullyCreated) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto cmdq = clCreateCommandQueue(pContext, devices[0], ooq, &retVal);
    EXPECT_NE(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(clCreateCommandQueueTest, givenOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverSwitchesToBatchingMode) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto &csr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> &>(pContext->getDevice(0)->getCommandStreamReceiver());
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);

    auto cmdq = clCreateCommandQueue(pContext, devices[0], ooq, &retVal);
    EXPECT_EQ(DispatchMode::BatchedDispatch, csr.dispatchMode);
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(clCreateCommandQueueTest, givenOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverSwitchesToNTo1SubmissionModel) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto &csr = reinterpret_cast<UltCommandStreamReceiver<FamilyType> &>(pContext->getDevice(0)->getCommandStreamReceiver());
    EXPECT_FALSE(csr.isNTo1SubmissionModelEnabled());

    auto cmdq = clCreateCommandQueue(pContext, devices[0], ooq, &retVal);
    EXPECT_TRUE(csr.isNTo1SubmissionModelEnabled());
    retVal = clReleaseCommandQueue(cmdq);
}

} // namespace ULT
