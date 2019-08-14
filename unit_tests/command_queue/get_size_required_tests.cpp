/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/enqueue_barrier.h"
#include "runtime/command_queue/enqueue_marker.h"
#include "runtime/event/event.h"
#include "test.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

struct GetSizeRequiredTest : public CommandEnqueueFixture,
                             public ::testing::Test {

    void SetUp() override {
        CommandEnqueueFixture::SetUp();
        dsh = &pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
        ioh = &pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
        ssh = &pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);

        usedBeforeDSH = dsh->getUsed();
        usedBeforeIOH = ioh->getUsed();
        usedBeforeSSH = ssh->getUsed();
    }

    void TearDown() override {
        CommandEnqueueFixture::TearDown();
    }

    IndirectHeap *dsh;
    IndirectHeap *ioh;
    IndirectHeap *ssh;

    size_t usedBeforeDSH;
    size_t usedBeforeIOH;
    size_t usedBeforeSSH;
};

HWTEST_F(GetSizeRequiredTest, finish) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();

    auto retVal = pCmdQ->finish(false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, commandStream.getUsed() - usedBeforeCS);
    EXPECT_EQ(0u, dsh->getUsed() - usedBeforeDSH);
    EXPECT_EQ(0u, ioh->getUsed() - usedBeforeIOH);
    EXPECT_EQ(0u, ssh->getUsed() - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredTest, enqueueMarker) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();

    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    cl_event eventBeingWaitedOn = &event1;
    cl_event eventReturned = nullptr;
    auto retVal = pCmdQ->enqueueMarkerWithWaitList(
        1,
        &eventBeingWaitedOn,
        &eventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t expectedStreamSize = 0;
    if (pCmdQ->getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        expectedStreamSize = alignUp(PipeControlHelper<FamilyType>::getSizeForPipeControlWithPostSyncOperation(pDevice->getHardwareInfo()),
                                     +MemoryConstants::cacheLineSize);
    }
    EXPECT_EQ(expectedStreamSize, commandStream.getUsed() - usedBeforeCS);
    EXPECT_EQ(0u, dsh->getUsed() - usedBeforeDSH);
    EXPECT_EQ(0u, ioh->getUsed() - usedBeforeIOH);
    EXPECT_EQ(0u, ssh->getUsed() - usedBeforeSSH);

    clReleaseEvent(eventReturned);
}

HWTEST_F(GetSizeRequiredTest, enqueueBarrierDoesntConsumeAnySpace) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();

    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    cl_event eventBeingWaitedOn = &event1;
    cl_event eventReturned = nullptr;
    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        1,
        &eventBeingWaitedOn,
        &eventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t expectedStreamSize = 0;
    if (pCmdQ->getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        expectedStreamSize = alignUp(PipeControlHelper<FamilyType>::getSizeForPipeControlWithPostSyncOperation(pDevice->getHardwareInfo()),
                                     +MemoryConstants::cacheLineSize);
    }

    EXPECT_EQ(expectedStreamSize, commandStream.getUsed() - usedBeforeCS);

    clReleaseEvent(eventReturned);
}
