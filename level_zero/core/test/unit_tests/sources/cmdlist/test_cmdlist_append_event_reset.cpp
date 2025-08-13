/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_command_encoder.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {

using CommandListAppendEventReset = Test<CommandListFixture>;
using CommandListAppendUsedPacketSignalEvent = Test<CommandListEventUsedPacketSignalFixture>;
using CommandListAppendEventResetSecondaryBatchBuffer = Test<CommandListSecondaryBatchBufferFixture>;

HWTEST_F(CommandListAppendEventReset, givenCmdlistWhenResetEventAppendedThenStoreDataImmIsGenerated) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    auto result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto gpuAddress = event->getCompletionFieldGpuAddress(device);
    auto itorSdi = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    uint32_t sdiFound = 0;
    ASSERT_NE(0u, itorSdi.size());
    for (auto it : itorSdi) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*it);
        EXPECT_EQ(gpuAddress, cmd->getAddress());
        gpuAddress += event->getSinglePacketSize();
        sdiFound++;
    }
    EXPECT_NE(0u, sdiFound);
}

HWTEST_F(CommandListAppendEventReset, givenCmdlistWhenResetEventWithTimeStampIsAppendedThenStoreDataImmAndPostSyncWriteIsGenerated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    auto gpuAddress = event->getCompletionFieldGpuAddress(device);

    auto &hwInfo = device->getHwInfo();
    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    uint32_t maxPackets = EventPacketsCount::eventPackets;
    if (l0GfxCoreHelper.useDynamicEventPacketsCount(hwInfo)) {
        maxPackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());
    }

    auto itorSdi = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

    if (maxPackets == 1) {
        EXPECT_EQ(0u, itorSdi.size());
    } else {
        uint32_t sdiFound = 0;
        ASSERT_NE(0u, itorSdi.size());
        for (auto it : itorSdi) {
            auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*it);
            EXPECT_EQ(gpuAddress, cmd->getAddress());
            gpuAddress += event->getSinglePacketSize();
            sdiFound++;
        }
        EXPECT_EQ(maxPackets - 1, sdiFound);
    }

    uint32_t postSyncFound = 0;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_INITIAL);
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            postSyncFound++;
        }
    }
    EXPECT_EQ(1u, postSyncFound);
}

HWTEST_F(CommandListAppendEventResetSecondaryBatchBuffer, whenResetEventIsAppendedAndNoSpaceIsAvailableThenNextCommandBufferIsCreated) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto firstBatchBufferAllocation = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();

    auto useSize = commandList->getCmdContainer().getCommandStream()->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    commandList->getCmdContainer().getCommandStream()->getSpace(useSize);

    auto result = commandList->appendEventReset(event->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondBatchBufferAllocation = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();

    EXPECT_NE(firstBatchBufferAllocation, secondBatchBufferAllocation);
}

HWTEST_F(CommandListAppendEventReset, givenCopyOnlyCmdlistWhenResetEventAppendedThenMiFlushWithPostSyncIsGenerated) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    commandList.reset(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false)));

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    auto result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itorPC = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;

    auto gpuAddress = event->getCompletionFieldGpuAddress(device);

    for (auto it : itorPC) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*it);
        if (cmd->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_INITIAL);
            EXPECT_EQ(cmd->getDestinationAddress(), gpuAddress);
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendEventReset, givenCmdlistWhenAppendingEventResetThenEventPoolGraphicsAllocationIsAddedToResidencyContainer) {
    auto result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &residencyContainer = commandList->getCmdContainer().getResidencyContainer();
    auto eventPoolAlloc = &eventPool->getAllocation();
    for (auto alloc : eventPoolAlloc->getGraphicsAllocations()) {
        auto itor =
            std::find(std::begin(residencyContainer), std::end(residencyContainer), alloc);
        EXPECT_NE(itor, std::end(residencyContainer));
    }
}

HWTEST_F(CommandListAppendEventReset, givenRegularCmdlistWhenAppendingEventResetThenHostBufferCachingDisabledPermanently) {
    MockEvent event;
    event.isCompleted = MockEvent::STATE_SIGNALED;
    auto result = commandList->appendEventReset(event.toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(MockEvent::HOST_CACHING_DISABLED_PERMANENT, event.isCompleted);
}

HWTEST_F(CommandListAppendEventReset, givenImmediateCmdlistWhenAppendingEventResetThenHostBufferCachingDisabled) {
    MockEvent event;
    event.isCompleted = MockEvent::STATE_SIGNALED;

    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    auto result = commandList->appendEventReset(event.toHandle());
    commandList->cmdListType = CommandList::CommandListType::typeRegular;

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(MockEvent::HOST_CACHING_DISABLED, event.isCompleted);
}

HWTEST_F(CommandListAppendEventReset, WhenIsCompletedClearedThenSetStateSignaledWhenSignalAgain) {
    event->reset();
    EXPECT_FALSE(event->isAlreadyCompleted());
    event->hostSignal(false);
    EXPECT_TRUE(event->isAlreadyCompleted());
}

HWTEST_F(CommandListAppendEventReset, WhenIsCompletedDisabledThenDontSetStateSignaledWhenSignalAgain) {
    auto result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(event->isAlreadyCompleted());
    event->hostSignal(false);
    EXPECT_FALSE(event->isAlreadyCompleted());
}

HWTEST_F(CommandListAppendEventReset, givenRegularCommandListWhenHostCachingDisabledThenDisablePermanently) {
    auto result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(event->isAlreadyCompleted());
    event->reset();
    event->hostSignal(false);
    EXPECT_FALSE(event->isAlreadyCompleted());
}

HWTEST_F(CommandListAppendEventReset, givenRegulatCommandListWhenHostCachingDisabledThenEnableAfterCpuReset) {
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    commandList->appendEventReset(event->toHandle());
    commandList->cmdListType = CommandList::CommandListType::typeRegular;

    EXPECT_FALSE(event->isAlreadyCompleted());
    event->reset();
    event->hostSignal(false);
    EXPECT_TRUE(event->isAlreadyCompleted());
}

HWTEST_F(CommandListAppendEventReset, givenImmediateCmdlistWhenAppendingEventResetThenCommandsAreExecuted) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    auto result = commandList0->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(CommandListAppendUsedPacketSignalEvent, givenTimestampEventUsedInResetThenPipeControlAppendedCorrectly) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->getCmdContainer();

    auto &hwInfo = device->getHwInfo();
    auto &l0GfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    event->setPacketsInUse(3u);
    commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(3u, event->getPacketsInUse());

    auto contextOffset = event->getContextEndOffset();
    auto baseAddr = event->getGpuAddress(device);
    auto gpuAddress = ptrOffset(baseAddr, contextOffset);

    uint32_t maxPackets = EventPacketsCount::eventPackets;
    if (l0GfxCoreHelper.useDynamicEventPacketsCount(hwInfo)) {
        maxPackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(device->getNEODevice()->getRootDeviceEnvironment());
    }
    gpuAddress += ((maxPackets - 1) * event->getSinglePacketSize());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    uint32_t postSyncFound = 0u;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_CLEARED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_FALSE(cmd->getDcFlushEnable());
            gpuAddress += event->getSinglePacketSize();
            postSyncFound++;
        }
    }
    ASSERT_EQ(1u, postSyncFound);
}

HWTEST_F(CommandListAppendEventReset, givenEventWithHostScopeUsedInResetThenPipeControlWithDcFlushAppended) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->getCmdContainer();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));

    commandList->appendEventReset(event->toHandle());
    auto gpuAddress = event->getCompletionFieldGpuAddress(device);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_CLEARED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()), cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenMultiTileCommandListWhenAppendingMultiPacketEventThenExpectCorrectNumberOfStoreDataImmAndResetPostSyncAndMultiBarrierCommands, IsAtLeastXeCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    size_t useSize = cmdStream->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    cmdStream->getSpace(useSize);

    constexpr uint32_t packets = 2u;
    event->setPacketsInUse(packets);
    event->setEventTimestampFlag(false);
    event->setUsingContextEndOffset(true);
    event->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    commandList->partitionCount = packets;
    returnValue = commandList->appendEventReset(event->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, event->getPacketsInUse());

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    size_t expectedSize = NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(device->getNEODevice()->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData) +
                          commandList->estimateBufferSizeMultiTileBarrier(device->getNEODevice()->getRootDeviceEnvironment());
    size_t usedSize = cmdStream->getUsed();
    EXPECT_EQ(expectedSize, usedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedSize));

    auto itorSdi = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    // multi tile barrier self-cleanup commands
    ASSERT_EQ(2u, itorSdi.size());

    auto pipeControlList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControlList.size());
    uint32_t postSyncFound = 0;
    auto postSyncPipeControlItor = cmdList.end();
    for (auto &it : pipeControlList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_CLEARED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()), cmd->getDcFlushEnable());
            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
            postSyncFound++;
            gpuAddress += event->getSinglePacketSize() * commandList->partitionCount;
            postSyncPipeControlItor = it;
        }
    }
    EXPECT_EQ(1u, postSyncFound);
    postSyncPipeControlItor++;
    ASSERT_NE(cmdList.end(), postSyncPipeControlItor);

    // find multi tile barrier section: pipe control + atomic/semaphore
    auto itorPipeControl = find<PIPE_CONTROL *>(postSyncPipeControlItor, cmdList.end());
    ASSERT_NE(cmdList.end(), itorPipeControl);

    auto itorAtomic = find<MI_ATOMIC *>(itorPipeControl, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAtomic);

    auto itorSemaphore = find<MI_SEMAPHORE_WAIT *>(itorAtomic, cmdList.end());
    ASSERT_NE(cmdList.end(), itorSemaphore);
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenCopyCommandListWhenAppendingMultiPacketEventThenExpectCorrectNumberOfMiFlushResetPostSyncCommands, IsAtLeastXeCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<FamilyType::gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    constexpr uint32_t packets = 2u;
    event->setPacketsInUse(packets);
    event->setUsingContextEndOffset(true);

    size_t usedBeforeSize = cmdStream->getUsed();
    returnValue = commandList->appendEventReset(event->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, event->getPacketsInUse());
    size_t usedAfterSize = cmdStream->getUsed();

    NEO::EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&device->getNEODevice()->getRootDeviceEnvironment())};
    size_t expectedSize = NEO::EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) * packets;
    EXPECT_EQ(expectedSize, (usedAfterSize - usedBeforeSize));

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), usedBeforeSize),
        expectedSize));

    uint32_t miFlushCountFactor = 1;
    waArgs.isWaRequired = false;
    if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs) > 0) {
        miFlushCountFactor = 2;
    }
    auto expectedMiFlushCount = packets * miFlushCountFactor;

    auto itorMiFlush = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedMiFlushCount, static_cast<uint32_t>(itorMiFlush.size()));

    for (uint32_t i = 0; i < expectedMiFlushCount; i++) {
        if ((miFlushCountFactor == 2) && (i % 2 == 0)) {
            continue;
        }
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*itorMiFlush[i]);
        EXPECT_EQ(gpuAddress, cmd->getDestinationAddress());
        EXPECT_EQ(Event::STATE_CLEARED, cmd->getImmediateData());
        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, cmd->getPostSyncOperation());

        gpuAddress += event->getSinglePacketSize();
    }
}

} // namespace ult
} // namespace L0
