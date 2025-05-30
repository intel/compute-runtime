/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "gtest/gtest.h"

using namespace NEO;

typedef HelloWorldTest<HelloWorldFixtureFactory> EventTests;
TEST_F(EventTests, WhenEnqueingKernelThenCorrectEventIsReturned) {

    cl_event event = nullptr;
    auto retVal = callOneWorkItemNDRKernel(nullptr, 0, &event);

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
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_NDRANGE_KERNEL), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent;
}

TEST_F(EventTests, WhenEnqueingKernelThenEventReturnedShouldBeMaxOfInputEventsAndCmdQPlus1) {
    uint32_t taskLevelCmdQ = 17;
    pCmdQ->taskLevel = taskLevelCmdQ;

    uint32_t taskLevelEvent1 = 8;
    uint32_t taskLevelEvent2 = 19;
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent1, 15);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, taskLevelEvent2, 16);

    cl_event eventWaitList[] =
        {
            &event1,
            &event2};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    auto retVal = callOneWorkItemNDRKernel(eventWaitList, numEventsInWaitList, &event);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(19u + 1u, pEvent->taskLevel);

    delete pEvent;
}

TEST_F(EventTests, WhenWaitingForEventThenPipeControlIsNotInserted) {
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();

    auto retVal = callOneWorkItemNDRKernel(eventWaitList, numEventsInWaitList, &event);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);

    retVal = Event::waitForEvents(1, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    // we expect event is completed
    TaskCountType taskCountOfEvent = pEvent->peekTaskCount();
    EXPECT_LE(taskCountOfEvent, pCmdQ->getHwTag());
    // no more tasks after WFE, no need to write PC

    auto expectedTaskLevel = pEvent->taskLevel.load();
    if (!csr.isUpdateTagFromWaitEnabled()) {
        expectedTaskLevel++;
    }
    EXPECT_EQ(expectedTaskLevel, csr.peekTaskLevel());

    pCmdQ->finish();

    // Check CL_EVENT_COMMAND_TYPE
    {
        cl_command_type cmdType = 0;
        size_t sizeReturned = 0;
        auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_NDRANGE_KERNEL), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent;
}

TEST_F(EventTests, GivenTwoEnqueuesWhenWaitingForBothEventsThenTaskLevelIsCorrect) {

    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event[2] = {};

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();

    auto retVal = callOneWorkItemNDRKernel(eventWaitList, numEventsInWaitList, &event[0]);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event[0]);

    auto pEvent0 = castToObject<Event>(event[0]);
    EXPECT_EQ(pCmdQ->taskLevel, pEvent0->taskLevel);

    retVal = callOneWorkItemNDRKernel(eventWaitList, numEventsInWaitList, &event[1]);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event[1]);

    auto pEvent1 = castToObject<Event>(event[1]);
    EXPECT_EQ(pCmdQ->taskLevel, pEvent1->taskLevel);
    EXPECT_GT(pEvent1->taskLevel, pEvent0->taskLevel);

    retVal = Event::waitForEvents(2, event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto expectedTaskLevel1 = pEvent1->taskLevel.load();
    if (!csr.isUpdateTagFromWaitEnabled()) {
        expectedTaskLevel1++;
    }
    EXPECT_EQ(expectedTaskLevel1, csr.peekTaskLevel());
    pCmdQ->finish();
    EXPECT_EQ(expectedTaskLevel1, csr.peekTaskLevel());
    // Check CL_EVENT_COMMAND_TYPE
    {
        cl_command_type cmdType = 0;
        size_t sizeReturned = 0;
        auto result = clGetEventInfo(pEvent0, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_NDRANGE_KERNEL), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent0;
    delete pEvent1;
}

TEST_F(EventTests, GivenNoEventsWhenEnqueuingKernelThenTaskLevelIsIncremented) {
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();

    auto retVal = callOneWorkItemNDRKernel(eventWaitList, numEventsInWaitList, &event);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);

    retVal = Event::waitForEvents(1, &event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto taskLevelEvent = pEvent->taskLevel.load();
    if (!csr.isUpdateTagFromWaitEnabled()) {
        taskLevelEvent++;
    }

    EXPECT_EQ(taskLevelEvent, csr.peekTaskLevel());

    retVal = callOneWorkItemNDRKernel(eventWaitList, numEventsInWaitList, nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    taskLevelEvent = pEvent->taskLevel.load();
    if (!csr.isUpdateTagFromWaitEnabled()) {
        taskLevelEvent += 2;
    } else {
        taskLevelEvent += 1;
    }
    EXPECT_EQ(taskLevelEvent, csr.peekTaskLevel());

    pCmdQ->finish();
    EXPECT_EQ(taskLevelEvent, csr.peekTaskLevel());

    // Check CL_EVENT_COMMAND_TYPE
    {
        cl_command_type cmdType = 0;
        size_t sizeReturned = 0;
        auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_NDRANGE_KERNEL), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }

    delete pEvent;
}

TEST_F(EventTests, WhenEnqueuingMarkerThenPassedEventHasTheSameLevelAsPreviousCommand) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();

    auto retVal = callOneWorkItemNDRKernel(eventWaitList, numEventsInWaitList, &event);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);

    auto pEvent = (Event *)event;

    EXPECT_EQ(pEvent->taskLevel + 1, csr.peekTaskLevel());

    cl_event event2 = nullptr;

    retVal = clEnqueueMarkerWithWaitList(pCmdQ, 1, &event, &event2);

    auto pEvent2 = castToObject<Event>(event2);

    if (csr.peekTimestampPacketWriteEnabled()) {
        EXPECT_EQ(pEvent2->taskLevel, pEvent->taskLevel + 1);
    } else {
        EXPECT_EQ(pEvent2->taskLevel, pEvent->taskLevel);
    }

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event2);

    retVal = clWaitForEvents(1, &event2);
    ASSERT_EQ(CL_SUCCESS, retVal);

    if (csr.peekTimestampPacketWriteEnabled()) {
        EXPECT_EQ(csr.peekTaskLevel(), pCmdQ->taskLevel + 1);
    } else {
        EXPECT_EQ(csr.peekTaskLevel(), pEvent->taskLevel + 1);
    }

    clReleaseEvent(event);
    clReleaseEvent(event2);
}

HWTEST_F(EventTests, givenEnqueueKernelBlockedOnserEventWhenEnqueueHasOutEventWithProfilingThenPCisProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockKernelWithInternals mockKernelWithInternals(*pClDevice);
    MockKernel *kernel = mockKernelWithInternals.mockKernel;
    UserEvent userEvent;
    cl_event userEventWaitlist[] = {&userEvent};
    cl_event outEvent;
    auto ccsStart = pCmdQ->getGpgpuCommandStreamReceiver().getCS().getUsed();
    auto mockCmdQueue = static_cast<MockCommandQueueHw<FamilyType> *>(pCmdQ);
    mockCmdQueue->commandQueueProperties |= CL_QUEUE_PROFILING_ENABLE;
    EXPECT_EQ(CL_SUCCESS, pCmdQ->enqueueKernel(kernel, 1, nullptr, nullptr, nullptr, 1, userEventWaitlist, &outEvent));

    userEvent.setStatus(CL_COMPLETE);
    {
        HardwareParse ccsHwParser;
        ccsHwParser.parseCommands<FamilyType>(pCmdQ->getGpgpuCommandStreamReceiver().getCS(0), ccsStart);
        const auto pipeControlItor = find<PIPE_CONTROL *>(ccsHwParser.cmdList.begin(), ccsHwParser.cmdList.end());
        EXPECT_NE(pipeControlItor, ccsHwParser.cmdList.end());
    }

    EXPECT_EQ(CL_SUCCESS, pCmdQ->finish());
    clReleaseEvent(outEvent);
}