/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using CommandListAppendGen9 = Test<DeviceFixture>;

TEST(CommandListAppendMemoryRangesBarrier, WhenAppendingMemoryRangesBarrierThenSuccessIsReturned) {
    MockCommandList commandList;

    uint32_t numRanges = 1;
    const size_t pRangeSizes = 1;
    const char *ranges[pRangeSizes];
    const void **pRanges = reinterpret_cast<const void **>(&ranges[0]);

    auto result = zeCommandListAppendMemoryRangesBarrier(commandList.toHandle(),
                                                         numRanges, &pRangeSizes,
                                                         pRanges, nullptr, 0,
                                                         nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendGen9, WhenAppendingMemoryRangesBarrierThenPipeControlAddedToCommandStream, IsGen9) {
    uint32_t numRanges = 1;
    const size_t pRangeSizes = 1;
    const char *ranges[pRangeSizes];
    const void **pRanges = reinterpret_cast<const void **>(&ranges[0]);

    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_FALSE(ret);
    auto usedSpaceBefore =
        commandList->commandContainer.getCommandStream()->getUsed();
    commandList->appendMemoryRangesBarrier(numRanges, &pRangeSizes, pRanges,
                                           nullptr, 0, nullptr);
    auto usedSpaceAfter =
        commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(
            commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
        usedSpaceAfter));

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(cmd->getDcFlushEnable());

    commandList->destroy();
}

HWTEST2_F(CommandListAppendGen9, givenSignalEventWhenAppendingMemoryRangesBarrierThenSecondPipeControlAddedToCommandStreamForCompletion, IsGen9) {
    uint32_t numRanges = 1;
    const size_t pRangeSizes = 1;
    const char *ranges[pRangeSizes];
    const void **pRanges = reinterpret_cast<const void **>(&ranges[0]);

    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    bool ret = commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ASSERT_FALSE(ret);
    ze_event_pool_desc_t eventPoolDesc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        1};

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        ZE_EVENT_SCOPE_FLAG_HOST,
        ZE_EVENT_SCOPE_FLAG_HOST};

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto context = std::make_unique<ContextImp>(device->getDriverHandle());
    auto hDevice = device->toHandle();
    auto eventPool = whiteboxCast(EventPool::create(device->getDriverHandle(), context.get(), 1, &hDevice, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = whiteboxCast(Event::create<uint32_t>(eventPool, &eventDesc, device));
    auto usedSpaceBefore =
        commandList->commandContainer.getCommandStream()->getUsed();
    commandList->appendMemoryRangesBarrier(numRanges, &pRangeSizes, pRanges,
                                           event->toHandle(), 0, nullptr);
    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    // Ensure we have two pipe controls: one for barrier, one for signal
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto itor = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_FALSE(itor.empty());
    ASSERT_LT(1, static_cast<int>(itor.size()));

    delete event;
    delete eventPool;
    commandList->destroy();
}

} // namespace ult
} // namespace L0
