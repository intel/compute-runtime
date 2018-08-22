/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/event.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "test.h"

using namespace OCLRT;

class MigrateMemObjectsFixture
    : public DeviceFixture,
      public CommandQueueHwFixture {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueHwFixture::SetUp(pDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
    }

    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

typedef Test<MigrateMemObjectsFixture> MigrateMemObjectsTest;

TEST_F(MigrateMemObjectsTest, ValidInputsReturnsNullEvent) {

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

TEST_F(MigrateMemObjectsTest, ValidInputsAndBlockedOnEventReturnsNullEvent) {

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

TEST_F(MigrateMemObjectsTest, ValidInputsReturnsNonNullEvent) {

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
