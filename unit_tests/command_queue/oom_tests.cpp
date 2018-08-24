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

#include "runtime/event/event.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/memory_manager/memory_manager.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include "unit_tests/mocks/mock_context.h"

using namespace OCLRT;

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
        context = new MockContext(pDevice);
        CommandQueueFixture::SetUp(context, pDevice, 0);

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

    auto retVal = pCmdQ->finish(false);

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
