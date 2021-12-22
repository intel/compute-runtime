/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"

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

        EXPECT_EQ(cmd->getSemaphoreGraphicsAddress() & addressSpace, event->getGpuAddress(device) & addressSpace);
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

        EXPECT_EQ(cmd->getSemaphoreGraphicsAddress() & addressSpace, event->getGpuAddress(device) & addressSpace);
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
    for (auto alloc : eventPoolAlloc->getGraphicsAllocations()) {
        auto itor =
            std::find(std::begin(residencyContainer), std::end(residencyContainer), alloc);
        EXPECT_NE(itor, std::end(residencyContainer));
    }
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

    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
    }
}

HWTEST_F(CommandListAppendWaitOnEvent, WhenAppendingWaitOnTimestampEventWithThreePacketsThenSemaphoreWaitCmdIsGeneratedThreeTimes) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    event->setPacketsInUse(3u);
    ze_event_handle_t hEventHandle = event->toHandle();
    result = commandList->appendWaitOnEvents(1, &hEventHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itorSW = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorSW.size());
    uint32_t semaphoreWaitsFound = 0;

    for (auto it : itorSW) {
        auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                  cmd->getCompareOperation());
        EXPECT_EQ(cmd->getSemaphoreDataDword(), static_cast<uint32_t>(-1));
        EXPECT_EQ(gpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);
        EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, cmd->getWaitMode());

        semaphoreWaitsFound++;
        gpuAddress += event->getSinglePacketSize();
    }
    ASSERT_EQ(3u, semaphoreWaitsFound);
}

HWTEST_F(CommandListAppendWaitOnEvent, WhenAppendingWaitOnTimestampEventWithThreeKernelsThenSemaphoreWaitCmdIsGeneratedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    event->setPacketsInUse(3u);
    event->kernelCount = 2;
    event->setPacketsInUse(3u);
    event->kernelCount = 3;
    event->setPacketsInUse(3u);
    ASSERT_EQ(9u, event->getPacketsInUse());

    ze_event_handle_t hEventHandle = event->toHandle();
    result = commandList->appendWaitOnEvents(1, &hEventHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itorSW = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorSW.size());
    uint32_t semaphoreWaitsFound = 0;

    for (auto it : itorSW) {
        auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                  cmd->getCompareOperation());
        EXPECT_EQ(cmd->getSemaphoreDataDword(), static_cast<uint32_t>(-1));
        EXPECT_EQ(gpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);
        EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, cmd->getWaitMode());

        semaphoreWaitsFound++;
        gpuAddress += event->getSinglePacketSize();
    }
    ASSERT_EQ(9u, semaphoreWaitsFound);
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenCommandListWhenAppendWriteGlobalTimestampCalledWithWaitOnEventsThenSemaphoreWaitAndPipeControlForTimestampEncoded, IsAtLeastSkl) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    uint64_t timestampAddress = 0x12345678555500;
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

    EXPECT_EQ(cmd->getSemaphoreGraphicsAddress() & addressSpace, event->getGpuAddress(device) & addressSpace);
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
            EXPECT_EQ(timestampAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmdPC));
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendWaitOnEvent, givenCommandBufferIsEmptyWhenAppendingWaitOnEventThenAllocateNewCommandBuffer) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto consumeSpace = commandList->commandContainer.getCommandStream()->getAvailableSpace();
    consumeSpace -= sizeof(MI_BATCH_BUFFER_END);
    commandList->commandContainer.getCommandStream()->getSpace(consumeSpace);

    size_t expectedConsumedSpace = sizeof(MI_SEMAPHORE_WAIT);
    if (MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo)) {
        expectedConsumedSpace += sizeof(PIPE_CONTROL);
    }

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    ze_event_handle_t hEventHandle = event->toHandle();

    auto oldCommandBuffer = commandList->commandContainer.getCommandStream()->getGraphicsAllocation();
    auto result = commandList->appendWaitOnEvents(1, &hEventHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    auto newCommandBuffer = commandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    EXPECT_EQ(expectedConsumedSpace, usedSpaceAfter);
    EXPECT_NE(oldCommandBuffer, newCommandBuffer);

    auto gpuAddress = event->getGpuAddress(device);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      commandList->commandContainer.getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    if (MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo)) {
        ASSERT_NE(cmdList.end(), itorPC);
        {
            auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
            ASSERT_NE(cmd, nullptr);

            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
        }
    } else {
        EXPECT_EQ(cmdList.end(), itorPC);
    }

    auto itorSW = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorSW.size());
    uint32_t semaphoreWaitsFound = 0;
    for (auto it : itorSW) {
        auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                  cmd->getCompareOperation());
        EXPECT_EQ(cmd->getSemaphoreDataDword(), std::numeric_limits<uint32_t>::max());
        EXPECT_EQ(gpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);
        EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, cmd->getWaitMode());

        semaphoreWaitsFound++;
        gpuAddress += event->getSinglePacketSize();
    }
    EXPECT_EQ(1u, semaphoreWaitsFound);
}

} // namespace ult
} // namespace L0
