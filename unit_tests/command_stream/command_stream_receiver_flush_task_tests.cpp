/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "reg_configs_common.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/event/user_event.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/preamble.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/utilities/linux/debug_env_reader.h"
#include "test.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/ult_command_stream_receiver_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

using namespace OCLRT;

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTaskTests;

HWTEST_F(CommandStreamReceiverFlushTaskTests, shouldSeeCommandsOnFirstFlush) {
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

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenOverrideThreadArbitrationPolicyDebugVariableSetWhenFlushingThenRequestRequiredMode) {
    DebugManagerStateRestore restore;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.requiredThreadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
    commandStreamReceiver.lastSentThreadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;

    DebugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);

    flushTask(commandStreamReceiver);

    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobin, commandStreamReceiver.lastSentThreadArbitrationPolicy);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, taskCountShouldBeUpdated) {
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    flushTask(commandStreamReceiver);

    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenconfigureCSRtoNonDirtyStateWhenFlushTaskIsCalledThenNoCommandsAreAdded) {
    configureCSRtoNonDirtyState<FamilyType>();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);
    EXPECT_EQ(0u, commandStreamReceiver.commandStream.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenTaskIsSubmittedViaCsrThenBbEndCoversPaddingEnoughToFitMiBatchBufferStart) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    configureCSRtoNonDirtyState<FamilyType>();

    mockCsr.getCS(1024u);
    auto &csrCommandStream = mockCsr.commandStream;

    //we do level change that will emit PPC, fill all the space so only BB end fits.
    taskLevel++;
    auto ppcSize = mockCsr.getRequiredPipeControlSize();
    auto fillSize = MemoryConstants::cacheLineSize - ppcSize - sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    csrCommandStream.getSpace(fillSize);
    auto expectedUsedSize = 2 * MemoryConstants::cacheLineSize;

    flushTask(mockCsr);

    EXPECT_EQ(expectedUsedSize, mockCsr.commandStream.getUsed());
}
HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenTaskIsSubmittedViaCommandStreamThenBbEndCoversPaddingEnoughToFitMiBatchBufferStart) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);
    configureCSRtoNonDirtyState<FamilyType>();

    auto fillSize = MemoryConstants::cacheLineSize - sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    commandStream.getSpace(fillSize);
    DispatchFlags dispatchFlags;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr.flushTask(commandStream,
                      0,
                      dsh,
                      ioh,
                      ssh,
                      taskLevel,
                      dispatchFlags,
                      *pDevice);

    auto expectedUsedSize = 2 * MemoryConstants::cacheLineSize;
    EXPECT_EQ(expectedUsedSize, commandStream.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenflushTaskThenDshAndIohNotEvictable) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags;

    mockCsr.flushTask(commandStream,
                      0,
                      dsh,
                      ioh,
                      ssh,
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
    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr.submissionAggregator.reset(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.preemptionMode = PreemptionMode::MidThread;

    mockCsr.flushTask(commandStream,
                      0,
                      dsh,
                      ioh,
                      ssh,
                      taskLevel,
                      dispatchFlags,
                      *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto sipAllocation = pDevice->getExecutionEnvironment()->getBuiltIns()->getSipKernel(SipKernelType::Csr, *pDevice).getSipAllocation();
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
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags;
    dispatchFlags.preemptionMode = PreemptionMode::MidThread;

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto sipAllocation = pDevice->getExecutionEnvironment()->getBuiltIns()->getSipKernel(SipKernelType::Csr, *pDevice).getSipAllocation();
    bool found = false;
    for (auto allocation : mockCsr->copyOfAllocations) {
        if (allocation == sipAllocation) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, sameTaskLevelShouldntSendAPipeControl) {
    WhitelistedRegisters forceRegs = {0};
    pDevice->setForceWhitelistedRegs(true, &forceRegs);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>();

    flushTask(commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver.taskLevel);

    auto sizeUsed = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(sizeUsed, 0u);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDeviceWithThreadGroupPreemptionSupportThenDontSendMediaVfeStateIfNotDirty) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));
    WhitelistedRegisters forceRegs = {0};
    pDevice->setForceWhitelistedRegs(true, &forceRegs);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>();

    flushTask(*commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver->peekTaskLevel());

    auto sizeUsed = commandStreamReceiver->commandStream.getUsed();
    EXPECT_EQ(0u, sizeUsed);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, higherTaskLevelShouldSendAPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    configureCSRtoNonDirtyState<FamilyType>();
    commandStreamReceiver.taskLevel = taskLevel / 2;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver.peekTaskLevel());
    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenHigherTaskLevelWhenTimestampPacketWriteIsEnabledThenDontAddPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = true;
    commandStreamReceiver.isPreambleSent = true;
    configureCSRtoNonDirtyState<FamilyType>();
    commandStreamReceiver.taskLevel = taskLevel;
    taskLevel++; // submit with higher taskLevel

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenSamplerCacheFlushNotRequiredThenDontSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    OCLRT::WorkaroundTable *waTable = nullptr;
    waTable = const_cast<WorkaroundTable *>(pDevice->getWaTable());

    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastPreemptionMode = pDevice->getPreemptionMode();
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired);
    configureCSRtoNonDirtyState<FamilyType>();
    commandStreamReceiver.taskLevel = taskLevel;
    bool tmp = waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads;
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
    flushTask(commandStreamReceiver);

    EXPECT_EQ(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired, commandStreamReceiver.peekSamplerCacheFlushRequired());

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = tmp;
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenSamplerCacheFlushBeforeThenSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
    configureCSRtoNonDirtyState<FamilyType>();
    commandStreamReceiver.taskLevel = taskLevel;
    OCLRT::WorkaroundTable *waTable = nullptr;
    waTable = const_cast<WorkaroundTable *>(pDevice->getWaTable());

    bool tmp = waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads;
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushAfter, commandStreamReceiver.peekSamplerCacheFlushRequired());

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto pipeControlCmd = (typename FamilyType::PIPE_CONTROL *)*itorPC;
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = tmp;
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenSamplerCacheFlushBeforeAndWaSamplerCacheFlushBetweenRedescribedSurfaceReadsDasabledThenDontSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
    configureCSRtoNonDirtyState<FamilyType>();
    commandStreamReceiver.taskLevel = taskLevel;
    OCLRT::WorkaroundTable *waTable = nullptr;
    waTable = const_cast<WorkaroundTable *>(pDevice->getWaTable());

    bool tmp = waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads;
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = false;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore, commandStreamReceiver.peekSamplerCacheFlushRequired());

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = tmp;
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, whenSamplerCacheFlushAfterThenSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushAfter);
    configureCSRtoNonDirtyState<FamilyType>();
    commandStreamReceiver.taskLevel = taskLevel;
    OCLRT::WorkaroundTable *waTable = nullptr;
    waTable = const_cast<WorkaroundTable *>(pDevice->getWaTable());

    bool tmp = waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads;
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired, commandStreamReceiver.peekSamplerCacheFlushRequired());

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto pipeControlCmd = (typename FamilyType::PIPE_CONTROL *)*itorPC;
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = tmp;
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, completionStampValid) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    //simulate our CQ is stale for 10 TL's
    commandStreamReceiver.taskLevel = taskLevel + 10;

    auto completionStamp = flushTask(commandStreamReceiver);

    EXPECT_EQ(completionStamp.taskLevel, commandStreamReceiver.peekTaskLevel());
    EXPECT_EQ(completionStamp.taskCount, commandStreamReceiver.peekTaskCount());
    EXPECT_EQ(completionStamp.flushStamp, commandStreamReceiver.flushStamp->peekStamp());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, completionStamp) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto deviceEngineType = pDevice->getEngineType();
    auto completionStamp = flushTask(commandStreamReceiver);

    EXPECT_EQ(1u, completionStamp.taskCount);
    EXPECT_EQ(taskLevel, completionStamp.taskLevel);
    EXPECT_EQ(commandStreamReceiver.flushStamp->peekStamp(), completionStamp.flushStamp);
    EXPECT_EQ(0u, completionStamp.deviceOrdinal);
    EXPECT_EQ(deviceEngineType, completionStamp.engineType);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, stateBaseAddressTracking) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    EXPECT_FALSE(commandStreamReceiver.dshState.updateAndCheck(&dsh));
    EXPECT_FALSE(commandStreamReceiver.iohState.updateAndCheck(&ioh));
    EXPECT_FALSE(commandStreamReceiver.sshState.updateAndCheck(&ssh));
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, stateBaseAddressProgrammingShouldMatchTracking) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    auto gmmHelper = pDevice->getGmmHelper();
    auto stateHeapMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);
    auto l3CacheOnMocs = gmmHelper->getMOCS(CacheSettings::l3CacheOn);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    auto &commandStreamCSR = commandStreamReceiver.commandStream;
    HardwareParse::parseCommands<FamilyType>(commandStreamCSR, 0);
    HardwareParse::findHardwareCommands<FamilyType>();

    ASSERT_NE(nullptr, cmdStateBaseAddress);
    auto &cmd = *reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStateBaseAddress);

    EXPECT_EQ(dsh.getCpuBase(), reinterpret_cast<void *>(cmd.getDynamicStateBaseAddress()));
    EXPECT_EQ(commandStreamReceiver.getMemoryManager()->getInternalHeapBaseAddress(), cmd.getInstructionBaseAddress());
    EXPECT_EQ(ioh.getCpuBase(), reinterpret_cast<void *>(cmd.getIndirectObjectBaseAddress()));
    EXPECT_EQ(ssh.getCpuBase(), reinterpret_cast<void *>(cmd.getSurfaceStateBaseAddress()));

    EXPECT_EQ(l3CacheOnMocs, cmd.getStatelessDataPortAccessMemoryObjectControlState());
    EXPECT_EQ(stateHeapMocs, cmd.getInstructionMemoryObjectControlState());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenStateBaseAddressWhenItIsRequiredThenThereIsPipeControlPriorToItWithTextureCacheFlush) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>();
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
    auto pipeControlCmd = (typename FamilyType::PIPE_CONTROL *)*pipeControlItor;
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, preambleShouldBeSentIfNeverSent) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;
    flushTask(commandStreamReceiver);

    EXPECT_TRUE(commandStreamReceiver.isPreambleSent);
    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, pipelineSelectShouldBeSentIfNeverSentPreambleAndMediaSamplerRequirementChanged) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;
    commandStreamReceiver.lastMediaSamplerConfig = -1;
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_NE(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, pipelineSelectShouldBeSentIfNeverSentPreambleAndMediaSamplerRequirementNotChanged) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;
    commandStreamReceiver.lastMediaSamplerConfig = 0;
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_NE(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, pipelineSelectShouldNotBeSentIfSentPreambleAndMediaSamplerRequirementDoesntChanged) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastMediaSamplerConfig = 0;
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_EQ(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}
HWTEST_F(CommandStreamReceiverFlushTaskTests, pipelineSelectShouldBeSentIfSentPreambleAndMediaSamplerRequirementDoesntChanged) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastMediaSamplerConfig = 1;
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_NE(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, stateBaseAddressShouldBeSentIfNeverSent) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.overrideMediaVFEStateDirty(false);
    flushTask(commandStreamReceiver);

    EXPECT_GT(commandStreamReceiver.commandStream.getUsed(), 0u);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, stateBaseAddressShouldBeSentIfSizeChanged) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto dshSize = dsh.getMaxAvailableSpace();
    auto iohSize = ioh.getMaxAvailableSpace();
    auto sshSize = ssh.getMaxAvailableSpace();

    dsh.replaceBuffer(dsh.getCpuBase(), 0);
    ioh.replaceBuffer(ioh.getCpuBase(), 0);
    ssh.replaceBuffer(ssh.getCpuBase(), 0);

    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.overrideMediaVFEStateDirty(false);

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
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>();

    dsh.replaceBuffer(nullptr, 0);
    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenSshHeapChangeWhenFlushTaskIsCalledThenSbaIsReloaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>();

    ssh.replaceBuffer(nullptr, 0);
    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenIohHeapChangeWhenFlushTaskIsCalledThenSbaIsReloaded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>();

    ioh.replaceBuffer(nullptr, 0);
    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<typename FamilyType::STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), stateBaseAddressItor);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, stateBaseAddressShouldNotBeSentIfTheSame) {
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

HWTEST_F(CommandStreamReceiverFlushTaskTests, shouldntAddAnyCommandsToCQCSIfEmpty) {
    WhitelistedRegisters forceRegs = {0};
    pDevice->setForceWhitelistedRegs(true, &forceRegs);
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    auto usedBefore = commandStream.getUsed();
    flushTask(commandStreamReceiver);

    EXPECT_EQ(usedBefore, commandStream.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, blockingflushTaskAddsPCToClient) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    auto blocking = true;
    flushTask(commandStreamReceiver, blocking);

    parseCommands<FamilyType>(commandStream, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, blockingFlushWithNoPreviousDependencies) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    taskLevel = 5;
    commandStreamReceiver.taskLevel = 6;

    auto blocking = true;
    flushTask(commandStreamReceiver, blocking);

    EXPECT_EQ(7u, commandStreamReceiver.peekTaskLevel());
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, nonblockingFlushWithNoPreviousDependencies) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    taskLevel = 5;
    commandStreamReceiver.taskLevel = 6;

    auto blocking = false;
    flushTask(commandStreamReceiver, blocking);

    EXPECT_EQ(6u, commandStreamReceiver.peekTaskLevel());
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, flushTaskWithOnlyEnoughMemoryForPreamble) {
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;

    WhitelistedRegisters forceRegs = {0};
    pDevice->setForceWhitelistedRegs(true, &forceRegs);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    // Force a PIPE_CONTROL through a taskLevel transition
    taskLevel = commandStreamReceiver.peekTaskLevel() + 1;

    commandStreamReceiver.lastSentCoherencyRequest = 0;
    auto l3Config = PreambleHelper<FamilyType>::getL3Config(pDevice->getHardwareInfo(), false);
    commandStreamReceiver.lastSentL3Config = l3Config;

    auto &csrCS = commandStreamReceiver.getCS();
    size_t sizeNeededForPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    size_t sizeNeeded = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);
    sizeNeeded -= sizeof(MI_BATCH_BUFFER_START); // no task to submit
    sizeNeeded += sizeof(MI_BATCH_BUFFER_END);   // no task to submit, add BBE to CSR stream
    sizeNeeded = alignUp(sizeNeeded, MemoryConstants::cacheLineSize);

    csrCS.getSpace(csrCS.getAvailableSpace() - sizeNeededForPreamble);

    flushTask(commandStreamReceiver);

    EXPECT_EQ(sizeNeeded, csrCS.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, flushTaskWithOnlyEnoughMemoryForPreambleAndSba) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;

    WhitelistedRegisters forceRegs = {0};
    pDevice->setForceWhitelistedRegs(true, &forceRegs);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    // Force a PIPE_CONTROL through a taskLevel transition
    taskLevel = commandStreamReceiver.peekTaskLevel() + 1;
    commandStreamReceiver.lastSentCoherencyRequest = 0;

    auto l3Config = PreambleHelper<FamilyType>::getL3Config(pDevice->getHardwareInfo(), false);
    commandStreamReceiver.lastSentL3Config = l3Config;

    auto &csrCS = commandStreamReceiver.getCS();
    size_t sizeNeededForPreamble = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    size_t sizeNeededForStateBaseAddress = sizeof(STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL);
    size_t sizeNeeded = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);
    sizeNeeded -= sizeof(MI_BATCH_BUFFER_START); // no task to submit
    sizeNeeded += sizeof(MI_BATCH_BUFFER_END);   // no task to submit, add BBE to CSR stream
    sizeNeeded = alignUp(sizeNeeded, MemoryConstants::cacheLineSize);

    csrCS.getSpace(csrCS.getAvailableSpace() - sizeNeededForPreamble - sizeNeededForStateBaseAddress);

    flushTask(commandStreamReceiver);

    EXPECT_EQ(sizeNeeded, csrCS.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, flushTaskWithOnlyEnoughMemoryForPreambleSbaAndPc) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    commandStream.getSpace(sizeof(PIPE_CONTROL));

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    // Force a PIPE_CONTROL through a taskLevel transition
    taskLevel = commandStreamReceiver.peekTaskLevel() + 1;

    commandStreamReceiver.lastSentCoherencyRequest = 0;

    auto l3Config = PreambleHelper<FamilyType>::getL3Config(pDevice->getHardwareInfo(), false);
    commandStreamReceiver.lastSentL3Config = l3Config;

    auto &csrCS = commandStreamReceiver.getCS();
    size_t sizeNeeded = commandStreamReceiver.getRequiredCmdStreamSizeAligned(flushTaskFlags, *pDevice);

    csrCS.getSpace(csrCS.getAvailableSpace() - sizeNeeded);
    auto expectedBase = csrCS.getCpuBase();

    // This case handles when we have *just* enough space
    auto expectedUsed = csrCS.getUsed() + sizeNeeded;

    flushTask(commandStreamReceiver, flushTaskFlags.blocking, 0, flushTaskFlags.requiresCoherency, flushTaskFlags.lowPriority);

    // Verify that we didn't grab a new CS buffer
    EXPECT_EQ(expectedUsed, csrCS.getUsed());
    EXPECT_EQ(expectedBase, csrCS.getCpuBase());
}

template <typename FamilyType>
struct CommandStreamReceiverHwLog : public UltCommandStreamReceiver<FamilyType> {
    CommandStreamReceiverHwLog(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment) : UltCommandStreamReceiver<FamilyType>(hwInfoIn, executionEnvironment),
                                                                                                           flushCount(0) {
    }

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) override {
        ++flushCount;
        return 0;
    }

    int flushCount;
};

HWTEST_F(CommandStreamReceiverFlushTaskTests, flushTaskWithBothCSCallsFlushOnce) {
    CommandStreamReceiverHwLog<FamilyType> commandStreamReceiver(*platformDevices[0], *pDevice->executionEnvironment);
    commandStreamReceiver.setMemoryManager(pDevice->getMemoryManager());
    commandStreamReceiver.initializeTagAllocation();
    commandStream.getSpace(sizeof(typename FamilyType::MI_NOOP));

    flushTask(commandStreamReceiver);
    EXPECT_EQ(1, commandStreamReceiver.flushCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, flushTaskWithBothCSCallsChainsWithBatchBufferStart) {
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
    EXPECT_EQ(expectedAddress, bbs->getBatchBufferStartAddressGraphicsaddress472());

    // MI_BATCH_BUFFER_START from UMD must be PPGTT for security reasons
    EXPECT_EQ(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT, bbs->getAddressSpaceIndicator());
}

typedef Test<DeviceFixture> CommandStreamReceiverCQFlushTaskTests;
HWTEST_F(CommandStreamReceiverCQFlushTaskTests, getCSShouldReturnACSWithEnoughSizeCSRTraffic) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();

    // NOTE: This test attempts to reserve the maximum amount
    // of memory such that if a client gets everything he wants
    // we don't overflow/corrupt memory when CSR appends its
    // work.
    size_t sizeCQReserves = CSRequirements::minCommandQueueCommandStreamSize;

    size_t sizeRequested = 0x1000 - sizeCQReserves;
    auto &commandStream = commandQueue.getCS(sizeRequested);
    ASSERT_GE(0x1000u, commandStream.getMaxAvailableSpace());

    EXPECT_GE(commandStream.getAvailableSpace(), sizeRequested);
    commandStream.getSpace(sizeRequested - sizeCQReserves);

    GraphicsAllocation allocation((void *)MemoryConstants::pageSize, 1);
    IndirectHeap linear(&allocation);

    auto blocking = true;
    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = blocking;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    commandStreamReceiver.flushTask(
        commandStream,
        0,
        linear,
        linear,
        linear,
        1,
        dispatchFlags,
        *pDevice);

    auto expectedSize = 0x1000u - sizeCQReserves;

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

HWTEST_F(CommandStreamReceiverFlushTaskTests, blockingFlushTaskWithOnlyPipeControl) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    // Configure the CSR to not need to submit any state or commands
    configureCSRtoNonDirtyState<FamilyType>();

    // Force a PIPE_CONTROL through a blocking flag
    auto blocking = true;
    auto &commandStreamTask = commandQueue.getCS(1024);
    auto &commandStreamCSR = commandStreamReceiver->getCS();
    commandStreamReceiver->lastSentCoherencyRequest = 0;

    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = blocking;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    commandStreamReceiver->flushTask(
        commandStreamTask,
        0,
        dsh,
        ioh,
        ssh,
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

HWTEST_F(CommandStreamReceiverFlushTaskTests, FlushTaskBlockingHasPipeControlWithDCFlush) {
    WhitelistedRegisters forceRegs = {0};
    pDevice->setForceWhitelistedRegs(true, &forceRegs);
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    configureCSRtoNonDirtyState<FamilyType>();

    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();

    size_t pipeControlCount = static_cast<CommandStreamReceiverHw<FamilyType> &>(commandStreamReceiver).getRequiredPipeControlSize() / sizeof(PIPE_CONTROL);

    auto &commandStreamTask = commandQueue.getCS(1024);

    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = true;
    dispatchFlags.dcFlush = true;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    commandStreamReceiver.flushTask(
        commandStreamTask,
        0,
        dsh,
        ioh,
        ssh,
        taskLevel,
        dispatchFlags,
        *pDevice);

    parseCommands<FamilyType>(commandStreamTask, 0);
    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);

    if (::renderCoreFamily != IGFX_GEN8_CORE) {
        // Verify that the dcFlushEnabled bit is set in PC
        auto pCmdWA = reinterpret_cast<PIPE_CONTROL *>(*itorPC);
        EXPECT_EQ(true, pCmdWA->getDcFlushEnable());

        if (pipeControlCount > 1) {
            // Search taskCS for PC to analyze
            auto pipeControlTask = genCmdCast<typename FamilyType::PIPE_CONTROL *>(
                ptrOffset(commandStreamTask.getCpuBase(), 24));
            ASSERT_NE(nullptr, pipeControlTask);

            // Verify that the dcFlushEnabled bit is not set in PC
            auto pCmd = reinterpret_cast<PIPE_CONTROL *>(pipeControlTask);
            EXPECT_EQ(false, pCmd->getDcFlushEnable());
        }
    } else {
        // Verify that the dcFlushEnabled bit is not set in PC
        auto pCmd = reinterpret_cast<PIPE_CONTROL *>(*itorPC);
        EXPECT_EQ(true, pCmd->getDcFlushEnable());
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenBlockedKernelRequiringDCFlushWhenUnblockedThenDCFlushIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    cl_event blockingEvent;
    MockEvent<UserEvent> mockEvent(&ctx);
    blockingEvent = &mockEvent;
    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {0, 1, 2};
    cl_int retVal = 0;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);

    auto &commandStreamCSR = commandStreamReceiver.getCS();
    auto &commandStreamTask = commandQueue.getCS(1024);

    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, 1, &blockingEvent, 0);

    // Expect nothing was sent
    EXPECT_EQ(0u, commandStreamCSR.getUsed());

    // Unblock Event
    mockEvent.setStatus(CL_COMPLETE);

    cmdList.clear();
    // Parse command list
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);

    // Verify that the dcFlushEnabled bit is set in PC
    auto pCmdWA = reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_EQ(true, pCmdWA->getDcFlushEnable());

    buffer->release();
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenBlockedKernelNotRequiringDCFlushWhenUnblockedThenDCFlushIsNotAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    cl_event blockingEvent;
    MockEvent<UserEvent> mockEvent(&ctx);
    blockingEvent = &mockEvent;
    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {0, 1, 2};
    cl_int retVal = 0;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);

    auto &commandStreamCSR = commandStreamReceiver.getCS();
    auto &commandStreamTask = commandQueue.getCS(1024);

    commandQueue.enqueueWriteBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, 1, &blockingEvent, 0);

    // Expect nothing was sent
    EXPECT_EQ(0u, commandStreamCSR.getUsed());

    // Unblock Event
    mockEvent.setStatus(CL_COMPLETE);

    cmdList.clear();
    // Parse command list
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);

    // Verify that the dcFlushEnabled bit is set in PC
    auto pCmdWA = reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pCmdWA->getDcFlushEnable());

    buffer->release();
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, FlushTaskWithTaskCSPassedAsCommandStreamParam) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &commandStreamTask = commandQueue.getCS(1024);
    auto deviceEngineType = pDevice->getEngineType();

    DispatchFlags dispatchFlags;

    // Pass taskCS as command stream parameter
    auto cs = commandStreamReceiver.flushTask(
        commandStreamTask,
        0,
        dsh,
        ioh,
        ssh,
        taskLevel,
        dispatchFlags,
        *pDevice);

    // Verify that flushTask returned a valid completion stamp
    EXPECT_EQ(commandStreamReceiver.peekTaskCount(), cs.taskCount);
    EXPECT_EQ(commandStreamReceiver.peekTaskLevel(), cs.taskLevel);
    EXPECT_EQ(0u, cs.deviceOrdinal);
    EXPECT_EQ(deviceEngineType, cs.engineType);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, TrackSentTagsWhenEmptyQueue) {
    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    uint32_t taskCount = 0;
    taskLevel = taskCount;
    commandQueue.taskCount = taskCount;
    commandQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());
    commandQueue.finish(false);
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());
    commandQueue.finish(true);
    //nothings sent to the HW, no need to bump tags
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(0u, commandQueue.latestTaskCountWaited);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, TrackSentTagsWhenNonDcFlushWithInitialTaskCountZero) {
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t GWS = 1;

    uint32_t taskCount = 0;
    taskLevel = taskCount;
    commandQueue.taskCount = taskCount;
    commandQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    // finish after enqueued kernel(cmdq task count = 1)
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    commandQueue.finish(false);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, commandQueue.latestTaskCountWaited);
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());

    // finish again - dont call flush task
    commandQueue.finish(false);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, commandQueue.latestTaskCountWaited);
    EXPECT_EQ(1u, commandStreamReceiver.peekTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, TrackSentTagsWhenDcFlush) {
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t GWS = 1;
    size_t tempBuffer[] = {0, 1, 2};
    cl_int retVal;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);

    uint32_t taskCount = 0;
    taskLevel = taskCount;
    commandQueue.taskCount = taskCount;
    commandQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    // finish(dcFlush=true) from blocking MapBuffer after enqueued kernel
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    auto ptr = commandQueue.enqueueMapBuffer(buffer, CL_TRUE, CL_MAP_READ, 0, sizeof(tempBuffer), 0, nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    // cmdQ task count = 2, finish again
    commandQueue.finish(false);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, commandQueue.latestTaskCountWaited);

    // finish again - dont flush task again
    commandQueue.finish(false);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());
    EXPECT_EQ(1u, commandQueue.latestTaskCountWaited);

    // finish(dcFlush=true) from MapBuffer again - dont call FinishTask n finished queue
    retVal = commandQueue.enqueueUnmapMemObject(buffer, ptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);
    ptr = commandQueue.enqueueMapBuffer(buffer, CL_TRUE, CL_MAP_READ, 0, sizeof(tempBuffer), 0, nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    //cleanup
    retVal = commandQueue.enqueueUnmapMemObject(buffer, ptr, 0, nullptr, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(retVal, CL_SUCCESS);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenPowerOfTwoGlobalWorkSizeAndNullLocalWorkgroupSizeWhenEnqueueKernelIsCalledThenGpGpuWalkerHasOptimalSIMDmask) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    size_t GWS = 1024;
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    auto &commandStreamTask = commandQueue.getCS(1024);
    parseCommands<FamilyType>(commandStreamTask, 0);
    auto itorCmd = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorCmd, cmdList.end());
    auto cmdGpGpuWalker = genCmdCast<GPGPU_WALKER *>(*itorCmd);

    //execution masks should be all active
    EXPECT_EQ(0xffffffffu, cmdGpGpuWalker->getBottomExecutionMask());
    EXPECT_EQ(0xffffffffu, cmdGpGpuWalker->getRightExecutionMask());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, TrackSentTagsWhenEventIsQueried) {
    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    cl_event event = nullptr;
    Event *pEvent;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {5, 5, 5};

    cl_int retVal;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);

    uint32_t taskCount = 0;
    taskLevel = taskCount;
    commandQueue.taskCount = taskCount;
    commandQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, 0, 0, &event);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    pEvent = (Event *)event;

    retVal = Event::waitForEvents(1, &event);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    retVal = clReleaseEvent(pEvent);
    retVal = clReleaseMemObject(buffer);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenNonBlockingMapWhenFinishIsCalledThenNothingIsSubmittedToTheHardware) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    size_t tempBuffer[] = {0, 1, 2};
    cl_int retVal;

    AlignedBuffer mockBuffer;

    uint32_t taskCount = 0;
    taskLevel = taskCount;
    commandQueue.taskCount = taskCount;
    commandQueue.taskLevel = taskCount;
    commandStreamReceiver.taskLevel = taskCount;
    commandStreamReceiver.taskCount = taskCount;
    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    auto ptr = commandQueue.enqueueMapBuffer(&mockBuffer, CL_FALSE, CL_MAP_READ, 0, sizeof(tempBuffer), 0, nullptr, nullptr, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(nullptr, ptr);

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    commandQueue.finish(false);

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    auto &commandStreamTask = commandQueue.getCS(1024);
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests,
            GivenFlushedCallRequiringDCFlushWhenBlockingEnqueueIsCalledThenPipeControlWithDCFlushIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;

    MockContext ctx(pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    cl_event event = nullptr;

    auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
    auto &commandStreamTask = commandQueue.getCS(1024);

    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {5, 5, 5};

    cl_int retVal;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(0u, commandStreamReceiver.peekLatestSentTaskCount());

    // Call requiring DCFlush, nonblocking
    buffer->forceDisallowCPUCopy = true;
    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, 0, 0, 0);

    EXPECT_EQ(1u, commandStreamReceiver.peekLatestSentTaskCount());

    commandQueue.enqueueReadBuffer(buffer, CL_TRUE, 0, sizeof(tempBuffer), dstBuffer, 0, 0, &event);

    EXPECT_EQ(2u, commandStreamReceiver.peekLatestSentTaskCount());

    EXPECT_EQ(2u, commandQueue.latestTaskCountWaited);

    // Parse command list to verify that PC was added to taskCS
    cmdList.clear();
    parseCommands<FamilyType>(commandStreamTask, 0);

    auto itorWalker = find<typename FamilyType::WALKER_TYPE *>(cmdList.begin(), cmdList.end());

    auto itorCmd = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    ASSERT_NE(cmdList.end(), itorCmd);

    auto cmdPC = genCmdCast<PIPE_CONTROL *>(*itorCmd);
    ASSERT_NE(nullptr, cmdPC);

    if (::renderCoreFamily != IGFX_GEN8_CORE) {
        // SKL+: two PIPE_CONTROLs following GPGPU_WALKER: first has DcFlush and second has Write HwTag
        EXPECT_TRUE(cmdPC->getDcFlushEnable());
        auto itorCmdP = ++((GenCmdList::iterator)itorCmd);
        EXPECT_NE(cmdList.end(), itorCmdP);
        auto itorCmd2 = find<PIPE_CONTROL *>(itorCmdP, cmdList.end());
        cmdPC = (PIPE_CONTROL *)*itorCmd2;
        EXPECT_FALSE(cmdPC->getDcFlushEnable());
    } else {
        // BDW: single PIPE_CONTROL following GPGPU_WALKER has DcFlush and Write HwTag
        EXPECT_TRUE(cmdPC->getDcFlushEnable());
    }

    retVal = clReleaseEvent(event);
    retVal = clReleaseMemObject(buffer);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDefaultCommandStreamReceiverThenRoundRobinPolicyIsSelected) {
    MockCsrHw<FamilyType> commandStreamReceiver(*platformDevices[0], *pDevice->executionEnvironment);
    EXPECT_EQ(PreambleHelper<FamilyType>::getDefaultThreadArbitrationPolicy(), commandStreamReceiver.peekThreadArbitrationPolicy());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, GivenKernelWithSlmWhenPreviousSLML3WasSentThenDontProgramL3) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t GWS = 1;
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    auto &commandStreamCSR = commandStreamReceiver->getCS();

    uint32_t L3Config = PreambleHelper<FamilyType>::getL3Config(*platformDevices[0], true);

    // Mark Pramble as sent, override L3Config to SLM config
    commandStreamReceiver->isPreambleSent = true;
    commandStreamReceiver->lastSentL3Config = L3Config;
    commandStreamReceiver->lastSentThreadArbitrationPolicy = kernel.mockKernel->getThreadArbitrationPolicy<FamilyType>();

    ((MockKernel *)kernel)->setTotalSLMSize(1024);

    cmdList.clear();
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    // Parse command list to verify that PC was added to taskCS
    parseCommands<FamilyType>(commandStreamCSR, 0);

    auto itorCmd = findMmio<FamilyType>(cmdList.begin(), cmdList.end(), L3CNTLRegisterOffset<FamilyType>::registerOffset);
    EXPECT_EQ(cmdList.end(), itorCmd);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, CreateCommandStreamReceiverHw) {
    const HardwareInfo hwInfo = *platformDevices[0];
    auto csrHw = CommandStreamReceiverHw<FamilyType>::create(hwInfo, executionEnvironment);
    EXPECT_NE(nullptr, csrHw);

    MemoryManager *mm = csrHw->createMemoryManager(false, false);
    EXPECT_EQ(nullptr, mm);
    GmmPageTableMngr *ptm = csrHw->createPageTableManager();
    EXPECT_EQ(nullptr, ptm);
    delete csrHw;

    DebugManager.flags.SetCommandStreamReceiver.set(0);
    int32_t GetCsr = DebugManager.flags.SetCommandStreamReceiver.get();
    EXPECT_EQ(0, GetCsr);

    ExecutionEnvironment executionEnvironment;
    auto csr = OCLRT::createCommandStream(&hwInfo, executionEnvironment);
    EXPECT_NE(nullptr, csr);
    delete csr;
    DebugManager.flags.SetCommandStreamReceiver.set(0);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, handleTagAndScratchAllocationsResidencyOnEachFlush) {
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    commandStreamReceiver->setRequiredScratchSize(1024); // whatever > 0

    flushTask(*commandStreamReceiver);

    auto tagAllocation = commandStreamReceiver->getTagAllocation();
    auto scratchAllocation = commandStreamReceiver->getScratchAllocation();
    ASSERT_NE(tagAllocation, nullptr);
    ASSERT_NE(scratchAllocation, nullptr);

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeResident(scratchAllocation));

    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(scratchAllocation));

    // call makeResident on tag and scratch allocations per each flush
    // DONT skip residency calls when scratch allocation is the same(new required size <= previous size)
    commandStreamReceiver->madeResidentGfxAllocations.clear(); // this is only history - we can clean this
    commandStreamReceiver->madeNonResidentGfxAllocations.clear();

    flushTask(*commandStreamReceiver); // 2nd flush

    auto NewScratchAllocation = commandStreamReceiver->getScratchAllocation();
    EXPECT_EQ(scratchAllocation, NewScratchAllocation); // Allocation unchanged. Dont skip residency handling

    EXPECT_TRUE(commandStreamReceiver->isMadeResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeResident(scratchAllocation));

    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(tagAllocation));
    EXPECT_TRUE(commandStreamReceiver->isMadeNonResident(scratchAllocation));
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenTwoConsecutiveNDRangeKernelsStateBaseAddressIsProgrammedOnceAndScratchAddressInMediaVFEStateIsProgrammedTwiceBothWithCorrectAddress) {
    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename PARSE::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    size_t GWS = 1;
    uint32_t scratchSize = 1024;
    SPatchMediaVFEState mediaVFEstate;

    mediaVFEstate.PerThreadScratchSpace = scratchSize;
    kernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

    EXPECT_EQ(false, kernel.mockKernel->isBuiltIn);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_FALSE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_TRUE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    cmdList.clear();
    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    // All state should be programmed before walker
    auto itorCmdForStateBase = itorStateBaseAddress;
    auto *mediaVfeState = (MEDIA_VFE_STATE *)*itorMediaVfeState;

    auto graphicsAllocationScratch = commandStreamReceiver->getScratchAllocation();

    ASSERT_NE(itorCmdForStateBase, itorWalker);
    auto *sba = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;
    auto GSHaddress = (uintptr_t)sba->getGeneralStateBaseAddress();
    uint64_t graphicsAddress = 0;

    // Get address ( offset in 32 bit addressing ) of sratch
    graphicsAddress = (uint64_t)graphicsAllocationScratch->getGpuAddressToPatch();

    if (pDevice->getDeviceInfo().force32BitAddressess == true && is64bit) {
        EXPECT_TRUE(graphicsAllocationScratch->is32BitAllocation);
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getGpuAddress() - GSHaddress, graphicsAddress);
    } else {
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getUnderlyingBuffer(), graphicsAddress);
    }

    uint64_t lowPartGraphicsAddress = (uint64_t)(graphicsAddress & 0xffffffff);
    uint64_t highPartGraphicsAddress = (uint64_t)((graphicsAddress >> 32) & 0xffffffff);

    uint64_t scratchBaseLowPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointer();
    uint64_t scratchBaseHighPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointerHigh();

    if (is64bit && !pDevice->getDeviceInfo().force32BitAddressess) {
        uint64_t expectedAddress = PreambleHelper<FamilyType>::getScratchSpaceOffsetFor64bit();
        EXPECT_EQ(expectedAddress, scratchBaseLowPart);
        EXPECT_EQ(0u, scratchBaseHighPart);
    } else {
        EXPECT_EQ(lowPartGraphicsAddress, scratchBaseLowPart);
        EXPECT_EQ(highPartGraphicsAddress, scratchBaseHighPart);
    }

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_EQ(pDevice->getMemoryManager()->allocator32Bit->getBase(), GSHaddress);
    } else {
        if (is64bit) {
            EXPECT_EQ(graphicsAddress - PreambleHelper<FamilyType>::getScratchSpaceOffsetFor64bit(), GSHaddress);
        } else {
            EXPECT_EQ(0u, GSHaddress);
        }
    }

    //now re-try to see if SBA is not programmed
    scratchSize *= 2;
    mediaVFEstate.PerThreadScratchSpace = scratchSize;

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorWalker, cmdList.end());

    // In 32 Bit addressing sba shouldn't be reprogrammed
    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_EQ(itorCmdForStateBase, cmdList.end());
    }

    auto itorMediaVfeStateSecond = find<MEDIA_VFE_STATE *>(itorWalker, cmdList.end());
    auto *cmdMediaVfeStateSecond = (MEDIA_VFE_STATE *)*itorMediaVfeStateSecond;

    EXPECT_NE(mediaVfeState, cmdMediaVfeStateSecond);

    uint64_t oldScratchAddr = ((uint64_t)scratchBaseHighPart << 32u) | scratchBaseLowPart;
    uint64_t newScratchAddr = ((uint64_t)cmdMediaVfeStateSecond->getScratchSpaceBasePointerHigh() << 32u) | cmdMediaVfeStateSecond->getScratchSpaceBasePointer();

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_NE(oldScratchAddr, newScratchAddr);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenNDRangeKernelAndReadBufferStateBaseAddressAndScratchAddressInMediaVFEStateIsProgrammedForNDRangeAndReprogramedForReadBufferAndGSBAFlagIsResetToFalse) {

    typedef typename FamilyType::PARSE PARSE;
    typedef typename PARSE::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename PARSE::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pDevice, 0);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    size_t GWS = 1;
    uint32_t scratchSize = 1024;
    SPatchMediaVFEState mediaVFEstate;

    mediaVFEstate.PerThreadScratchSpace = scratchSize;
    kernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

    EXPECT_EQ(false, kernel.mockKernel->isBuiltIn);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_FALSE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_TRUE(commandStreamReceiver->getGSBAFor32BitProgrammed());
    }

    cmdList.clear();
    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    // All state should be programmed before walker
    auto itorCmdForStateBase = itorStateBaseAddress;
    auto *mediaVfeState = (MEDIA_VFE_STATE *)*itorMediaVfeState;

    auto graphicsAllocationScratch = commandStreamReceiver->getScratchAllocation();

    ASSERT_NE(itorCmdForStateBase, itorWalker);
    auto *sba = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;
    auto GSHaddress = (uintptr_t)sba->getGeneralStateBaseAddress();
    uint64_t graphicsAddress = 0;

    // Get address ( offset in 32 bit addressing ) of sratch
    graphicsAddress = (uint64_t)graphicsAllocationScratch->getGpuAddressToPatch();

    if (pDevice->getDeviceInfo().force32BitAddressess == true && is64bit) {
        EXPECT_TRUE(graphicsAllocationScratch->is32BitAllocation);
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getGpuAddress() - GSHaddress, graphicsAddress);
    } else {
        EXPECT_EQ((uint64_t)graphicsAllocationScratch->getUnderlyingBuffer(), graphicsAddress);
    }

    uint64_t lowPartGraphicsAddress = (uint64_t)(graphicsAddress & 0xffffffff);
    uint64_t highPartGraphicsAddress = (uint64_t)((graphicsAddress >> 32) & 0xffffffff);

    uint64_t scratchBaseLowPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointer();
    uint64_t scratchBaseHighPart = (uint64_t)mediaVfeState->getScratchSpaceBasePointerHigh();

    if (is64bit && !pDevice->getDeviceInfo().force32BitAddressess) {
        lowPartGraphicsAddress = PreambleHelper<FamilyType>::getScratchSpaceOffsetFor64bit();
        highPartGraphicsAddress = 0u;
    }

    EXPECT_EQ(lowPartGraphicsAddress, scratchBaseLowPart);
    EXPECT_EQ(highPartGraphicsAddress, scratchBaseHighPart);

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_EQ(pDevice->getMemoryManager()->allocator32Bit->getBase(), GSHaddress);
    } else {
        if (is64bit) {
            EXPECT_EQ(graphicsAddress - PreambleHelper<FamilyType>::getScratchSpaceOffsetFor64bit(), GSHaddress);
        } else {
            EXPECT_EQ(0u, GSHaddress);
        }
    }

    size_t tempBuffer[] = {0, 1, 2};
    size_t dstBuffer[] = {0, 0, 0};
    cl_int retVal = 0;

    auto buffer = Buffer::create(&ctx, CL_MEM_USE_HOST_PTR, sizeof(tempBuffer), tempBuffer, retVal);

    commandQueue.enqueueReadBuffer(buffer, CL_FALSE, 0, sizeof(tempBuffer), dstBuffer, 0, 0, 0);

    // Parse command list
    parseCommands<FamilyType>(commandQueue);

    itorCmdForStateBase = find<STATE_BASE_ADDRESS *>(itorWalker, cmdList.end());

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        EXPECT_NE(itorWalker, itorCmdForStateBase);

        if (itorCmdForStateBase != cmdList.end()) {

            auto *sba2 = (STATE_BASE_ADDRESS *)*itorCmdForStateBase;
            auto GSHaddress2 = (uintptr_t)sba2->getGeneralStateBaseAddress();

            EXPECT_NE(sba, sba2);
            EXPECT_EQ(0u, GSHaddress2);

            if (pDevice->getDeviceInfo().force32BitAddressess == true) {
                EXPECT_FALSE(commandStreamReceiver->getGSBAFor32BitProgrammed());
            }
        }
    }
    delete buffer;

    if (pDevice->getDeviceInfo().force32BitAddressess == true) {
        // Asserts placed after restoring old CSR to avoid heap corruption
        ASSERT_NE(itorCmdForStateBase, cmdList.end());
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, InForced32BitAllocationsModeDoNotStore32bitScratchAllocationOnReusableAllocationList) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.Force32bitAddressing.set(true);

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);

    pDevice->getMemoryManager()->setForce32BitAllocations(true);

    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    commandStreamReceiver->setRequiredScratchSize(4096); // whatever > 0 (in page size)
    flushTask(*commandStreamReceiver);

    auto scratchAllocation = commandStreamReceiver->getScratchAllocation();
    ASSERT_NE(scratchAllocation, nullptr);

    commandStreamReceiver->setRequiredScratchSize(8196); // whatever > first size

    flushTask(*commandStreamReceiver); // 2nd flush

    auto newScratchAllocation = commandStreamReceiver->getScratchAllocation();
    EXPECT_NE(scratchAllocation, newScratchAllocation); // Allocation changed

    std::unique_ptr<GraphicsAllocation> allocationReusable = pDevice->getMemoryManager()->obtainReusableAllocation(4096, false);

    if (allocationReusable.get() != nullptr) {
        if (is64bit) {
            EXPECT_NE(scratchAllocation, allocationReusable.get());
        }
        pDevice->getMemoryManager()->freeGraphicsMemory(allocationReusable.release());
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, InForced32BitAllocationsModeStore32bitScratchAllocationOnTemporaryAllocationList) {
    if (is64bit) {
        DebugManagerStateRestore dbgRestorer;
        DebugManager.flags.Force32bitAddressing.set(true);

        auto commandStreamReceiver = new MockCsrHw<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);

        pDevice->getMemoryManager()->setForce32BitAllocations(true);

        pDevice->resetCommandStreamReceiver(commandStreamReceiver);

        commandStreamReceiver->setRequiredScratchSize(4096); // whatever > 0 (in page size)
        flushTask(*commandStreamReceiver);

        auto scratchAllocation = commandStreamReceiver->getScratchAllocation();
        ASSERT_NE(scratchAllocation, nullptr);

        commandStreamReceiver->setRequiredScratchSize(8196); // whatever > first size

        flushTask(*commandStreamReceiver); // 2nd flush

        auto newScratchAllocation = commandStreamReceiver->getScratchAllocation();
        EXPECT_NE(scratchAllocation, newScratchAllocation); // Allocation changed

        std::unique_ptr<GraphicsAllocation> allocationTemporary = pDevice->getMemoryManager()->graphicsAllocations.detachAllocation(0, nullptr, true);

        EXPECT_EQ(scratchAllocation, allocationTemporary.get());
        pDevice->getMemoryManager()->freeGraphicsMemory(allocationTemporary.release());
    }
}

TEST(CacheSettings, GivenCacheSettingWhenCheckedForValuesThenProperValuesAreSelected) {
    EXPECT_EQ(static_cast<uint32_t>(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), CacheSettings::l3CacheOff);
    EXPECT_EQ(static_cast<uint32_t>(GMM_RESOURCE_USAGE_OCL_BUFFER), CacheSettings::l3CacheOn);
}

HWTEST_F(UltCommandStreamReceiverTest, addPipeControlWithFlushAllCaches) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.FlushAllCaches.set(true);

    auto &csr = pDevice->getCommandStreamReceiver();

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    csr.addPipeControl(stream, false);

    parseCommands<FamilyType>(stream, 0);

    PIPE_CONTROL *pipeControl = getCommand<PIPE_CONTROL>();

    ASSERT_NE(nullptr, pipeControl);

    // WA pipeControl added
    if (cmdList.size() == 2) {
        pipeControl++;
    }

    EXPECT_TRUE(pipeControl->getDcFlushEnable());
    EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pipeControl->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getPipeControlFlushEnable());
    EXPECT_TRUE(pipeControl->getVfCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getStateCacheInvalidationEnable());
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, givenEnabledPreemptionWhenFlushTaskCalledThenDontProgramMediaVfeStateAgain) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    HardwareParse hwParser;

    flushTask(csr, false, 0);
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);

    auto cmd = hwParser.getCommand<typename FamilyType::MEDIA_VFE_STATE>();
    EXPECT_NE(nullptr, cmd);

    // program again
    csr.overrideMediaVFEStateDirty(false);
    auto offset = csr.commandStream.getUsed();
    flushTask(csr, false, commandStream.getUsed());
    hwParser.cmdList.clear();
    hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

    cmd = hwParser.getCommand<typename FamilyType::MEDIA_VFE_STATE>();
    EXPECT_EQ(nullptr, cmd);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverFlushTaskTests, flushTaskWithPCWhenPreambleSentAndL3ConfigChanged) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    CsrSizeRequestFlags csrSizeRequest = {};

    commandStream.getSpace(sizeof(PIPE_CONTROL));
    flushTaskFlags.useSLM = true;
    flushTaskFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    // Force a PIPE_CONTROL through a taskLevel transition
    taskLevel = commandStreamReceiver.peekTaskLevel() + 1;
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastPreemptionMode = pDevice->getPreemptionMode();
    commandStreamReceiver.lastMediaSamplerConfig = 0;
    commandStreamReceiver.lastSentCoherencyRequest = false;
    commandStreamReceiver.lastSentThreadArbitrationPolicy = commandStreamReceiver.requiredThreadArbitrationPolicy;
    csrSizeRequest.l3ConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);

    auto &csrCS = commandStreamReceiver.getCS();
    size_t sizeNeeded = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    auto expectedUsed = csrCS.getUsed() + sizeNeeded;
    expectedUsed = alignUp(expectedUsed, MemoryConstants::cacheLineSize);

    commandStreamReceiver.flushTask(commandStream, 0, dsh, ioh, ssh, taskLevel, flushTaskFlags, *pDevice);

    // Verify that we didn't grab a new CS buffer
    EXPECT_EQ(expectedUsed, csrCS.getUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenPreambleSentThenRequiredCsrSizeDependsOnL3ConfigChanged) {
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    commandStreamReceiver.isPreambleSent = true;

    csrSizeRequest.l3ConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto l3ConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);
    auto expectedDifference = commandStreamReceiver.getCmdSizeForL3Config();

    csrSizeRequest.l3ConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto l3ConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    auto difference = l3ConfigChangedSize - l3ConfigNotChangedSize;
    EXPECT_EQ(expectedDifference, difference);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenPreambleNotSentThenRequiredCsrSizeDoesntDependOnL3ConfigChanged) {
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    commandStreamReceiver.isPreambleSent = false;

    csrSizeRequest.l3ConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto l3ConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    csrSizeRequest.l3ConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto l3ConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flushTaskFlags, *pDevice);

    EXPECT_EQ(l3ConfigNotChangedSize, l3ConfigChangedSize);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenPreambleNotSentThenRequiredCsrSizeDoesntDependOnmediaSamplerConfigChanged) {
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags;
    commandStreamReceiver.isPreambleSent = false;

    csrSizeRequest.mediaSamplerConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    csrSizeRequest.mediaSamplerConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    EXPECT_EQ(mediaSamplerConfigChangedSize, mediaSamplerConfigNotChangedSize);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenPreambleSentThenRequiredCsrSizeDependsOnmediaSamplerConfigChanged) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags;
    commandStreamReceiver.isPreambleSent = true;

    csrSizeRequest.mediaSamplerConfigChanged = false;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigNotChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    csrSizeRequest.mediaSamplerConfigChanged = true;
    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    auto mediaSamplerConfigChangedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    EXPECT_NE(mediaSamplerConfigChangedSize, mediaSamplerConfigNotChangedSize);
    auto difference = mediaSamplerConfigChangedSize - mediaSamplerConfigNotChangedSize;
    EXPECT_EQ(sizeof(PIPELINE_SELECT), difference);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenSamplerCacheFlushSentThenRequiredCsrSizeContainsPipecontrolSize) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    UltCommandStreamReceiver<FamilyType> &commandStreamReceiver = (UltCommandStreamReceiver<FamilyType> &)pDevice->getCommandStreamReceiver();
    CsrSizeRequestFlags csrSizeRequest = {};
    DispatchFlags flags;
    commandStreamReceiver.isPreambleSent = true;

    commandStreamReceiver.overrideCsrSizeReqFlags(csrSizeRequest);
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired);
    auto samplerCacheNotFlushedSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
    auto samplerCacheFlushBeforeSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);
    EXPECT_EQ(samplerCacheNotFlushedSize, samplerCacheFlushBeforeSize);

    OCLRT::WorkaroundTable *waTable = const_cast<WorkaroundTable *>(pDevice->getWaTable());
    bool tmp = waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads;
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    samplerCacheFlushBeforeSize = commandStreamReceiver.getRequiredCmdStreamSize(flags, *pDevice);

    auto difference = samplerCacheFlushBeforeSize - samplerCacheNotFlushedSize;
    EXPECT_EQ(sizeof(typename FamilyType::PIPE_CONTROL), difference);
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = tmp;
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInNonDirtyStateWhenflushTaskIsCalledThenNoFlushIsCalled) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    configureCSRtoNonDirtyState<FamilyType>();

    DispatchFlags dispatchFlags;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInNonDirtyStateAndBatchingModeWhenflushTaskIsCalledWithDisabledPreemptionThenSubmissionIsNotRecorded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>();

    DispatchFlags dispatchFlags;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0, mockCsr->flushCalledCount);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    //surfaces are non resident
    auto &surfacesForResidency = mockCsr->getResidencyAllocations();
    EXPECT_EQ(0u, surfacesForResidency.size());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenFlushTaskIsCalledThenNothingIsSubmittedToTheHwAndSubmissionIsRecorded) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());

    EXPECT_EQ(cmdBufferList.peekHead(), cmdBufferList.peekTail());
    auto cmdBuffer = cmdBufferList.peekHead();
    //two more because of preemption allocation and sipKernel in Mid Thread preemption mode
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    //we should have 3 heaps, tag allocation and csr command stream + cq
    EXPECT_EQ(5u + csrSurfaceCount, cmdBuffer->surfaces.size());

    EXPECT_EQ(0, mockCsr->flushCalledCount);

    //we should be submitting via csr
    EXPECT_EQ(cmdBuffer->batchBuffer.commandBufferAllocation, mockCsr->commandStream.getGraphicsAllocation());
    EXPECT_EQ(cmdBuffer->batchBuffer.startOffset, 0u);
    EXPECT_FALSE(cmdBuffer->batchBuffer.requiresCoherency);
    EXPECT_FALSE(cmdBuffer->batchBuffer.low_priority);

    //find BB END
    parseCommands<FamilyType>(commandStream, 0);

    auto itBBend = find<MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    void *bbEndAddress = *itBBend;

    EXPECT_EQ(bbEndAddress, cmdBuffer->batchBufferEndLocation);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeAndTwoRecordedCommandBuffersWhenFlushTaskIsCalledThenBatchBuffersAreCombined) {

    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto primaryBatch = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto secondBatchBuffer = primaryBatch->next;

    auto bbEndLocation = primaryBatch->batchBufferEndLocation;
    auto secondBatchBufferAddress = (uint64_t)ptrOffset(secondBatchBuffer->batchBuffer.commandBufferAllocation->getGpuAddress(),
                                                        secondBatchBuffer->batchBuffer.startOffset);

    mockCsr->flushBatchedSubmissions();

    auto batchBufferStart = genCmdCast<MI_BATCH_BUFFER_START *>(bbEndLocation);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(secondBatchBufferAddress, batchBufferStart->getBatchBufferStartAddressGraphicsaddress472());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeAndThreeRecordedCommandBuffersWhenFlushTaskIsCalledThenBatchBuffersAreCombined) {

    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto primaryBatch = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto lastBatchBuffer = primaryBatch->next->next;

    auto bbEndLocation = primaryBatch->next->batchBufferEndLocation;
    auto lastBatchBufferAddress = (uint64_t)ptrOffset(lastBatchBuffer->batchBuffer.commandBufferAllocation->getGpuAddress(),
                                                      lastBatchBuffer->batchBuffer.startOffset);

    mockCsr->flushBatchedSubmissions();

    auto batchBufferStart = genCmdCast<MI_BATCH_BUFFER_START *>(bbEndLocation);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(lastBatchBufferAddress, batchBufferStart->getBatchBufferStartAddressGraphicsaddress472());
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeAndThreeRecordedCommandBuffersThatUsesAllResourceWhenFlushTaskIsCalledThenBatchBuffersAreNotCombined) {

    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;

    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    auto memorySize = (size_t)pDevice->getDeviceInfo().globalMemSize;
    GraphicsAllocation largeAllocation(nullptr, memorySize);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->makeResident(largeAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    mockCsr->makeResident(largeAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto primaryBatch = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();

    auto bbEndLocation = primaryBatch->next->batchBufferEndLocation;

    mockCsr->flushBatchedSubmissions();

    auto batchBufferStart = genCmdCast<MI_BATCH_BUFFER_START *>(bbEndLocation);
    ASSERT_EQ(nullptr, batchBufferStart);
    auto bbEnd = genCmdCast<MI_BATCH_BUFFER_END *>(bbEndLocation);
    EXPECT_NE(nullptr, bbEnd);

    EXPECT_EQ(3, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenFlushTaskIsCalledTwiceThenNothingIsSubmittedToTheHwAndTwoSubmissionAreRecorded) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto initialBase = commandStream.getCpuBase();
    auto initialUsed = commandStream.getUsed();

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    //ensure command stream still used
    EXPECT_EQ(initialBase, commandStream.getCpuBase());
    auto baseAfterFirstFlushTask = commandStream.getCpuBase();
    auto usedAfterFirstFlushTask = commandStream.getUsed();

    dispatchFlags.requiresCoherency = true;
    dispatchFlags.lowPriority = true;

    mockCsr->flushTask(commandStream,
                       commandStream.getUsed(),
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto baseAfterSecondFlushTask = commandStream.getCpuBase();
    auto usedAfterSecondFlushTask = commandStream.getUsed();
    EXPECT_EQ(initialBase, commandStream.getCpuBase());

    EXPECT_EQ(baseAfterSecondFlushTask, baseAfterFirstFlushTask);
    EXPECT_EQ(baseAfterFirstFlushTask, initialBase);

    EXPECT_GT(usedAfterFirstFlushTask, initialUsed);
    EXPECT_GT(usedAfterSecondFlushTask, usedAfterFirstFlushTask);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());

    EXPECT_NE(cmdBufferList.peekHead(), cmdBufferList.peekTail());
    EXPECT_NE(nullptr, cmdBufferList.peekTail());
    EXPECT_NE(nullptr, cmdBufferList.peekHead());

    auto cmdBuffer1 = cmdBufferList.peekHead();
    auto cmdBuffer2 = cmdBufferList.peekTail();

    EXPECT_GT(cmdBuffer2->batchBufferEndLocation, cmdBuffer1->batchBufferEndLocation);

    EXPECT_FALSE(cmdBuffer1->batchBuffer.requiresCoherency);
    EXPECT_TRUE(cmdBuffer2->batchBuffer.requiresCoherency);

    EXPECT_FALSE(cmdBuffer1->batchBuffer.low_priority);
    EXPECT_TRUE(cmdBuffer2->batchBuffer.low_priority);

    EXPECT_GT(cmdBuffer2->batchBuffer.startOffset, cmdBuffer1->batchBuffer.startOffset);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenRecordedBatchBufferIsBeingSubmittedThenFlushIsCalledWithRecordedCommandBuffer) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>();
    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.requiresCoherency = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    mockCsr->lastSentCoherencyRequest = 1;

    commandStream.getSpace(4);

    mockCsr->flushTask(commandStream,
                       4,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0, mockCsr->flushCalledCount);

    auto &surfacesForResidency = mockCsr->getResidencyAllocations();
    EXPECT_EQ(0u, surfacesForResidency.size());

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());
    auto cmdBuffer = cmdBufferList.peekHead();

    //preemption allocation + sip kernel
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;

    EXPECT_EQ(4u + csrSurfaceCount, cmdBuffer->surfaces.size());

    //copy those surfaces
    std::vector<GraphicsAllocation *> residentSurfaces = cmdBuffer->surfaces;

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_TRUE(graphicsAllocation->isResident(0u));
        EXPECT_EQ(1, graphicsAllocation->residencyTaskCount[0u]);
    }

    mockCsr->flushBatchedSubmissions();

    EXPECT_FALSE(mockCsr->recordedCommandBuffer->batchBuffer.low_priority);
    EXPECT_TRUE(mockCsr->recordedCommandBuffer->batchBuffer.requiresCoherency);
    EXPECT_EQ(mockCsr->recordedCommandBuffer->batchBuffer.commandBufferAllocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(4u, mockCsr->recordedCommandBuffer->batchBuffer.startOffset);
    EXPECT_EQ(1, mockCsr->flushCalledCount);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCommandBuffers().peekIsEmpty());

    EXPECT_EQ(0u, surfacesForResidency.size());

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_FALSE(graphicsAllocation->isResident(0u));
    }
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrCreatedWithDedicatedDebugFlagWhenItIsCreatedThenItHasProperDispatchMode) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CsrDispatchMode.set(static_cast<uint32_t>(DispatchMode::AdaptiveDispatch));
    std::unique_ptr<MockCsrHw2<FamilyType>> mockCsr(new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment));
    EXPECT_EQ(DispatchMode::AdaptiveDispatch, mockCsr->dispatchMode);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenBlockingCommandIsSendThenItIsFlushedAndNotBatched) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>();

    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenBufferToFlushWhenFlushTaskCalledThenUpdateFlushStamp) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    commandStream.getSpace(1);
    EXPECT_EQ(0, mockCsr->flushCalledCount);
    auto previousFlushStamp = mockCsr->flushStamp->peekStamp();
    auto cmplStamp = flushTask(*mockCsr);
    EXPECT_GT(mockCsr->flushStamp->peekStamp(), previousFlushStamp);
    EXPECT_EQ(mockCsr->flushStamp->peekStamp(), cmplStamp.flushStamp);
    EXPECT_EQ(1, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenNothingToFlushWhenFlushTaskCalledThenDontFlushStamp) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    configureCSRtoNonDirtyState<FamilyType>();

    EXPECT_EQ(0, mockCsr->flushCalledCount);
    auto previousFlushStamp = mockCsr->flushStamp->peekStamp();
    auto cmplStamp = flushTask(*mockCsr);
    EXPECT_EQ(mockCsr->flushStamp->peekStamp(), previousFlushStamp);
    EXPECT_EQ(previousFlushStamp, cmplStamp.flushStamp);
    EXPECT_EQ(0, mockCsr->flushCalledCount);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenFlushTaskIsCalledThenFlushedTaskCountIsNotModifed) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(1u, mockCsr->peekLatestSentTaskCount());
    EXPECT_EQ(0u, mockCsr->peekLatestFlushedTaskCount());

    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(1u, mockCsr->peekLatestSentTaskCount());
    EXPECT_EQ(1u, mockCsr->peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInDefaultModeWhenFlushTaskIsCalledThenFlushedTaskCountIsModifed) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    auto &csr = pDevice->getCommandStreamReceiver();

    csr.flushTask(commandStream,
                  0,
                  dsh,
                  ioh,
                  ssh,
                  taskLevel,
                  dispatchFlags,
                  *pDevice);

    EXPECT_EQ(1u, csr.peekLatestSentTaskCount());
    EXPECT_EQ(1u, csr.peekLatestFlushedTaskCount());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenWaitForTaskCountIsCalledWithTaskCountThatWasNotYetFlushedThenBatchedCommandBuffersAreSubmitted) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());

    EXPECT_EQ(0u, mockCsr->peekLatestFlushedTaskCount());

    auto cmdBuffer = cmdBufferList.peekHead();
    EXPECT_EQ(1u, cmdBuffer->taskCount);

    mockCsr->waitForCompletionWithTimeout(false, 1, 1);

    EXPECT_EQ(1u, mockCsr->peekLatestFlushedTaskCount());

    EXPECT_TRUE(cmdBufferList.peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenEnqueueIsMadeThenCurrentMemoryUsedIsTracked) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();

    uint64_t expectedUsed = 0;
    for (const auto &resource : cmdBuffer->surfaces) {
        expectedUsed += resource->getUnderlyingBufferSize();
    }

    EXPECT_EQ(expectedUsed, mockCsr->peekTotalMemoryUsed());

    //after flush it goes to 0
    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(0u, mockCsr->peekTotalMemoryUsed());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenSusbsequentEnqueueIsMadeThenOnlyNewResourcesAreTrackedForMemoryUsage) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();

    uint64_t expectedUsed = 0;
    for (const auto &resource : cmdBuffer->surfaces) {
        expectedUsed += resource->getUnderlyingBufferSize();
    }

    auto additionalSize = 1234;

    GraphicsAllocation graphicsAllocation(nullptr, additionalSize);
    mockCsr->makeResident(graphicsAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(expectedUsed + additionalSize, mockCsr->peekTotalMemoryUsed());
    mockCsr->flushBatchedSubmissions();
}

struct MockedMemoryManager : public OsAgnosticMemoryManager {
    MockedMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(false, false, executionEnvironment) {}
    bool isMemoryBudgetExhausted() const override { return budgetExhausted; }
    bool budgetExhausted = false;
};

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenTotalResourceUsedExhaustsTheBudgetThenDoImplicitFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);
    ExecutionEnvironment executionEnvironment;
    auto mockedMemoryManager = new MockedMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(mockedMemoryManager);
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], executionEnvironment);
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(mockCsr));
    mockCsr->setMemoryManager(mockedMemoryManager);
    mockCsr->initializeTagAllocation();
    mockCsr->setPreemptionCsrAllocation(pDevice->getPreemptionAllocation());
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockedMemoryManager->budgetExhausted = true;

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();

    uint64_t expectedUsed = 0;
    for (const auto &resource : cmdBuffer->surfaces) {
        expectedUsed += resource->getUnderlyingBufferSize();
    }

    EXPECT_EQ(expectedUsed, mockCsr->peekTotalMemoryUsed());

    auto budgetSize = (size_t)pDevice->getDeviceInfo().globalMemSize;
    GraphicsAllocation hugeAllocation(nullptr, budgetSize / 4);
    mockCsr->makeResident(hugeAllocation);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    //expect 2 flushes, since we cannot batch those submissions
    EXPECT_EQ(2u, mockCsr->peekLatestFlushedTaskCount());
    EXPECT_EQ(0u, mockCsr->peekTotalMemoryUsed());
    EXPECT_TRUE(mockedSubmissionsAggregator->peekCommandBuffers().peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests,
         givenCsrInBatchingModeWhenTwoTasksArePassedWithTheSameLevelThenThereIsNoPipeControlBetweenThemAfterFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(taskLevelPriorToSubmission, mockCsr->peekTaskLevel());

    //validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_NE(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_NE(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(firstCmdBuffer->pipeControlThatMayBeErasedLocation, secondCmdBuffer->pipeControlThatMayBeErasedLocation);

    auto ppc = genCmdCast<typename FamilyType::PIPE_CONTROL *>(firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, ppc);
    auto ppc2 = genCmdCast<typename FamilyType::PIPE_CONTROL *>(secondCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, ppc2);

    //flush needs to bump the taskLevel
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(taskLevelPriorToSubmission + 1, mockCsr->peekTaskLevel());

    //decode commands to confirm no pipe controls between Walkers

    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    //make sure they are not the same
    EXPECT_NE(cmdList.end(), itorBatchBufferStartFirst);
    EXPECT_NE(cmdList.end(), itorBatchBufferStartSecond);
    EXPECT_NE(itorBatchBufferStartFirst, itorBatchBufferStartSecond);

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_EQ(itorPipeControl, itorBatchBufferStartSecond);

    //first pipe control is nooped, second pipe control is untouched
    auto noop1 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc);
    auto noop2 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc2);
    EXPECT_NE(nullptr, noop1);
    EXPECT_EQ(nullptr, noop2);

    auto ppcAfterChange = genCmdCast<typename FamilyType::PIPE_CONTROL *>(ppc2);
    EXPECT_NE(nullptr, ppcAfterChange);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenDcFlushIsNotRequiredThenItIsNotSet) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);
    EXPECT_FALSE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenCommandAreSubmittedThenDcFlushIsAdded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);

    mockCsr->flushBatchedSubmissions();
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWithOutOfOrderModeFisabledWhenCommandAreSubmittedThenDcFlushIsAdded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    parseCommands<FamilyType>(commandStream);
    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itorPipeControl);

    mockCsr->flushBatchedSubmissions();
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenDcFlushIsRequiredThenPipeControlIsNotRegistredForNooping) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.dcFlush = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto cmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_EQ(nullptr, cmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, cmdBuffer->epiloguePipeControlLocation);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests,
         givenCsrInBatchingModeAndOoqFlagSetToFalseWhenTwoTasksArePassedWithTheSameLevelThenThereIsPipeControlBetweenThemAfterFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(taskLevelPriorToSubmission, mockCsr->peekTaskLevel());

    //validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_EQ(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_EQ(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);

    mockCsr->flushBatchedSubmissions();

    //decode commands to confirm no pipe controls between Walkers

    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_NE(itorPipeControl, itorBatchBufferStartSecond);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeAndOoqFlagSetToFalseWhenTimestampPacketWriteIsEnabledThenNoopPipeControl) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = false;

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->timestampPacketWriteEnabled = false;
    mockCsr->flushTask(commandStream, 0, dsh, ioh, ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);
    mockCsr->flushTask(commandStream, 0, dsh, ioh, ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);

    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_EQ(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_EQ(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);

    mockCsr->flushBatchedSubmissions();

    mockCsr->timestampPacketWriteEnabled = true;
    mockCsr->flushTask(commandStream, 0, dsh, ioh, ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);
    mockCsr->flushTask(commandStream, 0, dsh, ioh, ssh, taskLevelPriorToSubmission, dispatchFlags, *pDevice);

    firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    EXPECT_NE(nullptr, firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    secondCmdBuffer = firstCmdBuffer->next;
    EXPECT_NE(nullptr, secondCmdBuffer->pipeControlThatMayBeErasedLocation);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrInBatchingModeWhenPipeControlForNoopAddressIsNullThenPipeControlIsNotNooped) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto ppc1Location = firstCmdBuffer->pipeControlThatMayBeErasedLocation;

    firstCmdBuffer->pipeControlThatMayBeErasedLocation = nullptr;

    auto ppc = genCmdCast<typename FamilyType::PIPE_CONTROL *>(ppc1Location);
    EXPECT_NE(nullptr, ppc);

    //call flush, both pipe controls must remain untouched
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(taskLevelPriorToSubmission + 1, mockCsr->peekTaskLevel());

    //decode commands to confirm no pipe controls between Walkers
    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_NE(itorPipeControl, itorBatchBufferStartSecond);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests,
         givenCsrInBatchingModeWhenThreeTasksArePassedWithTheSameLevelThenThereIsNoPipeControlBetweenThemAfterFlush) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.outOfOrderExecutionAllowed = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    auto taskLevelPriorToSubmission = mockCsr->peekTaskLevel();

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    //now emit with the same taskLevel
    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevelPriorToSubmission,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(taskLevelPriorToSubmission, mockCsr->peekTaskLevel());

    //validate if we recorded ppc positions
    auto firstCmdBuffer = mockedSubmissionsAggregator->peekCommandBuffers().peekHead();
    auto secondCmdBuffer = firstCmdBuffer->next;
    auto thirdCmdBuffer = firstCmdBuffer->next->next;

    EXPECT_NE(nullptr, thirdCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(firstCmdBuffer->pipeControlThatMayBeErasedLocation, thirdCmdBuffer->pipeControlThatMayBeErasedLocation);

    auto ppc = genCmdCast<typename FamilyType::PIPE_CONTROL *>(firstCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto ppc2 = genCmdCast<typename FamilyType::PIPE_CONTROL *>(secondCmdBuffer->pipeControlThatMayBeErasedLocation);
    auto ppc3 = genCmdCast<typename FamilyType::PIPE_CONTROL *>(thirdCmdBuffer->pipeControlThatMayBeErasedLocation);
    EXPECT_NE(nullptr, ppc2);
    EXPECT_NE(nullptr, ppc3);

    //flush needs to bump the taskLevel
    mockCsr->flushBatchedSubmissions();
    EXPECT_EQ(taskLevelPriorToSubmission + 1, mockCsr->peekTaskLevel());

    //decode commands to confirm no pipe controls between Walkers

    parseCommands<FamilyType>(commandQueue);

    auto itorBatchBufferStartFirst = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    auto itorBatchBufferStartSecond = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartFirst, cmdList.end());
    auto itorBatchBufferStartThird = find<typename FamilyType::MI_BATCH_BUFFER_START *>(++itorBatchBufferStartSecond, cmdList.end());

    //make sure they are not the same
    EXPECT_NE(cmdList.end(), itorBatchBufferStartFirst);
    EXPECT_NE(cmdList.end(), itorBatchBufferStartSecond);
    EXPECT_NE(cmdList.end(), itorBatchBufferStartThird);
    EXPECT_NE(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_NE(itorBatchBufferStartThird, itorBatchBufferStartSecond);

    auto itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartFirst, itorBatchBufferStartSecond);
    EXPECT_EQ(itorPipeControl, itorBatchBufferStartSecond);

    itorPipeControl = find<typename FamilyType::PIPE_CONTROL *>(itorBatchBufferStartSecond, itorBatchBufferStartThird);
    EXPECT_EQ(itorPipeControl, itorBatchBufferStartThird);

    //first pipe control is nooped, second pipe control is untouched
    auto noop1 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc);
    auto noop2 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc2);
    auto noop3 = genCmdCast<typename FamilyType::MI_NOOP *>(ppc3);

    EXPECT_NE(nullptr, noop1);
    EXPECT_NE(nullptr, noop2);
    EXPECT_EQ(nullptr, noop3);

    auto ppcAfterChange = genCmdCast<typename FamilyType::PIPE_CONTROL *>(ppc3);
    EXPECT_NE(nullptr, ppcAfterChange);
}

typedef UltCommandStreamReceiverTest CommandStreamReceiverCleanupTests;
HWTEST_F(CommandStreamReceiverFlushTaskTests, givenCsrWhenTemporaryAndReusableAllocationsArePresentThenCleanupResourcesOnlyCleansThoseAboveLatestFlushTaskLevel) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto memoryManager = pDevice->getMemoryManager();

    auto temporaryToClean = memoryManager->allocateGraphicsMemory(4096u);
    auto temporaryToHold = memoryManager->allocateGraphicsMemory(4096u);

    auto reusableToClean = memoryManager->allocateGraphicsMemory(4096u);
    auto reusableToHold = memoryManager->allocateGraphicsMemory(4096u);

    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporaryToClean), TEMPORARY_ALLOCATION);
    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(temporaryToHold), TEMPORARY_ALLOCATION);
    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(reusableToClean), REUSABLE_ALLOCATION);
    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(reusableToHold), REUSABLE_ALLOCATION);

    temporaryToClean->taskCount = 1;
    reusableToClean->taskCount = 1;

    temporaryToHold->taskCount = 10;
    reusableToHold->taskCount = 10;

    commandStreamReceiver.latestFlushedTaskCount = 9;
    commandStreamReceiver.cleanupResources();

    EXPECT_EQ(reusableToHold, memoryManager->allocationsForReuse.peekHead());
    EXPECT_EQ(reusableToHold, memoryManager->allocationsForReuse.peekTail());

    EXPECT_EQ(temporaryToHold, memoryManager->graphicsAllocations.peekHead());
    EXPECT_EQ(temporaryToHold, memoryManager->graphicsAllocations.peekTail());

    commandStreamReceiver.latestFlushedTaskCount = 11;
    commandStreamReceiver.cleanupResources();
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());
    EXPECT_TRUE(memoryManager->graphicsAllocations.peekIsEmpty());
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDispatchFlagsWithThrottleSetToLowWhenFlushTaskIsCalledThenThrottleIsSetInBatchBuffer) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.throttle = QueueThrottle::LOW;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.throttle, QueueThrottle::LOW);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDispatchFlagsWithThrottleSetToMediumWhenFlushTaskIsCalledThenThrottleIsSetInBatchBuffer) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.throttle = QueueThrottle::MEDIUM;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.throttle, QueueThrottle::MEDIUM);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenDispatchFlagsWithThrottleSetToHighWhenFlushTaskIsCalledThenThrottleIsSetInBatchBuffer) {
    typedef typename FamilyType::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    DispatchFlags dispatchFlags;
    dispatchFlags.throttle = QueueThrottle::HIGH;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    auto cmdBuffer = cmdBufferList.peekHead();

    EXPECT_EQ(cmdBuffer->batchBuffer.throttle, QueueThrottle::HIGH);
}
