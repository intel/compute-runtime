/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

struct FlushTest
    : public DeviceFixture,
      public CommandQueueFixture,
      public CommandStreamFixture,
      public ::testing::Test {

    using CommandQueueFixture::SetUp;

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(nullptr, pClDevice, 0);
        CommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() override {
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

TEST_F(FlushTest, WhenFlushingThenSuccessIsReturned) {
    auto retVal = pCmdQ->flush();

    EXPECT_EQ(retVal, CL_SUCCESS);
}
