/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct OOMSetting {
    bool oomCS;
    bool oomISH;
};

static OOMSetting oomSettings[] = {
    {true, false},
    {false, true},
    {true, true}};

struct OOMCommandQueueTest : public ClDeviceFixture,
                             public CommandQueueFixture,
                             public ::testing::Test {

    using CommandQueueFixture::setUp;

    OOMCommandQueueTest() {
    }

    void SetUp() override {
        ClDeviceFixture::setUp();
        context = new MockContext(pClDevice);
        CommandQueueFixture::setUp(context, pClDevice, 0);
    }

    void TearDown() override {
        CommandQueueFixture::tearDown();
        context->release();
        ClDeviceFixture::tearDown();
    }

    MockContext *context;
};

HWTEST_F(OOMCommandQueueTest, WhenFinishingThenMaxAvailableSpaceIsNotExceeded) {
    for (const auto &oomSetting : oomSettings) {
        auto oomSize = 10u;
        if (oomSetting.oomCS) {
            auto &cs = pCmdQ->getCS(oomSize);
            cs.getSpace(cs.getAvailableSpace() - oomSize);
        }
        if (oomSetting.oomISH) {
            auto &ish = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, oomSize);
            ish.getSpace(ish.getAvailableSpace() - oomSize);
        }
        auto &commandStream = pCmdQ->getCS(1024);
        auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 10);
        auto usedBeforeCS = commandStream.getUsed();
        auto usedBeforeISH = indirectHeap.getUsed();

        auto retVal = pCmdQ->finish(false);

        auto usedAfterCS = commandStream.getUsed();
        auto usedAfterISH = indirectHeap.getUsed();
        EXPECT_LE(usedAfterCS - usedBeforeCS, commandStream.getMaxAvailableSpace());
        EXPECT_LE(usedAfterISH - usedBeforeISH, indirectHeap.getMaxAvailableSpace());

        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

HWTEST_F(OOMCommandQueueTest, WhenEnqueingMarkerThenMaxAvailableSpaceIsNotExceeded) {
    for (const auto &oomSetting : oomSettings) {
        auto oomSize = 10u;
        if (oomSetting.oomCS) {
            auto &cs = pCmdQ->getCS(oomSize);
            cs.getSpace(cs.getAvailableSpace() - oomSize);
        }
        if (oomSetting.oomISH) {
            auto &ish = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, oomSize);
            ish.getSpace(ish.getAvailableSpace() - oomSize);
        }
        auto &commandStream = pCmdQ->getCS(1024);
        auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 10);
        auto usedBeforeCS = commandStream.getUsed();
        auto usedBeforeISH = indirectHeap.getUsed();

        Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
        cl_event eventBeingWaitedOn = &event1;
        cl_event eventReturned = nullptr;
        auto retVal = pCmdQ->enqueueMarkerWithWaitList(
            1,
            &eventBeingWaitedOn,
            &eventReturned);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto usedAfterCS = commandStream.getUsed();
        auto usedAfterISH = indirectHeap.getUsed();
        EXPECT_LE(usedAfterCS - usedBeforeCS, commandStream.getMaxAvailableSpace());
        EXPECT_LE(usedAfterISH - usedBeforeISH, indirectHeap.getMaxAvailableSpace());

        delete (Event *)eventReturned;
    }
}

HWTEST_F(OOMCommandQueueTest, WhenEnqueingBarrierThenMaxAvailableSpaceIsNotExceeded) {
    for (const auto &oomSetting : oomSettings) {
        auto oomSize = 10u;
        if (oomSetting.oomCS) {
            auto &cs = pCmdQ->getCS(oomSize);
            cs.getSpace(cs.getAvailableSpace() - oomSize);
        }
        if (oomSetting.oomISH) {
            auto &ish = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, oomSize);
            ish.getSpace(ish.getAvailableSpace() - oomSize);
        }
        auto &commandStream = pCmdQ->getCS(1024);
        auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::Type::dynamicState, 10);
        auto usedBeforeCS = commandStream.getUsed();
        auto usedBeforeISH = indirectHeap.getUsed();

        Event event1(pCmdQ, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
        cl_event eventBeingWaitedOn = &event1;
        cl_event eventReturned = nullptr;
        auto retVal = pCmdQ->enqueueBarrierWithWaitList(
            1,
            &eventBeingWaitedOn,
            &eventReturned);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto usedAfterCS = commandStream.getUsed();
        auto usedAfterISH = indirectHeap.getUsed();
        EXPECT_LE(usedAfterCS - usedBeforeCS, commandStream.getMaxAvailableSpace());
        EXPECT_LE(usedAfterISH - usedBeforeISH, indirectHeap.getMaxAvailableSpace());

        delete (Event *)eventReturned;
    }
}
