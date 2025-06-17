/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
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
using CommandListAppendWaitOnSecondaryBatchBufferEvent = Test<CommandListSecondaryBatchBufferFixture>;

HWTEST_F(CommandListAppendWaitOnEvent, WhenAppendingWaitOnEventThenSemaphoreWaitCmdIsGenerated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = commandList->appendWaitOnEvents(1, &hEventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
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
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(immCommandList.get());

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = static_cast<CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily> *>(immCommandList.get())->addEventsToCmdList(1, &hEventHandle, nullptr, true, true, true, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      immCommandList->getCmdContainer().getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itor = find<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), RegisterOffsets::csGprR4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), RegisterOffsets::csGprR0);
    lrrCmd++;
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), RegisterOffsets::csGprR4 + 4);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), RegisterOffsets::csGprR0 + 4);

    auto eventGpuAddr = event->getCompletionFieldGpuAddress(this->device);

    // conditional bb_start
    auto lrmCmd = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(++lrrCmd);
    EXPECT_EQ(lrmCmd->getRegisterAddress(), RegisterOffsets::csGprR7);
    EXPECT_EQ(lrmCmd->getMemoryAddress(), eventGpuAddr);

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(++lrmCmd);
    EXPECT_EQ(lriCmd->getRegisterOffset(), RegisterOffsets::csGprR7 + 4);
    EXPECT_EQ(lriCmd->getDataDword(), 0u);

    lriCmd++;
    EXPECT_EQ(lriCmd->getRegisterOffset(), RegisterOffsets::csGprR8);
    EXPECT_EQ(lriCmd->getDataDword(), static_cast<uint32_t>(Event::State::STATE_CLEARED));

    lriCmd++;
    EXPECT_EQ(lriCmd->getRegisterOffset(), RegisterOffsets::csGprR8 + 4);
    EXPECT_EQ(lriCmd->getDataDword(), 0u);

    auto miMathCmd = reinterpret_cast<MI_MATH *>(++lriCmd);
    EXPECT_EQ(miMathCmd->DW0.BitField.DwordLength, 3u);

    auto miAluCmd = reinterpret_cast<MI_MATH_ALU_INST_INLINE *>(++miMathCmd);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeLoad), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::srca), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::gpr7), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeLoad), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::srcb), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::gpr8), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeSub), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeNone), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeNone), miAluCmd->DW0.BitField.Operand2);

    miAluCmd++;
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::opcodeStore), miAluCmd->DW0.BitField.ALUOpcode);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::gpr7), miAluCmd->DW0.BitField.Operand1);
    EXPECT_EQ(static_cast<uint32_t>(AluRegisters::zf), miAluCmd->DW0.BitField.Operand2);

    lrrCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(++miAluCmd);
    EXPECT_EQ(lrrCmd->getSourceRegisterAddress(), RegisterOffsets::csGprR7);
    EXPECT_EQ(lrrCmd->getDestinationRegisterAddress(), RegisterOffsets::csPredicateResult2);

    auto predicateCmd = reinterpret_cast<MI_SET_PREDICATE *>(++lrrCmd);
    EXPECT_EQ(static_cast<typename MI_SET_PREDICATE::PREDICATE_ENABLE>(MiPredicateType::noopOnResult2Clear), predicateCmd->getPredicateEnable());

    auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(++predicateCmd);
    EXPECT_EQ(1u, bbStartCmd->getPredicationEnable());
    EXPECT_EQ(1u, bbStartCmd->getIndirectAddressEnable());

    predicateCmd = reinterpret_cast<MI_SET_PREDICATE *>(++bbStartCmd);
    EXPECT_EQ(static_cast<typename MI_SET_PREDICATE::PREDICATE_ENABLE>(MiPredicateType::disable), predicateCmd->getPredicateEnable());
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCmdListWithDirectSubmissionAndRelaxedOrderingWhenAppendingApiWaitOnEventsThenUseSemaphore, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(immCommandList.get());

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = zeCommandListAppendWaitOnEvents(immCommandList->toHandle(), 1, &hEventHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      immCommandList->getCmdContainer().getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCmdListAndAppendingRegularCommandlistWithWaitOnEventsThenUseSemaphore, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);

    ze_event_handle_t hEventHandle = event->toHandle();
    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandListRegular->close();
    auto commandListHandle = commandListRegular->toHandle();

    // 1st append can carry preamble
    auto result = immCommandList->appendCommandLists(1u, &commandListHandle, nullptr, 1u, &hEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // 2nd append should carry only wait events and bb_start to regular command list
    auto usedSpaceBefore = immCommandList->getCmdContainer().getCommandStream()->getUsed();

    result = immCommandList->appendCommandLists(1u, &commandListHandle, nullptr, 1u, &hEventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(immCommandList->getCmdContainer().getCommandStream()->getCpuBase(), usedSpaceBefore),
                                                      usedSpaceAfter - usedSpaceBefore));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto itorBBStart = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itorBBStart);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListImmediateHwWithWaitEventFail : public WhiteBox<::L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> {
  public:
    using BaseClass = WhiteBox<::L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>;
    MockCommandListImmediateHwWithWaitEventFail() : BaseClass() {
        this->cmdListType = CommandList::CommandListType::typeImmediate;
    }
    using BaseClass::appendSignalEventPostWalker;
    using BaseClass::applyMemoryRangesBarrier;
    using BaseClass::cmdListType;
    using BaseClass::cmdQImmediate;
    using BaseClass::copyThroughLockedPtrEnabled;
    using BaseClass::dcFlushSupport;
    using BaseClass::dependenciesPresent;
    using BaseClass::dummyBlitWa;
    using BaseClass::isSyncModeQueue;
    using BaseClass::isTbxMode;
    using BaseClass::setupFillKernelArguments;

    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CommandToPatchContainer *outWaitCmds,
                                   bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation) override {
        if (forceWaitEventError) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        appendWaitEventCalled++;
        return BaseClass::appendWaitOnEvents(numEvents, phEvent, outWaitCmds, relaxedOrderingAllowed, trackDependencies, apiRequest, skipAddingWaitEventsToResidency, skipFlush, copyOffloadOperation);
    };

    ze_result_t appendSignalEvent(ze_event_handle_t hEvent, bool relaxedOrderingDispatch) override {
        if (forceSignalEventError) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        appendSignalEventCalled++;
        return BaseClass::appendSignalEvent(hEvent, relaxedOrderingDispatch);
    }

    ze_result_t executeCommandListImmediateWithFlushTask(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation,
                                                         bool copyOffloadSubmission, bool requireTaskCountUpdate,
                                                         MutexLock *outerLock,
                                                         std::unique_lock<std::mutex> *outerLockForIndirect) override {
        ++executeCommandListImmediateWithFlushTaskCalledCount;
        if (callBaseExecute) {
            return BaseClass::executeCommandListImmediateWithFlushTask(performMigration, hasStallingCmds, hasRelaxedOrderingDependencies, appendOperation, copyOffloadSubmission, requireTaskCountUpdate,
                                                                       outerLock, outerLockForIndirect);
        }
        return executeCommandListImmediateWithFlushTaskReturnValue;
    }

    bool callBaseExecute = false;
    bool forceSignalEventError = false;
    bool forceWaitEventError = false;

    ze_result_t executeCommandListImmediateWithFlushTaskReturnValue = ZE_RESULT_SUCCESS;
    uint32_t executeCommandListImmediateWithFlushTaskCalledCount = 0;

    uint32_t appendSignalEventCalled = 0u;
    uint32_t appendWaitEventCalled = 0u;
};

class MockCommandQueueExecute : public Mock<CommandQueue> {
  public:
    MockCommandQueueExecute(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : Mock(device, csr, desc) {}
    ze_result_t executeCommandLists(
        uint32_t numCommandLists,
        ze_command_list_handle_t *phCommandLists,
        ze_fence_handle_t hFence,
        bool performMigration,
        NEO::LinearStream *parentImmediateCommandlistLinearStream,
        std::unique_lock<std::mutex> *outerLockForIndirect) override {
        if (forceQueueExecuteError) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
        executeCalledCount++;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t destroy() override {
        return ZE_RESULT_SUCCESS;
    }

    uint32_t executeCalledCount = 0;
    bool forceQueueExecuteError = false;
};

using CommandListImmediateAppendRegularTest = Test<CommandListFixture>;
HWTEST2_F(CommandListImmediateAppendRegularTest, givenImmediateCommandListAndAppendRegularCommandlistWhenWaitFailsThenExecuteNotCalled, IsAtLeastXeHpcCore) {
    ze_command_queue_desc_t queueDesc = {};
    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    MockCommandQueueExecute queue(device, &mockCommandStreamReceiver, &queueDesc);

    auto cmdList = new MockCommandListImmediateHwWithWaitEventFail<FamilyType::gfxCoreFamily>;
    cmdList->cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList->forceWaitEventError = true;
    cmdList->cmdQImmediate = &queue;
    cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    auto result = cmdList->appendCommandLists(0u, nullptr, nullptr, 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(queue.executeCalledCount, 0u);

    cmdList->destroy();
}

HWTEST2_F(CommandListImmediateAppendRegularTest, givenImmediateCommandListAndAppendRegularCommandlistWhenWaitSucceedsAndQueueExecFailsThenSignalEventNotCalled, IsAtLeastXeHpcCore) {
    ze_command_queue_desc_t queueDesc = {};
    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    MockCommandQueueExecute queue(device, &mockCommandStreamReceiver, &queueDesc);
    queue.forceQueueExecuteError = true;

    ze_event_handle_t hEventHandle = event->toHandle();
    auto cmdList = new MockCommandListImmediateHwWithWaitEventFail<FamilyType::gfxCoreFamily>;
    cmdList->cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList->cmdQImmediate = &queue;
    cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    auto result = cmdList->appendCommandLists(0u, nullptr, nullptr, 1u, &hEventHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
    EXPECT_EQ(queue.executeCalledCount, 0u);

    cmdList->destroy();
}

HWTEST2_F(CommandListImmediateAppendRegularTest, givenImmediateCommandListAndAppendRegularCommandlistWhenSubOperationsSucceedThenFinalCallSucceeds, IsAtLeastXeHpcCore) {
    ze_command_queue_desc_t queueDesc = {};
    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    MockCommandQueueExecute queue(device, &mockCommandStreamReceiver, &queueDesc);

    ze_event_handle_t hEventHandle = event->toHandle();
    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    auto signalEvent = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    ze_event_handle_t hSignalEventHandle = signalEvent->toHandle();

    auto cmdList = new MockCommandListImmediateHwWithWaitEventFail<FamilyType::gfxCoreFamily>;
    cmdList->cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList->cmdQImmediate = &queue;
    cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    auto result = cmdList->appendCommandLists(0u, nullptr, hSignalEventHandle, 1u, &hEventHandle);
    EXPECT_EQ(queue.executeCalledCount, 1u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList->destroy();
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockAppendRegularCommandlistWithWaitOnEvents : public MockCommandListForAppendLaunchKernel<gfxCoreFamily> {
  public:
    MockAppendRegularCommandlistWithWaitOnEvents() : MockCommandListForAppendLaunchKernel<gfxCoreFamily>() {}
    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CommandToPatchContainer *outWaitCmds,
                                   bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation) override {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    };
};

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCmdListAndAppendingRegularCommandlistWithWaitOnEventsAndForceInvalidReturnThenCheckReturnStatus, IsAtLeastXeHpcCore) {
    MockAppendRegularCommandlistWithWaitOnEvents<FamilyType::gfxCoreFamily> cmdList;

    cmdList.initialize(device, NEO::EngineGroupType::compute, 0u);
    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = cmdList.appendCommandLists(0u, nullptr, nullptr, 1u, &hEventHandle);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCmdListWithDirectSubmissionAndRelaxedOrderingWhenAppendingBarrierThenUseSemaphore, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(immCommandList.get());

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = zeCommandListAppendBarrier(immCommandList->toHandle(), nullptr, 1, &hEventHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      immCommandList->getCmdContainer().getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListAppendWaitOnEvent, givenImmediateCmdListWithDirectSubmissionAndRelaxedOrderingWhenAppendingMemoryBarriersThenUseSemaphore, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    const size_t rangeSizes = 1;
    const void *ranges = nullptr;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(immCommandList.get());

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->getCsr(false));

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);

    ze_event_handle_t hEventHandle = event->toHandle();
    auto result = zeCommandListAppendMemoryRangesBarrier(immCommandList->toHandle(), 1, &rangeSizes, &ranges, nullptr, 1, &hEventHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      immCommandList->getCmdContainer().getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListAppendWaitOnEvent, givenTwoEventsWhenWaitOnEventsAppendedThenTwoSemaphoreWaitCmdsAreGenerated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    ze_event_handle_t handles[2] = {event->toHandle(), event->toHandle()};

    auto result = commandList->appendWaitOnEvents(2, handles, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
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
    auto result = commandList->appendWaitOnEvents(1, &hEventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &residencyContainer = commandList->getCmdContainer().getResidencyContainer();
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
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    ze_event_handle_t hEventHandle = event->toHandle();

    auto result = commandList->appendWaitOnEvents(1, &hEventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    if (NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()) && !whiteBoxCmdList->l3FlushAfterPostSyncRequired) {
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
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    event->setPacketsInUse(3u);
    ze_event_handle_t hEventHandle = event->toHandle();
    result = commandList->appendWaitOnEvents(1, &hEventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
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
    NEO::debugManager.flags.UseDynamicEventPacketsCount.set(0);

    ze_result_t result = ZE_RESULT_SUCCESS;
    commandList.reset(CommandList::whiteboxCast(CommandList::create(device->getHwInfo().platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false)));

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    event->setMaxKernelCount(3u);

    event->setPacketsInUse(3u);
    event->increaseKernelCount();
    event->setPacketsInUse(3u);
    event->increaseKernelCount();
    event->setPacketsInUse(3u);
    ASSERT_EQ(9u, event->getPacketsInUse());

    ze_event_handle_t hEventHandle = event->toHandle();
    result = commandList->appendWaitOnEvents(1, &hEventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
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

HWTEST_F(CommandListAppendWaitOnEvent, givenCommandListWhenAppendWriteGlobalTimestampCalledWithWaitOnEventsThenSemaphoreWaitAndPipeControlForTimestampEncoded) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;

    uint64_t dstAddress = 0x123456785500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(dstAddress);
    auto &commandContainer = commandList->getCmdContainer();
    commandContainer.getResidencyContainer().clear();

    ze_event_handle_t hEventHandle = event->toHandle();

    commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 1, &hEventHandle);

    auto residencyContainer = commandContainer.getResidencyContainer();
    auto timestampAlloc = residencyContainer[1];
    EXPECT_EQ(dstAddress, reinterpret_cast<uint64_t>(timestampAlloc->getUnderlyingBuffer()));
    auto timestampAddress = timestampAlloc->getGpuAddress();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0),
        commandContainer.getCommandStream()->getUsed()));

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

HWTEST_F(CommandListAppendWaitOnSecondaryBatchBufferEvent, givenCommandBufferIsEmptyWhenAppendingWaitOnEventThenAllocateNewCommandBuffer) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto consumeSpace = commandList->getCmdContainer().getCommandStream()->getAvailableSpace();
    consumeSpace -= sizeof(MI_BATCH_BUFFER_END);
    commandList->getCmdContainer().getCommandStream()->getSpace(consumeSpace);

    size_t expectedConsumedSpace = NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();
    if (MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()) && !commandList->l3FlushAfterPostSyncRequired) {
        expectedConsumedSpace += sizeof(PIPE_CONTROL);
    }

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        0,
        ZE_EVENT_SCOPE_FLAG_DEVICE};

    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    ze_event_handle_t hEventHandle = event->toHandle();

    auto oldCommandBuffer = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();
    auto result = commandList->appendWaitOnEvents(1, &hEventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    auto newCommandBuffer = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();

    EXPECT_EQ(expectedConsumedSpace, usedSpaceAfter);
    EXPECT_NE(oldCommandBuffer, newCommandBuffer);

    auto gpuAddress = event->getCompletionFieldGpuAddress(device);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      commandList->getCmdContainer().getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    if (MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()) && !commandList->l3FlushAfterPostSyncRequired) {
        ASSERT_NE(cmdList.end(), itorPC);
        {
            auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
            ASSERT_NE(cmd, nullptr);

            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()), cmd->getDcFlushEnable());
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

using MultTileCommandListAppendWaitOnEvent = Test<MultiTileCommandListFixture<false, false, false, -1>>;
HWTEST2_F(MultTileCommandListAppendWaitOnEvent,
          GivenMultiTileCmdListWhenPartitionedEventUsedToWaitThenExpectProperGpuAddressAndSemaphoreCount, IsAtLeastXeCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    size_t expectedSize = commandList->partitionCount * NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();

    event->setPacketsInUse(commandList->partitionCount);
    event->setUsingContextEndOffset(true);

    ze_event_handle_t eventHandle = event->toHandle();

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    auto result = commandList->appendWaitOnEvents(1, &eventHandle, nullptr, false, true, false, false, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_EQ(expectedSize, (usedSpaceAfter - usedSpaceBefore));

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), usedSpaceBefore),
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

HWTEST_F(CommandListAppendWaitOnEvent, givenImmediateCommandListWhenAppendWaitOnNotSignaledEventThenWait) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_event_handle_t eventHandle = event->toHandle();

    EXPECT_FALSE(cmdList.dependenciesPresent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList.appendWaitOnEvents(1, &eventHandle, nullptr, false, true, false, false, false, false));
    EXPECT_TRUE(cmdList.dependenciesPresent);
}

HWTEST_F(CommandListAppendWaitOnEvent, givenImmediateCommandListWhenAppendWaitOnAlreadySignaledEventThenDontWait) {
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);
    MockCommandListImmediateHw<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.dcFlushSupport = false;
    event->hostSignal(false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus());
    ze_event_handle_t eventHandle = event->toHandle();

    EXPECT_FALSE(cmdList.dependenciesPresent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList.appendWaitOnEvents(1, &eventHandle, nullptr, false, true, false, false, false, false));
    EXPECT_FALSE(cmdList.dependenciesPresent);
}

using TbxImmediateCommandListTest = Test<TbxImmediateCommandListFixture>;

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendLaunchKernelThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    ze_group_count_t group = {1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandListImmediate->appendLaunchKernel(kernel->toHandle(), group, nullptr, 1, &eventHandle, launchParams);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendLaunchKernelIndirectThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    ze_group_count_t group = {1, 1, 1};
    commandListImmediate->appendLaunchKernelIndirect(kernel->toHandle(), group, nullptr, 1, &eventHandle, false);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendBarrierThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    commandListImmediate->appendBarrier(nullptr, 1, &eventHandle, false);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendMemoryCopyThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    CmdListMemoryCopyParams copyParams = {};
    commandListImmediate->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 1, &eventHandle, copyParams);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
    ultCsr.getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(neoDevice);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendMemoryCopyRegionThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};
    CmdListMemoryCopyParams copyParams = {};
    commandListImmediate->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 1, &eventHandle, copyParams);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
    ultCsr.getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(neoDevice);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendMemoryFillThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 4096, 4096u, &dstBuffer);

    CmdListMemoryCopyParams copyParams = {};
    int one = 1;
    commandListImmediate->appendMemoryFill(dstBuffer, reinterpret_cast<void *>(&one), sizeof(one), 4096,
                                           nullptr, 1, &eventHandle, copyParams);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);

    context->freeMem(dstBuffer);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendWaitOnEventsThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    commandListImmediate->appendWaitOnEvents(1, &eventHandle, nullptr, false, true, false, false, false, false);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendWriteGlobalTimestampThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    auto eventHandle = event->toHandle();
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(0x12345678555500);
    commandListImmediate->appendWriteGlobalTimestamp(dstptr, nullptr, 1, &eventHandle);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendImageCopyRegionThenDoNotDownloadAllocations) {
    if (!neoDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
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
    CmdListMemoryCopyParams copyParams = {};
    commandListImmediate->appendImageCopyRegion(imageDst->toHandle(), imageSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 1, &eventHandle, copyParams);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendImageCopyFromMemoryThenDoNotDownloadAllocations) {
    if (!neoDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyBufferToImage3dBytes);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t desc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    L0::Image *imagePtr;
    auto result = Image::create(neoDevice->getHardwareInfo().platform.eProductFamily, device, &desc, &imagePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Image> image(imagePtr);
    CmdListMemoryCopyParams copyParams = {};
    auto eventHandle = event->toHandle();
    commandListImmediate->appendImageCopyFromMemory(imagePtr->toHandle(), ptr, nullptr, nullptr, 1, &eventHandle, copyParams);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
    ultCsr.getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(neoDevice);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendImageCopyToMemoryThenDoNotDownloadAllocations) {
    if (!neoDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }

    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImage3dToBufferBytes);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    void *ptr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t desc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    L0::Image *imagePtr;
    auto result = Image::create(neoDevice->getHardwareInfo().platform.eProductFamily, device, &desc, &imagePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    std::unique_ptr<L0::Image> image(imagePtr);
    CmdListMemoryCopyParams copyParams = {};

    auto eventHandle = event->toHandle();
    commandListImmediate->appendImageCopyToMemory(ptr, imagePtr->toHandle(), nullptr, nullptr, 1, &eventHandle, copyParams);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
    ultCsr.getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(neoDevice);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendMemoryRangesBarrierThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *ranges[rangeSizes];
    const void **rangesMemory = reinterpret_cast<const void **>(&ranges[0]);

    auto eventHandle = event->toHandle();
    commandListImmediate->appendMemoryRangesBarrier(numRanges, &rangeSizes, rangesMemory, nullptr, 1, &eventHandle);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
}

HWTEST_TEMPLATED_F(TbxImmediateCommandListTest, givenTbxModeOnFlushTaskImmediateAsyncCommandListWhenAppendLaunchCooperativeKernelThenDoNotDownloadAllocations) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    ze_group_count_t groupCount{1, 1, 1};
    auto eventHandle = event->toHandle();

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, &eventHandle, cooperativeParams);

    EXPECT_EQ(0u, ultCsr.downloadAllocationsCalledCount);
}

HWTEST_F(CommandListAppendWaitOnEvent, GivenOutCmdListProvidedAndSkipResidencyFlagTrueWhenAppendingEventToCmdListThenSemaphoreWaitStoredAndEventAllocationNotAdded) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    uint32_t eventCount = 4;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = eventCount;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto eventPoolAllocation = eventPool->getAllocation().getDefaultGraphicsAllocation();

    std::vector<std::unique_ptr<L0::Event>> events;
    std::vector<ze_event_handle_t> eventHandles;
    events.reserve(eventCount);
    eventHandles.reserve(eventCount);
    ze_event_desc_t eventDesc = {};
    for (uint32_t i = 0; i < eventCount; i++) {
        eventDesc.index = i;
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
        eventHandles.emplace_back(event->toHandle());
        events.emplace_back(std::move(event));
    }

    CommandToPatchContainer outSemaphoreWaitCmds;

    auto &cmdListResidency = commandList->getCmdContainer().getResidencyContainer();
    auto cmdListStream = commandList->getCmdContainer().getCommandStream();
    auto usedSpaceBefore = cmdListStream->getUsed();

    bool skipResidency = true;
    result = commandList->appendWaitOnEvents(eventCount, eventHandles.data(), &outSemaphoreWaitCmds, false, false, false, skipResidency, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = cmdListStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdListStream->getCpuBase(), usedSpaceBefore),
                                                      usedSpaceAfter - usedSpaceBefore));

    auto semWaitList = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

    auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

    ASSERT_EQ(eventCount, semWaitList.size());
    ASSERT_EQ(eventCount, outSemaphoreWaitCmds.size());

    for (uint32_t i = 0; i < eventCount; i++) {
        EXPECT_EQ(CommandToPatch::WaitEventSemaphoreWait, outSemaphoreWaitCmds[i].type);
        auto parsedCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semWaitList[i]);
        auto outCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(outSemaphoreWaitCmds[i].pDestination);

        ASSERT_EQ(parsedCmd, outCmd);

        auto eventGpuAddress = events[i]->getGpuAddress(device) + outSemaphoreWaitCmds[i].offset;
        EXPECT_EQ(eventGpuAddress & addressSpace, outCmd->getSemaphoreGraphicsAddress() & eventGpuAddress);
    }

    auto eventResidencyIt = std::find(cmdListResidency.begin(), cmdListResidency.end(), eventPoolAllocation);
    EXPECT_EQ(cmdListResidency.end(), eventResidencyIt);

    skipResidency = false;
    result = commandList->appendWaitOnEvents(eventCount, eventHandles.data(), &outSemaphoreWaitCmds, false, false, false, skipResidency, false, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    eventResidencyIt = std::find(cmdListResidency.begin(), cmdListResidency.end(), eventPoolAllocation);
    EXPECT_NE(cmdListResidency.end(), eventResidencyIt);
}

} // namespace ult
} // namespace L0
