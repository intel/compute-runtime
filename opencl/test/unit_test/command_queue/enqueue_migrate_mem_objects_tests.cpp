/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

class MigrateMemObjectsFixture
    : public ClDeviceFixture,
      public CommandQueueHwFixture {
  public:
    void setUp() {
        ClDeviceFixture::setUp();
        CommandQueueHwFixture::setUp(pClDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
    }

    void tearDown() {
        CommandQueueHwFixture::tearDown();
        ClDeviceFixture::tearDown();
    }
};

typedef Test<MigrateMemObjectsFixture> MigrateMemObjectsTest;

TEST_F(MigrateMemObjectsTest, GivenNullEventWhenMigratingEventsThenSuccessIsReturned) {

    MockBuffer buffer;
    auto bufferMemObj = static_cast<cl_mem>(&buffer);
    auto pBufferMemObj = &bufferMemObj;

    auto retVal = pCmdQ->enqueueMigrateMemObjects(
        1,
        pBufferMemObj,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(MigrateMemObjectsTest, GivenValidEventListWhenMigratingEventsThenSuccessIsReturned) {

    MockBuffer buffer;
    auto bufferMemObj = static_cast<cl_mem>(&buffer);
    auto pBufferMemObj = &bufferMemObj;

    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};

    auto retVal = pCmdQ->enqueueMigrateMemObjects(
        1,
        pBufferMemObj,
        CL_MIGRATE_MEM_OBJECT_HOST,
        1,
        eventWaitList,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(MigrateMemObjectsTest, GivenGpuHangAndBlockingCallsAndValidEventListWhenMigratingEventsThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    MockBuffer buffer;
    auto bufferMemObj = static_cast<cl_mem>(&buffer);

    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};

    const auto enqueueResult = mockCommandQueueHw.enqueueMigrateMemObjects(
        1,
        &bufferMemObj,
        CL_MIGRATE_MEM_OBJECT_HOST,
        1,
        eventWaitList,
        nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

TEST_F(MigrateMemObjectsTest, GivenEventPointerWhenMigratingEventsThenEventIsReturned) {

    MockBuffer buffer;
    auto bufferMemObj = static_cast<cl_mem>(&buffer);
    auto pBufferMemObj = &bufferMemObj;

    cl_event event = nullptr;

    auto retVal = pCmdQ->enqueueMigrateMemObjects(
        1,
        pBufferMemObj,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, event);

    Event *eventObject = (Event *)event;
    delete eventObject;
}
