/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "test.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_command_queue.h"

#include "gtest/gtest.h"

using namespace NEO;

struct FinishFixture : public DeviceFixture,
                       public CommandQueueHwFixture,
                       public CommandStreamFixture,
                       public HardwareParse {

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueHwFixture::SetUp(pDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
        CommandStreamFixture::SetUp(pCmdQ);
        ASSERT_NE(nullptr, pCS);
        HardwareParse::SetUp();
    }

    void TearDown() override {
        HardwareParse::TearDown();
        CommandStreamFixture::TearDown();
        CommandQueueHwFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

typedef Test<FinishFixture> FinishTest;

HWTEST_F(FinishTest, GivenCsGreaterThanCqWhenFinishIsCalledThenPipeControlIsNotAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // HW = 1, CQ = 1, CS = 2 (last PC was 1)
    // This means there is no work in CQ that needs a PC
    uint32_t originalHwTag = 1;
    uint32_t originalCSRLevel = 2;
    uint32_t originalCQLevel = 1;
    *commandStreamReceiver.getTagAddress() = originalHwTag;
    commandStreamReceiver.taskLevel = originalCSRLevel; // Must be greater than or equal to HW
    pCmdQ->taskLevel = originalCQLevel;

    auto retVal = pCmdQ->finish();
    ASSERT_EQ(CL_SUCCESS, retVal);

    // Don't need to artificially execute PIPE_CONTROL.
    // Nothing should have been sent
    //*pCS->getTagAddress() = originalCSRLevel;

    EXPECT_EQ(commandStreamReceiver.peekTaskLevel(), originalCSRLevel);
    EXPECT_EQ(pCmdQ->taskLevel, originalCQLevel);
    EXPECT_GE(pCmdQ->getHwTag(), pCmdQ->taskLevel);

    auto sizeUsed = pCS->getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, pCmdBuffer, sizeUsed));

    auto itorCmd = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorCmd);
}

HWTEST_F(FinishTest, WhenFinishIsCalledThenPipeControlIsNotAddedToCqCommandStream) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    auto retVal = pCmdQ->finish();
    ASSERT_EQ(CL_SUCCESS, retVal);

    // Check for PIPE_CONTROL
    parseCommands<FamilyType>(pCmdQ->getCS(1024));
    auto itorCmd = reverse_find<PIPE_CONTROL *>(cmdList.rbegin(), cmdList.rend());
    EXPECT_EQ(cmdList.rend(), itorCmd);
}
HWTEST_F(FinishTest, givenFreshQueueWhenFinishIsCalledThenCommandStreamIsNotAllocated) {
    MockContext contextWithMockCmdQ(pDevice, true);
    MockCommandQueueHw<FamilyType> cmdQ(&contextWithMockCmdQ, pDevice, 0);

    auto retVal = cmdQ.finish();
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, cmdQ.peekCommandStream());
}
