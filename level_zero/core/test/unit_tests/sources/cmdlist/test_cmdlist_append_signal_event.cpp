/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using CommandListAppendSignalEvent = Test<CommandListFixture>;
HWTEST_F(CommandListAppendSignalEvent, WhenAppendingSignalEventWithoutScopeThenMiStoreImmIsGenerated) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    auto result = commandList->appendSignalEvent(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto baseAddr = event->getGpuAddress(device);
    if (event->isUsingContextEndOffset()) {
        baseAddr += event->getContextEndOffset();
    }
    auto itor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itor, cmdList.end());
    auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
    EXPECT_EQ(baseAddr, cmd->getAddress());
}

HWTEST_F(CommandListAppendSignalEvent, givenCmdlistWhenAppendingSignalEventThenEventPoolGraphicsAllocationIsAddedToResidencyContainer) {
    auto result = commandList->appendSignalEvent(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &residencyContainer = commandList->commandContainer.getResidencyContainer();
    auto eventPoolAlloc = &eventPool->getAllocation();
    for (auto alloc : eventPoolAlloc->getGraphicsAllocations()) {
        auto itor =
            std::find(std::begin(residencyContainer), std::end(residencyContainer), alloc);
        EXPECT_NE(itor, std::end(residencyContainer));
    }
}

HWTEST_F(CommandListAppendSignalEvent, givenEventWithScopeFlagDeviceWhenAppendingSignalEventThenPipeControlHasNoDcFlush) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPoolHostVisible = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto eventHostVisible = std::unique_ptr<Event>(Event::create<uint32_t>(eventPoolHostVisible.get(), &eventDesc, device));

    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    result = commandList->appendSignalEvent(eventHostVisible->toHandle());
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
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendSignalEvent, givenCommandListWhenAppendWriteGlobalTimestampCalledWithSignalEventThenPipeControlForTimestampAndSignalEncoded, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->commandContainer;

    uint64_t timestampAddress = 0x12345678555500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);

    commandList->appendWriteGlobalTimestamp(dstptr, event->toHandle(), 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    while (cmd->getPostSyncOperation() != POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP) {
        itorPC++;
        itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
        EXPECT_NE(cmdList.end(), itorPC);
        cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    }
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_FALSE(cmd->getDcFlushEnable());
    EXPECT_EQ(timestampAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));

    itorPC++;
    itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    while (cmd->getPostSyncOperation() != POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
        itorPC++;
        itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
        EXPECT_NE(cmdList.end(), itorPC);
        cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    }
    EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_FALSE(cmd->getDcFlushEnable());
}

HWTEST2_F(CommandListAppendSignalEvent, givenTimestampEventUsedInSignalThenPipeControlAppendedCorrectly, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->commandContainer;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    commandList->appendSignalEvent(event->toHandle());
    auto contextOffset = event->getContextEndOffset();
    auto baseAddr = event->getGpuAddress(device);
    auto gpuAddress = ptrOffset(baseAddr, contextOffset);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_FALSE(cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendSignalEvent,
          givenMultiTileCommandListWhenAppendingScopeEventSignalThenExpectPartitionedPipeControl, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto cmdStream = commandList->commandContainer.getCommandStream();

    size_t useSize = cmdStream->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    cmdStream->getSpace(useSize);

    constexpr uint32_t packets = 2u;

    event->setEventTimestampFlag(false);
    event->setUsingContextEndOffset(true);
    event->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    commandList->partitionCount = packets;
    ze_result_t returnValue = commandList->appendSignalEvent(event->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(packets, event->getPacketsInUse());

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();
    auto &hwInfo = device->getNEODevice()->getHardwareInfo();

    size_t expectedSize = NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(hwInfo, false);
    size_t usedSize = cmdStream->getUsed();
    EXPECT_EQ(expectedSize, usedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedSize));

    auto pipeControlList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControlList.size());
    uint32_t postSyncFound = 0;
    for (auto &it : pipeControlList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
            postSyncFound++;
            gpuAddress += event->getSinglePacketSize();
        }
    }
    EXPECT_EQ(1u, postSyncFound);
}

HWTEST2_F(CommandListAppendSignalEvent,
          givenMultiTileCommandListWhenAppendingNonScopeEventSignalThenExpectPartitionedStoreDataImm, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto cmdStream = commandList->commandContainer.getCommandStream();

    size_t useSize = cmdStream->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    cmdStream->getSpace(useSize);

    constexpr uint32_t packets = 2u;

    event->setEventTimestampFlag(false);
    event->setUsingContextEndOffset(true);
    event->signalScope = 0;

    commandList->partitionCount = packets;
    ze_result_t returnValue = commandList->appendSignalEvent(event->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(packets, event->getPacketsInUse());

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    size_t expectedSize = NEO::EncodeStoreMemory<GfxFamily>::getStoreDataImmSize();
    size_t usedSize = cmdStream->getUsed();
    EXPECT_EQ(expectedSize, usedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedSize));

    auto storeDataImmList = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, storeDataImmList.size());
    uint32_t postSyncFound = 0;
    for (auto &it : storeDataImmList) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*it);
        EXPECT_EQ(gpuAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        EXPECT_EQ(0u, cmd->getDataDword1());
        EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, cmd->getDwordLength());
        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
        postSyncFound++;
        gpuAddress += event->getSinglePacketSize();
    }
    EXPECT_EQ(1u, postSyncFound);
}

HWTEST2_F(CommandListAppendSignalEvent,
          givenMultiTileCommandListWhenAppendingScopeEventSignalAfterWalkerThenExpectPartitionedPipeControl, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdStream = commandList->commandContainer.getCommandStream();

    size_t useSize = cmdStream->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    cmdStream->getSpace(useSize);

    constexpr uint32_t packets = 2u;

    event->setEventTimestampFlag(false);
    event->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    commandList->partitionCount = packets;
    commandList->appendSignalEventPostWalker(event.get(), false);
    EXPECT_EQ(packets, event->getPacketsInUse());

    auto gpuAddress = event->getGpuAddress(device);
    if (event->isUsingContextEndOffset()) {
        gpuAddress += event->getContextEndOffset();
    }
    auto &hwInfo = device->getNEODevice()->getHardwareInfo();

    size_t expectedSize = NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(hwInfo, false);
    size_t usedSize = cmdStream->getUsed();
    EXPECT_EQ(expectedSize, usedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedSize));

    auto pipeControlList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControlList.size());
    uint32_t postSyncFound = 0;
    for (auto &it : pipeControlList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
            postSyncFound++;
            gpuAddress += event->getSinglePacketSize();
        }
    }
    EXPECT_EQ(1u, postSyncFound);
}

HWTEST2_F(CommandListAppendSignalEvent,
          givenMultiTileCommandListWhenAppendWriteGlobalTimestampCalledWithSignalEventThenWorkPartitionedRegistersAreUsed, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->commandContainer;

    uint64_t timestampAddress = 0x12345678555500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);

    constexpr uint32_t packets = 2u;

    event->setEventTimestampFlag(true);
    commandList->partitionCount = packets;

    commandList->appendWriteGlobalTimestamp(dstptr, event->toHandle(), 0, nullptr);
    EXPECT_EQ(packets, event->getPacketsInUse());

    auto eventGpuAddress = event->getGpuAddress(device);
    uint64_t contextStartAddress = eventGpuAddress + event->getContextStartOffset();
    uint64_t globalStartAddress = eventGpuAddress + event->getGlobalStartOffset();
    uint64_t contextEndAddress = eventGpuAddress + event->getContextEndOffset();
    uint64_t globalEndAddress = eventGpuAddress + event->getGlobalEndOffset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    while (cmd->getPostSyncOperation() != POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP) {
        itorPC++;
        itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
        EXPECT_NE(cmdList.end(), itorPC);
        cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    }
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_FALSE(cmd->getDcFlushEnable());
    EXPECT_EQ(timestampAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));

    auto startCmdList = cmdList.begin();
    validateTimestampRegisters<FamilyType>(cmdList,
                                           startCmdList,
                                           REG_GLOBAL_TIMESTAMP_LDW, globalStartAddress,
                                           GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, contextStartAddress,
                                           true);

    if (UnitTestHelper<FamilyType>::timestampRegisterHighAddress()) {
        uint64_t globalStartAddressHigh = globalStartAddress + sizeof(uint32_t);
        uint64_t contextStartAddressHigh = contextStartAddress + sizeof(uint32_t);
        validateTimestampRegisters<FamilyType>(cmdList,
                                               startCmdList,
                                               REG_GLOBAL_TIMESTAMP_UN, globalStartAddressHigh,
                                               0x23AC, contextStartAddressHigh,
                                               true);
    }

    validateTimestampRegisters<FamilyType>(cmdList,
                                           startCmdList,
                                           REG_GLOBAL_TIMESTAMP_LDW, globalEndAddress,
                                           GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, contextEndAddress,
                                           true);

    if (UnitTestHelper<FamilyType>::timestampRegisterHighAddress()) {
        uint64_t globalEndAddressHigh = globalEndAddress + sizeof(uint32_t);
        uint64_t contextEndAddressHigh = contextEndAddress + sizeof(uint32_t);
        validateTimestampRegisters<FamilyType>(cmdList,
                                               startCmdList,
                                               REG_GLOBAL_TIMESTAMP_UN, globalEndAddressHigh,
                                               0x23AC, contextEndAddressHigh,
                                               true);
    }
}

} // namespace ult
} // namespace L0
