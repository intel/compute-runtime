/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

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
            auto gpuAddress = event->getGpuAddress(device);
            EXPECT_EQ(cmd->getAddressHigh(), gpuAddress >> 32u);
            EXPECT_EQ(cmd->getAddress(), uint32_t(gpuAddress));
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendEventReset, whenResetEventIsAppendedAndNoSpaceIsAvailableThenNextCommandBufferIsCreated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto firstBatchBufferAllocation = commandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    auto useSize = commandList->commandContainer.getCommandStream()->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    commandList->commandContainer.getCommandStream()->getSpace(useSize);

    auto result = commandList->appendEventReset(event->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondBatchBufferAllocation = commandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    EXPECT_NE(firstBatchBufferAllocation, secondBatchBufferAllocation);
}

HWTEST_F(CommandListAppendEventReset, givenCopyOnlyCmdlistWhenResetEventAppendedThenMiFlushWithPostSyncIsGenerated) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    commandList.reset(whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue)));

    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    auto result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itorPC = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*it);
        if (cmd->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_INITIAL);
            auto gpuAddress = event->getGpuAddress(device);
            EXPECT_EQ(cmd->getDestinationAddress(), gpuAddress);
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
    for (auto alloc : eventPoolAlloc->getGraphicsAllocations()) {
        auto itor =
            std::find(std::begin(residencyContainer), std::end(residencyContainer), alloc);
        EXPECT_NE(itor, std::end(residencyContainer));
    }
}

using SklPlusMatcher = IsAtLeastProduct<IGFX_SKYLAKE>;
HWTEST2_F(CommandListAppendEventReset, givenImmediateCmdlistWhenAppendingEventResetThenCommandsAreExecuted, SklPlusMatcher) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::RenderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    auto result = commandList0->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendEventReset, givenTimestampEventUsedInResetThenPipeControlAppendedCorrectly, SklPlusMatcher) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->commandContainer;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    event->setPacketsInUse(16u);
    commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(1u, event->getPacketsInUse());

    auto contextOffset = event->getContextEndOffset();
    auto baseAddr = event->getGpuAddress(device);
    auto gpuAddress = ptrOffset(baseAddr, contextOffset);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    uint32_t postSyncFound = 0u;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_CLEARED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(cmd->getAddressHigh(), gpuAddress >> 32u);
            EXPECT_EQ(cmd->getAddress(), uint32_t(gpuAddress));
            EXPECT_FALSE(cmd->getDcFlushEnable());
            postSyncFound++;
            gpuAddress += event->getSinglePacketSize();
        }
    }
    ASSERT_EQ(EventPacketsCount::eventPackets, postSyncFound);
}

HWTEST2_F(CommandListAppendEventReset, givenEventWithHostScopeUsedInResetThenPipeControlWithDcFlushAppended, SklPlusMatcher) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->commandContainer;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    commandList->appendEventReset(event->toHandle());
    auto gpuAddress = event->getGpuAddress(device);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_CLEARED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(cmd->getAddressHigh(), gpuAddress >> 32u);
            EXPECT_EQ(cmd->getAddress(), uint32_t(gpuAddress));
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}
} // namespace ult
} // namespace L0
