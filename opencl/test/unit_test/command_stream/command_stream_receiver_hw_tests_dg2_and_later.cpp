/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"

using namespace NEO;
using MatcherIsRTCapable = IsAtLeastXeCore;

struct CommandStreamReceiverHwTestDg2AndLater : public DeviceFixture,
                                                public HardwareParse,
                                                public ::testing::Test {

    void SetUp() override {
        DeviceFixture::setUp();
        HardwareParse::setUp();
    }

    void TearDown() override {
        HardwareParse::tearDown();
        DeviceFixture::tearDown();
    }
};

HWTEST2_F(CommandStreamReceiverHwTestDg2AndLater, givenGen12AndLaterWhenRayTracingEnabledThenCommandIsAddedToBatchBuffer, MatcherIsRTCapable) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto osContext = pDevice->getDefaultEngine().osContext;
    commandStreamReceiver.setupContext(*osContext);

    auto cmdSize = commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo());
    EXPECT_EQ(sizeof(_3DSTATE_BTD), cmdSize);

    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::scratchSurface, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    commandStreamReceiver.perDssBackedBuffer = allocation;
    std::unique_ptr<char[]> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.usePerDssBackedBuffer = true;

    EXPECT_FALSE(commandStreamReceiver.isPerDssBackedBufferSent);
    commandStreamReceiver.programPerDssBackedBuffer(cs, *pDevice, dispatchFlags);
    EXPECT_EQ(sizeof(_3DSTATE_BTD), cs.getUsed());

    _3DSTATE_BTD *cmd = genCmdCast<_3DSTATE_BTD *>(cs.getCpuBase());
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(RayTracingHelper::getMemoryBackedFifoSizeToPatch(), cmd->getPerDssMemoryBackedBufferSize());
    EXPECT_EQ(allocation->getGpuAddressToPatch(), cmd->getMemoryBackedBufferBasePointer());
    EXPECT_TRUE(commandStreamReceiver.isPerDssBackedBufferSent);
}

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTaskDg2AndLaterTests;

HWTEST2_F(CommandStreamReceiverFlushTaskDg2AndLaterTests, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenPerDssBackedBufferThenThereIsPipeControlPriorToIt, IsDG2) {
    DebugManagerStateRestore restore;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;

    auto expectedCmdSize = sizeof(_3DSTATE_BTD) + sizeof(PIPE_CONTROL);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto cmdSize = commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo());
    EXPECT_EQ(expectedCmdSize, cmdSize);

    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::scratchSurface, pDevice->getDeviceBitfield());
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
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    dispatchFlags.threadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();

    commandStreamReceiver.streamProperties.stateComputeMode.setPropertiesAll(false, dispatchFlags.numGrfRequired,
                                                                             dispatchFlags.threadArbitrationPolicy, PreemptionMode::Disabled);
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
    EXPECT_EQ(RayTracingHelper::getMemoryBackedFifoSizeToPatch(), cmd3dStateBtdCmd->getPerDssMemoryBackedBufferSize());
    EXPECT_EQ(allocation->getGpuAddressToPatch(), cmd3dStateBtdCmd->getMemoryBackedBufferBasePointer());
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
    auto osContext = pDevice->getDefaultEngine().osContext;
    commandStreamReceiver.setupContext(*osContext);

    auto cmdSize = commandStreamReceiver.getCmdSizeForPerDssBackedBuffer(pDevice->getHardwareInfo());
    EXPECT_EQ(sizeof(_3DSTATE_BTD), cmdSize);

    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::scratchSurface, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    commandStreamReceiver.perDssBackedBuffer = allocation;
    std::unique_ptr<char[]> buffer(new char[cmdSize]);
    LinearStream cs(buffer.get(), cmdSize);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.usePerDssBackedBuffer = true;
    commandStreamReceiver.isPerDssBackedBufferSent = true;

    commandStreamReceiver.programPerDssBackedBuffer(cs, *pDevice, dispatchFlags);
    EXPECT_EQ(0u, cs.getUsed());
}

HWTEST2_F(CommandStreamReceiverHwTestDg2AndLater, givenNotXE_HPG_COREWhenCheckingNewResourceImplicitFlushThenReturnFalse, IsNotXeHpgCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto osContext = pDevice->getDefaultEngine().osContext;
    commandStreamReceiver.setupContext(*osContext);

    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsNewResourceImplicitFlush());
}

HWTEST2_F(CommandStreamReceiverHwTestDg2AndLater, givenNotXE_HP_COREWhenCheckingNewResourceGpuIdleThenReturnFalse, IsAtLeastXeCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto osContext = pDevice->getDefaultEngine().osContext;
    commandStreamReceiver.setupContext(*osContext);

    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsGpuIdleImplicitFlush());
}
