/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_event.h"

using namespace NEO;

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTaskTests;

HWTEST_F(CommandStreamReceiverFlushTaskTests, WhenFlushingTaskThenCommandStreamReceiverGetsUpdated) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenForceCsrReprogrammingDebugVariableSetWhenFlushingThenInitProgrammingFlagsShouldBeCalled) {
    DebugManagerStateRestore restore;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DebugManager.flags.ForceCsrReprogramming.set(true);

    flushTask(commandStreamReceiver);

    EXPECT_TRUE(commandStreamReceiver.initProgrammingFlagsCalled);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenForceCsrFlushingDebugVariableSetWhenFlushingThenFlushBatchedSubmissionsShouldBeCalled) {
    DebugManagerStateRestore restore;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DebugManager.flags.ForceCsrFlushing.set(true);

    flushTask(commandStreamReceiver);

    EXPECT_TRUE(commandStreamReceiver.flushBatchedSubmissionsCalled);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenForceImplicitFlushDebugVariableSetWhenFlushingThenFlushBatchedSubmissionsShouldBeCalled) {
    DebugManagerStateRestore restore;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.dispatchMode = DispatchMode::BatchedDispatch;

    DebugManager.flags.ForceImplicitFlush.set(true);

    flushTask(commandStreamReceiver);

    EXPECT_TRUE(commandStreamReceiver.flushBatchedSubmissionsCalled);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenOverrideThreadArbitrationPolicyDebugVariableSetWhenFlushingThenRequestRequiredMode) {
    DebugManagerStateRestore restore;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    DebugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);

    EXPECT_EQ(-1, commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);

    flushTask(commandStreamReceiver);
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobin,
              commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, WhenFlushingTaskThenTaskCountIsIncremented) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenconfigureCSRtoNonDirtyStateWhenFlushTaskIsCalledThenNoCommandsAreAdded) {
    configureCSRtoNonDirtyState<FamilyType>(false);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);
    EXPECT_EQ(0u, commandStreamReceiver.commandStream.getUsed());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenMultiOsContextCommandStreamReceiverWhenFlushTaskIsCalledThenCommandStreamReceiverStreamIsUsed) {
    configureCSRtoNonDirtyState<FamilyType>(false);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.multiOsContextCapable = true;
    commandStream.getSpace(4);

    flushTask(commandStreamReceiver);
    EXPECT_EQ(MemoryConstants::cacheLineSize, commandStreamReceiver.commandStream.getUsed());
    auto batchBufferStart = genCmdCast<typename FamilyType::MI_BATCH_BUFFER_START *>(commandStreamReceiver.commandStream.getCpuBase());
    EXPECT_NE(nullptr, batchBufferStart);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenTaskIsSubmittedViaCsrThenBbEndCoversPaddingEnoughToFitMiBatchBufferStart) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr.timestampPacketWriteEnabled = false;

    configureCSRtoNonDirtyState<FamilyType>(false);

    mockCsr.getCS(1024u);
    auto &csrCommandStream = mockCsr.commandStream;

    //we do level change that will emit PPC, fill all the space so only BB end fits.
    taskLevel++;
    auto ppcSize = MemorySynchronizationCommands<FamilyType>::getSizeForSingleBarrier();
    auto fillSize = MemoryConstants::cacheLineSize - ppcSize - sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    csrCommandStream.getSpace(fillSize);
    auto expectedUsedSize = 2 * MemoryConstants::cacheLineSize;

    flushTask(mockCsr);

    EXPECT_EQ(expectedUsedSize, mockCsr.commandStream.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenTaskIsSubmittedViaCommandStreamThenBbEndCoversPaddingEnoughToFitMiBatchBufferStart) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    configureCSRtoNonDirtyState<FamilyType>(false);

    auto fillSize = MemoryConstants::cacheLineSize - sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    commandStream.getSpace(fillSize);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr.flushTask(commandStream,
                      0,
                      &dsh,
                      &ioh,
                      &ssh,
                      taskLevel,
                      dispatchFlags,
                      *pDevice);

    auto expectedUsedSize = 2 * MemoryConstants::cacheLineSize;
    EXPECT_EQ(expectedUsedSize, commandStream.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenflushTaskThenDshAndIohNotEvictable) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    mockCsr.flushTask(commandStream,
                      0,
                      &dsh,
                      &ioh,
                      &ssh,
                      taskLevel,
                      dispatchFlags,
                      *pDevice);

    EXPECT_EQ(dsh.getGraphicsAllocation()->peekEvictable(), true);
    EXPECT_EQ(ssh.getGraphicsAllocation()->peekEvictable(), true);
    EXPECT_EQ(ioh.getGraphicsAllocation()->peekEvictable(), true);

    dsh.getGraphicsAllocation()->setEvictable(false);
    EXPECT_EQ(dsh.getGraphicsAllocation()->peekEvictable(), false);
    dsh.getGraphicsAllocation()->setEvictable(true);
    EXPECT_EQ(dsh.getGraphicsAllocation()->peekEvictable(), true);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeAndMidThreadPreemptionWhenFlushTaskIsCalledThenSipKernelIsMadeResident) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr.useNewResourceImplicitFlush = false;
    mockCsr.useGpuIdleImplicitFlush = false;

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr.submissionAggregator.reset(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionMode::MidThread;
    auto sipType = SipKernel::getSipKernelType(*pDevice);
    SipKernel::initSipKernel(sipType, *pDevice);

    mockCsr.flushTask(commandStream,
                      0,
                      &dsh,
                      &ioh,
                      &ssh,
                      taskLevel,
                      dispatchFlags,
                      *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto sipAllocation = SipKernel::getSipKernel(*pDevice).getSipAllocation();
    bool found = false;
    for (auto allocation : cmdBuffer->surfaces) {
        if (allocation == sipAllocation) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInDefaultModeAndMidThreadPreemptionWhenFlushTaskIsCalledThenSipKernelIsMadeResident) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionMode::MidThread;
    auto sipType = SipKernel::getSipKernelType(*pDevice);
    SipKernel::initSipKernel(sipType, *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto sipAllocation = SipKernel::getSipKernel(*pDevice).getSipAllocation();
    bool found = false;
    for (auto allocation : mockCsr->copyOfAllocations) {
        if (allocation == sipAllocation) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenFlushThenCommandBufferAlreadyHasProperTaskCountsAndIsNotIncludedInResidencyVector) {
    struct MockCsrFlushCmdBuffer : public MockCommandStreamReceiver {
        using MockCommandStreamReceiver::MockCommandStreamReceiver;
        NEO::SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
            EXPECT_EQ(batchBuffer.commandBufferAllocation->getResidencyTaskCount(this->osContext->getContextId()), this->taskCount + 1);
            EXPECT_EQ(batchBuffer.commandBufferAllocation->getTaskCount(this->osContext->getContextId()), this->taskCount + 1);
            EXPECT_EQ(std::find(allocationsForResidency.begin(), allocationsForResidency.end(), batchBuffer.commandBufferAllocation), allocationsForResidency.end());
            return NEO::SubmissionStatus::SUCCESS;
        }
    };

    auto mockCsr = new MockCsrFlushCmdBuffer(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionMode::MidThread;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, GivenSameTaskLevelWhenFlushingTaskThenDoNotSendPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>(false);

    flushTask(commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver.taskLevel);

    auto sizeUsed = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(sizeUsed, 0u);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenDeviceWithThreadGroupPreemptionSupportThenDontSendMediaVfeStateIfNotDirty) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>(false);

    flushTask(*commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver->peekTaskLevel());

    auto sizeUsed = commandStreamReceiver->commandStream.getUsed();
    EXPECT_EQ(0u, sizeUsed);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenHigherTaskLevelWhenFlushingTaskThenSendPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.timestampPacketWriteEnabled = false;

    configureCSRtoNonDirtyState<FamilyType>(false);
    commandStreamReceiver.taskLevel = taskLevel / 2;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver.peekTaskLevel());
    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenCommandStreamReceiverWithInstructionCacheRequestWhenFlushTaskIsCalledThenPipeControlWithInstructionCacheIsEmitted) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    configureCSRtoNonDirtyState<FamilyType>(false);

    commandStreamReceiver.registerInstructionCacheFlush();
    EXPECT_EQ(1u, commandStreamReceiver.recursiveLockCounter);

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_FALSE(commandStreamReceiver.requiresInstructionCacheFlush);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenHigherTaskLevelWhenTimestampPacketWriteIsEnabledThenDontAddPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = true;
    commandStreamReceiver.isPreambleSent = true;
    configureCSRtoNonDirtyState<FamilyType>(false);
    commandStreamReceiver.taskLevel = taskLevel;
    taskLevel++; // submit with higher taskLevel

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, WhenForcePipeControlPriorToWalkerIsSetThenAddExtraPipeControls) {
    DebugManagerStateRestore stateResore;
    DebugManager.flags.ForcePipeControlPriorToWalker.set(true);
    DebugManager.flags.FlushAllCaches.set(true);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    configureCSRtoNonDirtyState<FamilyType>(false);
    commandStreamReceiver.taskLevel = taskLevel;

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    GenCmdList::iterator itor = cmdList.begin();
    int counterPC = 0;
    while (itor != cmdList.end()) {
        auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itor);
        if (pipeControl) {
            switch (counterPC) {
            case 0: // First pipe control with CS Stall
                EXPECT_EQ(bool(pipeControl->getCommandStreamerStallEnable()), true);
                if (pDevice->getHardwareInfo().platform.eProductFamily == IGFX_BROADWELL) {
                    EXPECT_EQ(bool(pipeControl->getDcFlushEnable()), true);
                } else {
                    EXPECT_EQ(bool(pipeControl->getDcFlushEnable()), false);
                }
                EXPECT_EQ(bool(pipeControl->getRenderTargetCacheFlushEnable()), false);
                EXPECT_EQ(bool(pipeControl->getInstructionCacheInvalidateEnable()), false);
                EXPECT_EQ(bool(pipeControl->getTextureCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getPipeControlFlushEnable()), false);
                EXPECT_EQ(bool(pipeControl->getVfCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getConstantCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getStateCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getTlbInvalidate()), false);
                break;
            case 1: // Second pipe control with all flushes
                EXPECT_EQ(bool(pipeControl->getCommandStreamerStallEnable()), true);
                EXPECT_EQ(bool(pipeControl->getDcFlushEnable()), true);
                EXPECT_EQ(bool(pipeControl->getRenderTargetCacheFlushEnable()), true);
                EXPECT_EQ(bool(pipeControl->getInstructionCacheInvalidateEnable()), true);
                EXPECT_EQ(bool(pipeControl->getTextureCacheInvalidationEnable()), true);
                EXPECT_EQ(bool(pipeControl->getPipeControlFlushEnable()), true);
                EXPECT_EQ(bool(pipeControl->getVfCacheInvalidationEnable()), true);
                EXPECT_EQ(bool(pipeControl->getConstantCacheInvalidationEnable()), true);
                EXPECT_EQ(bool(pipeControl->getStateCacheInvalidationEnable()), true);
                EXPECT_EQ(bool(pipeControl->getTlbInvalidate()), true);
            default:
                break;
            }
            counterPC++;
        }

        ++itor;
    }

    EXPECT_EQ(counterPC, 2);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, whenSamplerCacheFlushNotRequiredThenDontSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    NEO::WorkaroundTable *waTable = &pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;

    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastPreemptionMode = pDevice->getPreemptionMode();
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired);
    configureCSRtoNonDirtyState<FamilyType>(false);
    commandStreamReceiver.taskLevel = taskLevel;
    waTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
    flushTask(commandStreamReceiver);

    EXPECT_EQ(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenSamplerCacheFlushBeforeThenSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
    configureCSRtoNonDirtyState<FamilyType>(false);
    commandStreamReceiver.taskLevel = taskLevel;
    NEO::WorkaroundTable *waTable = &pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;

    waTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushAfter, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto pipeControlCmd = (typename FamilyType::PIPE_CONTROL *)*itorPC;
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, whenSamplerCacheFlushBeforeAndWaSamplerCacheFlushBetweenRedescribedSurfaceReadsDasabledThenDontSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
    configureCSRtoNonDirtyState<FamilyType>(false);
    commandStreamReceiver.taskLevel = taskLevel;
    NEO::WorkaroundTable *waTable = &pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;

    waTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = false;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenSamplerCacheFlushAfterThenSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushAfter);
    configureCSRtoNonDirtyState<FamilyType>(false);
    commandStreamReceiver.taskLevel = taskLevel;
    NEO::WorkaroundTable *waTable = &pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;

    waTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto pipeControlCmd = (typename FamilyType::PIPE_CONTROL *)*itorPC;
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenStaleCqWhenFlushingTaskThenCompletionStampIsValid) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    //simulate our CQ is stale for 10 TL's
    commandStreamReceiver.taskLevel = taskLevel + 10;

    auto completionStamp = flushTask(commandStreamReceiver);

    EXPECT_EQ(completionStamp.taskLevel, commandStreamReceiver.peekTaskLevel());
    EXPECT_EQ(completionStamp.taskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(completionStamp.flushStamp, commandStreamReceiver.flushStamp->peekStamp());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, WhenFlushingTaskThenCompletionStampIsValid) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto completionStamp = flushTask(commandStreamReceiver);

    EXPECT_EQ(1u, completionStamp.taskCount);
    EXPECT_EQ(taskLevel, completionStamp.taskLevel);
    EXPECT_EQ(commandStreamReceiver.flushStamp->peekStamp(), completionStamp.flushStamp);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, WhenFlushingTaskThenStateBaseAddressIsCorrect) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        EXPECT_TRUE(commandStreamReceiver.dshState.updateAndCheck(&dsh));
    } else {
        EXPECT_FALSE(commandStreamReceiver.dshState.updateAndCheck(&dsh));
    }
    EXPECT_FALSE(commandStreamReceiver.iohState.updateAndCheck(&ioh));
    EXPECT_FALSE(commandStreamReceiver.sshState.updateAndCheck(&ssh));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, WhenFlushingTaskThenStateBaseAddressProgrammingShouldMatchTracking) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    auto gmmHelper = pDevice->getGmmHelper();
    auto stateHeapMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);
    auto l3CacheOnMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    auto &commandStreamCSR = commandStreamReceiver.commandStream;
    parseCommands<FamilyType>(commandStreamCSR, 0);
    HardwareParse::findHardwareCommands<FamilyType>();

    ASSERT_NE(nullptr, cmdStateBaseAddress);
    auto &cmd = *reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStateBaseAddress);

    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    auto instructionHeapBaseAddress = commandStreamReceiver.getMemoryManager()->getInternalHeapBaseAddress(commandStreamReceiver.rootDeviceIndex, !hwHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo()));

    EXPECT_EQ(dsh.getCpuBase(), reinterpret_cast<void *>(cmd.getDynamicStateBaseAddress()));
    EXPECT_EQ(instructionHeapBaseAddress, cmd.getInstructionBaseAddress());
    EXPECT_EQ(ioh.getCpuBase(), reinterpret_cast<void *>(cmd.getIndirectObjectBaseAddress()));
    EXPECT_EQ(ssh.getCpuBase(), reinterpret_cast<void *>(cmd.getSurfaceStateBaseAddress()));

    EXPECT_EQ(l3CacheOnMocs, cmd.getStatelessDataPortAccessMemoryObjectControlState());
    EXPECT_EQ(stateHeapMocs, cmd.getInstructionMemoryObjectControlState());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenStateBaseAddressWhenItIsRequiredThenThereIsPipeControlPriorToItWithTextureCacheFlush) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
    auto pipeControlCmd = (typename FamilyType::PIPE_CONTROL *)*pipeControlItor;
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControlCmd->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenNotApplicableL3ConfigWhenFlushingTaskThenDontReloadSba) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    {
        flushTaskFlags.l3CacheSettings = L3CachingSettings::l3CacheOn;
        flushTask(commandStreamReceiver);

        parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
        auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), stateBaseAddressItor);
    }

    {
        flushTaskFlags.l3CacheSettings = L3CachingSettings::NotApplicable;
        auto offset = commandStreamReceiver.commandStream.getUsed();

        flushTask(commandStreamReceiver);

        cmdList.clear();
        parseCommands<FamilyType>(commandStreamReceiver.commandStream, offset);
        auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), stateBaseAddressItor);
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenNotApplicableGrfConfigWhenFlushingTaskThenDontReloadSba) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    {
        flushTaskFlags.numGrfRequired = GrfConfig::DefaultGrfNumber;
        flushTask(commandStreamReceiver);

        parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
        auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), stateBaseAddressItor);
    }

    {
        flushTaskFlags.numGrfRequired = GrfConfig::NotApplicable;
        auto offset = commandStreamReceiver.commandStream.getUsed();

        flushTask(commandStreamReceiver);

        cmdList.clear();
        parseCommands<FamilyType>(commandStreamReceiver.commandStream, offset);
        auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), stateBaseAddressItor);
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenPreambleNotSentWhenFlushingTaskThenPreambleIsSent) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;
    flushTask(commandStreamReceiver);

    EXPECT_TRUE(commandStreamReceiver.isPreambleSent);
    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenFlushTaskWhenInitProgrammingFlagsIsCalledThenBindingTableBaseAddressRequiredIsSetCorrecty) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.initProgrammingFlags();
    EXPECT_TRUE(commandStreamReceiver.bindingTableBaseAddressRequired);

    flushTask(commandStreamReceiver);
    EXPECT_FALSE(commandStreamReceiver.bindingTableBaseAddressRequired);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenFlushTaskWhenInitProgrammingFlagsIsNotCalledThenBindingTableBaseAddressRequiredIsSetCorrectly) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.bindingTableBaseAddressRequired);

    flushTask(commandStreamReceiver);
    EXPECT_FALSE(commandStreamReceiver.bindingTableBaseAddressRequired);
}

HWTEST2_F(CommandStreamReceiverFlushTaskTests, GivenPreambleNotSentAndMediaSamplerRequirementChangedWhenFlushingTaskThenPipelineSelectIsSent, IsAtMostXeHpcCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;
    commandStreamReceiver.lastMediaSamplerConfig = -1;
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_NE(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}

HWTEST2_F(CommandStreamReceiverFlushTaskTests, GivenPreambleNotSentAndMediaSamplerRequirementNotChangedWhenFlushingTaskThenPipelineSelectIsSent, IsAtMostXeHpcCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;
    commandStreamReceiver.lastMediaSamplerConfig = 0;
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_NE(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenPreambleSentAndMediaSamplerRequirementNotChangedWhenFlushingTaskThenPipelineSelectIsNotSent) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastMediaSamplerConfig = 0;
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    const auto &hwInfoConfig = *HwInfoConfig::get(pDevice->getHardwareInfo().platform.eProductFamily);
    if (hwInfoConfig.is3DPipelineSelectWARequired()) {
        EXPECT_NE(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
    } else {
        EXPECT_EQ(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
    }
}
HWTEST2_F(CommandStreamReceiverFlushTaskTests, GivenPreambleSentAndMediaSamplerRequirementChangedWhenFlushingTaskThenPipelineSelectIsSent, IsAtMostXeHpcCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastMediaSamplerConfig = 1;
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_NE(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenStateBaseAddressNotSentWhenFlushingTaskThenStateBaseAddressIsSent) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setMediaVFEStateDirty(false);
    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenSizeChangedWhenFlushingTaskThenStateBaseAddressIsSent) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto dshSize = dsh.getMaxAvailableSpace();
    auto iohSize = ioh.getMaxAvailableSpace();
    auto sshSize = ssh.getMaxAvailableSpace();

    dsh.replaceBuffer(dsh.getCpuBase(), 0);
    ioh.replaceBuffer(ioh.getCpuBase(), 0);
    ssh.replaceBuffer(ssh.getCpuBase(), 0);

    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setMediaVFEStateDirty(false);

    configureCSRHeapStatesToNonDirty<FamilyType>();

    dsh.replaceBuffer(dsh.getCpuBase(), dshSize);
    ioh.replaceBuffer(ioh.getCpuBase(), iohSize);
    ssh.replaceBuffer(ssh.getCpuBase(), sshSize);

    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDshHeapChangeWhenFlushTaskIsCalledThenSbaIsReloaded) {
    bool deviceUsesDsh = pDevice->getHardwareInfo().capabilityTable.supportsImages;
    if (!deviceUsesDsh) {
        GTEST_SKIP();
    }
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>(false);

    dsh.replaceBuffer(nullptr, 0);
    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenSshHeapChangeWhenFlushTaskIsCalledThenSbaIsReloaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>(false);

    ssh.replaceBuffer(nullptr, 0);
    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenIohHeapChangeWhenFlushTaskIsCalledThenSbaIsReloaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>(false);

    ioh.replaceBuffer(nullptr, 0);
    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenStateBaseAddressNotChangedWhenFlushingTaskThenStateBaseAddressIsNotSent) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    configureCSRHeapStatesToNonDirty<FamilyType>();

    flushTask(commandStreamReceiver);

    auto base = commandStreamReceiver.commandStream.getCpuBase();

    auto stateBaseAddress = base
                                ? genCmdCast<typename FamilyType::STATE_BASE_ADDRESS *>(base)
                                : nullptr;
    EXPECT_EQ(nullptr, stateBaseAddress);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenEmptyCqsWhenFlushingTaskThenCommandNotAdded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto usedBefore = commandStream.getUsed();
    flushTask(commandStreamReceiver);

    EXPECT_EQ(usedBefore, commandStream.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenBlockingWhenFlushingTaskThenPipeControlIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blocking = true;
    flushTask(commandStreamReceiver, blocking);

    parseCommands<FamilyType>(commandStream, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenBlockingWithNoPreviousDependenciesWhenFlushingTaskThenTaskLevelIsIncremented) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    taskLevel = 5;
    commandStreamReceiver.taskLevel = 6;

    auto blocking = true;
    flushTask(commandStreamReceiver, blocking);

    EXPECT_EQ(7u, commandStreamReceiver.peekTaskLevel());
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenNonBlockingWithNoPreviousDependenciesWhenFlushingTaskThenTaskLevelIsNotIncremented) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    taskLevel = 5;
    commandStreamReceiver.taskLevel = 6;

    auto blocking = false;
    flushTask(commandStreamReceiver, blocking);

    EXPECT_EQ(6u, commandStreamReceiver.peekTaskLevel());
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenEnoughMemoryOnlyForPreambleWhenFlushingTaskThenOnlyAvailableMemoryIsUsed) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;

    hardwareInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0u));
    auto &commandStreamReceiver = mockDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.timestampPacketWriteEnabled = false;
    // Force a PIPE_CONTROL through a taskLevel transition
    taskLevel = commandStreamReceiver.peekTaskLevel() + 1;

    commandStreamReceiver.streamProperties.stateComputeMode.isCoherencyRequired.value = 0;
    auto l3Config = PreambleHelper<FamilyType>::getL3Config(pDevice->getHardwareInfo(), false);
    commandStreamReceiver.lastSentL3Config = l3Config;

    auto &csrCS = commandStreamReceiver.getCS();
    size_t sizeNeededForPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*mockDevice);
    size_t sizeNeeded = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *mockDevice);
    sizeNeeded -= sizeof(MI_BATCH_BUFFER_START); // no task to submit
    sizeNeeded += sizeof(MI_BATCH_BUFFER_END);   // no task to submit, add BBE to CSR stream
    sizeNeeded = alignUp(sizeNeeded, MemoryConstants::cacheLineSize);

    csrCS.getSpace(csrCS.getAvailableSpace() - sizeNeededForPreamble);

    commandStreamReceiver.streamProperties.stateComputeMode.setProperties(flushTaskFlags.requiresCoherency, flushTaskFlags.numGrfRequired,
                                                                          flushTaskFlags.threadArbitrationPolicy, PreemptionMode::Disabled, *defaultHwInfo);
    flushTask(commandStreamReceiver);

    EXPECT_EQ(sizeNeeded, csrCS.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenEnoughMemoryOnlyForPreambleAndSbaWhenFlushingTaskThenOnlyAvailableMemoryIsUsed) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;

    hardwareInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0u));
    auto &commandStreamReceiver = mockDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.timestampPacketWriteEnabled = false;
    // Force a PIPE_CONTROL through a taskLevel transition
    taskLevel = commandStreamReceiver.peekTaskLevel() + 1;

    auto l3Config = PreambleHelper<FamilyType>::getL3Config(mockDevice->getHardwareInfo(), false);
    commandStreamReceiver.lastSentL3Config = l3Config;

    auto &csrCS = commandStreamReceiver.getCS();
    size_t sizeNeededForPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*mockDevice);
    size_t sizeNeededForStateBaseAddress = sizeof(STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL);
    size_t sizeNeeded = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *mockDevice);
    sizeNeeded -= sizeof(MI_BATCH_BUFFER_START); // no task to submit
    sizeNeeded += sizeof(MI_BATCH_BUFFER_END);   // no task to submit, add BBE to CSR stream
    sizeNeeded = alignUp(sizeNeeded, MemoryConstants::cacheLineSize);

    csrCS.getSpace(csrCS.getAvailableSpace() - sizeNeededForPreamble - sizeNeededForStateBaseAddress);

    commandStreamReceiver.streamProperties.stateComputeMode.setProperties(flushTaskFlags.requiresCoherency, flushTaskFlags.numGrfRequired,
                                                                          flushTaskFlags.threadArbitrationPolicy, PreemptionMode::Disabled, *defaultHwInfo);
    flushTask(commandStreamReceiver);

    EXPECT_EQ(sizeNeeded, csrCS.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenEnoughMemoryOnlyForPreambleAndSbaAndPipeControlWhenFlushingTaskThenOnlyAvailableMemoryIsUsed) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    commandStream.getSpace(sizeof(PIPE_CONTROL));

    hardwareInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0u));
    auto &commandStreamReceiver = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = false;
    // Force a PIPE_CONTROL through a taskLevel transition
    taskLevel = commandStreamReceiver.peekTaskLevel() + 1;

    commandStreamReceiver.streamProperties.stateComputeMode.isCoherencyRequired.value = 0;

    auto l3Config = PreambleHelper<FamilyType>::getL3Config(mockDevice->getHardwareInfo(), false);
    commandStreamReceiver.lastSentL3Config = l3Config;

    auto &csrCS = commandStreamReceiver.getCS();
    size_t sizeNeeded = commandStreamReceiver.getRequiredCmdStreamSizeAligned(flushTaskFlags, *mockDevice);

    csrCS.getSpace(csrCS.getAvailableSpace() - sizeNeeded);
    auto expectedBase = csrCS.getCpuBase();

    // This case handles when we have *just* enough space
    auto expectedUsed = csrCS.getUsed() + sizeNeeded;

    flushTaskFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(mockDevice->getHardwareInfo());

    commandStreamReceiver.streamProperties.stateComputeMode.setProperties(flushTaskFlags.requiresCoherency, flushTaskFlags.numGrfRequired,
                                                                          flushTaskFlags.threadArbitrationPolicy, PreemptionMode::Disabled, *defaultHwInfo);
    commandStreamReceiver.flushTask(
        commandStream,
        0,
        &dsh,
        &ioh,
        &ssh,
        taskLevel,
        flushTaskFlags,
        *mockDevice);

    // Verify that we didn't grab a new CS buffer
    EXPECT_EQ(expectedUsed, csrCS.getUsed());
    EXPECT_EQ(expectedBase, csrCS.getCpuBase());
}

template <typename FamilyType>
struct CommandStreamReceiverHwLog : public UltCommandStreamReceiver<FamilyType> {
    CommandStreamReceiverHwLog(ExecutionEnvironment &executionEnvironment,
                               uint32_t rootDeviceIndex,
                               const DeviceBitfield deviceBitfield)
        : UltCommandStreamReceiver<FamilyType>(executionEnvironment, rootDeviceIndex, deviceBitfield) {
    }

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        ++flushCount;
        return SubmissionStatus::SUCCESS;
    }

    int flushCount = 0;
};

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenBothCsWhenFlushingTaskThenFlushOnce) {
    CommandStreamReceiverHwLog<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*pDevice->getDefaultEngine().osContext);
    commandStreamReceiver.initializeTagAllocation();
    commandStreamReceiver.createPreemptionAllocation();
    commandStream.getSpace(sizeof(typename FamilyType::MI_NOOP));

    flushTask(commandStreamReceiver);
    EXPECT_EQ(1, commandStreamReceiver.flushCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenBothCsWhenFlushingTaskThenChainWithBatchBufferStart) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::MI_NOOP MI_NOOP;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    // Reserve space for 16 NOOPs
    commandStream.getSpace(16 * sizeof(MI_NOOP));

    // Submit starting at 8 NOOPs
    size_t startOffset = 8 * sizeof(MI_NOOP);
    flushTask(commandStreamReceiver, false, startOffset);

    // Locate the MI_BATCH_BUFFER_START
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto itorBBS = find<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBBS);

    auto bbs = genCmdCast<MI_BATCH_BUFFER_START *>(*itorBBS);
    ASSERT_NE(nullptr, bbs);

    // Expect to see address based on startOffset of task
    auto expectedAddress = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptrOffset(commandStream.getCpuBase(), startOffset)));
    EXPECT_EQ(expectedAddress, bbs->getBatchBufferStartAddress());

    // MI_BATCH_BUFFER_START from UMD must be PPGTT for security reasons
    EXPECT_EQ(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT, bbs->getAddressSpaceIndicator());
}

typedef Test<ClDeviceFixture> CommandStreamReceiverCQFlushTaskTests;
HWTEST_F(CommandStreamReceiverCQFlushTaskTests, WhenGettingCsThenReturnCsWithEnoughSize) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();

    // NOTE: This test attempts to reserve the maximum amount
    // of memory such that if a client gets everything he wants
    // we don't overflow/corrupt memory when CSR appends its
    // work.
    size_t sizeCQReserves = CSRequirements::minCommandQueueCommandStreamSize;

    size_t sizeRequested = MemoryConstants::pageSize64k - sizeCQReserves;
    auto &commandStream = commandQueue.getCS(sizeRequested);
    auto expect = alignUp(sizeRequested + CSRequirements::csOverfetchSize, MemoryConstants::pageSize64k);
    ASSERT_GE(expect, commandStream.getMaxAvailableSpace());

    EXPECT_GE(commandStream.getAvailableSpace(), sizeRequested);
    commandStream.getSpace(sizeRequested - sizeCQReserves);

    MockGraphicsAllocation allocation((void *)MemoryConstants::pageSize64k, 1);
    IndirectHeap linear(&allocation);

    auto blocking = true;
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.blocking = blocking;

    commandStreamReceiver.flushTask(
        commandStream,
        0,
        &linear,
        &linear,
        &linear,
        1,
        dispatchFlags,
        *pDevice);

    auto expectedSize = MemoryConstants::pageSize64k - sizeCQReserves;

    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        expectedSize -= sizeof(typename FamilyType::PIPE_CONTROL);
    }
    expectedSize = alignUp(expectedSize, MemoryConstants::cacheLineSize);

    auto currentUsed = commandStream.getUsed();
    EXPECT_EQ(0u, currentUsed % MemoryConstants::cacheLineSize);

    //depending on the size of commands we may need whole additional cacheline for alignment
    if (currentUsed != expectedSize) {
        EXPECT_EQ(expectedSize - MemoryConstants::cacheLineSize, currentUsed);
    } else {
        EXPECT_EQ(expectedSize, currentUsed);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, GivenBlockingWhenFlushingTaskThenAddPipeControlOnlyToTaskCs) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    // Configure the CSR to not need to submit any state or commands
    configureCSRtoNonDirtyState<FamilyType>(false);

    // Force a PIPE_CONTROL through a blocking flag
    auto blocking = true;
    auto &commandStreamTask = commandQueue.getCS(1024);
    auto &commandStreamCSR = commandStreamReceiver->getCS();
    commandStreamReceiver->streamProperties.stateComputeMode.isCoherencyRequired.value = 0;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.blocking = blocking;
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    commandStreamReceiver->flushTask(
        commandStreamTask,
        0,
        &dsh,
        &ioh,
        &ssh,
        taskLevel,
        dispatchFlags,
        *pDevice);

    // Verify that taskCS got modified, while csrCS remained intact
    EXPECT_GT(commandStreamTask.getUsed(), 0u);
    EXPECT_EQ(0u, commandStreamCSR.getUsed());

    // Parse command list to verify that PC got added to taskCS
    cmdList.clear();
    parseCommands<FamilyType>(commandStreamTask, 0);
    auto itorTaskCS = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorTaskCS);

    // Parse command list to verify that PC wasn't added to csrCS
    cmdList.clear();
    parseCommands<FamilyType>(commandStreamCSR, 0);
    auto numberOfPC = getCommandsList<PIPE_CONTROL>().size();
    EXPECT_EQ(0u, numberOfPC);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, GivenBlockingWhenFlushingTaskThenAddPipeControlWithDcFlush) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    configureCSRtoNonDirtyState<FamilyType>(false);

    auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();

    size_t pipeControlCount = UltMemorySynchronizationCommands<FamilyType>::getExpectedPipeControlCount(pDevice->getHardwareInfo());

    auto &commandStreamTask = commandQueue.getCS(1024);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.blocking = true;
    dispatchFlags.dcFlush = true;
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    commandStreamReceiver.flushTask(
        commandStreamTask,
        0,
        &dsh,
        &ioh,
        &ssh,
        taskLevel,
        dispatchFlags,
        *pDevice);

    parseCommands<FamilyType>(commandStreamTask, 0);
    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);

    if (UnitTestHelper<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
        // Verify that the dcFlushEnabled bit is set in PC
        auto pCmdWA = reinterpret_cast<PIPE_CONTROL *>(*itorPC);
        EXPECT_FALSE(pCmdWA->getDcFlushEnable());

        if (pipeControlCount > 1) {
            // Search taskCS for PC to analyze
            itorPC = find<PIPE_CONTROL *>(++itorPC, cmdList.end());
            auto pipeControlTask = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPC);
            ASSERT_NE(nullptr, pipeControlTask);

            // Verify that the dcFlushEnabled bit is not set in PC
            auto pCmd = reinterpret_cast<PIPE_CONTROL *>(pipeControlTask);
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pCmd->getDcFlushEnable());
        }
    } else {
        auto pCmd = reinterpret_cast<PIPE_CONTROL *>(*itorPC);
        EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pCmd->getDcFlushEnable());
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenBlockedKernelRequiringDCFlushWhenUnblockedThenDCFlushIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    MockContext ctx(pClDevice);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = false;
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    cl_event blockingEvent;
    MockEvent<UserEvent> mockEvent(&ctx);
    blockingEvent = &mockEvent;
    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {0, 1, 2};
    cl_int retVal = 0;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);

    auto &commandStreamCSR = commandStreamReceiver.getCS();

    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, nullptr, 1, &blockingEvent, 0);

    // Expect nothing was sent
    EXPECT_EQ(0u, commandStreamCSR.getUsed());

    // Unblock Event
    mockEvent.setStatus(CL_COMPLETE);
    auto &commandStreamTask = *commandStreamReceiver.lastFlushedCommandStream;

    cmdList.clear();
    // Parse command list
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    if (UnitTestHelper<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
        itorPC++;
        itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
        EXPECT_NE(cmdList.end(), itorPC);
    }

    // Verify that the dcFlushEnabled bit is set in PC
    auto pCmdWA = reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pCmdWA->getDcFlushEnable());

    buffer->release();
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDispatchFlagsWhenCallFlushTaskThenThreadArbitrationPolicyIsSetProperly) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    dispatchFlags.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(dispatchFlags.threadArbitrationPolicy, mockCsr->streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

class CommandStreamReceiverFlushTaskMemoryCompressionTests : public UltCommandStreamReceiverTest,
                                                             public ::testing::WithParamInterface<MemoryCompressionState> {};

HWTEST_P(CommandStreamReceiverFlushTaskMemoryCompressionTests, givenCsrWithMemoryCompressionStateNotApplicableWhenFlushTaskIsCalledThenUseLastMemoryCompressionState) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.memoryCompressionState = MemoryCompressionState::NotApplicable;

    mockCsr.lastMemoryCompressionState = GetParam();
    MemoryCompressionState lastMemoryCompressionState = mockCsr.lastMemoryCompressionState;

    mockCsr.flushTask(commandStream,
                      0,
                      &dsh,
                      &ioh,
                      &ssh,
                      taskLevel,
                      dispatchFlags,
                      *pDevice);
    EXPECT_EQ(lastMemoryCompressionState, mockCsr.lastMemoryCompressionState);
}

HWTEST_P(CommandStreamReceiverFlushTaskMemoryCompressionTests, givenCsrWithMemoryCompressionStateApplicableWhenFlushTaskIsCalledThenUpdateLastMemoryCompressionState) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    dispatchFlags.memoryCompressionState = GetParam();

    if (dispatchFlags.memoryCompressionState == MemoryCompressionState::NotApplicable) {

        for (auto memoryCompressionState : {MemoryCompressionState::NotApplicable, MemoryCompressionState::Disabled, MemoryCompressionState::Enabled}) {
            mockCsr.lastMemoryCompressionState = memoryCompressionState;
            MemoryCompressionState lastMemoryCompressionState = mockCsr.lastMemoryCompressionState;
            mockCsr.flushTask(commandStream,
                              0,
                              &dsh,
                              &ioh,
                              &ssh,
                              taskLevel,
                              dispatchFlags,
                              *pDevice);
            EXPECT_EQ(lastMemoryCompressionState, mockCsr.lastMemoryCompressionState);
        }
    } else {

        for (auto memoryCompressionState : {MemoryCompressionState::NotApplicable, MemoryCompressionState::Disabled, MemoryCompressionState::Enabled}) {
            mockCsr.lastMemoryCompressionState = memoryCompressionState;
            mockCsr.flushTask(commandStream,
                              0,
                              &dsh,
                              &ioh,
                              &ssh,
                              taskLevel,
                              dispatchFlags,
                              *pDevice);
            EXPECT_EQ(dispatchFlags.memoryCompressionState, mockCsr.lastMemoryCompressionState);
        }
    }
}

INSTANTIATE_TEST_CASE_P(
    CommandStreamReceiverFlushTaskMemoryCompressionTestsValues,
    CommandStreamReceiverFlushTaskMemoryCompressionTests,
    testing::Values(MemoryCompressionState::NotApplicable, MemoryCompressionState::Disabled, MemoryCompressionState::Enabled));
