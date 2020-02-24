/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "test.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clCreateCommandQueueTest;

namespace ULT {

TEST_F(clCreateCommandQueueTest, GivenCorrectParametersWhenCreatingCommandQueueThenCommandQueueIsCreatedAndSuccessIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;

    cmdQ = clCreateCommandQueue(pContext, devices[testedRootDeviceIndex], properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateCommandQueueTest, GivenNullContextWhenCreatingCommandQueueThenInvalidContextErrorIsReturned) {
    clCreateCommandQueue(nullptr, devices[testedRootDeviceIndex], 0, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreateCommandQueueTest, GivenNullDeviceWhenCreatingCommandQueueThenInvalidDeviceErrorIsReturned) {
    clCreateCommandQueue(pContext, nullptr, 0, &retVal);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clCreateCommandQueueTest, GivenInvalidPropertiesWhenCreatingCommandQueueThenInvalidValueErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0xf0000;

    cmdQ = clCreateCommandQueue(pContext, devices[testedRootDeviceIndex], properties, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clCreateCommandQueueTest, GivenOoqParametersWhenQueueIsCreatedThenQueueIsSucesfullyCreated) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto cmdq = clCreateCommandQueue(pContext, devices[testedRootDeviceIndex], ooq, &retVal);
    EXPECT_NE(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(clCreateCommandQueueTest, GivenOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverSwitchesToBatchingMode) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto clDevice = castToObject<ClDevice>(devices[testedRootDeviceIndex]);
    auto mockDevice = reinterpret_cast<MockDevice *>(&clDevice->getDevice());
    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);

    auto cmdq = clCreateCommandQueue(pContext, devices[testedRootDeviceIndex], ooq, &retVal);
    EXPECT_EQ(DispatchMode::BatchedDispatch, csr.dispatchMode);
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(clCreateCommandQueueTest, GivenForcedDispatchModeAndOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverDoesntSwitchToBatchingMode) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::ImmediateDispatch));

    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto clDevice = castToObject<ClDevice>(devices[testedRootDeviceIndex]);
    auto mockDevice = reinterpret_cast<MockDevice *>(&clDevice->getDevice());
    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);

    auto cmdq = clCreateCommandQueue(pContext, devices[testedRootDeviceIndex], ooq, &retVal);
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(clCreateCommandQueueTest, GivenOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverSwitchesToNTo1SubmissionModel) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto clDevice = castToObject<ClDevice>(devices[testedRootDeviceIndex]);
    auto mockDevice = reinterpret_cast<MockDevice *>(&clDevice->getDevice());
    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_FALSE(csr.isNTo1SubmissionModelEnabled());

    auto cmdq = clCreateCommandQueue(pContext, devices[testedRootDeviceIndex], ooq, &retVal);
    EXPECT_TRUE(csr.isNTo1SubmissionModelEnabled());
    retVal = clReleaseCommandQueue(cmdq);
}

} // namespace ULT
