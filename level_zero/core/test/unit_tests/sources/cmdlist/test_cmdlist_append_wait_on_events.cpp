/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using CommandListAppendWaitOnEvent = Test<CommandListFixture>;
using CommandListAppendWaitOnUsedPacketSignalEvent = Test<CommandListEventUsedPacketSignalFixture>;

HWTEST_F(CommandListAppendWaitOnEvent, WhenAppendingWaitOnEventThenSemaphoreWaitCmdIsGenerated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = commandList->appendWaitOnEvents(1, &hEventHandle, false);
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

        uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

        EXPECT_EQ(gpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);
        EXPECT_EQ(cmd->getWaitMode(),
                  MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
    }
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCmdListWithDirectSubmissionAndRelaxedOrderingWhenAppendingWaitOnEventsThenUseConditionalStartInsteadOfSemaphore, IsAtLeastXeHpcCore) {
    using MI_MATH_ALU_INST_INLINE = typename FamilyType::MI_MATH_ALU_INST_INLINE;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_SET_PREDICATE = typename FamilyType::MI_SET_PREDICATE;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH = typename FamilyType::MI_MATH;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(immCommandList->csr);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = immCommandList->appendWaitOnEvents(1, &hEventHandle, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      immCommandList->commandContainer.getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itor = find<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), CS_GPR_R4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), CS_GPR_R0);
    lrrCmd++;
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), CS_GPR_R4 + 4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), CS_GPR_R0 + 4);

    auto eventGpuAddr = event->getCompletionFieldGpuAddress(this->device);

    // conditional bb_start
    auto lrmCmd = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(++lrrCmd);
    EXPECT_EQ(lrmCmd->getRegisterAddress(), CS_GPR_R7);
    EXPECT_EQ(lrmCmd->getMemoryAddress(), eventGpuAddr);

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++lrmCmd);
    EXPECT_EQ(lriCmd->getRegisterOffset(), CS_GPR_R7 + 4);
    EXPECT_EQ(lriCmd->getDataDword(), 0u);

    lriCmd++;
    EXPECT_EQ(lriCmd->getRegisterOffset(), CS_GPR_R8);
    EXPECT_EQ(lriCmd->getDataDword(), static_cast<uint32_t>(Event::State::STATE_CLEARED));

    lriCmd++;
    EXPECT_EQ(lriCmd->getRegisterOffset(), CS_GPR_R8 + 4);
    EXPECT_EQ(lriCmd->getDataDword(), 0u);

    auto miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    EXPECT_EQ(miMathCmd->DW0.BitField.DwordLength, 3u);

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_LOAD), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_SRCA), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_7), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_LOAD), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_SRCB), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_8), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_SUB), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_NONE), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_NONE), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::OPCODE_STORE), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_7), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::R_ZF), miAluCmd->DW0.BitField.Operand2);

    lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(++miAluCmd);
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), CS_GPR_R7);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), CS_PREDICATE_RESULT_2);

    auto predicateCmd = reinterpret_cast<MI_SET_PREDICATE *>(++lrrCmd);
    EXPECT_EQ(static_cast<typename MI_SET_PREDICATE::PREDICATE_ENABLE>(MiPredicateType::NoopOnResult2Clear), predicateCmd->getPredicateEnable());

    auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(++predicateCmd);
    EXPECT_EQ(1u, bbStartCmd->getPredicationEnable());
    EXPECT_EQ(1u, bbStartCmd->getIndirectAddressEnable());

    predicateCmd = reinterpret_cast<MI_SET_PREDICATE *>(++bbStartCmd);
    EXPECT_EQ(static_cast<typename MI_SET_PREDICATE::PREDICATE_ENABLE>(MiPredicateType::Disable), predicateCmd->getPredicateEnable());
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCmdListWithDirectSubmissionAndRelaxedOrderingWhenAppendingApiWaitOnEventsThenUseSemaphore, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(immCommandList->csr);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = zeCommandListAppendWaitOnEvents(immCommandList->toHandle(), 1, &hEventHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      immCommandList->commandContainer.getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCmdListWithDirectSubmissionAndRelaxedOrderingWhenAppendingBarrierThenUseSemaphore, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(immCommandList->csr);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = zeCommandListAppendBarrier(immCommandList->toHandle(), nullptr, 1, &hEventHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      immCommandList->commandContainer.getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCmdListWithDirectSubmissionAndRelaxedOrderingWhenAppendingMemoryBarriersThenUseSemaphore, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    const size_t rangeSizes = 1;
    const void *ranges = nullptr;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(immCommandList->csr);

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = zeCommandListAppendMemoryRangesBarrier(immCommandList->toHandle(), 1, &rangeSizes, &ranges, nullptr, 1, &hEventHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->commandContainer.getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      immCommandList->commandContainer.getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListAppendWaitOnEvent, givenTwoEventsWhenWaitOnEventsAppendedThenTwoSemaphoreWaitCmdsAreGenerated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    ze_event_handle_t handles[2] = {event->toHandle(), event->toHandle()};

    auto result = commandList->appendWaitOnEvents(2, handles, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(2u, itor.size());

    auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

    uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

    for (int i = 0; i < 2; i++) {
        auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor[i]);
        EXPECT_EQ(cmd->getCompareOperation(),
                  MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(static_cast<uint32_t>(-1), cmd->getSemaphoreDataDword());
        EXPECT_EQ(gpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);
        EXPECT_EQ(cmd->getWaitMode(),
                  MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);
    }
}

HWTEST_F(CommandListAppendWaitOnEvent, WhenAppendingWaitOnEventsThenEventGraphicsAllocationIsAddedToResidencyContainer) {
    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = commandList->appendWaitOnEvents(1, &hEventHandle, false);
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
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    ze_event_handle_t hEventHandle = event->toHandle();

    auto result = commandList->appendWaitOnEvents(1, &hEventHandle, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getHwInfo())) {
        itor--;
        auto cmd = genCmdCast<PIPE_CONTROL *>(*itor);

        ASSERT_NE(nullptr, cmd);
        EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
        EXPECT_TRUE(cmd->getDcFlushEnable());
    } else {
        if (cmdList.begin() != itor) {
            itor--;
            EXPECT_EQ(nullptr, genCmdCast<PIPE_CONTROL *>(*itor));
        }
    }
}

HWTEST_F(CommandListAppendWaitOnUsedPacketSignalEvent, WhenAppendingWaitOnTimestampEventWithThreePacketsThenSemaphoreWaitCmdIsGeneratedThreeTimes) {
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
    result = commandList->appendWaitOnEvents(1, &hEventHandle, false);
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

HWTEST_F(CommandListAppendWaitOnUsedPacketSignalEvent, WhenAppendingWaitOnTimestampEventWithThreeKernelsThenSemaphoreWaitCmdIsGeneratedCorrectly) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.UseDynamicEventPacketsCount.set(0);

    ze_result_t result = ZE_RESULT_SUCCESS;
    commandList.reset(whiteboxCast(CommandList::create(device->getHwInfo().platform.eProductFamily, device, NEO::EngineGroupType::RenderCompute, 0u, result)));

    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    event->setMaxKernelCount(3u);

    event->setPacketsInUse(3u);
    event->increaseKernelCount();
    event->setPacketsInUse(3u);
    event->increaseKernelCount();
    event->setPacketsInUse(3u);
    ASSERT_EQ(9u, event->getPacketsInUse());

    ze_event_handle_t hEventHandle = event->toHandle();
    result = commandList->appendWaitOnEvents(1, &hEventHandle, false);
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

    uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

    EXPECT_EQ(gpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);
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
    auto result = commandList->appendWaitOnEvents(1, &hEventHandle, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    auto newCommandBuffer = commandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    EXPECT_EQ(expectedConsumedSpace, usedSpaceAfter);
    EXPECT_NE(oldCommandBuffer, newCommandBuffer);

    auto gpuAddress = event->getCompletionFieldGpuAddress(device);

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

using MultTileCommandListAppendWaitOnEvent = Test<MultiTileCommandListFixture<false, false, false>>;
HWTEST2_F(MultTileCommandListAppendWaitOnEvent,
          GivenMultiTileCmdListWhenPartitionedEventUsedToWaitThenExpectProperGpuAddressAndSemaphoreCount, IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    size_t expectedSize = commandList->partitionCount * sizeof(MI_SEMAPHORE_WAIT);

    event->setPacketsInUse(commandList->partitionCount);
    event->setUsingContextEndOffset(true);

    ze_event_handle_t eventHandle = event->toHandle();

    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    auto result = commandList->appendWaitOnEvents(1, &eventHandle, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(expectedSize, (usedSpaceAfter - usedSpaceBefore));

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), usedSpaceBefore),
                                                      expectedSize));

    auto itorSW = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorSW.size());
    uint32_t semaphoreWaitsFound = 0;
    for (auto it : itorSW) {
        auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);

        EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                  cmd->getCompareOperation());
        EXPECT_EQ(cmd->getSemaphoreDataDword(), std::numeric_limits<uint32_t>::max());
        EXPECT_EQ(gpuAddress, cmd->getSemaphoreGraphicsAddress());
        EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, cmd->getWaitMode());

        semaphoreWaitsFound++;
        gpuAddress += event->getSinglePacketSize();
    }
    EXPECT_EQ(2u, semaphoreWaitsFound);
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCommandListWhenAppendWaitOnNotSignaledEventThenWait, IsAtLeastSkl) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    ze_event_handle_t eventHandle = event->toHandle();

    EXPECT_FALSE(cmdList.dependenciesPresent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList.appendWaitOnEvents(1, &eventHandle, false));
    EXPECT_TRUE(cmdList.dependenciesPresent);
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCommandListWhenAppendWaitOnAlreadySignaledEventThenDontWait, IsAtLeastSkl) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;
    cmdList.dcFlushSupport = false;
    event->hostSignal();
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus());
    ze_event_handle_t eventHandle = event->toHandle();

    EXPECT_FALSE(cmdList.dependenciesPresent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList.appendWaitOnEvents(1, &eventHandle, false));
    EXPECT_FALSE(cmdList.dependenciesPresent);
}

using TbxImmediateCommandListTest = Test<TbxImmediateCommandListFixture>;

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendLaunchKernelThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    ze_group_count_t group = {1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandListImmediate->appendLaunchKernel(kernel->toHandle(), &group, nullptr, 1, &eventHandle, launchParams);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendLaunchKernelIndirectThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    ze_group_count_t group = {1, 1, 1};
    commandListImmediate->appendLaunchKernelIndirect(kernel->toHandle(), &group, nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendBarrierThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    commandListImmediate->appendBarrier(nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendMemoryCopyThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    commandListImmediate->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendMemoryCopyRegionThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    commandListImmediate->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendMemoryFillThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 4096, 4096u, &dstBuffer);

    int one = 1;
    commandListImmediate->appendMemoryFill(dstBuffer, reinterpret_cast<void *>(&one), sizeof(one), 4096,
                                           nullptr, 1u, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);

    context->freeMem(dstBuffer);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendWaitOnEventsThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    commandListImmediate->appendWaitOnEvents(1u, &eventHandle, false);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendWriteGlobalTimestampThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(0x12345678555500);
    commandListImmediate->appendWriteGlobalTimestamp(dstptr, nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendImageCopyRegionThenExpectDownloadAllocations) {
    if (!neoDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::Kernel> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    ze_image_desc_t desc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    L0::Image *imagePtr;

    auto result = Image::create(neoDevice->getHardwareInfo().platform.eProductFamily, device, &desc, &imagePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Image> imageDst(imagePtr);

    result = Image::create(neoDevice->getHardwareInfo().platform.eProductFamily, device, &desc, &imagePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Image> imageSrc(imagePtr);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};

    auto eventHandle = event->toHandle();
    commandListImmediate->appendImageCopyRegion(imageDst->toHandle(), imageSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendImageCopyFromMemoryThenExpectDownloadAllocations) {
    if (!neoDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyBufferToImage3dBytes);
    auto mockBuiltinKernel = static_cast<Mock<::L0::Kernel> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t desc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    L0::Image *imagePtr;
    auto result = Image::create(neoDevice->getHardwareInfo().platform.eProductFamily, device, &desc, &imagePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Image> image(imagePtr);

    auto eventHandle = event->toHandle();
    commandListImmediate->appendImageCopyFromMemory(imagePtr->toHandle(), ptr, nullptr, nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendImageCopyToMemoryThenExpectDownloadAllocations) {
    if (!neoDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImage3dToBufferBytes);
    auto mockBuiltinKernel = static_cast<Mock<::L0::Kernel> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t desc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    L0::Image *imagePtr;
    auto result = Image::create(neoDevice->getHardwareInfo().platform.eProductFamily, device, &desc, &imagePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Image> image(imagePtr);

    auto eventHandle = event->toHandle();
    commandListImmediate->appendImageCopyToMemory(ptr, imagePtr->toHandle(), nullptr, nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendMemoryRangesBarrierThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *ranges[rangeSizes];
    const void **rangesMemory = reinterpret_cast<const void **>(&ranges[0]);

    auto eventHandle = event->toHandle();
    commandListImmediate->appendMemoryRangesBarrier(numRanges, &rangeSizes, rangesMemory, nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendLaunchCooperativeKernelThenExpectDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    ze_group_count_t groupCount{1, 1, 1};
    auto eventHandle = event->toHandle();
    commandListImmediate->appendLaunchCooperativeKernel(kernel->toHandle(), &groupCount, nullptr, 1, &eventHandle);

    EXPECT_TRUE(ultCsr.downloadAllocationsCalled);
}

} // namespace ult
} // namespace L0
