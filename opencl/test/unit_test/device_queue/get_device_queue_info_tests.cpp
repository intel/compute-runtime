/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/fixtures/device_host_queue_fixture.h"

using namespace NEO;
using namespace DeviceHostQueue;

class GetDeviceQueueInfoTest : public DeviceHostQueueFixture<DeviceQueue> {
  public:
    using BaseClass = DeviceHostQueueFixture<DeviceQueue>;

    void SetUp() override {
        BaseClass::SetUp();
        if (!this->pContext->getDevice(0u)->getHardwareInfo().capabilityTable.supportsDeviceEnqueue) {
            GTEST_SKIP();
        }
        deviceQueue = createQueueObject(deviceQueueProperties::allProperties);
        ASSERT_NE(deviceQueue, nullptr);
    }

    void TearDown() override {
        if (deviceQueue)
            delete deviceQueue;
        BaseClass::TearDown();
    }

    DeviceQueue *deviceQueue = nullptr;
};

HWCMDTEST_F(IGFX_GEN8_CORE, GetDeviceQueueInfoTest, context) {
    cl_context contextReturned = nullptr;

    retVal = deviceQueue->getCommandQueueInfo(
        CL_QUEUE_CONTEXT,
        sizeof(contextReturned),
        &contextReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((cl_context)pContext, contextReturned);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetDeviceQueueInfoTest, device) {
    cl_device_id deviceExpected = testedClDevice;
    cl_device_id deviceIdReturned = nullptr;

    retVal = deviceQueue->getCommandQueueInfo(
        CL_QUEUE_DEVICE,
        sizeof(deviceIdReturned),
        &deviceIdReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(deviceExpected, deviceIdReturned);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetDeviceQueueInfoTest, queueProperties) {
    cl_command_queue_properties propertiesReturned = 0;

    retVal = deviceQueue->getCommandQueueInfo(
        CL_QUEUE_PROPERTIES,
        sizeof(propertiesReturned),
        &propertiesReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(deviceQueueProperties::allProperties[1], propertiesReturned);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetDeviceQueueInfoTest, queueSize) {
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
HWCMDTEST_F(IGFX_GEN8_CORE, GetDeviceQueueInfoTest, queueDeviceDefault) {
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

HWCMDTEST_F(IGFX_GEN8_CORE, GetDeviceQueueInfoTest, profiling) {
    EXPECT_TRUE(deviceQueue->isProfilingEnabled());
}

HWCMDTEST_F(IGFX_GEN8_CORE, GetDeviceQueueInfoTest, invalidParameter) {
    uint32_t tempValue = 0;

    retVal = deviceQueue->getCommandQueueInfo(
        static_cast<cl_command_queue_info>(0),
        sizeof(tempValue),
        &tempValue,
        nullptr);
    EXPECT_EQ(tempValue, 0u);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}
