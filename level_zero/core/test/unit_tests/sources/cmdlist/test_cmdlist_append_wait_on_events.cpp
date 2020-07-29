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

using CommandListAppendWaitOnEvent = Test<CommandListFixture>;

HWTEST_F(CommandListAppendWaitOnEvent, WhenAppendingWaitOnEventThenSemaphoreWaitCmdIsGenerated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = commandList->appendWaitOnEvents(1, &hEventHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    {
        auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
        EXPECT_EQ(cmd->getCompareOperation(),
                  MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(static_cast<uint32_t>(-1), cmd->getSemaphoreDataDword());

        auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

        EXPECT_EQ(cmd->getSemaphoreGraphicsAddress() & addressSpace, event->getGpuAddress() & addressSpace);
        EXPECT_EQ(cmd->getWaitMode(),
                  MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
    }
}

HWTEST_F(CommandListAppendWaitOnEvent, givenTwoEventsWhenWaitOnEventsAppendedThenTwoSemaphoreWaitCmdsAreGenerated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    ze_event_handle_t handles[2] = {event->toHandle(), event->toHandle()};

    auto result = commandList->appendWaitOnEvents(2, handles);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u, itor.size());

    for (int i = 0; i < 2; i++) {
        auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor[i]);
        EXPECT_EQ(cmd->getCompareOperation(),
                  MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(static_cast<uint32_t>(-1), cmd->getSemaphoreDataDword());

        auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

        EXPECT_EQ(cmd->getSemaphoreGraphicsAddress() & addressSpace, event->getGpuAddress() & addressSpace);
        EXPECT_EQ(cmd->getWaitMode(),
                  MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
    }
}

HWTEST_F(CommandListAppendWaitOnEvent, WhenAppendingWaitOnEventsThenEventGraphicsAllocationIsAddedToResidencyContainer) {
    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = commandList->appendWaitOnEvents(1, &hEventHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &residencyContainer = commandList->commandContainer.getResidencyContainer();
    auto eventPoolAlloc = &eventPool->getAllocation();
    auto itor =
        std::find(std::begin(residencyContainer), std::end(residencyContainer), eventPoolAlloc);
    EXPECT_NE(itor, std::end(residencyContainer));
}

HWTEST_F(CommandListAppendWaitOnEvent, givenEventWithWaitScopeFlagDeviceWhenAppendingWaitOnEventThenPCWithDcFlushIsGenerated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    auto event = std::unique_ptr<Event>(Event::create(eventPool.get(), &eventDesc, device));
    ze_event_handle_t hEventHandle = event->toHandle();

    auto result = commandList->appendWaitOnEvents(1, &hEventHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end()).back();
    ASSERT_NE(cmdList.end(), itorPC);
    {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
        ASSERT_NE(cmd, nullptr);

        EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
}

using Platforms = IsAtLeastProduct<IGFX_SKYLAKE>;
HWTEST2_F(CommandListAppendWaitOnEvent, givenCommandListWhenAppendWriteGlobalTimestampCalledWithWaitOnEventsThenSemaphoreWaitAndPipeControlForTimestampEncoded, Platforms) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t timestampAddress = 0x12345678555500;
    uint32_t timestampAddressLow = (uint32_t)(timestampAddress & 0xFFFFFFFF);
    uint32_t timestampAddressHigh = (uint32_t)(timestampAddress >> 32);
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);
    ze_event_handle_t hEventHandle = event->toHandle();

    commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 1, &hEventHandle);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(-1), cmd->getSemaphoreDataDword());

    auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

    EXPECT_EQ(cmd->getSemaphoreGraphicsAddress() & addressSpace, event->getGpuAddress() & addressSpace);
    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);

    itor++;

    auto itorPC = findAll<PIPE_CONTROL *>(itor, cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmdPC = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmdPC->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP) {
            EXPECT_TRUE(cmdPC->getCommandStreamerStallEnable());
            EXPECT_FALSE(cmdPC->getDcFlushEnable());
            EXPECT_EQ(cmdPC->getAddressHigh(), timestampAddressHigh);
            EXPECT_EQ(cmdPC->getAddress(), timestampAddressLow);
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

} // namespace ult
} // namespace L0