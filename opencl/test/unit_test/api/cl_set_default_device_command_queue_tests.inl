/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "opencl/source/context/context.h"
#include "opencl/source/device_queue/device_queue.h"
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
        deviceQueue = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], properties, &retVal);

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

HWCMDTEST_F(IGFX_GEN8_CORE, clSetDefaultDeviceCommandQueueApiTest, GivenValidParamsWhenSettingDefaultDeviceQueueThenSuccessIsReturned) {
    retVal = clSetDefaultDeviceCommandQueue(pContext, devices[testedRootDeviceIndex], deviceQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(static_cast<_device_queue *>(deviceQueue), static_cast<_device_queue *>(pContext->getDefaultDeviceQueue()));
}

HWCMDTEST_F(IGFX_GEN8_CORE, clSetDefaultDeviceCommandQueueApiTest, GivenValidParamsWhenReplacingDefaultDeviceQueueThenSuccessIsReturned) {
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES,
                                        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,
                                        0,
                                        0};
    auto pDevice = castToObject<ClDevice>(devices[testedRootDeviceIndex]);

    if (pDevice->getSharedDeviceInfo().maxOnDeviceQueues > 1) {
        auto newDeviceQueue = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], properties, &retVal);
        ASSERT_NE(nullptr, newDeviceQueue);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = clSetDefaultDeviceCommandQueue(pContext, devices[testedRootDeviceIndex], newDeviceQueue);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(static_cast<_device_queue *>(newDeviceQueue), static_cast<_device_queue *>(pContext->getDefaultDeviceQueue()));

        clReleaseCommandQueue(newDeviceQueue);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, clSetDefaultDeviceCommandQueueApiTest, GivenNullContextWhenSettingDefaultDeviceQueueThenClInvalidContextErrorIsReturned) {
    retVal = clSetDefaultDeviceCommandQueue(nullptr, devices[testedRootDeviceIndex], deviceQueue);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
}

HWCMDTEST_F(IGFX_GEN8_CORE, clSetDefaultDeviceCommandQueueApiTest, GivenNullDeviceWhenSettingDefaultDeviceQueueThenClInvalidDeviceErrorIsReturned) {
    retVal = clSetDefaultDeviceCommandQueue(pContext, nullptr, deviceQueue);
    ASSERT_EQ(CL_INVALID_DEVICE, retVal);
}

HWCMDTEST_F(IGFX_GEN8_CORE, clSetDefaultDeviceCommandQueueApiTest, GivenNullDeviceQueueWhenSettingDefaultDeviceQueueThenClInvalidCommandQueueErrorIsReturned) {
    retVal = clSetDefaultDeviceCommandQueue(pContext, devices[testedRootDeviceIndex], nullptr);
    ASSERT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

HWCMDTEST_F(IGFX_GEN8_CORE, clSetDefaultDeviceCommandQueueApiTest, GivenHostQueueAsDeviceQueueWhenSettingDefaultDeviceQueueThenClInvalidCommandQueueErrorIsReturned) {
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, 0, 0, 0};
    cl_command_queue hostQueue = clCreateCommandQueueWithProperties(pContext, devices[testedRootDeviceIndex], properties, &retVal);
    ASSERT_NE(nullptr, hostQueue);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetDefaultDeviceCommandQueue(pContext, devices[testedRootDeviceIndex], hostQueue);
    ASSERT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clReleaseCommandQueue(hostQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWCMDTEST_F(IGFX_GEN8_CORE, clSetDefaultDeviceCommandQueueApiTest, GivenIncorrectDeviceQueueWhenSettingDefaultDeviceQueueThenClInvalidCommandQueueErrorIsReturned) {
    auto context2 = clCreateContext(nullptr, 1u, &testedClDevice, nullptr, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES,
                                        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE,
                                        0,
                                        0};
    cl_command_queue deviceQueueCtx2 = clCreateCommandQueueWithProperties(context2, devices[testedRootDeviceIndex], properties, &retVal);
    ASSERT_NE(nullptr, deviceQueueCtx2);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetDefaultDeviceCommandQueue(pContext, devices[testedRootDeviceIndex], deviceQueueCtx2);
    ASSERT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);

    retVal = clReleaseCommandQueue(deviceQueueCtx2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context2);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
