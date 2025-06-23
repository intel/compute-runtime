/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "test_traits_common.h"

using namespace NEO;

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTaskXeHPAndLaterTests;
using CommandStreamReceiverFlushTaskXeHPAndLaterTestsWithMockCsrHw = UltCommandStreamReceiverTestWithCsrT<MockCsrHw>;
using CommandStreamReceiverFlushTaskXeHPAndLaterTestsWithMockCsrHw2 = UltCommandStreamReceiverTestWithCsrT<MockCsrHw2>;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenReprogrammingSshThenBindingTablePoolIsProgrammed) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
        if (commandStreamReceiver.heaplessModeEnabled) {
            GTEST_SKIP();
        }
        flushTask(commandStreamReceiver);
        parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
        auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
        ASSERT_NE(nullptr, bindingTablePoolAlloc);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
        EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
        EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER),
                  bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenReprogrammingSshThenBindingTablePoolIsProgrammedWithCachingOffWhenDebugKeyPresent) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        DebugManagerStateRestore restorer;
        debugManager.flags.DisableCachingForHeaps.set(1);

        auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
        if (commandStreamReceiver.heaplessModeEnabled) {
            GTEST_SKIP();
        }

        flushTask(commandStreamReceiver);
        parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
        auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
        ASSERT_NE(nullptr, bindingTablePoolAlloc);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
        EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
        EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED),
                  bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenNotReprogrammingSshThenBindingTablePoolIsNotProgrammed) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
        if (commandStreamReceiver.heaplessModeEnabled) {
            GTEST_SKIP();
        }

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
        commandStreamReceiver.flushTask(commandStream, 0, &ioh, &dsh, &ssh, taskLevel, flushTaskFlags, *pDevice);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(commandStreamReceiver.getCS(0), offset);
        stateBaseAddress = hwParser.getCommand<typename FamilyType::STATE_BASE_ADDRESS>();
        EXPECT_NE(nullptr, stateBaseAddress);
        bindingTablePoolAlloc = hwParser.getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
        EXPECT_EQ(nullptr, bindingTablePoolAlloc);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenStateBaseAddressWhenItIsRequiredThenThereIsPipeControlPriorToItWithTextureCacheFlushAndHdc) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));

    auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment()), pipeControlCmd->getDcFlushEnable());
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControlCmd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledAndStateBaseAddressWhenItIsRequiredThenThereIsPipeControlPriorToIt) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));

    auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControlCmd));
    EXPECT_TRUE(pipeControlCmd->getAmfsFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getStateCacheInvalidationEnable());
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControlCmd));
}

HWTEST2_F(CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenSBACommandToProgramOnSingleCCSSetupThenThereIsPipeControlPriorToIt, IsXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    hardwareInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, 0u));
    auto &commandStreamReceiver = mockDevice->getUltCommandStreamReceiver<FamilyType>();

    MockOsContext ccsOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    commandStreamReceiver.setupContext(ccsOsContext);

    configureCSRtoNonDirtyState<FamilyType>(false);
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));

    auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenNotReprogrammingSshButInitProgrammingFlagsThenBindingTablePoolIsProgrammed) {
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
        if (commandStreamReceiver.heaplessModeEnabled) {
            GTEST_SKIP();
        }
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
}

using isXeHPOrAbove = IsAtLeastProduct<IGFX_XE_HP_SDV>;
HWTEST2_F(CommandStreamReceiverFlushTaskXeHPAndLaterTests, whenFlushAllCachesVariableIsSetAndAddPipeControlIsCalledThenFieldsAreProperlySet, isXeHPOrAbove) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.FlushAllCaches.set(true);

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

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

    auto sizeUsedBefore = commandStreamReceiver.commandStream.getUsed();
    flushTask(commandStreamReceiver);

    auto sizeUsedAfter = commandStreamReceiver.commandStream.getUsed();

    EXPECT_EQ(sizeUsedBefore, sizeUsedAfter);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenCsrInBatchingModeWhenTaskIsSubmittedViaCsrThenBbEndCoversPaddingEnoughToFitMiBatchBufferStart) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr.timestampPacketWriteEnabled = false;

    configureCSRtoNonDirtyState<FamilyType>(true);

    mockCsr.getCS(1024u);
    auto &csrCommandStream = mockCsr.commandStream;

    auto usedBefore = mockCsr.commandStream.getUsed();

    // we do level change that will emit PPC, fill all the space so only BB end fits.
    taskLevel++;
    auto ppcSize = MemorySynchronizationCommands<FamilyType>::getSizeForSingleBarrier();
    auto fillSize = MemoryConstants::cacheLineSize - ppcSize - sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    csrCommandStream.getSpace(fillSize);
    auto expectedUsedSize = 2 * MemoryConstants::cacheLineSize;

    flushTask(mockCsr);

    auto usedAfter = mockCsr.commandStream.getUsed();
    auto sizeUsed = usedAfter - usedBefore;
    EXPECT_EQ(expectedUsedSize, sizeUsed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, GivenSameTaskLevelThenDontSendPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>(true);
    auto usedBefore = commandStreamReceiver.commandStream.getUsed();
    flushTask(commandStreamReceiver);
    auto usedAfter = commandStreamReceiver.commandStream.getUsed();

    EXPECT_EQ(taskLevel, commandStreamReceiver.taskLevel);
    EXPECT_EQ(usedBefore, usedAfter);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTestsWithMockCsrHw, givenDeviceWithThreadGroupPreemptionSupportThenDontSendMediaVfeStateIfNotDirty) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));

    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    if (commandStreamReceiver->getHeaplessStateInitEnabled()) {
        GTEST_SKIP();
    }

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>(true);

    flushTask(*commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver->peekTaskLevel());

    auto sizeUsed = commandStreamReceiver->commandStream.getUsed();
    EXPECT_EQ(0u, sizeUsed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenCommandStreamReceiverWithInstructionCacheRequestWhenFlushTaskIsCalledThenPipeControlWithInstructionCacheIsEmitted) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    auto startRecursiveLockCounter = commandStreamReceiver.recursiveLockCounter.load();
    configureCSRtoNonDirtyState<FamilyType>(true);

    commandStreamReceiver.registerInstructionCacheFlush();
    EXPECT_EQ(startRecursiveLockCounter + 1u, commandStreamReceiver.recursiveLockCounter);

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

    auto offset = commandStreamReceiver.commandStream.getUsed();
    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, offset);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, WhenForcePipeControlPriorToWalkerIsSetThenAddExtraPipeControls) {
    DebugManagerStateRestore stateResore;
    debugManager.flags.ForcePipeControlPriorToWalker.set(true);
    debugManager.flags.FlushAllCaches.set(true);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    commandStreamReceiver.isPreambleSent = true;
    configureCSRtoNonDirtyState<FamilyType>(true);
    commandStreamReceiver.taskLevel = taskLevel;

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    GenCmdList::iterator itor = cmdList.begin();
    int counterPC = 0;
    while (itor != cmdList.end()) {
        if (counterPC == 0 && isStallingBarrier<FamilyType>(itor)) {
            counterPC++;
            itor++;
            continue;
        }
        auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itor);
        if (pipeControl != nullptr) {
            // Second pipe control with all flushes
            EXPECT_EQ(1, counterPC);
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
            counterPC++;
            break;
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
    waTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    auto usedBefore = commandStreamReceiver.commandStream.getUsed();

    flushTask(commandStreamReceiver);

    auto usedAfter = commandStreamReceiver.commandStream.getUsed();

    EXPECT_EQ(usedBefore, usedAfter);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedBefore);

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

    waTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = false;

    auto usedBefore = commandStreamReceiver.commandStream.getUsed();

    flushTask(commandStreamReceiver);

    auto usedAfter = commandStreamReceiver.commandStream.getUsed();

    EXPECT_EQ(usedBefore, usedAfter);

    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, usedBefore);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, WhenFlushingTaskThenStateBaseAddressProgrammingShouldMatchTracking) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    auto gmmHelper = pDevice->getGmmHelper();
    auto stateHeapMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);
    auto l1CacheOnMocs = gmmHelper->getL1EnabledMOCS();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

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

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTestsWithMockCsrHw, GivenBlockingWhenFlushingTaskThenPipeControlProgrammedCorrectly) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto commandStreamReceiver = static_cast<MockCsrHw<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    commandStreamReceiver->heaplessStateInitialized = true;

    // Configure the CSR to not need to submit any state or commands
    configureCSRtoNonDirtyState<FamilyType>(true);

    // Force a PIPE_CONTROL through a blocking flag
    auto blocking = true;
    auto &commandStreamTask = commandQueue.getCS(1024);
    auto &commandStreamCSR = commandStreamReceiver->getCS();
    auto sizeUsedBeforeFlushCSR = commandStreamCSR.getUsed();

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
    EXPECT_EQ(sizeUsedBeforeFlushCSR, commandStreamCSR.getUsed());

    // Parse command list to verify that PC got added to taskCS
    cmdList.clear();
    parseCommands<FamilyType>(commandStreamTask, 0);
    auto itorTaskCS = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorTaskCS);

    // Parse command list to verify that PC wasn't added to csrCS
    cmdList.clear();
    parseCommands<FamilyType>(commandStreamCSR, sizeUsedBeforeFlushCSR);
    auto numberOfPC = getCommandsList<PIPE_CONTROL>().size();
    EXPECT_EQ(0u, numberOfPC);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTestsWithMockCsrHw2, givenCsrInNonDirtyStateWhenflushTaskIsCalledThenNoFlushIsCalled) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->heaplessStateInitialized = true;

    configureCSRtoNonDirtyState<FamilyType>(true);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0u, mockCsr->flushCalledCount);
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTestsWithMockCsrHw2, givenCsrInNonDirtyStateAndBatchingModeWhenflushTaskIsCalledWithDisabledPreemptionThenSubmissionIsNotRecorded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>(true);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(mockCsr->getHeaplessStateInitEnabled() ? 1u : 0u, mockCsr->flushCalledCount);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    // surfaces are non resident
    auto &surfacesForResidency = mockCsr->getResidencyAllocations();
    EXPECT_EQ(0u, surfacesForResidency.size());
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTestsWithMockCsrHw2, givenCsrInBatchingModeWhenRecordedBatchBufferIsBeingSubmittedThenFlushIsCalledWithRecordedCommandBuffer) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr->getResidencyAllocations().clear();
    mockCsr->heaplessStateInitialized = true;

    auto mockedSubmissionsAggregator = new MockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>(true);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    mockCsr->streamProperties.stateComputeMode.isCoherencyRequired.value = 0;

    mockCsr->flushTask(commandStream,
                       4,
                       &dsh,
                       &ioh,
                       &ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0u, mockCsr->flushCalledCount);

    auto &surfacesForResidency = mockCsr->getResidencyAllocations();
    EXPECT_EQ(0u, surfacesForResidency.size());

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());
    auto cmdBuffer = cmdBufferList.peekHead();

    // preemption allocation + sip kernel
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount -= pDevice->getHardwareInfo().capabilityTable.supportsImages ? 0 : 1;
    csrSurfaceCount += mockCsr->globalFenceAllocation ? 1 : 0;
    csrSurfaceCount += mockCsr->clearColorAllocation ? 1 : 0;
    csrSurfaceCount += pDevice->getRTMemoryBackedBuffer() ? 1 : 0;

    EXPECT_EQ(4u + csrSurfaceCount, cmdBuffer->surfaces.size());

    // copy those surfaces
    std::vector<GraphicsAllocation *> residentSurfaces = cmdBuffer->surfaces;

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_TRUE(graphicsAllocation->isResident(mockCsr->getOsContext().getContextId()));
        EXPECT_EQ(1u, graphicsAllocation->getResidencyTaskCount(mockCsr->getOsContext().getContextId()));
    }

    mockCsr->flushBatchedSubmissions();

    EXPECT_FALSE(mockCsr->recordedCommandBuffer->batchBuffer.lowPriority);
    EXPECT_EQ(mockCsr->recordedCommandBuffer->batchBuffer.commandBufferAllocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(4u, mockCsr->recordedCommandBuffer->batchBuffer.startOffset);
    EXPECT_EQ(1u, mockCsr->flushCalledCount);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCommandBuffers().peekIsEmpty());

    EXPECT_EQ(0u, surfacesForResidency.size());

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_FALSE(graphicsAllocation->isResident(mockCsr->getOsContext().getContextId()));
    }
}

HWCMDTEST_TEMPLATED_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTestsWithMockCsrHw2, givenNothingToFlushWhenFlushTaskCalledThenDontFlushStamp) {
    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());
    mockCsr->heaplessStateInitialized = true;

    configureCSRtoNonDirtyState<FamilyType>(true);

    EXPECT_EQ(0u, mockCsr->flushCalledCount);
    auto previousFlushStamp = mockCsr->flushStamp->peekStamp();
    auto cmplStamp = flushTask(*mockCsr);
    EXPECT_EQ(mockCsr->flushStamp->peekStamp(), previousFlushStamp);
    EXPECT_EQ(previousFlushStamp, cmplStamp.flushStamp);
    EXPECT_EQ(0u, mockCsr->flushCalledCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenEpilogueRequiredFlagWhenTaskIsSubmittedDirectlyThenItPointsBackToCsr) {
    configureCSRtoNonDirtyState<FamilyType>(true);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
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
                                    &dsh,
                                    &ioh,
                                    &ssh,
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
    EXPECT_EQ(batchBufferStart->getBatchBufferStartAddress(), commandStreamReceiverStream.getGraphicsAllocation()->getGpuAddress());

    parseCommands<FamilyType>(commandStreamReceiverStream, 0);

    itBBend = find<typename FamilyType::MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    void *bbEndAddress = *itBBend;

    EXPECT_EQ(commandStreamReceiverStream.getCpuBase(), bbEndAddress);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiverStream.getGraphicsAllocation()));
}

struct CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests : public CommandStreamReceiverFlushTaskXeHPAndLaterTests {
    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(2);
        debugManager.flags.EnableImplicitScaling.set(1);
        parsePipeControl = true;
        CommandStreamReceiverFlushTaskXeHPAndLaterTests::SetUp();
    }

    template <typename GfxFamily>
    void verifyPipeControl(UltCommandStreamReceiver<GfxFamily> &commandStreamReceiver, uint32_t expectedTaskCount, bool workLoadPartition) {
        using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

        uint64_t gpuAddressTagAllocation = commandStreamReceiver.getTagAllocation()->getGpuAddress();

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
                EXPECT_EQ(gpuAddressTagAllocation, NEO::UnitTestHelper<GfxFamily>::getPipeControlPostSyncAddress(*pipeControl));
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
        uint32_t expectedAddressOffsetData = commandStreamReceiver.getImmWritePostSyncWriteOffset();
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
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
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
            givenMultipleStaticActivePartitionsWhenFlushingTagUpdateThenExpectTagUpdatePipeControlWithPartitionFlagOnAndActivePartitionConfig) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    EXPECT_EQ(1u, commandStreamReceiver.activePartitionsConfig);
    commandStreamReceiver.activePartitions = 2;
    commandStreamReceiver.taskCount = 3;
    EXPECT_TRUE(commandStreamReceiver.staticWorkPartitioningEnabled);
    flushTask(commandStreamReceiver, true);
    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.flushTagUpdate());
    EXPECT_EQ(2u, commandStreamReceiver.activePartitionsConfig);

    prepareLinearStream<FamilyType>(commandStream, 0);
    verifyPipeControl<FamilyType>(commandStreamReceiver, 4, true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests,
            givenSingleStaticActivePartitionWhenFlushingTaskThenExpectTagUpdatePipeControlWithoutPartitionFlagOnAndNoActivePartitionConfig) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
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

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests, givenMultipleStaticActivePartitionsWhenFlushingTaskTwiceThenExpectTagUpdatePipeControlWithPartitionFlagOnAndNoActivePartitionConfigAtSecondFlush) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
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

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests,
            givenMultipleDynamicActivePartitionsWhenFlushingTaskTwiceThenExpectTagUpdatePipeControlWithoutPartitionFlagAndPartitionRegisters) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
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
            givenMultipleDynamicActivePartitionsWhenFlushingTagUpdateThenExpectTagUpdatePipeControlWithoutPartitionFlag) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(1);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    commandStreamReceiver.activePartitions = 2;
    commandStreamReceiver.taskCount = 3;
    commandStreamReceiver.staticWorkPartitioningEnabled = false;
    EXPECT_EQ(SubmissionStatus::success, commandStreamReceiver.flushTagUpdate());

    prepareLinearStream<FamilyType>(commandStreamReceiver.commandStream, 0);
    verifyPipeControl<FamilyType>(commandStreamReceiver, 4, false);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPAndLaterMultiTileTests,
            givenMultipleStaticActivePartitionsAndDirectSubmissionActiveWhenFlushingTaskThenExpectTagUpdatePipeControlWithPartitionFlagOnAndNoActivePartitionConfig) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    commandStreamReceiver.directSubmission = std::make_unique<MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>>(commandStreamReceiver);
    commandStreamReceiver.directSubmissionAvailable = true;

    EXPECT_EQ(1u, commandStreamReceiver.activePartitionsConfig);
    EXPECT_EQ(2u, commandStreamReceiver.activePartitions);
    EXPECT_TRUE(commandStreamReceiver.staticWorkPartitioningEnabled);

    commandStreamReceiver.taskCount = 3;
    flushTask(commandStreamReceiver, true);
    EXPECT_EQ(1u, commandStreamReceiver.activePartitionsConfig);

    prepareLinearStream<FamilyType>(commandStream, 0);
    verifyPipeControl<FamilyType>(commandStreamReceiver, 4, true);

    prepareLinearStream<FamilyType>(commandStreamReceiver.commandStream, 0);
    verifyActivePartitionConfig<FamilyType>(commandStreamReceiver, false);
}
