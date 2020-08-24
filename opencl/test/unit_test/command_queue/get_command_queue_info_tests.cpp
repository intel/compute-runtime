/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

struct GetCommandQueueInfoTest : public ClDeviceFixture,
                                 public ContextFixture,
                                 public CommandQueueFixture,
                                 ::testing::TestWithParam<uint64_t /*cl_command_queue_properties*/> {
    using CommandQueueFixture::SetUp;
    using ContextFixture::SetUp;

    GetCommandQueueInfoTest() {
    }

    void SetUp() override {
        properties = GetParam();
        ClDeviceFixture::SetUp();

        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);
        CommandQueueFixture::SetUp(pContext, pClDevice, properties);
    }

    void TearDown() override {
        CommandQueueFixture::TearDown();
        ContextFixture::TearDown();
        ClDeviceFixture::TearDown();
    }

    const HardwareInfo *pHwInfo = nullptr;
    cl_command_queue_properties properties;
};

TEST_P(GetCommandQueueInfoTest, GivenClQueueContextWhenGettingCommandQueueInfoThenSuccessIsReturned) {
    cl_context contextReturned = nullptr;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_CONTEXT,
        sizeof(contextReturned),
        &contextReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((cl_context)pContext, contextReturned);
}

TEST_P(GetCommandQueueInfoTest, GivenClQueueDeviceWhenGettingCommandQueueInfoThenSuccessIsReturned) {
    cl_device_id deviceExpected = pClDevice;
    cl_device_id deviceReturned = nullptr;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_DEVICE,
        sizeof(deviceReturned),
        &deviceReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(deviceExpected, deviceReturned);
}

TEST_P(GetCommandQueueInfoTest, GivenClQueuePropertiesWhenGettingCommandQueueInfoThenSuccessIsReturned) {
    cl_command_queue_properties cmdqPropertiesReturned = 0;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_PROPERTIES,
        sizeof(cmdqPropertiesReturned),
        &cmdqPropertiesReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(properties, cmdqPropertiesReturned);
}

TEST_P(GetCommandQueueInfoTest, givenNonDeviceQueueWhenQueryingQueueSizeThenInvalidCommandQueueErrorIsReturned) {
    cl_uint queueSize = 0;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_SIZE,
        sizeof(queueSize),
        &queueSize,
        nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_P(GetCommandQueueInfoTest, GivenClQueueDeviceDefaultWhenGettingCommandQueueInfoThenSuccessIsReturned) {
    cl_command_queue commandQueueReturned = nullptr;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_DEVICE_DEFAULT,
        sizeof(commandQueueReturned),
        &commandQueueReturned,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // host queue can't be default device queue
    EXPECT_NE(pCmdQ, commandQueueReturned);
}

TEST_P(GetCommandQueueInfoTest, GivenInvalidParameterWhenGettingCommandQueueInfoThenInvalidValueIsReturned) {
    cl_uint parameterReturned = 0;
    cl_command_queue_info invalidParameter = 0xdeadbeef;

    auto retVal = pCmdQ->getCommandQueueInfo(
        invalidParameter,
        sizeof(parameterReturned),
        &parameterReturned,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

INSTANTIATE_TEST_CASE_P(
    GetCommandQueueInfoTest,
    GetCommandQueueInfoTest,
    ::testing::ValuesIn(DefaultCommandQueueProperties));
