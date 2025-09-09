/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"

using namespace NEO;

struct GetSizeRequiredTest : public CommandEnqueueFixture,
                             public ::testing::Test {

    void SetUp() override {
        CommandEnqueueFixture::setUp();
        dsh = &pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u);
        ioh = &pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u);
        ssh = &pCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u);

        usedBeforeDSH = dsh->getUsed();
        usedBeforeIOH = ioh->getUsed();
        usedBeforeSSH = ssh->getUsed();
    }

    void TearDown() override {
        CommandEnqueueFixture::tearDown();
    }

    IndirectHeap *dsh;
    IndirectHeap *ioh;
    IndirectHeap *ssh;

    size_t usedBeforeDSH;
    size_t usedBeforeIOH;
    size_t usedBeforeSSH;
};

HWTEST_F(GetSizeRequiredTest, WhenFinishingThenHeapsAndCommandBufferAreNotConsumed) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto usedBeforeCS = commandStream.getUsed();

    auto retVal = pCmdQ->finish(false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(0u, commandStream.getUsed() - usedBeforeCS);
    EXPECT_EQ(0u, dsh->getUsed() - usedBeforeDSH);
    EXPECT_EQ(0u, ioh->getUsed() - usedBeforeIOH);
    EXPECT_EQ(0u, ssh->getUsed() - usedBeforeSSH);
}

HWTEST_F(GetSizeRequiredTest, WhenEnqueuingMarkerThenHeapsAreNotConsumed) {
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    cl_event eventBeingWaitedOn = &event1;
    cl_event eventReturned = nullptr;
    auto retVal = pCmdQ->enqueueMarkerWithWaitList(
        1,
        &eventBeingWaitedOn,
        &eventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, dsh->getUsed() - usedBeforeDSH);
    EXPECT_EQ(0u, ioh->getUsed() - usedBeforeIOH);
    EXPECT_EQ(0u, ssh->getUsed() - usedBeforeSSH);

    clReleaseEvent(eventReturned);
}

HWTEST_F(GetSizeRequiredTest, WhenEnqueuingBarrierThenHeapsAreNotConsumed) {
    Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    cl_event eventBeingWaitedOn = &event1;
    cl_event eventReturned = nullptr;
    auto retVal = pCmdQ->enqueueBarrierWithWaitList(
        1,
        &eventBeingWaitedOn,
        &eventReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, dsh->getUsed() - usedBeforeDSH);
    EXPECT_EQ(0u, ioh->getUsed() - usedBeforeIOH);
    EXPECT_EQ(0u, ssh->getUsed() - usedBeforeSSH);

    clReleaseEvent(eventReturned);
}
