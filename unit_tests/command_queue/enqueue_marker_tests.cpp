/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/user_event.h"
#include "test.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

using MarkerTest = Test<CommandEnqueueFixture>;

HWTEST_F(MarkerTest, GivenCsrAndCmdqWithSameTaskLevelWhenEnqueingMarkerThenPipeControlIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Set task levels to known values.
    uint32_t originalCSRLevel = 2;
    commandStreamReceiver.taskLevel = originalCSRLevel;
    pCmdQ->taskLevel = originalCSRLevel;

    uint32_t originalTaskCount = 15;
    commandStreamReceiver.taskCount = originalTaskCount;

    cl_uint numEventsInWaitList = 0;
    const cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    auto retVal = pCmdQ->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // Should sync CSR & CmdQ levels.
    EXPECT_EQ(commandStreamReceiver.peekTaskLevel(), pCmdQ->taskLevel);

    parseCommands<FamilyType>(*pCmdQ);

    // If CSR == CQ then a PC is required.
    auto itorCmd = reverse_find<PIPE_CONTROL *>(cmdList.rbegin(), cmdList.rend());
    EXPECT_EQ(cmdList.rend(), itorCmd);
}

HWTEST_F(MarkerTest, GivenCsrAndCmdqWithDifferentTaskLevelsWhenEnqueingMarkerThenPipeControlIsNotAdded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Set task levels to known values.
    commandStreamReceiver.taskLevel = 2;
    pCmdQ->taskLevel = 1;

    cl_uint numEventsInWaitList = 0;
    const cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    auto retVal = pCmdQ->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // Should sync CSR & CmdQ levels.
    EXPECT_EQ(1u, pCmdQ->taskLevel);
    EXPECT_EQ(2u, commandStreamReceiver.peekTaskLevel());

    auto sizeUsed = pCS->getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, pCmdBuffer, sizeUsed));

    // If CSR > CQ then a PC isn't required.
    auto itorCmd = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorCmd);
}

TEST_F(MarkerTest, WhenEnqueingMarkerThenEventIsReturned) {
    cl_uint numEventsInWaitList = 0;
    const cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;
    auto retVal = pCmdQ->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, event);

    // Check CL_EVENT_COMMAND_TYPE
    {
        std::unique_ptr<Event> pEvent((Event *)(event));
        cl_command_type cmdType = 0;
        size_t sizeReturned = 0;
        auto result = clGetEventInfo(pEvent.get(), CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_MARKER), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);
    }
}

HWTEST_F(MarkerTest, WhenEnqueingMarkerThenReturnedEventShouldHaveEqualDepthToLastCommandPacketInCommandQueue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Set task levels to known values.
    commandStreamReceiver.taskLevel = 2;
    pCmdQ->taskLevel = 1;

    cl_uint numEventsInWaitList = 0;
    const cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;
    auto retVal = pCmdQ->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);
    std::unique_ptr<Event> pEvent((Event *)(event));

    // Shouldn't sync to CSR
    // should sync to command queue last packet
    EXPECT_EQ(1u, pEvent->taskLevel);
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);
}

HWTEST_F(MarkerTest, GivenEventWithWaitDependenciesWhenEnqueingMarkerThenCsrLevelAndCmdqLevelShouldSync) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint32_t initialTaskLevel = 7;

    // In N:1, CSR is always highest task level.
    commandStreamReceiver.taskLevel = initialTaskLevel;

    // In N:1, pCmdQ.level <= CSR.level
    pCmdQ->taskLevel = initialTaskLevel;

    // In N:1, event.level <= pCmdQ.level
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 6);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2,
            &event3};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto retVal = pCmdQ->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);
    std::unique_ptr<Event> pEvent((Event *)(event));

    // Should sync CSR & CmdQ levels.
    if (pCmdQ->getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        EXPECT_EQ(initialTaskLevel, pCmdQ->taskLevel);
        EXPECT_EQ(initialTaskLevel + 1, commandStreamReceiver.peekTaskLevel());
    } else {
        EXPECT_EQ(commandStreamReceiver.peekTaskLevel(), pCmdQ->taskLevel);
    }
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);
    EXPECT_EQ(7u, pEvent->taskLevel);
}

TEST_F(MarkerTest, givenMultipleEventWhenTheyArePassedToMarkerThenOutputEventHasHighestTaskCount) {
    // combine events with different task counts, max is 16
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 6);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2,
            &event3};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto initialTaskCount = pCmdQ->taskCount;

    pCmdQ->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);

    std::unique_ptr<Event> pEvent((Event *)(event));

    if (pCmdQ->getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        EXPECT_EQ(initialTaskCount + 1, pCmdQ->taskCount);
        EXPECT_EQ(initialTaskCount + 1, pEvent->peekTaskCount());
    } else {
        EXPECT_EQ(16u, pCmdQ->taskCount);
        EXPECT_EQ(16u, pEvent->peekTaskCount());
    }
}

TEST_F(MarkerTest, givenMultipleEventsAndCompletedUserEventWhenTheyArePassedToMarkerThenOutputEventHasHighestTaskCount) {
    // combine events with different task counts, max is 16
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 6);
    UserEvent userEvent(&pCmdQ->getContext());
    userEvent.setStatus(CL_COMPLETE);

    cl_event eventWaitList[] =
        {
            &event1,
            &event2,
            &event3,
            &userEvent};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto initialTaskCount = pCmdQ->taskCount;

    pCmdQ->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);

    std::unique_ptr<Event> pEvent((Event *)(event));

    if (pCmdQ->getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        EXPECT_EQ(initialTaskCount + 1, pCmdQ->taskCount);
        EXPECT_EQ(initialTaskCount + 1, pEvent->peekTaskCount());
    } else {
        EXPECT_EQ(16u, pCmdQ->taskCount);
        EXPECT_EQ(16u, pEvent->peekTaskCount());
    }
}

HWTEST_F(MarkerTest, givenMarkerCallFollowingNdrangeCallInBatchedModeWhenWaitForEventsIsCalledThenFlushStampIsProperlyUpdated) {
    MockKernelWithInternals mockKernel(*this->pDevice, this->context);

    auto &ultCommandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    ultCommandStreamReceiver.overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    cl_event eventFromNdr = nullptr;
    size_t gws[] = {1};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, &eventFromNdr);
    cl_event eventFromMarker = nullptr;
    pCmdQ->enqueueMarkerWithWaitList(1u, &eventFromNdr, &eventFromMarker);

    ultCommandStreamReceiver.flushStamp->setStamp(1u);

    clEnqueueWaitForEvents(pCmdQ, 1u, &eventFromMarker);

    auto neoEvent = castToObject<Event>(eventFromMarker);
    EXPECT_EQ(1u, neoEvent->flushStamp->peekStamp());

    clReleaseEvent(eventFromMarker);
    clReleaseEvent(eventFromNdr);
}
