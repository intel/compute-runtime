/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/context/context.h"

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

} // namespace ULT
