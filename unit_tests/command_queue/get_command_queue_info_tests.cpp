/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "gtest/gtest.h"

using namespace OCLRT;

struct GetCommandQueueInfoTest : public DeviceFixture,
                                 public ContextFixture,
                                 public CommandQueueFixture,
                                 ::testing::TestWithParam<uint64_t /*cl_command_queue_properties*/> {
    using CommandQueueFixture::SetUp;
    using ContextFixture::SetUp;

    GetCommandQueueInfoTest() {
    }

    void SetUp() override {
        properties = GetParam();
        DeviceFixture::SetUp();

        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        CommandQueueFixture::SetUp(pContext, pDevice, properties);
    }

    void TearDown() override {
        CommandQueueFixture::TearDown();
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    const HardwareInfo *pHwInfo = nullptr;
    cl_command_queue_properties properties;
};

TEST_P(GetCommandQueueInfoTest, CONTEXT) {
    cl_context contextReturned = nullptr;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_CONTEXT,
        sizeof(contextReturned),
        &contextReturned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((cl_context)pContext, contextReturned);
}

TEST_P(GetCommandQueueInfoTest, DEVICE) {
    cl_device_id device_expected = pDevice;
    cl_device_id device_id_returned = nullptr;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_DEVICE,
        sizeof(device_id_returned),
        &device_id_returned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(device_expected, device_id_returned);
}

TEST_P(GetCommandQueueInfoTest, QUEUE_PROPERTIES) {
    cl_command_queue_properties command_queue_properties_returned = 0;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_PROPERTIES,
        sizeof(command_queue_properties_returned),
        &command_queue_properties_returned,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(properties, command_queue_properties_returned);
}

TEST_P(GetCommandQueueInfoTest, QUEUE_SIZE) {
    cl_uint queueSize = 0;

    auto retVal = pCmdQ->getCommandQueueInfo(
        CL_QUEUE_SIZE,
        sizeof(queueSize),
        &queueSize,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_P(GetCommandQueueInfoTest, QUEUE_DEVICE_DEFAULT) {
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

INSTANTIATE_TEST_CASE_P(
    GetCommandQueueInfoTest,
    GetCommandQueueInfoTest,
    ::testing::ValuesIn(DefaultCommandQueueProperties));
