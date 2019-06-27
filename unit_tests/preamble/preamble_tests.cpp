/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/preamble.h"
#include "runtime/utilities/stackvec.h"
#include "test.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

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
    auto cmdSizePreemptionDisabled = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice);
    EXPECT_EQ(0u, cmdSizePreemptionDisabled);

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);
        auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice);
        EXPECT_LT(cmdSizePreemptionDisabled, cmdSizePreemptionMidThread);

        StackVec<char, 8192> preambleBuffer(8192);
        LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

        StackVec<char, 4096> preemptionBuffer;
        preemptionBuffer.resize(cmdSizePreemptionMidThread);
        LinearStream preemptionStream(&*preemptionBuffer.begin(), preemptionBuffer.size());

        uintptr_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
        MockGraphicsAllocation csrSurface(reinterpret_cast<void *>(minCsrAlignment), 1024);

        PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U,
                                                    ThreadArbitrationPolicy::RoundRobin, &csrSurface);

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
    EXPECT_EQ(DebugModeRegisterOffset::registerOffset, pCmd->getRegisterOffset());
    EXPECT_EQ(DebugModeRegisterOffset::debugEnabledValue, pCmd->getDataDword());

    it++;

    pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*it);
    EXPECT_EQ(TdDebugControlRegisterOffset::registerOffset, pCmd->getRegisterOffset());
    EXPECT_EQ(TdDebugControlRegisterOffset::debugEnabledValue, pCmd->getDataDword());
}

HWTEST_F(PreambleTest, givenKernelDebuggingActiveWhenPreambleIsProgrammedThenProgramKernelDebuggingIsCalled) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    mockDevice->setSourceLevelDebuggerActive(false);

    StackVec<char, 8192> preambleBuffer(8192);
    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U,
                                                ThreadArbitrationPolicy::RoundRobin, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(preambleStream);
    auto cmdList = hwParser.getCommandsList<MI_LOAD_REGISTER_IMM>();

    auto miLoadRegImmCountWithoutDebugging = cmdList.size();

    mockDevice->setSourceLevelDebuggerActive(true);
    auto preemptionAllocation = mockDevice->getCommandStreamReceiver().getPreemptionAllocation();

    StackVec<char, 8192> preambleBuffer2(8192);
    preambleStream.replaceBuffer(&*preambleBuffer2.begin(), preambleBuffer2.size());
    PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U,
                                                ThreadArbitrationPolicy::RoundRobin, preemptionAllocation);
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

    mockDevice->setSourceLevelDebuggerActive(false);
    size_t withoutDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    mockDevice->setSourceLevelDebuggerActive(true);
    size_t withDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    EXPECT_LT(withoutDebugging, withDebugging);

    size_t diff = withDebugging - withoutDebugging;
    size_t sizeExpected = 2 * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(sizeExpected, diff);
}

HWTEST_F(PreambleTest, givenDefaultPreambleWhenGetThreadsMaxNumberIsCalledThenMaximumNumberOfThreadsIsReturned) {
    const HardwareInfo &hwInfo = **platformDevices;
    uint32_t threadsPerEU = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount) + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    uint32_t value = HwHelper::getMaxThreadsForVfe(hwInfo);

    uint32_t expected = hwInfo.gtSystemInfo.EUCount * threadsPerEU;
    EXPECT_EQ(expected, value);
}
