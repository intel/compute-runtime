/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

struct FlushTest
    : public ClDeviceFixture,
      public CommandQueueFixture,
      public CommandStreamFixture,
      public ::testing::Test {

    using CommandQueueFixture::setUp;

    void SetUp() override {
        ClDeviceFixture::setUp();
        CommandQueueFixture::setUp(nullptr, pClDevice, 0);
        CommandStreamFixture::setUp(pCmdQ);
    }

    void TearDown() override {
        CommandStreamFixture::tearDown();
        CommandQueueFixture::tearDown();
        ClDeviceFixture::tearDown();
    }
};

TEST_F(FlushTest, WhenFlushingThenSuccessIsReturned) {
    auto retVal = pCmdQ->flush();

    EXPECT_EQ(retVal, CL_SUCCESS);
}
