/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"

using namespace NEO;
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
