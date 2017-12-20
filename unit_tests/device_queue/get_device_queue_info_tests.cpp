/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/context/context.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"

using namespace OCLRT;
using namespace DeviceHostQueue;

class GetDeviceQueueInfoTest : public DeviceHostQueueFixture<DeviceQueue> {
  public:
    using BaseClass = DeviceHostQueueFixture<DeviceQueue>;

    void SetUp() override {
        BaseClass::SetUp();
        deviceQueue = createQueueObject(deviceQueueProperties::allProperties);
        ASSERT_NE(deviceQueue, nullptr);
    }

    void TearDown() override {
        if (deviceQueue)
            delete deviceQueue;
        BaseClass::TearDown();
    }

    DeviceQueue *deviceQueue;
};

TEST_F(GetDeviceQueueInfoTest, context) {
    cl_context contextReturned = nullptr;

    retVal = deviceQueue->getCommandQueueInfo(
        CL_QUEUE_CONTEXT,
        sizeof(contextReturned),
        &contextReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((cl_context)pContext, contextReturned);
}

TEST_F(GetDeviceQueueInfoTest, device) {
    cl_device_id deviceExpected = devices[0];
    cl_device_id deviceIdReturned = nullptr;

    retVal = deviceQueue->getCommandQueueInfo(
        CL_QUEUE_DEVICE,
        sizeof(deviceIdReturned),
        &deviceIdReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(deviceExpected, deviceIdReturned);
}

TEST_F(GetDeviceQueueInfoTest, queueProperties) {
    cl_command_queue_properties propertiesReturned = 0;

    retVal = deviceQueue->getCommandQueueInfo(
        CL_QUEUE_PROPERTIES,
        sizeof(propertiesReturned),
        &propertiesReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(deviceQueueProperties::allProperties[1], propertiesReturned);
}

TEST_F(GetDeviceQueueInfoTest, queueSize) {
    cl_uint queueSizeReturned = 0;

    retVal = deviceQueue->getCommandQueueInfo(
        CL_QUEUE_SIZE,
        sizeof(queueSizeReturned),
        &queueSizeReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(deviceQueue->getQueueSize(), queueSizeReturned);
}

// OCL 2.1
TEST_F(GetDeviceQueueInfoTest, queueDeviceDefault) {
    cl_command_queue commandQueueReturned = nullptr;

    retVal = deviceQueue->getCommandQueueInfo(
        CL_QUEUE_DEVICE_DEFAULT,
        sizeof(commandQueueReturned),
        &commandQueueReturned,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // 1 device queue is supported which is default
    EXPECT_EQ(deviceQueue, commandQueueReturned);
}

TEST_F(GetDeviceQueueInfoTest, profiling) {
    EXPECT_TRUE(deviceQueue->isProfilingEnabled());
}

TEST_F(GetDeviceQueueInfoTest, invalidParameter) {
    uint32_t tempValue = 0;

    retVal = deviceQueue->getCommandQueueInfo(
        static_cast<cl_command_queue_info>(0),
        sizeof(tempValue),
        &tempValue,
        nullptr);
    EXPECT_EQ(tempValue, 0u);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}
