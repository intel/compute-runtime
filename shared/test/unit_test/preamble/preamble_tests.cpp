/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "reg_configs_common.h"
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

HWCMDTEST_F(IGFX_GEN8_CORE, PreambleTest, givenMidthreadPreemptionWhenPreambleAdditionalCommandsSizeIsQueriedThenSizeForPreemptionPreambleIsReturned) {
    using GPGPU_CSR_BASE_ADDRESS = typename FamilyType::GPGPU_CSR_BASE_ADDRESS;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);

        auto cmdSize = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
        EXPECT_EQ(PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice), cmdSize);
        EXPECT_EQ(sizeof(GPGPU_CSR_BASE_ADDRESS), cmdSize);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, PreambleTest, givenMidThreadPreemptionWhenPreambleIsProgrammedThenStateSipAndCsrBaseAddressCmdsAreAdded) {
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

        PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U, &csrSurface);

        PreemptionHelper::programStateSip<FamilyType>(preemptionStream, *mockDevice);

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
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    auto size = PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(true);
    auto sizeExpected = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(sizeExpected, size);
}

HWTEST_F(PreambleTest, givenInactiveKernelDebuggingWhenPreambleKernelDebuggingCommandsSizeIsQueriedThenZeroIsReturned) {
    auto size = PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(false);
    EXPECT_EQ(0u, size);
}

HWTEST_F(PreambleTest, whenKernelDebuggingCommandsAreProgrammedThenCorrectCommandsArePlacedIntoStream) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    auto bufferSize = PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(true);
    auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);

    LinearStream stream(buffer.get(), bufferSize);
    PreambleHelper<FamilyType>::programKernelDebugging(&stream);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream);
    auto cmdList = hwParser.getCommandsList<MI_LOAD_REGISTER_IMM>();

    ASSERT_EQ(2u, cmdList.size());

    auto it = cmdList.begin();

    MI_LOAD_REGISTER_IMM *pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*it);
    EXPECT_EQ(UnitTestHelper<FamilyType>::getDebugModeRegisterOffset(), pCmd->getRegisterOffset());
    EXPECT_EQ(UnitTestHelper<FamilyType>::getDebugModeRegisterValue(), pCmd->getDataDword());
    it++;

    pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*it);
    EXPECT_EQ(UnitTestHelper<FamilyType>::getTdCtlRegisterOffset(), pCmd->getRegisterOffset());
    EXPECT_EQ(UnitTestHelper<FamilyType>::getTdCtlRegisterValue(), pCmd->getDataDword());
}

HWTEST_F(PreambleTest, givenKernelDebuggingActiveWhenPreambleIsProgrammedThenProgramKernelDebuggingIsCalled) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    mockDevice->setDebuggerActive(false);

    StackVec<char, 8192> preambleBuffer(8192);
    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(preambleStream);
    auto cmdList = hwParser.getCommandsList<MI_LOAD_REGISTER_IMM>();

    auto miLoadRegImmCountWithoutDebugging = cmdList.size();

    mockDevice->setDebuggerActive(true);
    auto preemptionAllocation = mockDevice->getGpgpuCommandStreamReceiver().getPreemptionAllocation();

    StackVec<char, 8192> preambleBuffer2(8192);
    preambleStream.replaceBuffer(&*preambleBuffer2.begin(), preambleBuffer2.size());
    PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U, preemptionAllocation);
    HardwareParse hwParser2;
    hwParser2.parseCommands<FamilyType>(preambleStream);
    cmdList = hwParser2.getCommandsList<MI_LOAD_REGISTER_IMM>();

    auto miLoadRegImmCountWithDebugging = cmdList.size();
    ASSERT_LT(miLoadRegImmCountWithoutDebugging, miLoadRegImmCountWithDebugging);
    EXPECT_EQ(2u, miLoadRegImmCountWithDebugging - miLoadRegImmCountWithoutDebugging);
}

HWTEST_F(PreambleTest, givenKernelDebuggingActiveAndMidThreadPreemptionWhenGetAdditionalCommandsSizeIsCalledThen2MiLoadRegisterImmCmdsAreAdded) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);

    mockDevice->setDebuggerActive(false);
    size_t withoutDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    mockDevice->setDebuggerActive(true);
    size_t withDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    EXPECT_LT(withoutDebugging, withDebugging);

    size_t diff = withDebugging - withoutDebugging;
    size_t sizeExpected = 2 * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(sizeExpected, diff);
}

HWTEST_F(PreambleTest, givenDefaultPreambleWhenGetThreadsMaxNumberIsCalledThenMaximumNumberOfThreadsIsReturned) {
    const HardwareInfo &hwInfo = *defaultHwInfo;
    uint32_t threadsPerEU = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount) + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    uint32_t value = HwHelper::getMaxThreadsForVfe(hwInfo);

    uint32_t expected = hwInfo.gtSystemInfo.EUCount * threadsPerEU;
    EXPECT_EQ(expected, value);
}

HWTEST_F(PreambleTest, givenMaxHwThreadsPercentDebugVariableWhenGetThreadsMaxNumberIsCalledThenMaximumNumberOfThreadsIsCappedToRequestedNumber) {
    const HardwareInfo &hwInfo = *defaultHwInfo;
    uint32_t threadsPerEU = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount) + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    DebugManagerStateRestore debugManagerStateRestore;
    DebugManager.flags.MaxHwThreadsPercent.set(80);
    uint32_t value = HwHelper::getMaxThreadsForVfe(hwInfo);

    uint32_t expected = int(hwInfo.gtSystemInfo.EUCount * threadsPerEU * 80 / 100.0f);
    EXPECT_EQ(expected, value);
}

HWTEST_F(PreambleTest, givenMinHwThreadsUnoccupiedDebugVariableWhenGetThreadsMaxNumberIsCalledThenMaximumNumberOfThreadsIsCappedToMatchRequestedNumber) {
    const HardwareInfo &hwInfo = *defaultHwInfo;
    uint32_t threadsPerEU = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount) + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    DebugManagerStateRestore debugManagerStateRestore;
    DebugManager.flags.MinHwThreadsUnoccupied.set(2);
    uint32_t value = HwHelper::getMaxThreadsForVfe(hwInfo);

    uint32_t expected = hwInfo.gtSystemInfo.EUCount * threadsPerEU - 2;
    EXPECT_EQ(expected, value);
}

HWCMDTEST_F(IGFX_GEN8_CORE, PreambleTest, WhenProgramVFEStateIsCalledThenCorrectVfeStateAddressIsReturned) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    char buffer[64];
    MockGraphicsAllocation graphicsAllocation(buffer, sizeof(buffer));
    LinearStream preambleStream(&graphicsAllocation, graphicsAllocation.getUnderlyingBuffer(), graphicsAllocation.getUnderlyingBufferSize());
    uint64_t addressToPatch = 0xC0DEC0DE;
    uint64_t expectedAddress = 0xC0DEC000;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&preambleStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 1024u, addressToPatch, 10u, emptyProperties);
    EXPECT_GE(reinterpret_cast<uintptr_t>(pVfeCmd), reinterpret_cast<uintptr_t>(preambleStream.getCpuBase()));
    EXPECT_LT(reinterpret_cast<uintptr_t>(pVfeCmd), reinterpret_cast<uintptr_t>(preambleStream.getCpuBase()) + preambleStream.getUsed());

    auto &vfeCmd = *reinterpret_cast<MEDIA_VFE_STATE *>(pVfeCmd);
    EXPECT_EQ(10u, vfeCmd.getMaximumNumberOfThreads());
    EXPECT_EQ(1u, vfeCmd.getNumberOfUrbEntries());
    EXPECT_EQ(expectedAddress, vfeCmd.getScratchSpaceBasePointer());
    EXPECT_EQ(0u, vfeCmd.getScratchSpaceBasePointerHigh());
}

HWCMDTEST_F(IGFX_GEN8_CORE, PreambleTest, WhenGetScratchSpaceAddressOffsetForVfeStateIsCalledThenCorrectOffsetIsReturned) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    char buffer[64];
    MockGraphicsAllocation graphicsAllocation(buffer, sizeof(buffer));
    LinearStream preambleStream(&graphicsAllocation, graphicsAllocation.getUnderlyingBuffer(), graphicsAllocation.getUnderlyingBufferSize());
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    FlatBatchBufferHelperHw<FamilyType> helper(*mockDevice->getExecutionEnvironment());
    uint64_t addressToPatch = 0xC0DEC0DE;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&preambleStream, mockDevice->getHardwareInfo(), EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, mockDevice->getHardwareInfo(), 1024u, addressToPatch, 10u, emptyProperties);

    auto offset = PreambleHelper<FamilyType>::getScratchSpaceAddressOffsetForVfeState(&preambleStream, pVfeCmd);
    EXPECT_NE(0u, offset);
    EXPECT_EQ(MEDIA_VFE_STATE::PATCH_CONSTANTS::SCRATCHSPACEBASEPOINTER_BYTEOFFSET + reinterpret_cast<uintptr_t>(pVfeCmd),
              offset + reinterpret_cast<uintptr_t>(preambleStream.getCpuBase()));
}

HWCMDTEST_F(IGFX_GEN8_CORE, PreambleTest, WhenIsSystolicModeConfigurableThenReturnFalse) {
    auto result = PreambleHelper<FamilyType>::isSystolicModeConfigurable(*defaultHwInfo);
    EXPECT_FALSE(result);
}

HWCMDTEST_F(IGFX_GEN8_CORE, PreambleTest, WhenAppendProgramPipelineSelectThenNothingChanged) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    PIPELINE_SELECT cmd = FamilyType::cmdInitPipelineSelect;
    cmd.setMaskBits(pipelineSelectEnablePipelineSelectMaskBits);
    PreambleHelper<FamilyType>::appendProgramPipelineSelect(&cmd, true, *defaultHwInfo);
    EXPECT_EQ(pipelineSelectEnablePipelineSelectMaskBits, cmd.getMaskBits());
}

HWTEST_F(PreambleTest, givenSetForceSemaphoreDelayBetweenWaitsWhenProgramSemaphoreDelayThenSemaWaitPollRegisterIsProgrammed) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    DebugManagerStateRestore debugManagerStateRestore;
    uint32_t newDelay = 10u;
    DebugManager.flags.ForceSemaphoreDelayBetweenWaits.set(newDelay);

    auto bufferSize = PreambleHelper<FamilyType>::getSemaphoreDelayCommandSize();
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), bufferSize);
    auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);

    LinearStream stream(buffer.get(), bufferSize);
    PreambleHelper<FamilyType>::programSemaphoreDelay(&stream);

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
    DebugManager.flags.ForceSemaphoreDelayBetweenWaits.set(-1);

    auto bufferSize = PreambleHelper<FamilyType>::getSemaphoreDelayCommandSize();
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), bufferSize);
    auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);

    LinearStream stream(buffer.get(), bufferSize);
    PreambleHelper<FamilyType>::programSemaphoreDelay(&stream);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream);
    auto cmdList = hwParser.getCommandsList<MI_LOAD_REGISTER_IMM>();
    ASSERT_EQ(0u, cmdList.size());
}
