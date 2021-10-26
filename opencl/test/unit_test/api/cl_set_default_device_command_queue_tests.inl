/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "opencl/source/context/context.h"
#include "opencl/source/device_queue/device_queue.h"
#include "opencl/test/unit_test/fixtures/device_queue_matcher.h"
#include "test.h"

#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct clSetDefaultDeviceCommandQueueApiTest : public api_tests {
    clSetDefaultDeviceCommandQueueApiTest() {
    }

    void SetUp() override {
        api_tests::SetUp();
        cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES,
                                            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,
                                            0,
                                            0};
        deviceQueue = clCreateCommandQueueWithProperties(pContext, testedClDevice, properties, &retVal);

        if (!pContext->getDevice(0u)->getHardwareInfo().capabilityTable.supportsDeviceEnqueue) {
            ASSERT_EQ(nullptr, deviceQueue);
            EXPECT_EQ(CL_INVALID_QUEUE_PROPERTIES, retVal);
            GTEST_SKIP();
        } else {
            ASSERT_NE(nullptr, deviceQueue);
            ASSERT_EQ(CL_SUCCESS, retVal);
        }
    }

    void TearDown() override {
        if (deviceQueue) {
            retVal = clReleaseCommandQueue(deviceQueue);
            EXPECT_EQ(CL_SUCCESS, retVal);
        }
        api_tests::TearDown();
    }

    cl_command_queue deviceQueue = nullptr;
};

HWTEST2_F(clSetDefaultDeviceCommandQueueApiTest, GivenValidParamsWhenSettingDefaultDeviceQueueThenSuccessIsReturned, DeviceEnqueueSupport) {
    retVal = clSetDefaultDeviceCommandQueue(pContext, testedClDevice, deviceQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(static_cast<_device_queue *>(deviceQueue), static_cast<_device_queue *>(pContext->getDefaultDeviceQueue()));
}

HWTEST2_F(clSetDefaultDeviceCommandQueueApiTest, GivenValidParamsWhenReplacingDefaultDeviceQueueThenSuccessIsReturned, DeviceEnqueueSupport) {
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES,
                                        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,
                                        0,
                                        0};
    auto pDevice = castToObject<ClDevice>(testedClDevice);

    if (pDevice->getDeviceInfo().maxOnDeviceQueues > 1) {
        auto newDeviceQueue = clCreateCommandQueueWithProperties(pContext, testedClDevice, properties, &retVal);
        ASSERT_NE(nullptr, newDeviceQueue);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = clSetDefaultDeviceCommandQueue(pContext, testedClDevice, newDeviceQueue);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(static_cast<_device_queue *>(newDeviceQueue), static_cast<_device_queue *>(pContext->getDefaultDeviceQueue()));

        clReleaseCommandQueue(newDeviceQueue);
    }
}

HWTEST2_F(clSetDefaultDeviceCommandQueueApiTest, GivenNullContextWhenSettingDefaultDeviceQueueThenClInvalidContextErrorIsReturned, DeviceEnqueueSupport) {
    retVal = clSetDefaultDeviceCommandQueue(nullptr, testedClDevice, deviceQueue);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

HWTEST2_F(clSetDefaultDeviceCommandQueueApiTest, GivenNullDeviceWhenSettingDefaultDeviceQueueThenClInvalidDeviceErrorIsReturned, DeviceEnqueueSupport) {
    retVal = clSetDefaultDeviceCommandQueue(pContext, nullptr, deviceQueue);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(clSetDefaultDeviceCommandQueueApiTest, GivenDeviceNotSupportingDeviceEnqueueWhenSettingDefaultDeviceQueueThenClInvalidOperationErrorIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceDeviceEnqueueSupport.set(0);

    retVal = clSetDefaultDeviceCommandQueue(pContext, testedClDevice, nullptr);
    ASSERT_EQ(CL_INVALID_OPERATION, retVal);
}

HWTEST2_F(clSetDefaultDeviceCommandQueueApiTest, GivenNullDeviceQueueWhenSettingDefaultDeviceQueueThenClInvalidCommandQueueErrorIsReturned, DeviceEnqueueSupport) {
    retVal = clSetDefaultDeviceCommandQueue(pContext, testedClDevice, nullptr);
    ASSERT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

HWTEST2_F(clSetDefaultDeviceCommandQueueApiTest, GivenHostQueueAsDeviceQueueWhenSettingDefaultDeviceQueueThenClInvalidCommandQueueErrorIsReturned, DeviceEnqueueSupport) {
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, 0, 0, 0};
    cl_command_queue hostQueue = clCreateCommandQueueWithProperties(pContext, testedClDevice, properties, &retVal);
    ASSERT_NE(nullptr, hostQueue);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetDefaultDeviceCommandQueue(pContext, testedClDevice, hostQueue);
    ASSERT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clReleaseCommandQueue(hostQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST2_F(clSetDefaultDeviceCommandQueueApiTest, GivenIncorrectDeviceQueueWhenSettingDefaultDeviceQueueThenClInvalidCommandQueueErrorIsReturned, DeviceEnqueueSupport) {
    auto context2 = clCreateContext(nullptr, 1u, &testedClDevice, nullptr, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES,
                                        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,
                                        0,
                                        0};
    cl_command_queue deviceQueueCtx2 = clCreateCommandQueueWithProperties(context2, testedClDevice, properties, &retVal);
    ASSERT_NE(nullptr, deviceQueueCtx2);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetDefaultDeviceCommandQueue(pContext, testedClDevice, deviceQueueCtx2);
    ASSERT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clReleaseCommandQueue(deviceQueueCtx2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
