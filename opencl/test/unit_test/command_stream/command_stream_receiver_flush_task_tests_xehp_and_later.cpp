/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_debugger.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_submissions_aggregator.h"
#include "test.h"

using namespace NEO;

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTaskXeHPAndLaterTests;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenReprogrammingSshThenBindingTablePoolIsProgrammed) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
    auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTablePoolAlloc);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
    EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
    EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER),
              bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenReprogrammingSshThenBindingTablePoolIsProgrammedWithCachingOffWhenDebugKeyPresent) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisableCachingForHeaps.set(1);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
    auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTablePoolAlloc);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
    EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
    EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED),
              bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenNotReprogrammingSshThenBindingTablePoolIsNotProgrammed) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
    auto stateBaseAddress = getCommand<typename FamilyType::STATE_BASE_ADDRESS>();
    EXPECT_NE(nullptr, stateBaseAddress);
    auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTablePoolAlloc);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
    EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
    EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER),
              bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());

    auto offset = commandStreamReceiver.getCS(0).getUsed();
    // make SBA dirty (using ioh as dsh and dsh as ioh just to force SBA reprogramming)
    commandStreamReceiver.flushTask(commandStream, 0, ioh, dsh, ssh, taskLevel, flushTaskFlags, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.getCS(0), offset);
    stateBaseAddress = hwParser.getCommand<typename FamilyType::STATE_BASE_ADDRESS>();
    EXPECT_NE(nullptr, stateBaseAddress);
    bindingTablePoolAlloc = hwParser.getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTablePoolAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenStateBaseAddressWhenItIsRequiredThenThereIsPipeControlPriorToItWithTextureCacheFlushAndHdc) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));

    auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControlCmd->getDcFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenProgramPipeControlPriorToNonPipelinedStateCommandDebugKeyAndStateBaseAddressWhenItIsRequiredThenThereIsPipeControlPriorToIt) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ProgramPipeControlPriorToNonPipelinedStateCommand.set(true);

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));

    auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControlCmd->getAmfsFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenProgramPipeControlPriorToNonPipelinedStateCommandDebugKeyAndStateSipWhenItIsRequiredThenThereIsPipeControlPriorToIt) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ProgramPipeControlPriorToNonPipelinedStateCommand.set(true);

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    pDevice->executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockDebugger);

    auto sipType = SipKernel::getSipKernelType(*pDevice);
    SipKernel::initSipKernel(sipType, *pDevice);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));

    auto pipeControlIterator = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);

    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControlCmd->getAmfsFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getStateCacheInvalidationEnable());

    auto sipIterator = find<STATE_SIP *>(cmdList.begin(), cmdList.end());
    auto sipCmd = genCmdCast<STATE_SIP *>(*sipIterator);

    auto sipAllocation = SipKernel::getSipKernel(*pDevice).getSipAllocation();

    EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipCmd->getSystemInstructionPointer());
}

HWTEST2_F(CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenProgramPipeControlPriorToNonPipelinedStateCommandDebugKeyAndStateSipWhenA0SteppingIsActivatedThenOnlyGlobalSipIsProgrammed, IsXEHP) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ProgramPipeControlPriorToNonPipelinedStateCommand.set(true);

    using STATE_SIP = typename FamilyType::STATE_SIP;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    hardwareInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0u));
    auto &commandStreamReceiver = mockDevice->getUltCommandStreamReceiver<FamilyType>();

    mockDevice->executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockDebugger);

    auto sipType = SipKernel::getSipKernelType(*mockDevice);
    SipKernel::initSipKernel(sipType, *mockDevice);

    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);

    flushTaskFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(mockDevice->getHardwareInfo());

    commandStreamReceiver.flushTask(
        commandStream,
        0,
        dsh,
        ioh,
        ssh,
        taskLevel,
        flushTaskFlags,
        *mockDevice);

    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));

    auto itorLRI = findMmio<FamilyType>(cmdList.begin(), cmdList.end(), 0xE42C);
    EXPECT_NE(cmdList.end(), itorLRI);

    auto cmdLRI = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto sipAddress = cmdLRI->getDataDword() & 0xfffffff8;
    auto sipAllocation = SipKernel::getSipKernel(*mockDevice).getSipAllocation();

    EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenNotReprogrammingSshButInitProgrammingFlagsThenBindingTablePoolIsProgrammed) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
    auto stateBaseAddress = getCommand<typename FamilyType::STATE_BASE_ADDRESS>();
    EXPECT_NE(nullptr, stateBaseAddress);
    auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTablePoolAlloc);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
    EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
    EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER),
              bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());

    auto offset = commandStreamReceiver.getCS(0).getUsed();
    commandStreamReceiver.initProgrammingFlags();
    flushTask(commandStreamReceiver);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.getCS(0), offset);
    stateBaseAddress = hwParser.getCommand<typename FamilyType::STATE_BASE_ADDRESS>();
    EXPECT_NE(nullptr, stateBaseAddress);
    bindingTablePoolAlloc = hwParser.getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_NE(nullptr, bindingTablePoolAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenSbaProgrammingWhenHeapsAreNotProvidedThenDontProgram) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    uint64_t instructionHeapBase = 0x10000;
    uint64_t internalHeapBase = 0x10000;
    uint64_t generalStateBase = 0x30000;
    STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(&sbaCmd,
                                                                nullptr,
                                                                nullptr,
                                                                nullptr,
                                                                generalStateBase,
                                                                true,
                                                                0,
                                                                internalHeapBase,
                                                                instructionHeapBase,
                                                                0,
                                                                true,
                                                                false,
                                                                pDevice->getGmmHelper(),
                                                                false,
                                                                MemoryCompressionState::NotApplicable,
                                                                false,
                                                                1u);

    EXPECT_FALSE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBufferSize());

    EXPECT_FALSE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd.getInstructionBaseAddressModifyEnable());
    EXPECT_EQ(instructionHeapBase, sbaCmd.getInstructionBaseAddress());
    EXPECT_TRUE(sbaCmd.getInstructionBufferSizeModifyEnable());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, sbaCmd.getInstructionBufferSize());

    EXPECT_TRUE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
    if constexpr (is64bit) {
        EXPECT_EQ(GmmHelper::decanonize(internalHeapBase), sbaCmd.getGeneralStateBaseAddress());
    } else {
        EXPECT_EQ(generalStateBase, sbaCmd.getGeneralStateBaseAddress());
    }
    EXPECT_EQ(0xfffffu, sbaCmd.getGeneralStateBufferSize());

    EXPECT_EQ(0u, sbaCmd.getBindlessSurfaceStateBaseAddress());
    EXPECT_FALSE(sbaCmd.getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getBindlessSurfaceStateSize());
}

using isXeHPOrAbove = IsAtLeastProduct<IGFX_XE_HP_SDV>;
HWTEST2_F(CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenFlushAllCachesVariableIsSetAndAddPipeControlIsCalledThenFieldsAreProperlySet, isXeHPOrAbove) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.FlushAllCaches.set(true);

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addPipeControl(stream, args);

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
    // XeHP+ only field
    EXPECT_TRUE(pipeControl->getCompressionControlSurfaceCcsFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenconfigureCSRtoNonDirtyStateWhenFlushTaskIsCalledThenNoCommandsAreAdded) {
    configureCSRtoNonDirtyState<FamilyType>(true);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);
    EXPECT_EQ(0u, commandStreamReceiver.commandStream.getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenMultiOsContextCommandStreamReceiverWhenFlushTaskIsCalledThenCommandStreamReceiverStreamIsUsed) {
    configureCSRtoNonDirtyState<FamilyType>(true);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.multiOsContextCapable = true;
    commandStream.getSpace(4);

    flushTask(commandStreamReceiver);
    EXPECT_EQ(MemoryConstants::cacheLineSize, commandStreamReceiver.commandStream.getUsed());
    auto batchBufferStart = genCmdCast<typename FamilyType::MI_BATCH_BUFFER_START *>(commandStreamReceiver.commandStream.getCpuBase());
    EXPECT_NE(nullptr, batchBufferStart);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenCsrInBatchingModeWhenTaskIsSubmittedViaCsrThenBbEndCoversPaddingEnoughToFitMiBatchBufferStart) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr.timestampPacketWriteEnabled = false;

    configureCSRtoNonDirtyState<FamilyType>(true);

    mockCsr.getCS(1024u);
    auto &csrCommandStream = mockCsr.commandStream;

    //we do level change that will emit PPC, fill all the space so only BB end fits.
    taskLevel++;
    auto ppcSize = MemorySynchronizationCommands<FamilyType>::getSizeForSinglePipeControl();
    auto fillSize = MemoryConstants::cacheLineSize - ppcSize - sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    csrCommandStream.getSpace(fillSize);
    auto expectedUsedSize = 2 * MemoryConstants::cacheLineSize;

    flushTask(mockCsr);

    EXPECT_EQ(expectedUsedSize, mockCsr.commandStream.getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, GivenSameTaskLevelThenDontSendPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>(true);

    flushTask(commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver.taskLevel);

    auto sizeUsed = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(sizeUsed, 0u);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenDeviceWithThreadGroupPreemptionSupportThenDontSendMediaVfeStateIfNotDirty) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>(true);

    flushTask(*commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver->peekTaskLevel());

    auto sizeUsed = commandStreamReceiver->commandStream.getUsed();
    EXPECT_EQ(0u, sizeUsed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenCommandStreamReceiverWithInstructionCacheRequestWhenFlushTaskIsCalledThenPipeControlWithInstructionCacheIsEmitted) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    configureCSRtoNonDirtyState<FamilyType>(true);

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

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenHigherTaskLevelWhenTimestampPacketWriteIsEnabledThenDontAddPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = true;
    commandStreamReceiver.isPreambleSent = true;
    configureCSRtoNonDirtyState<FamilyType>(true);
    commandStreamReceiver.taskLevel = taskLevel;
    taskLevel++; // submit with higher taskLevel

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, WhenForcePipeControlPriorToWalkerIsSetThenAddExtraPipeControls) {
    DebugManagerStateRestore stateResore;
    DebugManager.flags.ForcePipeControlPriorToWalker.set(true);
    DebugManager.flags.FlushAllCaches.set(true);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    configureCSRtoNonDirtyState<FamilyType>(true);
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
                EXPECT_EQ(bool(pipeControl->getDcFlushEnable()), false);
                EXPECT_EQ(bool(pipeControl->getRenderTargetCacheFlushEnable()), false);
                EXPECT_EQ(bool(pipeControl->getInstructionCacheInvalidateEnable()), false);
                EXPECT_EQ(bool(pipeControl->getTextureCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getPipeControlFlushEnable()), false);
                EXPECT_EQ(bool(pipeControl->getVfCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getConstantCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getStateCacheInvalidationEnable()), false);
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
            default:
                break;
            }
            counterPC++;
        }

        ++itor;
    }

    EXPECT_EQ(counterPC, 2);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenSamplerCacheFlushNotRequiredThenDontSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    NEO::WorkaroundTable *waTable = &pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;

    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastPreemptionMode = pDevice->getPreemptionMode();
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired);
    configureCSRtoNonDirtyState<FamilyType>(true);
    commandStreamReceiver.taskLevel = taskLevel;
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
    flushTask(commandStreamReceiver);

    EXPECT_EQ(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenSamplerCacheFlushBeforeAndWaSamplerCacheFlushBetweenRedescribedSurfaceReadsDasabledThenDontSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
    configureCSRtoNonDirtyState<FamilyType>(true);
    commandStreamReceiver.taskLevel = taskLevel;
    NEO::WorkaroundTable *waTable = &pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;

    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = false;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, WhenFlushingTaskThenStateBaseAddressProgrammingShouldMatchTracking) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    auto gmmHelper = pDevice->getGmmHelper();
    auto stateHeapMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);
    auto l1CacheOnMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    auto &commandStreamCSR = commandStreamReceiver.commandStream;
    parseCommands<FamilyType>(commandStreamCSR, 0);
    HardwareParse::findHardwareCommands<FamilyType>();

    ASSERT_NE(nullptr, cmdStateBaseAddress);
    auto &cmd = *reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStateBaseAddress);

    EXPECT_EQ(dsh.getCpuBase(), reinterpret_cast<void *>(cmd.getDynamicStateBaseAddress()));
    EXPECT_EQ(commandStreamReceiver.getMemoryManager()->getInternalHeapBaseAddress(commandStreamReceiver.rootDeviceIndex, ioh.getGraphicsAllocation()->isAllocatedInLocalMemoryPool()), cmd.getInstructionBaseAddress());
    EXPECT_EQ(ssh.getCpuBase(), reinterpret_cast<void *>(cmd.getSurfaceStateBaseAddress()));

    EXPECT_EQ(l1CacheOnMocs, cmd.getStatelessDataPortAccessMemoryObjectControlState());
    EXPECT_EQ(stateHeapMocs, cmd.getInstructionMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, GivenBlockingWhenFlushingTaskThenPipeControlProgrammedCorrectly) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    // Configure the CSR to not need to submit any state or commands
    configureCSRtoNonDirtyState<FamilyType>(true);

    // Force a PIPE_CONTROL through a blocking flag
    auto blocking = true;
    auto &commandStreamTask = commandQueue.getCS(1024);
    auto &commandStreamCSR = commandStreamReceiver->getCS();
    commandStreamReceiver->lastSentCoherencyRequest = 0;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.blocking = blocking;
    dispatchFlags.guardCommandBufferWithPipeControl = true;

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

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenCsrInNonDirtyStateWhenflushTaskIsCalledThenNoFlushIsCalled) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    configureCSRtoNonDirtyState<FamilyType>(true);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
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

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenCsrInNonDirtyStateAndBatchingModeWhenflushTaskIsCalledWithDisabledPreemptionThenSubmissionIsNotRecorded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>(true);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
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

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenCsrInBatchingModeWhenRecordedBatchBufferIsBeingSubmittedThenFlushIsCalledWithRecordedCommandBuffer) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>(true);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.requiresCoherency = true;

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
    csrSurfaceCount += mockCsr->globalFenceAllocation ? 1 : 0;
    csrSurfaceCount += mockCsr->clearColorAllocation ? 1 : 0;

    EXPECT_EQ(4u + csrSurfaceCount, cmdBuffer->surfaces.size());

    //copy those surfaces
    std::vector<GraphicsAllocation *> residentSurfaces = cmdBuffer->surfaces;

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_TRUE(graphicsAllocation->isResident(mockCsr->getOsContext().getContextId()));
        EXPECT_EQ(1u, graphicsAllocation->getResidencyTaskCount(mockCsr->getOsContext().getContextId()));
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
        EXPECT_FALSE(graphicsAllocation->isResident(mockCsr->getOsContext().getContextId()));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenNothingToFlushWhenFlushTaskCalledThenDontFlushStamp) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    configureCSRtoNonDirtyState<FamilyType>(true);

    EXPECT_EQ(0, mockCsr->flushCalledCount);
    auto previousFlushStamp = mockCsr->flushStamp->peekStamp();
    auto cmplStamp = flushTask(*mockCsr);
    EXPECT_EQ(mockCsr->flushStamp->peekStamp(), previousFlushStamp);
    EXPECT_EQ(previousFlushStamp, cmplStamp.flushStamp);
    EXPECT_EQ(0, mockCsr->flushCalledCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenEpilogueRequiredFlagWhenTaskIsSubmittedDirectlyThenItPointsBackToCsr) {
    configureCSRtoNonDirtyState<FamilyType>(true);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    EXPECT_EQ(0u, commandStreamReceiver.getCmdSizeForEpilogue(dispatchFlags));

    dispatchFlags.epilogueRequired = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    EXPECT_EQ(MemoryConstants::cacheLineSize, commandStreamReceiver.getCmdSizeForEpilogue(dispatchFlags));

    auto data = commandStream.getSpace(MemoryConstants::cacheLineSize);
    memset(data, 0, MemoryConstants::cacheLineSize);
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    dsh,
                                    ioh,
                                    ssh,
                                    taskLevel,
                                    dispatchFlags,
                                    *pDevice);
    auto &commandStreamReceiverStream = commandStreamReceiver.getCS(0u);

    EXPECT_EQ(MemoryConstants::cacheLineSize * 2, commandStream.getUsed());
    EXPECT_EQ(MemoryConstants::cacheLineSize, commandStreamReceiverStream.getUsed());

    parseCommands<FamilyType>(commandStream, 0);

    auto itBBend = find<typename FamilyType::MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(itBBend, cmdList.end());

    auto itBatchBufferStart = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itBatchBufferStart, cmdList.end());

    auto batchBufferStart = genCmdCast<typename FamilyType::MI_BATCH_BUFFER_START *>(*itBatchBufferStart);
    EXPECT_EQ(batchBufferStart->getBatchBufferStartAddressGraphicsaddress472(), commandStreamReceiverStream.getGraphicsAllocation()->getGpuAddress());

    parseCommands<FamilyType>(commandStreamReceiverStream, 0);

    itBBend = find<typename FamilyType::MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    void *bbEndAddress = *itBBend;

    EXPECT_EQ(commandStreamReceiverStream.getCpuBase(), bbEndAddress);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiverStream.getGraphicsAllocation()));
}

struct CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests : public CommandStreamReceiverFlushTaskXeHPAndLaterTests {
    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(2);
        parsePipeControl = true;
        CommandStreamReceiverFlushTaskXeHPAndLaterTests::SetUp();
    }

    template <typename GfxFamily>
    void verifyPipeControl(UltCommandStreamReceiver<GfxFamily> &commandStreamReceiver, uint32_t expectedTaskCount, bool workLoadPartition) {
        using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

        uint64_t gpuAddressTagAllocation = commandStreamReceiver.getTagAllocation()->getGpuAddress();
        uint32_t gpuAddressLow = static_cast<uint32_t>(gpuAddressTagAllocation & 0x0000FFFFFFFFULL);
        uint32_t gpuAddressHigh = static_cast<uint32_t>(gpuAddressTagAllocation >> 32);

        bool pipeControlTagUpdate = false;
        bool pipeControlWorkloadPartition = false;

        auto itorPipeControl = pipeControlList.begin();
        while (itorPipeControl != pipeControlList.end()) {
            auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*itorPipeControl);
            if (pipeControl->getPostSyncOperation() == PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                pipeControlTagUpdate = true;
                if (pipeControl->getWorkloadPartitionIdOffsetEnable()) {
                    pipeControlWorkloadPartition = true;
                }
                EXPECT_EQ(gpuAddressLow, pipeControl->getAddress());
                EXPECT_EQ(gpuAddressHigh, pipeControl->getAddressHigh());
                EXPECT_EQ(expectedTaskCount, pipeControl->getImmediateData());
                break;
            }
            itorPipeControl++;
        }

        EXPECT_TRUE(pipeControlTagUpdate);
        if (workLoadPartition) {
            EXPECT_TRUE(pipeControlWorkloadPartition);
        } else {
            EXPECT_FALSE(pipeControlWorkloadPartition);
        }
    }

    template <typename GfxFamily>
    void verifyActivePartitionConfig(UltCommandStreamReceiver<GfxFamily> &commandStreamReceiver, bool activePartitionExists) {
        using MI_LOAD_REGISTER_MEM = typename GfxFamily::MI_LOAD_REGISTER_MEM;
        using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

        uint64_t expectedWparidData = 0u;
        if (activePartitionExists) {
            expectedWparidData = commandStreamReceiver.getWorkPartitionAllocationGpuAddress();
        }
        uint32_t expectedWparidRegister = 0x221C;
        uint32_t expectedAddressOffsetData = 8;
        uint32_t expectedAddressOffsetRegister = 0x23B4;

        bool wparidConfiguration = false;
        bool addressOffsetConfiguration = false;

        auto lrmList = getCommandsList<MI_LOAD_REGISTER_MEM>();
        auto itorWparidRegister = lrmList.begin();
        while (itorWparidRegister != lrmList.end()) {
            auto loadRegisterMem = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(*itorWparidRegister);

            if (loadRegisterMem->getRegisterAddress() == expectedWparidRegister) {
                wparidConfiguration = true;
                EXPECT_EQ(expectedWparidData, loadRegisterMem->getMemoryAddress());
                break;
            }
            itorWparidRegister++;
        }

        auto itorAddressOffsetRegister = lriList.begin();
        while (itorAddressOffsetRegister != lriList.end()) {
            auto loadRegisterImm = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorAddressOffsetRegister);

            if (loadRegisterImm->getRegisterOffset() == expectedAddressOffsetRegister) {
                addressOffsetConfiguration = true;
                EXPECT_EQ(expectedAddressOffsetData, loadRegisterImm->getDataDword());
                break;
            }
            itorAddressOffsetRegister++;
        }

        if (activePartitionExists) {
            EXPECT_TRUE(wparidConfiguration);
            EXPECT_TRUE(addressOffsetConfiguration);
        } else {
            EXPECT_FALSE(wparidConfiguration);
            EXPECT_FALSE(addressOffsetConfiguration);
        }
    }

    template <typename GfxFamily>
    void prepareLinearStream(LinearStream &parsedStream, size_t offset) {
        cmdList.clear();
        lriList.clear();
        pipeControlList.clear();

        parseCommands<GfxFamily>(parsedStream, offset);
        findHardwareCommands<GfxFamily>();
    }

    DebugManagerStateRestore restorer;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests,
            givenMultipleStaticActivePartitionsWhenFlushingTaskThenExpectTagUpdatePipeControlWithPartitionFlagOnAndActivePartitionConfig) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (pDevice->getPreemptionMode() == PreemptionMode::MidThread || pDevice->isDebuggerActive()) {
        commandStreamReceiver.createPreemptionAllocation();
    }
    EXPECT_EQ(1u, commandStreamReceiver.activePartitionsConfig);
    commandStreamReceiver.activePartitions = 2;
    commandStreamReceiver.taskCount = 3;
    EXPECT_TRUE(commandStreamReceiver.staticWorkPartitioningEnabled);
    flushTask(commandStreamReceiver, true);
    EXPECT_EQ(2u, commandStreamReceiver.activePartitionsConfig);

    prepareLinearStream<FamilyType>(commandStream, 0);
    verifyPipeControl<FamilyType>(commandStreamReceiver, 4, true);

    prepareLinearStream<FamilyType>(commandStreamReceiver.commandStream, 0);
    verifyActivePartitionConfig<FamilyType>(commandStreamReceiver, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests,
            givenMultipleDynamicActivePartitionsWhenFlushingTaskThenExpectTagUpdatePipeControlWithoutPartitionFlagOnAndNoActivePartitionConfig) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (pDevice->getPreemptionMode() == PreemptionMode::MidThread || pDevice->isDebuggerActive()) {
        commandStreamReceiver.createPreemptionAllocation();
    }
    commandStreamReceiver.activePartitions = 2;
    commandStreamReceiver.taskCount = 3;
    commandStreamReceiver.staticWorkPartitioningEnabled = false;
    flushTask(commandStreamReceiver, true);
    EXPECT_EQ(2u, commandStreamReceiver.activePartitionsConfig);

    prepareLinearStream<FamilyType>(commandStream, 0);
    verifyPipeControl<FamilyType>(commandStreamReceiver, 4, false);

    prepareLinearStream<FamilyType>(commandStreamReceiver.commandStream, 0);
    verifyActivePartitionConfig<FamilyType>(commandStreamReceiver, false);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests,
            givenSingleStaticActivePartitionWhenFlushingTaskThenExpectTagUpdatePipeControlWithoutPartitionFlagOnAndNoActivePartitionConfig) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (pDevice->getPreemptionMode() == PreemptionMode::MidThread || pDevice->isDebuggerActive()) {
        commandStreamReceiver.createPreemptionAllocation();
    }
    commandStreamReceiver.activePartitions = 1;
    commandStreamReceiver.taskCount = 3;
    flushTask(commandStreamReceiver, true);

    parseCommands<FamilyType>(commandStream, 0);
    parsePipeControl = true;
    findHardwareCommands<FamilyType>();

    prepareLinearStream<FamilyType>(commandStream, 0);
    verifyPipeControl<FamilyType>(commandStreamReceiver, 4, false);

    prepareLinearStream<FamilyType>(commandStreamReceiver.commandStream, 0);
    verifyActivePartitionConfig<FamilyType>(commandStreamReceiver, false);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests,
            givenMultipleStaticActivePartitionsWhenFlushingTaskTwiceThenExpectTagUpdatePipeControlWithPartitionFlagOnAndNoActivePartitionConfigAtSecondFlush) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (pDevice->getPreemptionMode() == PreemptionMode::MidThread || pDevice->isDebuggerActive()) {
        commandStreamReceiver.createPreemptionAllocation();
    }
    EXPECT_EQ(1u, commandStreamReceiver.activePartitionsConfig);
    commandStreamReceiver.activePartitions = 2;
    commandStreamReceiver.taskCount = 3;
    EXPECT_TRUE(commandStreamReceiver.staticWorkPartitioningEnabled);
    flushTask(commandStreamReceiver, true);
    EXPECT_EQ(2u, commandStreamReceiver.activePartitionsConfig);

    prepareLinearStream<FamilyType>(commandStream, 0);
    verifyPipeControl<FamilyType>(commandStreamReceiver, 4, true);

    prepareLinearStream<FamilyType>(commandStreamReceiver.commandStream, 0);
    verifyActivePartitionConfig<FamilyType>(commandStreamReceiver, true);

    size_t usedBeforeCmdStream = commandStream.getUsed();
    size_t usedBeforeCsrCmdStream = commandStreamReceiver.commandStream.getUsed();

    flushTask(commandStreamReceiver, true);

    prepareLinearStream<FamilyType>(commandStream, usedBeforeCmdStream);
    verifyPipeControl<FamilyType>(commandStreamReceiver, 5, true);

    prepareLinearStream<FamilyType>(commandStreamReceiver.commandStream, usedBeforeCsrCmdStream);
    verifyActivePartitionConfig<FamilyType>(commandStreamReceiver, false);
}
