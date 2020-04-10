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
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using CommandListAppendEventReset = Test<CommandListFixture>;

HWTEST_F(CommandListAppendEventReset, givenCmdlistWhenResetEventAppendedThenPostSyncWriteIsGenerated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    auto result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_INITIAL);
            auto gpuAddress = event->getGpuAddress();
            EXPECT_EQ(cmd->getAddressHigh(), gpuAddress >> 32u);
            EXPECT_EQ(cmd->getAddress(), uint32_t(gpuAddress));
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendEventReset, givenCmdlistWhenAppendingEventResetThenEventPoolGraphicsAllocationIsAddedToResidencyContainer) {
    auto result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &residencyContainer = commandList->commandContainer.getResidencyContainer();
    auto eventPoolAlloc = &eventPool->getAllocation();
    auto itor =
        std::find(std::begin(residencyContainer), std::end(residencyContainer), eventPoolAlloc);
    EXPECT_NE(itor, std::end(residencyContainer));
}

using SklPlusMatcher = IsAtLeastProduct<IGFX_SKYLAKE>;
HWTEST2_F(CommandListAppendEventReset, givenImmediateCmdlistWhenAppendingEventResetThenCommandsAreExecuted, SklPlusMatcher) {
    Mock<CommandQueue> cmdQueue;
    const ze_command_queue_desc_t desc = {};

    auto commandList = std::make_unique<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();
    ASSERT_NE(nullptr, commandList);
    bool ret = commandList->initialize(device, false);
    ASSERT_TRUE(ret);
    commandList->device = device;
    commandList->cmdQImmediate = &cmdQueue;
    commandList->cmdListType = CommandList::CommandListType::TYPE_IMMEDIATE;
    commandList->cmdQImmediateDesc = &desc;

    EXPECT_CALL(cmdQueue, executeCommandLists).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));
    EXPECT_CALL(cmdQueue, synchronize).Times(1).WillRepeatedly(::testing::Return(ZE_RESULT_SUCCESS));

    auto result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->cmdQImmediate = nullptr;
}
} // namespace ult
} // namespace L0