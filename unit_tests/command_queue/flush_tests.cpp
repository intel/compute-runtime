/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "gtest/gtest.h"

using namespace OCLRT;

struct FlushTest
    : public DeviceFixture,
      public CommandQueueFixture,
      public CommandStreamFixture,
      public IndirectHeapFixture,
      public ::testing::Test {

    using CommandQueueFixture::SetUp;
    using IndirectHeapFixture::SetUp;

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(nullptr, pDevice, 0);
        CommandStreamFixture::SetUp(pCmdQ);
        IndirectHeapFixture::SetUp(pCmdQ);
    }

    void TearDown() override {
        IndirectHeapFixture::TearDown();
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

TEST_F(FlushTest, simple) {
    auto retVal = pCmdQ->flush();

    EXPECT_EQ(retVal, CL_SUCCESS);
}
