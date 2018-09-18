/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/preamble.h"
#include "runtime/utilities/stackvec.h"
#include "unit_tests/gen_common/test.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/libult/mock_gfx_family.h"

#include <gtest/gtest.h>

#include <algorithm>

using PreambleTest = ::testing::Test;

using namespace OCLRT;

HWTEST_F(PreambleTest, PreemptionIsTakenIntoAccountWhenProgrammingPreamble) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto cmdSizePreambleMidThread = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice);

    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    auto cmdSizePreambleDisabled = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    auto cmdSizePreemptionDisabled = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice);

    EXPECT_LE(cmdSizePreemptionMidThread, cmdSizePreambleMidThread);
    EXPECT_LE(cmdSizePreemptionDisabled, cmdSizePreambleDisabled);

    EXPECT_LE(cmdSizePreemptionDisabled, cmdSizePreemptionMidThread);
    EXPECT_LE((cmdSizePreemptionMidThread - cmdSizePreemptionDisabled), (cmdSizePreambleMidThread - cmdSizePreambleDisabled));

    if (mockDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);
        StackVec<char, 8192> preambleBuffer(8192);
        LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

        StackVec<char, 4096> preemptionBuffer;
        preemptionBuffer.resize(cmdSizePreemptionMidThread);
        LinearStream preemptionStream(&*preemptionBuffer.begin(), preemptionBuffer.size());

        uintptr_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
        MockGraphicsAllocation csrSurface(reinterpret_cast<void *>(minCsrAlignment), 1024);

        PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U,
                                                    ThreadArbitrationPolicy::RoundRobin, &csrSurface);

        PreemptionHelper::programPreamble<FamilyType>(preemptionStream, *mockDevice, &csrSurface);

        ASSERT_LE(preemptionStream.getUsed(), preambleStream.getUsed());

        auto it = std::search(&preambleBuffer[0], &preambleBuffer[preambleStream.getUsed()],
                              &preemptionBuffer[0], &preemptionBuffer[preemptionStream.getUsed()]);
        EXPECT_NE(&preambleBuffer[preambleStream.getUsed()], it);
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
    mockDevice->allocatePreemptionAllocationIfNotPresent();

    StackVec<char, 8192> preambleBuffer2(8192);
    preambleStream.replaceBuffer(&*preambleBuffer2.begin(), preambleBuffer2.size());
    PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U,
                                                ThreadArbitrationPolicy::RoundRobin, mockDevice->getPreemptionAllocation());

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
    uint32_t threadsPerEU = (hwInfo.pSysInfo->ThreadCount / hwInfo.pSysInfo->EUCount) + hwInfo.capabilityTable.extraQuantityThreadsPerEU;
    uint32_t value = PreambleHelper<FamilyType>::getMaxThreadsForVfe(hwInfo);

    uint32_t expected = hwInfo.pSysInfo->EUCount * threadsPerEU;
    EXPECT_EQ(expected, value);
}

TEST(DefaultPreambleHelperTest, givenDefaultPreambleHelperWhenGetAdditionalCommandsSizeThenZeroIsReturned) {
    auto size = PreambleHelper<GENX>::getAdditionalCommandsSize(MockDevice(**platformDevices));
    EXPECT_EQ(0u, size);
}

TEST(DefaultPreambleHelperTest, givenDefaultPreambleHelperWhenProgramGenSpecificPreambleWorkAroundsThenDoNothing) {
    char preambleBuffer[4096];
    LinearStream preambleStream(preambleBuffer, 4096);
    size_t size = preambleStream.getUsed();

    PreambleHelper<GENX>::programGenSpecificPreambleWorkArounds(&preambleStream, **platformDevices);
    EXPECT_EQ(size, preambleStream.getUsed());
}
