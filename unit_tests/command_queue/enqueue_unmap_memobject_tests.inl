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
#include "test.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include <algorithm>

using namespace OCLRT;

struct EnqueueUnmapMemObjTest : public DeviceFixture,
                                public CommandQueueHwFixture,
                                public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueUnmapMemObjTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(pDevice, 0);
        BufferDefaults::context = new MockContext;
        buffer = BufferHelper<BufferUseHostPtr<>>::create();
        mappedPtr = pCmdQ->enqueueMapBuffer(buffer, CL_TRUE, CL_MAP_READ, 0, 8, 0, nullptr, nullptr, retVal);
    }

    void TearDown() override {
        delete buffer;
        delete BufferDefaults::context;
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    Buffer *buffer = nullptr;
    void *mappedPtr;
};

TEST_F(EnqueueUnmapMemObjTest, validAddressShouldReturnSuccess) {
    auto retVal = pCmdQ->enqueueUnmapMemObject(
        buffer,
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueUnmapMemObjTest, returnsEvent) {
    cl_event event = nullptr;
    auto retVal = pCmdQ->enqueueUnmapMemObject(
        buffer,
        mappedPtr,
        0,
        nullptr,
        &event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    ASSERT_NE(nullptr, event);
    auto pEvent = (Event *)event;
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);

    // Check CL_EVENT_COMMAND_TYPE
    {
        cl_command_type cmdType = 0;
        size_t sizeReturned = 0;
        auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_UNMAP_MEM_OBJECT), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent;
}

TEST_F(EnqueueUnmapMemObjTest, returnedEventHasGreaterThanOrEqualTaskLevelThanParentEvent) {
    uint32_t taskLevelCmdQ = 17;
    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    auto taskLevelMax = std::max({taskLevelCmdQ, taskLevelEvent1, taskLevelEvent2});

    pCmdQ->taskLevel = taskLevelCmdQ;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 15);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 16);

    cl_event eventWaitList[] = {
        &event1,
        &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    auto retVal = pCmdQ->enqueueUnmapMemObject(
        buffer,
        mappedPtr,
        numEventsInWaitList,
        eventWaitList,
        &event);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_GE(pEvent->taskLevel, taskLevelMax);

    delete pEvent;
}

HWTEST_F(EnqueueUnmapMemObjTest, UnmapEventProperties) {
    cl_event eventReturned = NULL;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.taskCount = 100;

    retVal = pCmdQ->enqueueUnmapMemObject(
        buffer,
        mappedPtr,
        0,
        nullptr,
        &eventReturned);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, eventReturned);

    auto eventObject = castToObject<Event>(eventReturned);
    EXPECT_EQ(0u, eventObject->peekTaskCount());
    EXPECT_TRUE(eventObject->updateStatusAndCheckCompletion());

    clReleaseEvent(eventReturned);
}

TEST_F(EnqueueUnmapMemObjTest, UnmapMemObjWaitEvent) {
    cl_event waitEvent = nullptr;
    cl_event retEvent = nullptr;

    auto buffer = clCreateBuffer(
        BufferDefaults::context,
        CL_MEM_READ_WRITE,
        20,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    auto ptrResult = clEnqueueMapBuffer(
        pCmdQ,
        buffer,
        CL_FALSE,
        CL_MAP_READ,
        0,
        8,
        0,
        nullptr,
        &waitEvent,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, ptrResult);
    EXPECT_NE(nullptr, waitEvent);

    retVal = clEnqueueUnmapMemObject(
        pCmdQ,
        buffer,
        ptrResult,
        1,
        &waitEvent,
        &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, retEvent);

    Event *wEvent = castToObject<Event>(waitEvent);
    EXPECT_EQ(CL_COMPLETE, wEvent->peekExecutionStatus());

    Event *rEvent = castToObject<Event>(retEvent);
    EXPECT_EQ(CL_COMPLETE, rEvent->peekExecutionStatus());

    retVal = clWaitForEvents(1, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseEvent(waitEvent);
    clReleaseEvent(retEvent);
}

HWTEST_F(EnqueueUnmapMemObjTest, givenEnqueueUnmapMemObjectWhenNonAubWritableBufferObjectMappedToHostPtrForWritingThenItShouldBeResetToAubWritable) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    ASSERT_NE(nullptr, buffer);
    buffer->getGraphicsAllocation()->setAubWritable(false);

    auto mappedForWritingPtr = pCmdQ->enqueueMapBuffer(buffer.get(), CL_TRUE, CL_MAP_WRITE, 0, 8, 0, nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, mappedForWritingPtr);

    retVal = pCmdQ->enqueueUnmapMemObject(
        buffer.get(),
        mappedForWritingPtr,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(buffer->getGraphicsAllocation()->isAubWritable());
}
