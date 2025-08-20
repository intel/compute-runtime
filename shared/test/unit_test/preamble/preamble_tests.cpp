/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipeline_select_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <gtest/gtest.h>

#include <algorithm>

using PreambleTest = ::testing::Test;

using namespace NEO;

HWTEST_F(PreambleTest, givenDisabledPreemptioWhenPreambleAdditionalCommandsSizeIsQueriedThenZeroIsReturned) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setPreemptionMode(PreemptionMode::Disabled);

    auto cmdSize = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    EXPECT_EQ(PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice), cmdSize);
    EXPECT_EQ(0u, cmdSize);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, PreambleTest, givenMidthreadPreemptionWhenPreambleAdditionalCommandsSizeIsQueriedThenSizeForPreemptionPreambleIsReturned) {
    using GPGPU_CSR_BASE_ADDRESS = typename FamilyType::GPGPU_CSR_BASE_ADDRESS;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);

        auto cmdSize = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
        EXPECT_EQ(PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice), cmdSize);
        EXPECT_EQ(sizeof(GPGPU_CSR_BASE_ADDRESS), cmdSize);
    }
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, PreambleTest, givenMidThreadPreemptionWhenPreambleIsProgrammedThenStateSipAndCsrBaseAddressCmdsAreAdded) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    using GPGPU_CSR_BASE_ADDRESS = typename FamilyType::GPGPU_CSR_BASE_ADDRESS;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    auto cmdSizePreemptionDisabled = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice, false);
    EXPECT_EQ(0u, cmdSizePreemptionDisabled);

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);
        auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice, false);
        EXPECT_LT(cmdSizePreemptionDisabled, cmdSizePreemptionMidThread);

        StackVec<char, 8192> preambleBuffer(8192);
        LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

        StackVec<char, 4096> preemptionBuffer;
        preemptionBuffer.resize(cmdSizePreemptionMidThread);
        LinearStream preemptionStream(&*preemptionBuffer.begin(), preemptionBuffer.size());

        uintptr_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
        MockGraphicsAllocation csrSurface(reinterpret_cast<void *>(minCsrAlignment), 1024);

        PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U, &csrSurface, false);

        PreemptionHelper::programStateSip<FamilyType>(preemptionStream, *mockDevice, nullptr);

        HardwareParse hwParserPreamble;
        hwParserPreamble.parseCommands<FamilyType>(preambleStream, 0);

        auto csrCmd = hwParserPreamble.getCommand<GPGPU_CSR_BASE_ADDRESS>();
        EXPECT_NE(nullptr, csrCmd);
        EXPECT_EQ(csrSurface.getGpuAddress(), csrCmd->getGpgpuCsrBaseAddress());

        HardwareParse hwParserPreemption;
        hwParserPreemption.parseCommands<FamilyType>(preemptionStream, 0);

        auto stateSipCmd = hwParserPreemption.getCommand<STATE_SIP>();
        EXPECT_NE(nullptr, stateSipCmd);
    }
}

HWTEST_F(PreambleTest, givenActiveKernelDebuggingWhenPreambleKernelDebuggingCommandsSizeIsQueriedThenCorrectSizeIsReturned) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto size = PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(true);
    auto expectedProgrammedCmdsCount = UnitTestHelper<FamilyType>::getMiLoadRegisterImmProgrammedCmdsCount(true);
    auto expectedSize = expectedProgrammedCmdsCount * sizeof(MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(expectedSize, size);
}

HWTEST_F(PreambleTest, givenInactiveKernelDebuggingWhenPreambleKernelDebuggingCommandsSizeIsQueriedThenZeroIsReturned) {
    auto size = PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(false);
    EXPECT_EQ(0u, size);
}

HWTEST_F(PreambleTest, givenDebuggerInitializedAndMidThreadPreemptionWhenGetAdditionalCommandsSizeIsCalledThen2MiLoadRegisterImmCmdsAreAddedInsteadOfBasePreambleProgramming) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);

    size_t withoutDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);

    mockDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(mockDevice.get());
    size_t withDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    EXPECT_LT(withoutDebugging, withDebugging);

    auto expectedProgrammedCmdsCount = UnitTestHelper<FamilyType>::getMiLoadRegisterImmProgrammedCmdsCount(true);
    size_t sizeExpected = expectedProgrammedCmdsCount * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(sizeExpected, withDebugging);
}

HWTEST_F(PreambleTest, givenDefaultPreambleWhenGetThreadsMaxNumberIsCalledThenMaximumNumberOfThreadsIsReturned) {
    const HardwareInfo &hwInfo = *defaultHwInfo;
    uint32_t threadsPerEU = hwInfo.gtSystemInfo.NumThreadsPerEu + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    uint32_t value = GfxCoreHelper::getMaxThreadsForVfe(hwInfo);

    uint32_t expected = hwInfo.gtSystemInfo.EUCount * threadsPerEU;
    EXPECT_EQ(expected, value);
}

HWTEST_F(PreambleTest, givenMaxHwThreadsPercentDebugVariableWhenGetThreadsMaxNumberIsCalledThenMaximumNumberOfThreadsIsCappedToRequestedNumber) {
    const HardwareInfo &hwInfo = *defaultHwInfo;
    uint32_t threadsPerEU = hwInfo.gtSystemInfo.NumThreadsPerEu + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.MaxHwThreadsPercent.set(80);
    uint32_t value = GfxCoreHelper::getMaxThreadsForVfe(hwInfo);

    uint32_t expected = int(hwInfo.gtSystemInfo.EUCount * threadsPerEU * 80 / 100.0f);
    EXPECT_EQ(expected, value);
}

HWTEST_F(PreambleTest, givenMinHwThreadsUnoccupiedDebugVariableWhenGetThreadsMaxNumberIsCalledThenMaximumNumberOfThreadsIsCappedToMatchRequestedNumber) {
    const HardwareInfo &hwInfo = *defaultHwInfo;
    uint32_t threadsPerEU = hwInfo.gtSystemInfo.NumThreadsPerEu + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.MinHwThreadsUnoccupied.set(2);
    uint32_t value = GfxCoreHelper::getMaxThreadsForVfe(hwInfo);

    uint32_t expected = hwInfo.gtSystemInfo.EUCount * threadsPerEU - 2;
    EXPECT_EQ(expected, value);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, PreambleTest, WhenProgramVFEStateIsCalledThenCorrectVfeStateAddressIsReturned) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    char buffer[64];
    MockGraphicsAllocation graphicsAllocation(buffer, sizeof(buffer));
    LinearStream preambleStream(&graphicsAllocation, graphicsAllocation.getUnderlyingBuffer(), graphicsAllocation.getUnderlyingBufferSize());
    uint64_t addressToPatch = 0xC0DEC0DE;
    uint64_t expectedAddress = 0xC0DEC000;

    uint64_t expectedGpuAddress = preambleStream.getCurrentGpuAddressPosition() + sizeof(PIPE_CONTROL);
    uint64_t cmdBufferGpuAddress = 0;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&preambleStream, *defaultHwInfo, EngineGroupType::renderCompute, &cmdBufferGpuAddress);
    EXPECT_EQ(expectedGpuAddress, cmdBufferGpuAddress);
    StreamProperties emptyProperties{};
    MockExecutionEnvironment executionEnvironment{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *executionEnvironment.rootDeviceEnvironments[0], 1024u, addressToPatch, 10u, emptyProperties);
    EXPECT_GE(reinterpret_cast<uintptr_t>(pVfeCmd), reinterpret_cast<uintptr_t>(preambleStream.getCpuBase()));
    EXPECT_LT(reinterpret_cast<uintptr_t>(pVfeCmd), reinterpret_cast<uintptr_t>(preambleStream.getCpuBase()) + preambleStream.getUsed());

    auto &vfeCmd = *reinterpret_cast<MEDIA_VFE_STATE *>(pVfeCmd);
    EXPECT_EQ(10u, vfeCmd.getMaximumNumberOfThreads());
    EXPECT_EQ(1u, vfeCmd.getNumberOfUrbEntries());
    EXPECT_EQ(expectedAddress, vfeCmd.getScratchSpaceBasePointer());
    EXPECT_EQ(0u, vfeCmd.getScratchSpaceBasePointerHigh());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, PreambleTest, WhenGetScratchSpaceAddressOffsetForVfeStateIsCalledThenCorrectOffsetIsReturned) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    char buffer[64];
    MockGraphicsAllocation graphicsAllocation(buffer, sizeof(buffer));
    LinearStream preambleStream(&graphicsAllocation, graphicsAllocation.getUnderlyingBuffer(), graphicsAllocation.getUnderlyingBufferSize());
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    FlatBatchBufferHelperHw<FamilyType> helper(*mockDevice->getExecutionEnvironment());
    uint64_t addressToPatch = 0xC0DEC0DE;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&preambleStream, mockDevice->getHardwareInfo(), EngineGroupType::renderCompute, nullptr);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, mockDevice->getRootDeviceEnvironment(), 1024u, addressToPatch, 10u, emptyProperties);

    auto offset = PreambleHelper<FamilyType>::getScratchSpaceAddressOffsetForVfeState(&preambleStream, pVfeCmd);
    EXPECT_NE(0u, offset);
    EXPECT_EQ(MEDIA_VFE_STATE::PATCH_CONSTANTS::SCRATCHSPACEBASEPOINTER_BYTEOFFSET + reinterpret_cast<uintptr_t>(pVfeCmd),
              offset + reinterpret_cast<uintptr_t>(preambleStream.getCpuBase()));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, PreambleTest, WhenIsSystolicModeConfigurableThenReturnFalse) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto result = PreambleHelper<FamilyType>::isSystolicModeConfigurable(rootDeviceEnvironment);
    EXPECT_FALSE(result);
}

HWTEST_F(PreambleTest, givenSetForceSemaphoreDelayBetweenWaitsWhenProgramSemaphoreDelayThenSemaWaitPollRegisterIsProgrammed) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    DebugManagerStateRestore debugManagerStateRestore;
    uint32_t newDelay = 10u;
    debugManager.flags.ForceSemaphoreDelayBetweenWaits.set(newDelay);

    auto bufferSize = PreambleHelper<FamilyType>::getSemaphoreDelayCommandSize();
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), bufferSize);
    auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);

    LinearStream stream(buffer.get(), bufferSize);
    PreambleHelper<FamilyType>::programSemaphoreDelay(&stream, false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream);
    auto cmdList = hwParser.getCommandsList<MI_LOAD_REGISTER_IMM>();
    ASSERT_EQ(1u, cmdList.size());

    auto it = cmdList.begin();

    MI_LOAD_REGISTER_IMM *pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*it);
    EXPECT_EQ(static_cast<uint32_t>(0x224c), pCmd->getRegisterOffset());
    EXPECT_EQ(newDelay, pCmd->getDataDword());
}

HWTEST_F(PreambleTest, givenNotSetForceSemaphoreDelayBetweenWaitsWhenProgramSemaphoreDelayThenSemaWaitPollRegisterIsNotProgrammed) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.ForceSemaphoreDelayBetweenWaits.set(-1);

    auto bufferSize = PreambleHelper<FamilyType>::getSemaphoreDelayCommandSize();
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), bufferSize);
    auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);

    LinearStream stream(buffer.get(), bufferSize);
    PreambleHelper<FamilyType>::programSemaphoreDelay(&stream, false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream);
    auto cmdList = hwParser.getCommandsList<MI_LOAD_REGISTER_IMM>();
    ASSERT_EQ(0u, cmdList.size());
}

HWTEST2_F(PreambleTest, whenCleanStateInPreambleIsSetAndProgramPipelineSelectIsCalledThenExtraPipelineSelectAndTwoExtraPipeControlsAdded, IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore stateRestore;
    debugManager.flags.CleanStateInPreamble.set(true);

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    constexpr size_t bufferSize = 256;
    uint8_t buffer[bufferSize];
    LinearStream stream(buffer, bufferSize);

    PipelineSelectArgs pipelineArgs;

    PreambleHelper<FamilyType>::programPipelineSelect(&stream, pipelineArgs, mockDevice->getRootDeviceEnvironment());
    size_t usedSpace = stream.getUsed();

    EXPECT_EQ(usedSpace, PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(mockDevice->getRootDeviceEnvironment()));

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream);

    auto numPipeControl = hwParser.getCommandsList<PIPE_CONTROL>().size();
    auto expectedNumPipeControl = 2u;
    if (MemorySynchronizationCommands<FamilyType>::isBarrierPriorToPipelineSelectWaRequired(mockDevice->getRootDeviceEnvironment())) {
        expectedNumPipeControl++;
    }
    EXPECT_EQ(expectedNumPipeControl, numPipeControl);
    auto numPipelineSelect = hwParser.getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(2u, numPipelineSelect);
}

HWTEST2_F(PreambleTest, GivenAtLeastXeHpCoreWhenPreambleRetrievesUrbEntryAllocationSizeThenValueIsCorrect, IsAtLeastXeCore) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(0u, actualVal);
}

using PreambleHwTest = PreambleFixture;

HWTEST2_F(PreambleHwTest, GivenAtLeastXeHpCoreWhenPreambleAddsPipeControlBeforeCommandThenExpectNothingToAdd, IsAtLeastXeCore) {
    constexpr size_t bufferSize = 64;
    uint8_t buffer[bufferSize];
    LinearStream stream(buffer, bufferSize);

    auto &hwInfo = pDevice->getHardwareInfo();

    PreambleHelper<FamilyType>::addPipeControlBeforeVfeCmd(&stream, &hwInfo, EngineGroupType::compute);
    EXPECT_EQ(0u, stream.getUsed());
}

HWTEST2_F(PreambleHwTest, givenHwWithForcedHeaplessModeWhenCallingSetSingleSliceDispatchModeThenDoNothing, IsAtLeastXeCore) {
    if (FamilyType::isHeaplessRequired() == false) {
        GTEST_SKIP();
    }
    PreambleHelper<FamilyType>::setSingleSliceDispatchMode(nullptr, true);
}
