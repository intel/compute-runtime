/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/event.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "gen_cmd_parse.h"
#include "test.h"

using namespace OCLRT;

struct MarkerFixture : public CommandEnqueueFixture {
  public:
    void SetUp() override {
        CommandEnqueueFixture::SetUp();
        WhitelistedRegisters forceRegs = {0};
        pDevice->setForceWhitelistedRegs(true, &forceRegs);
    }

    void TearDown() override {
        CommandEnqueueFixture::TearDown();
    }
};

typedef Test<MarkerFixture> MarkerTest;

HWCMDTEST_F(IGFX_GEN8_CORE, MarkerTest, CS_EQ_CQ_ShouldntAddPipeControl) {
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

HWTEST_F(MarkerTest, CS_GT_CQ_ShouldNotAddPipeControl) {
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

TEST_F(MarkerTest, returnsEvent) {
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

HWTEST_F(MarkerTest, returnedEventShouldHaveEqualDepthToLastCommandPacketInCommandQueue) {
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

HWTEST_F(MarkerTest, eventWithWaitDependenciesShouldSync) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // In N:1, CSR is always highest task level.
    commandStreamReceiver.taskLevel = 7;

    // In N:1, pCmdQ.level <= CSR.level
    pCmdQ->taskLevel = 7;

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
    EXPECT_EQ(commandStreamReceiver.peekTaskLevel(), pCmdQ->taskLevel);
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
    pCmdQ->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);

    std::unique_ptr<Event> pEvent((Event *)(event));

    EXPECT_EQ(16u, pCmdQ->taskCount);
    EXPECT_EQ(16u, pEvent->peekTaskCount());
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
    pCmdQ->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);

    std::unique_ptr<Event> pEvent((Event *)(event));

    EXPECT_EQ(16u, pCmdQ->taskCount);
    EXPECT_EQ(16u, pEvent->peekTaskCount());
}
