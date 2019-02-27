/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "test.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_device.h"

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clCreateCommandQueueTest;

namespace ULT {

TEST_F(clCreateCommandQueueTest, GivenCorrectParametersWhenCreatingCommandQueueThenCommandQueueIsCreatedAndSuccessIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;

    cmdQ = clCreateCommandQueue(pContext, devices[0], properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateCommandQueueTest, GivenNullContextWhenCreatingCommandQueueThenInvalidContextErrorIsReturned) {
    clCreateCommandQueue(nullptr, devices[0], 0, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreateCommandQueueTest, GivenNullDeviceWhenCreatingCommandQueueThenInvalidDeviceErrorIsReturned) {
    clCreateCommandQueue(pContext, nullptr, 0, &retVal);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreateCommandQueueTest, GivenInvalidPropertiesWhenCreatingCommandQueueThenInvalidValueErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0xf0000;

    cmdQ = clCreateCommandQueue(pContext, devices[0], properties, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateCommandQueueTest, GivenOoqParametersWhenQueueIsCreatedThenQueueIsSucesfullyCreated) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto cmdq = clCreateCommandQueue(pContext, devices[0], ooq, &retVal);
    EXPECT_NE(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(clCreateCommandQueueTest, GivenOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverSwitchesToBatchingMode) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto mockDevice = castToObject<MockDevice>(devices[0]);
    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);

    auto cmdq = clCreateCommandQueue(pContext, devices[0], ooq, &retVal);
    EXPECT_EQ(DispatchMode::BatchedDispatch, csr.dispatchMode);
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(clCreateCommandQueueTest, GivenOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverSwitchesToNTo1SubmissionModel) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto mockDevice = castToObject<MockDevice>(devices[0]);
    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_FALSE(csr.isNTo1SubmissionModelEnabled());

    auto cmdq = clCreateCommandQueue(pContext, devices[0], ooq, &retVal);
    EXPECT_TRUE(csr.isNTo1SubmissionModelEnabled());
    retVal = clReleaseCommandQueue(cmdq);
}

} // namespace ULT
