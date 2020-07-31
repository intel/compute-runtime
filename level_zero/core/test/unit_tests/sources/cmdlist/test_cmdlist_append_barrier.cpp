/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using CommandListAppendBarrier = Test<CommandListFixture>;

HWTEST_F(CommandListAppendBarrier, WhenAppendingBarrierThenPipeControlIsGenerated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    auto result = commandList->appendBarrier(nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), usedSpaceBefore),
                                                      usedSpaceAfter - usedSpaceBefore));

    // Find a PC w/ CS stall
    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);
    auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_FALSE(cmd->getDcFlushEnable());
}

HWTEST_F(CommandListAppendBarrier, GivenEventWhenAppendingBarrierThenTwoPipeControlsAreAddedToCommandStream) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    auto result = commandList->appendBarrier(event->toHandle(), 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    // Ensure we have two pipe controls: one for barrier, one for signal
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_FALSE(itor.empty());
    ASSERT_LT(1, static_cast<int>(itor.size()));
}
} // namespace ult
} // namespace L0