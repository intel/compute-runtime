/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "test.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"

using namespace NEO;

using BarrierTest = Test<CommandEnqueueFixture>;

HWTEST_F(BarrierTest, givenCsrWithHigherLevelThenCommandQueueWhenEnqueueBarrierIsCalledThenCommandQueueAlignsToCsrWithoutSendingAnyCommands) {
    auto pCmdQ = this->pCmdQ;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Set task levels to known values.
    uint32_t originalCSRLevel = 2;
    commandStreamReceiver.taskLevel = originalCSRLevel;
    pCmdQ->taskLevel = originalCSRLevel;

    uint32_t originalTaskCount = 15;
    commandStreamReceiver.taskCount = originalTaskCount;

    auto &csrCommandStream = commandStreamReceiver.commandStream;
    auto csrUsed = csrCommandStream.getUsed();

    cl_uint numEventsInWaitList = 0;
    const cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    auto &commandStream = pCmdQ->getCS(0);
    auto used = commandStream.getUsed();

    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // csr is untouched as we do not submit anything, cmd queue task level goes up as this is barrier call
    EXPECT_EQ(2u, commandStreamReceiver.peekTaskLevel());
    EXPECT_EQ(3u, pCmdQ->taskLevel);

    //make sure nothing was added to CommandStream or CSR-CommandStream and command queue still uses this stream
    EXPECT_EQ(used, commandStream.getUsed());
    EXPECT_EQ(&commandStream, &pCmdQ->getCS(0));

    EXPECT_EQ(csrUsed, csrCommandStream.getUsed());
    EXPECT_EQ(&csrCommandStream, &commandStreamReceiver.commandStream);
}

HWTEST_F(BarrierTest, CS_GT_CQ_ShouldNotAddPipeControl) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto pCS = this->pCS;
    auto pCmdQ = this->pCmdQ;
    auto pCmdBuffer = this->pCmdBuffer;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.setMediaVFEStateDirty(false);

    // Set task levels to known values.
    commandStreamReceiver.taskLevel = 2;
    pCmdQ->taskLevel = 1;

    cl_uint numEventsInWaitList = 0;
    const cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // Should sync CSR & CmdQ levels.
    EXPECT_GE(commandStreamReceiver.peekTaskLevel(), pCmdQ->taskLevel);

    auto sizeUsed = pCS->getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, pCmdBuffer, sizeUsed));

    // If CSR > CQ then a PC isn't required.
    auto itorCmd = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorCmd);
}

HWTEST_F(BarrierTest, returnsEvent) {
    auto pCmdQ = this->pCmdQ;

    cl_uint numEventsInWaitList = 0;
    const cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;
    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, event);

    // Check CL_EVENT_COMMAND_TYPE
    {
        auto pEvent = (Event *)event;
        cl_command_type cmdType = 0;
        size_t sizeReturned = 0;
        auto result = clGetEventInfo(pEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
        ASSERT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_BARRIER), cmdType);
        EXPECT_EQ(sizeof(cl_command_type), sizeReturned);

        delete pEvent;
    }
}

HWTEST_F(BarrierTest, returnedEventShouldHaveEqualDepth) {
    auto pCmdQ = this->pCmdQ;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Set task levels to known values.
    commandStreamReceiver.taskLevel = 2;
    pCmdQ->taskLevel = 1;

    cl_uint numEventsInWaitList = 0;
    const cl_event *eventWaitList = nullptr;
    cl_event event = nullptr;
    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);
    auto pEvent = (Event *)event;

    // Should sync all 3 (CSR, CmdQ, Event) levels.
    EXPECT_GE(commandStreamReceiver.peekTaskLevel(), pEvent->taskLevel);
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);

    delete pEvent;
}

HWTEST_F(BarrierTest, eventWithWaitDependenciesShouldSync) {
    auto pCmdQ = this->pCmdQ;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // In N:1, CSR is always highest task level.
    commandStreamReceiver.taskLevel = 7;

    // In N:1, pCmdQ.level <= CSR.level
    pCmdQ->taskLevel = 7;

    // In N:1, event.level <= pCmdQ.level
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 17);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2,
            &event3};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, event);
    auto pEvent = castToObject<Event>(event);
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();

    // in this case only cmdQ raises the taskLevel why csr stay intact
    EXPECT_EQ(8u, pCmdQ->taskLevel);
    if (csr.peekTimestampPacketWriteEnabled()) {
        EXPECT_EQ(pCmdQ->taskLevel + 1, commandStreamReceiver.peekTaskLevel());
    } else {
        EXPECT_EQ(7u, commandStreamReceiver.peekTaskLevel());
    }
    EXPECT_EQ(pCmdQ->taskLevel, pEvent->taskLevel);
    EXPECT_EQ(8u, pEvent->taskLevel);

    delete pEvent;
}
HWTEST_F(BarrierTest, givenNotBlockedCommandQueueAndEnqueueBarrierWithWaitlistReturningEventWhenCallIsMadeThenDontWaitUntilEventIsSignaled) {
    // In N:1, event.level <= pCmdQ.level
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    Event event2(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 17);
    cl_event eventWaitList[] =
        {
            &event1,
            &event2,
            &event3};
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    auto latestTaskCountWaitedBeforeEnqueue = this->pCmdQ->latestTaskCountWaited.load();
    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(latestTaskCountWaitedBeforeEnqueue, this->pCmdQ->latestTaskCountWaited);
    auto pEvent = castToObject<Event>(event);

    if (csr.peekTimestampPacketWriteEnabled()) {
        EXPECT_EQ(csr.peekTaskCount(), pEvent->peekTaskCount());
    } else {
        EXPECT_EQ(17u, pEvent->peekTaskCount());
    }
    EXPECT_TRUE(pEvent->updateStatusAndCheckCompletion());
    delete pEvent;
}

HWTEST_F(BarrierTest, givenBlockedCommandQueueAndEnqueueBarrierWithWaitlistReturningEventWhenCallIsMadeThenReturnEventIsNotSignaled) {
    UserEvent event2(&pCmdQ->getContext());
    cl_event eventWaitList[] =
        {
            &event2,
        };
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;
    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);

    EXPECT_EQ(CL_SUCCESS, retVal);
    auto pEvent = (Event *)event;
    EXPECT_EQ(pEvent->peekTaskCount(), Event::eventNotReady);
    event2.setStatus(CL_COMPLETE);
    clReleaseEvent(event);
}

HWTEST_F(BarrierTest, givenEmptyCommandStreamAndBlockedBarrierCommandWhenUserEventIsSignaledThenNewCommandStreamIsNotAcquired) {
    UserEvent event2(&pCmdQ->getContext());
    cl_event eventWaitList[] =
        {
            &event2,
        };
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);
    cl_event event = nullptr;

    auto &commandStream = pCmdQ->getCS(0);

    auto commandStreamStart = commandStream.getUsed();
    auto commandStreamBuffer = commandStream.getCpuBase();

    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        &event);

    EXPECT_EQ(CL_SUCCESS, retVal);

    // Consume all memory except what is needed for this enqueue
    size_t barrierCmdStreamSize = NEO::EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_BARRIER, false, false, *pCmdQ, nullptr);
    commandStream.getSpace(commandStream.getMaxAvailableSpace() - barrierCmdStreamSize);

    //now trigger event
    event2.setStatus(CL_COMPLETE);

    auto commandStreamStart2 = commandStream.getUsed();
    auto commandStreamBuffer2 = commandStream.getCpuBase();

    EXPECT_EQ(0u, commandStreamStart);
    EXPECT_GT(commandStreamStart2, 0u);
    EXPECT_EQ(commandStreamBuffer2, commandStreamBuffer);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), commandStream.getMaxAvailableSpace());

    clReleaseEvent(event);
}
