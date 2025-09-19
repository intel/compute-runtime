/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClCreateCommandQueueTest = ApiTests;

namespace ULT {

TEST_F(ClCreateCommandQueueTest, givenCorrectParametersWhenCreatingCommandQueueThenCommandQueueIsCreatedAndSuccessIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;

    cmdQ = clCreateCommandQueue(pContext, testedClDevice, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClCreateCommandQueueTest, givenNullContextWhenCreatingCommandQueueThenInvalidContextErrorIsReturned) {
    clCreateCommandQueue(nullptr, testedClDevice, 0, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(ClCreateCommandQueueTest, givenNullDeviceWhenCreatingCommandQueueThenInvalidDeviceErrorIsReturned) {
    clCreateCommandQueue(pContext, nullptr, 0, &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(ClCreateCommandQueueTest, givenDeviceNotAssociatedWithContextWhenCreatingCommandQueueThenInvalidDeviceErrorIsReturned) {
    UltClDeviceFactory deviceFactory{1, 0};
    EXPECT_FALSE(pContext->isDeviceAssociated(*deviceFactory.rootDevices[0]));
    clCreateCommandQueue(pContext, deviceFactory.rootDevices[0], 0, &retVal);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(ClCreateCommandQueueTest, givenInvalidPropertiesWhenCreatingCommandQueueThenInvalidValueErrorIsReturned) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0xf0000;

    cmdQ = clCreateCommandQueue(pContext, testedClDevice, properties, &retVal);

    ASSERT_EQ(nullptr, cmdQ);
    ASSERT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClCreateCommandQueueTest, givenOoqParametersWhenQueueIsCreatedThenQueueIsSuccessfullyCreated) {
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto cmdq = clCreateCommandQueue(pContext, testedClDevice, ooq, &retVal);
    EXPECT_NE(nullptr, cmdq);
    EXPECT_EQ(retVal, CL_SUCCESS);
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(ClCreateCommandQueueTest, givenOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverSwitchesToBatchingMode) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    using BaseType = typename CommandQueue::BaseType;
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto clDevice = castToObject<ClDevice>(testedClDevice);
    auto mockDevice = reinterpret_cast<MockDevice *>(&clDevice->getDevice());
    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::immediateDispatch, csr.dispatchMode);

    auto cmdq = clCreateCommandQueue(pContext, testedClDevice, ooq, &retVal);
    auto queue = castToObject<CommandQueue>(static_cast<BaseType *>(cmdq));
    EXPECT_EQ(DispatchMode::batchedDispatch, queue->getGpgpuCommandStreamReceiver().getDispatchMode());
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(ClCreateCommandQueueTest, givenOoqParametersWhenQueueIsCreatedAndUpdateTaskCountFromWaitEnabledThenCommandStreamReceiverDoesntSwitchToBatchingMode) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    using BaseType = typename CommandQueue::BaseType;
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto clDevice = castToObject<ClDevice>(testedClDevice);
    auto mockDevice = reinterpret_cast<MockDevice *>(&clDevice->getDevice());
    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::immediateDispatch, csr.dispatchMode);

    auto cmdq = clCreateCommandQueue(pContext, testedClDevice, ooq, &retVal);
    auto queue = castToObject<CommandQueue>(static_cast<BaseType *>(cmdq));
    EXPECT_EQ(DispatchMode::immediateDispatch, queue->getGpgpuCommandStreamReceiver().getDispatchMode());
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(ClCreateCommandQueueTest, givenForcedDispatchModeAndOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverDoesntSwitchToBatchingMode) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CsrDispatchMode.set(static_cast<int32_t>(DispatchMode::immediateDispatch));

    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto clDevice = castToObject<ClDevice>(testedClDevice);
    auto mockDevice = reinterpret_cast<MockDevice *>(&clDevice->getDevice());
    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::immediateDispatch, csr.dispatchMode);

    auto cmdq = clCreateCommandQueue(pContext, testedClDevice, ooq, &retVal);
    EXPECT_EQ(DispatchMode::immediateDispatch, csr.dispatchMode);
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(ClCreateCommandQueueTest, givenOoqParametersWhenQueueIsCreatedThenCommandStreamReceiverSwitchesToNTo1SubmissionModel) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    using BaseType = typename CommandQueue::BaseType;
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties ooq = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    auto clDevice = castToObject<ClDevice>(testedClDevice);
    auto mockDevice = reinterpret_cast<MockDevice *>(&clDevice->getDevice());
    auto &csr = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_FALSE(csr.isNTo1SubmissionModelEnabled());

    auto cmdq = clCreateCommandQueue(pContext, testedClDevice, ooq, &retVal);
    auto queue = castToObject<CommandQueue>(static_cast<BaseType *>(cmdq));
    EXPECT_TRUE(queue->getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled());
    retVal = clReleaseCommandQueue(cmdq);
}

HWTEST_F(ClCreateCommandQueueTest, givenGfxFamilyWhenQueueIsCreatedThenBcsEngineCountSetToValueFromGfxFamily) {
    using BaseType = typename CommandQueue::BaseType;
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties properties{};

    auto cmdq = clCreateCommandQueue(pContext, testedClDevice, properties, &retVal);
    auto queue = castToObject<CommandQueue>(static_cast<BaseType *>(cmdq));
    auto mockQueue = static_cast<MockCommandQueue *>(queue);

    EXPECT_EQ(FamilyType::bcsEngineCount, mockQueue->bcsEngineCount);

    retVal = clReleaseCommandQueue(cmdq);
}

} // namespace ULT
