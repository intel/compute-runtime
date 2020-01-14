/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/user_event.h"
#include "test.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"

using namespace NEO;

class MigrateMemObjectsFixture
    : public DeviceFixture,
      public CommandQueueHwFixture {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueHwFixture::SetUp(pClDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
    }

    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

typedef Test<MigrateMemObjectsFixture> MigrateMemObjectsTest;

TEST_F(MigrateMemObjectsTest, GivenNullEventWhenMigratingEventsThenSuccessIsReturned) {

    MockBuffer buffer;

    auto retVal = pCmdQ->enqueueMigrateMemObjects(
        1,
        (cl_mem *)&buffer,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MigrateMemObjectsTest, GivenValidEventListWhenMigratingEventsThenSuccessIsReturned) {

    MockBuffer buffer;

    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};

    auto retVal = pCmdQ->enqueueMigrateMemObjects(
        1,
        (cl_mem *)&buffer,
        CL_MIGRATE_MEM_OBJECT_HOST,
        1,
        eventWaitList,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MigrateMemObjectsTest, GivenEventPointerWhenMigratingEventsThenEventIsReturned) {

    MockBuffer buffer;

    cl_event event = nullptr;

    auto retVal = pCmdQ->enqueueMigrateMemObjects(
        1,
        (cl_mem *)&buffer,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, event);

    Event *eventObject = (Event *)event;
    delete eventObject;
}
