/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

using namespace NEO;

struct OOMSetting {
    bool oomCS;
    bool oomISH;
};

static OOMSetting oomSettings[] = {
    {true, false},
    {false, true},
    {true, true}};

struct OOMCommandQueueTest : public DeviceFixture,
                             public CommandQueueFixture,
                             public ::testing::TestWithParam<OOMSetting> {

    using CommandQueueFixture::SetUp;

    OOMCommandQueueTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        context = new MockContext(pClDevice);
        CommandQueueFixture::SetUp(context, pClDevice, 0);

        const auto &oomSetting = GetParam();
        auto oomSize = 10u;
        if (oomSetting.oomCS) {
            auto &cs = pCmdQ->getCS(oomSize);

            // CommandStream may be larger than requested so grab what wasnt requested
            cs.getSpace(cs.getAvailableSpace() - oomSize);
            ASSERT_EQ(oomSize, cs.getAvailableSpace());
        }

        if (oomSetting.oomISH) {
            auto &ish = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, oomSize);

            // IndirectHeap may be larger than requested so grab what wasnt requested
            ish.getSpace(ish.getAvailableSpace() - oomSize);
            ASSERT_EQ(oomSize, ish.getAvailableSpace());
        }
    }

    void TearDown() override {
        CommandQueueFixture::TearDown();
        context->release();
        DeviceFixture::TearDown();
    }

    MockContext *context;
};

HWTEST_P(OOMCommandQueueTest, finish) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 10);
    auto usedBeforeCS = commandStream.getUsed();
    auto usedBeforeISH = indirectHeap.getUsed();

    auto retVal = pCmdQ->finish();

    auto usedAfterCS = commandStream.getUsed();
    auto usedAfterISH = indirectHeap.getUsed();
    EXPECT_LE(usedAfterCS - usedBeforeCS, commandStream.getMaxAvailableSpace());
    EXPECT_LE(usedAfterISH - usedBeforeISH, indirectHeap.getMaxAvailableSpace());

    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_P(OOMCommandQueueTest, enqueueMarker) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 10);
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

HWTEST_P(OOMCommandQueueTest, enqueueBarrier) {
    auto &commandStream = pCmdQ->getCS(1024);
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 10);
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

INSTANTIATE_TEST_CASE_P(
    OOM,
    OOMCommandQueueTest,
    testing::ValuesIn(oomSettings));
