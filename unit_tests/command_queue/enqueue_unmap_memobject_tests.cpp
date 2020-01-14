/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/event.h"
#include "test.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"

#include <algorithm>

using namespace NEO;

struct EnqueueUnmapMemObjTest : public DeviceFixture,
                                public CommandQueueHwFixture,
                                public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueUnmapMemObjTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(pClDevice, 0);
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

TEST_F(EnqueueUnmapMemObjTest, GivenValidParamsWhenUnmappingMemoryObjectThenSuccessIsReturned) {
    auto retVal = pCmdQ->enqueueUnmapMemObject(
        buffer,
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueUnmapMemObjTest, GivenPointerToEventThenUnmappingMemoryObjectThenEventIsReturned) {
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

TEST_F(EnqueueUnmapMemObjTest, WhenUnmappingMemoryObjectThenReturnedEventHasGreaterThanOrEqualTaskLevelThanParentEvent) {
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

HWTEST_F(EnqueueUnmapMemObjTest, WhenUnmappingMemoryObjectThenEventIsUpdated) {
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

TEST_F(EnqueueUnmapMemObjTest, WhenUnmappingMemoryObjectThenWaitEventIsUpdated) {
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

HWTEST_F(EnqueueUnmapMemObjTest, givenEnqueueUnmapMemObjectWhenNonAubWritableBufferObjectMappedToHostPtrForWritingThenItShouldBeResetToAubAndTbxWritable) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    ASSERT_NE(nullptr, buffer);
    buffer->getGraphicsAllocation()->setAubWritable(false, GraphicsAllocation::defaultBank);
    buffer->getGraphicsAllocation()->setTbxWritable(false, GraphicsAllocation::defaultBank);

    auto mappedForWritingPtr = pCmdQ->enqueueMapBuffer(buffer.get(), CL_TRUE, CL_MAP_WRITE, 0, 8, 0, nullptr, nullptr, retVal);
    ASSERT_NE(nullptr, mappedForWritingPtr);

    retVal = pCmdQ->enqueueUnmapMemObject(
        buffer.get(),
        mappedForWritingPtr,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(buffer->getGraphicsAllocation()->isAubWritable(GraphicsAllocation::defaultBank));
    EXPECT_TRUE(buffer->getGraphicsAllocation()->isTbxWritable(GraphicsAllocation::defaultBank));
}

HWTEST_F(EnqueueUnmapMemObjTest, givenMemObjWhenUnmappingThenSetAubWritableBeforeEnqueueWrite) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DisableZeroCopyForBuffers.set(true);
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    auto image = std::unique_ptr<Image>(Image2dHelper<>::create(BufferDefaults::context));

    class MyMockCommandQueue : public MockCommandQueue {
      public:
        using MockCommandQueue::MockCommandQueue;
        cl_int enqueueWriteBuffer(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t cb, const void *ptr, GraphicsAllocation *mapAllocation,
                                  cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override {
            EXPECT_TRUE(buffer->getMapAllocation()->isAubWritable(GraphicsAllocation::defaultBank));
            EXPECT_TRUE(buffer->getMapAllocation()->isTbxWritable(GraphicsAllocation::defaultBank));
            return CL_SUCCESS;
        }

        cl_int enqueueWriteImage(Image *dstImage, cl_bool blockingWrite, const size_t *origin, const size_t *region,
                                 size_t inputRowPitch, size_t inputSlicePitch, const void *ptr, GraphicsAllocation *mapAllocation,
                                 cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override {
            EXPECT_TRUE(dstImage->getMapAllocation()->isAubWritable(GraphicsAllocation::defaultBank));
            EXPECT_TRUE(dstImage->getMapAllocation()->isTbxWritable(GraphicsAllocation::defaultBank));
            return CL_SUCCESS;
        }
    };

    MyMockCommandQueue myMockCmdQ(BufferDefaults::context, pClDevice, nullptr);

    {
        auto mapPtr = myMockCmdQ.enqueueMapBuffer(buffer.get(), CL_TRUE, CL_MAP_WRITE, 0, 8, 0, nullptr, nullptr, retVal);

        buffer->getMapAllocation()->setAubWritable(false, GraphicsAllocation::defaultBank);
        buffer->getMapAllocation()->setTbxWritable(false, GraphicsAllocation::defaultBank);

        myMockCmdQ.enqueueUnmapMemObject(buffer.get(), mapPtr, 0, nullptr, nullptr);
    }

    {
        size_t region[] = {1, 0, 0};
        size_t origin[] = {0, 0, 0};
        auto mapPtr = myMockCmdQ.enqueueMapImage(image.get(), CL_TRUE, CL_MAP_WRITE, origin, region, nullptr, nullptr, 0,
                                                 nullptr, nullptr, retVal);

        image->getMapAllocation()->setAubWritable(false, GraphicsAllocation::defaultBank);
        image->getMapAllocation()->setTbxWritable(false, GraphicsAllocation::defaultBank);

        myMockCmdQ.enqueueUnmapMemObject(image.get(), mapPtr, 0, nullptr, nullptr);
    }
}
