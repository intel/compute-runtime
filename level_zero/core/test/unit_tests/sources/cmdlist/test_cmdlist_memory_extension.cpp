/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/api/driver_experimental/public/zex_api.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

class CommandListWaitOnMemFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();
        ze_result_t returnValue;
        commandList.reset(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
        commandListBcs.reset(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue)));

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        eventPoolDesc.count = 2;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
        eventDesc.signal = 0;

        eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

        size_t size = sizeof(uint32_t);
        size_t alignment = 1u;
        ze_device_mem_alloc_desc_t deviceDesc = {};
        auto result = context->allocDeviceMem(device->toHandle(),
                                              &deviceDesc,
                                              size, alignment, &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);
    }

    void tearDown() {
        context->freeMem(ptr);
        event.reset(nullptr);
        eventPool.reset(nullptr);
        commandListBcs.reset(nullptr);
        commandList.reset(nullptr);
        DeviceFixture::tearDown();
    }

    std::unique_ptr<L0::ult::CommandList> commandList;
    std::unique_ptr<L0::ult::CommandList> commandListBcs;
    std::unique_ptr<EventPool> eventPool;
    std::unique_ptr<Event> event;
    uint32_t waitMemData = 1u;
    void *ptr = nullptr;
};

using CommandListAppendWaitOnMem = Test<CommandListWaitOnMemFixture>;

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataAndNotEqualOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_NOT_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataAndEqualOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataGreaterOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataGreaterThanEqualOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_GREATER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataLessThanOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_LESS_THAN_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndDataLessThanEqualOpThenSemaphoreWaitProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(cmd->getCompareOperation(),
              MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_LESS_THAN_OR_EQUAL_SDD);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());

    EXPECT_EQ(cmd->getWaitMode(),
              MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithValidAddressAndInvalidOpThenReturnsInvalid) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    ze_result_t result = ZE_RESULT_SUCCESS;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_BIT(6);
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithSignalEventAndHostScopeThenSemaphoreWaitAndPipeControlProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;
    std::unique_ptr<EventPool> signalEventPool;
    std::unique_ptr<Event> signalEvent;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    signalEventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    signalEvent = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, signalEvent->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    auto itorPC = findAll<PIPE_CONTROL *>(itor, cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            auto gpuAddress = event->getGpuAddress(this->device);
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithSignalEventAndNoScopeThenSemaphoreWaitAndPipeControlProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;
    std::unique_ptr<EventPool> signalEventPool;
    std::unique_ptr<Event> signalEvent;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    signalEventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    signalEvent = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, signalEvent->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    auto itorPC = findAll<PIPE_CONTROL *>(itor, cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            auto gpuAddress = event->getGpuAddress(this->device);
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_FALSE(cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemOnBcsWithSignalEventAndNoScopeThenSemaphoreWaitAndFlushDwProgrammedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandListBcs->commandContainer;
    std::unique_ptr<EventPool> signalEventPool;
    std::unique_ptr<Event> signalEvent;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    signalEventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    signalEvent = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandListBcs->appendWaitOnMemory(reinterpret_cast<void *>(&desc), ptr, waitMemData, signalEvent->toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    auto itorFDW = findAll<MI_FLUSH_DW *>(itor, cmdList.end());
    ASSERT_NE(0u, itorFDW.size());
    bool postSyncFound = false;
    for (auto it : itorFDW) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*it);
        if (cmd->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            auto gpuAddress = event->getGpuAddress(device);
            EXPECT_EQ(cmd->getDestinationAddress(), gpuAddress);
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendWaitOnMem, givenAppendWaitOnMemWithNoScopeAndSystemMemoryPtrThenAlignedPtrUsed, IsAtLeastSkl) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    size_t cmdListHostPtrSize = MemoryConstants::pageSize;
    void *cmdListHostBuffer = device->getNEODevice()->getMemoryManager()->allocateSystemMemory(cmdListHostPtrSize, cmdListHostPtrSize);
    void *startMemory = cmdListHostBuffer;
    void *baseAddress = alignDown(startMemory, MemoryConstants::pageSize);
    size_t expectedOffset = ptrDiff(startMemory, baseAddress);

    AlignedAllocationData outData = commandList->getAlignedAllocation(device, startMemory, cmdListHostPtrSize, false);
    ASSERT_NE(nullptr, outData.alloc);
    auto expectedGpuAddress = static_cast<uintptr_t>(alignDown(outData.alloc->getGpuAddress(), MemoryConstants::pageSize));
    EXPECT_EQ(startMemory, outData.alloc->getUnderlyingBuffer());
    EXPECT_EQ(expectedGpuAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(expectedOffset, outData.offset);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_wait_on_mem_desc_t desc;
    desc.actionFlag = ZEX_WAIT_ON_MEMORY_FLAG_LESSER_THAN_EQUAL;
    result = commandList->appendWaitOnMemory(reinterpret_cast<void *>(&desc), cmdListHostBuffer, waitMemData, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
    EXPECT_EQ(static_cast<uint32_t>(waitMemData), cmd->getSemaphoreDataDword());
    EXPECT_EQ(expectedGpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);

    commandList->removeHostPtrAllocations();
    device->getNEODevice()->getMemoryManager()->freeSystemMemory(cmdListHostBuffer);
}

using CommandListAppendWriteToMem = Test<CommandListWaitOnMemFixture>;

HWTEST_F(CommandListAppendWriteToMem, givenAppendWriteToMemWithNoScopeThenPipeControlEncodedCorrectly) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_write_to_mem_desc_t desc = {};
    uint64_t data = 0xabc;
    result = commandList->appendWriteToMemory(reinterpret_cast<void *>(&desc), ptr, data);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), data);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_FALSE(cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendWriteToMem, givenAppendWriteToMemOnBcsWithNoScopeThenFlushDwEncodedCorrectly) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandListBcs->commandContainer;

    zex_write_to_mem_desc_t desc = {};
    uint64_t data = 0xabc;
    result = commandListBcs->appendWriteToMemory(reinterpret_cast<void *>(&desc), ptr, data);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorFDW = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorFDW.size());
    bool postSyncFound = false;
    for (auto it : itorFDW) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*it);
        if (cmd->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD) {
            EXPECT_EQ(cmd->getImmediateData(), data);
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST_F(CommandListAppendWriteToMem, givenAppendWriteToMemWithScopeThenPipeControlEncodedCorrectly) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_write_to_mem_desc_t desc = {};
    desc.writeScope = ZEX_MEM_ACTION_SCOPE_FLAG_HOST;
    uint64_t data = 0xabc;
    result = commandList->appendWriteToMemory(reinterpret_cast<void *>(&desc), ptr, data);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), data);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendWriteToMem, givenAppendWriteToMemWithScopeThenPipeControlEncodedCorrectlyAlignedPtrUsed, IsAtLeastSkl) {
    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    size_t cmdListHostPtrSize = MemoryConstants::pageSize;
    void *cmdListHostBuffer = device->getNEODevice()->getMemoryManager()->allocateSystemMemory(cmdListHostPtrSize, cmdListHostPtrSize);
    void *startMemory = cmdListHostBuffer;
    void *baseAddress = alignDown(startMemory, MemoryConstants::pageSize);
    size_t expectedOffset = ptrDiff(startMemory, baseAddress);

    AlignedAllocationData outData = commandList->getAlignedAllocation(device, startMemory, cmdListHostPtrSize, false);
    ASSERT_NE(nullptr, outData.alloc);
    auto expectedGpuAddress = static_cast<uintptr_t>(alignDown(outData.alloc->getGpuAddress(), MemoryConstants::pageSize));
    EXPECT_EQ(startMemory, outData.alloc->getUnderlyingBuffer());
    EXPECT_EQ(expectedGpuAddress, outData.alignedAllocationPtr);
    EXPECT_EQ(expectedOffset, outData.offset);

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto &commandContainer = commandList->commandContainer;

    zex_write_to_mem_desc_t desc = {};
    desc.writeScope = ZEX_MEM_ACTION_SCOPE_FLAG_HOST;
    uint64_t data = 0xabc;
    result = commandList->appendWriteToMemory(reinterpret_cast<void *>(&desc), cmdListHostBuffer, data);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), data);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), cmd->getDcFlushEnable());
            uint64_t pcAddress = cmd->getAddress() | (static_cast<uint64_t>(cmd->getAddressHigh()) << 32);
            EXPECT_EQ(expectedGpuAddress, pcAddress);
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
    commandList->removeHostPtrAllocations();
    device->getNEODevice()->getMemoryManager()->freeSystemMemory(cmdListHostBuffer);
}

} // namespace ult
} // namespace L0
