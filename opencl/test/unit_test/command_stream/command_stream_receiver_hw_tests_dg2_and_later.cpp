/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"

using namespace NEO;
using MatcherIsRTCapable = IsAtLeastXeHpgCore;

struct CommandStreamReceiverHwTestDg2AndLater : public ClDeviceFixture,
                                                public HardwareParse,
                                                public ::testing::Test {

    void SetUp() override {
        ClDeviceFixture::SetUp();
        HardwareParse::SetUp();
    }

    void TearDown() override {
        HardwareParse::TearDown();
        ClDeviceFixture::TearDown();
    }
};

HWTEST2_F(CommandStreamReceiverHwTestDg2AndLater, givenGen12AndLaterWhenRayTracingEnabledThenCommandIsAddedToBatchBuffer, MatcherIsRTCapable) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    auto cmdSize = commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo());
    EXPECT_EQ(sizeof(_3DSTATE_BTD), cmdSize);

    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::SCRATCH_SURFACE, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    commandStreamReceiver.perDssBackedBuffer = allocation;
    std::unique_ptr<char> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.usePerDssBackedBuffer = true;

    EXPECT_FALSE(commandStreamReceiver.isPerDssBackedBufferSent);
    commandStreamReceiver.programPerDssBackedBuffer(cs, *pDevice, dispatchFlags);
    EXPECT_EQ(sizeof(_3DSTATE_BTD), cs.getUsed());

    _3DSTATE_BTD *cmd = genCmdCast<_3DSTATE_BTD *>(cs.getCpuBase());
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(RayTracingHelper::getMemoryBackedFifoSizeToPatch(), cmd->getBtdStateBody().getPerDssMemoryBackedBufferSize());
    EXPECT_EQ(allocation->getGpuAddressToPatch(), cmd->getBtdStateBody().getMemoryBackedBufferBasePointer());
    EXPECT_TRUE(commandStreamReceiver.isPerDssBackedBufferSent);
}

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTaskDg2AndLaterTests;

HWTEST2_F(CommandStreamReceiverFlushTaskDg2AndLaterTests, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenPerDssBackedBufferThenThereIsPipeControlPriorToIt, MatcherIsRTCapable) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;

    auto expectedCmdSize = sizeof(_3DSTATE_BTD) + sizeof(PIPE_CONTROL);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto cmdSize = commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo());
    EXPECT_EQ(expectedCmdSize, cmdSize);

    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::SCRATCH_SURFACE, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    commandStreamReceiver.perDssBackedBuffer = allocation;
    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    EXPECT_FALSE(commandStreamReceiver.isPerDssBackedBufferSent);
    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.usePerDssBackedBuffer = true;
    auto &hwHelper = NEO::HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    dispatchFlags.threadArbitrationPolicy = hwHelper.getDefaultThreadArbitrationPolicy();

    commandStreamReceiver.streamProperties.stateComputeMode.setProperties(dispatchFlags.requiresCoherency, dispatchFlags.numGrfRequired,
                                                                          dispatchFlags.threadArbitrationPolicy, PreemptionMode::Disabled, *defaultHwInfo);
    auto cmdSizeForAllCommands = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    dispatchFlags,
                                    *pDevice);

    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));

    auto cmd3dStateBtdIterator = find<_3DSTATE_BTD *>(cmdList.begin(), cmdList.end());
    auto cmd3dStateBtdCmd = genCmdCast<_3DSTATE_BTD *>(*cmd3dStateBtdIterator);

    ASSERT_NE(nullptr, cmd3dStateBtdCmd);
    EXPECT_EQ(RayTracingHelper::getMemoryBackedFifoSizeToPatch(), cmd3dStateBtdCmd->getBtdStateBody().getPerDssMemoryBackedBufferSize());
    EXPECT_EQ(allocation->getGpuAddressToPatch(), cmd3dStateBtdCmd->getBtdStateBody().getMemoryBackedBufferBasePointer());
    EXPECT_TRUE(commandStreamReceiver.isPerDssBackedBufferSent);

    --cmd3dStateBtdIterator;
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmd3dStateBtdIterator);

    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControlCmd));
    EXPECT_TRUE(pipeControlCmd->getAmfsFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());

    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    &dsh,
                                    &ioh,
                                    &ssh,
                                    taskLevel,
                                    dispatchFlags,
                                    *pDevice);
    auto cmdSizeForAllCommandsWithoutPCand3dState = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);
    EXPECT_EQ(cmdSizeForAllCommandsWithoutPCand3dState + expectedCmdSize, cmdSizeForAllCommands);
}

HWTEST2_F(CommandStreamReceiverHwTestDg2AndLater, givenGen12AndLaterWhenRayTracingEnabledButAlreadySentThenCommandIsNotAddedToBatchBuffer, MatcherIsRTCapable) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    auto cmdSize = commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo());
    EXPECT_EQ(sizeof(_3DSTATE_BTD), cmdSize);

    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::SCRATCH_SURFACE, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    commandStreamReceiver.perDssBackedBuffer = allocation;
    std::unique_ptr<char> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.usePerDssBackedBuffer = true;
    commandStreamReceiver.isPerDssBackedBufferSent = true;

    commandStreamReceiver.programPerDssBackedBuffer(cs, *pDevice, dispatchFlags);
    EXPECT_EQ(0u, cs.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTestDg2AndLater, givenNotXE_HP_COREWhenCheckingNewResourceImplicitFlushThenReturnFalse, IsAtLeastXeHpgCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsNewResourceImplicitFlush());
}

HWTEST2_F(CommandStreamReceiverHwTestDg2AndLater, givenNotXE_HP_COREWhenCheckingNewResourceGpuIdleThenReturnFalse, IsAtLeastXeHpgCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(*osContext);

    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsGpuIdleImplicitFlush());
}
