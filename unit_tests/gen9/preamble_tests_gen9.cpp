/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/source_level_debugger/source_level_debugger_preamble_test.h"
#include "gtest/gtest.h"

using namespace OCLRT;
typedef SKLFamily GfxFamily;

#include "unit_tests/source_level_debugger/source_level_debugger_preamble_test.inl"

using PreambleTestGen9 = ::testing::Test;

GEN9TEST_F(PreambleTestGen9, givenMidThreadPreemptionAndDebuggingActiveWhenPreambleIsPrograamedThenCorrectSipKernelIsUsed) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDebuggingActiveWhenPreambleIsPrograamedThenCorrectSipKernelIsUsedTest();
}

GEN9TEST_F(PreambleTestGen9, givenMidThreadPreemptionAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN9TEST_F(PreambleTestGen9, givenPreemptionDisabledAndDebuggingActiveWhenPreambleIsProgrammedThenCorrectSipKernelIsUsed) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenPreemptionDisabledAndDebuggingActiveWhenPreambleIsProgrammedThenCorrectSipKernelIsUsedTest();
}

GEN9TEST_F(PreambleTestGen9, givenPreemptionDisabledAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenPreemptionDisabledAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN9TEST_F(PreambleTestGen9, givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleIsPrograamedThenCorrectSipKernelIsUsed) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleIsPrograamedThenCorrectSipKernelIsUsedTest();
}

GEN9TEST_F(PreambleTestGen9, givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN9TEST_F(PreambleTestGen9, givenDisabledPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenDisabledPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN9TEST_F(PreambleTestGen9, givenKernelDebuggingActiveAndDisabledPreemptionWhenGetAdditionalCommandsSizeIsCalledThen2MiLoadRegisterImmCmdsAndStateSipAreInlcuded) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setSourceLevelDebuggerActive(false);
    size_t withoutDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    mockDevice->setSourceLevelDebuggerActive(true);
    size_t withDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    EXPECT_LT(withoutDebugging, withDebugging);

    size_t diff = withDebugging - withoutDebugging;
    size_t sizeExpected = sizeof(typename FamilyType::STATE_SIP) + 2 * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(sizeExpected, diff);
}

GEN9TEST_F(PreambleTestGen9, givenProgrammingPreambleWhenPreemptionIsTakenIntoAccountThenCSRBaseAddressIsEqualCSRGpuAddress) {
    using GPGPU_CSR_BASE_ADDRESS = typename FamilyType::GPGPU_CSR_BASE_ADDRESS;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice);
    std::array<char, 8192> preambleBuffer{};
    LinearStream preambleStream(&preambleBuffer, preambleBuffer.size());

    StackVec<char, 4096> preemptionBuffer;
    preemptionBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preemptionStream(&*preemptionBuffer.begin(), preemptionBuffer.size());

    uintptr_t csrGpuAddr = 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface(reinterpret_cast<void *>(csrGpuAddr), 1024);

    PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U,
                                                ThreadArbitrationPolicy::RoundRobin, &csrSurface);

    PreemptionHelper::programPreamble<FamilyType>(preemptionStream, *mockDevice, &csrSurface);

    HardwareParse hwParserFullPreamble;
    hwParserFullPreamble.parseCommands<FamilyType>(preambleStream, 0);
    auto cmd = hwParserFullPreamble.getCommand<GPGPU_CSR_BASE_ADDRESS>();
    EXPECT_NE(nullptr, cmd);
    EXPECT_EQ(static_cast<uint64_t>(csrGpuAddr), cmd->getGpgpuCsrBaseAddress());

    HardwareParse hwParserOnlyPreemption;
    hwParserOnlyPreemption.parseCommands<FamilyType>(preemptionStream, 0);
    cmd = hwParserOnlyPreemption.getCommand<GPGPU_CSR_BASE_ADDRESS>();
    EXPECT_NE(nullptr, cmd);
    EXPECT_EQ(static_cast<uint64_t>(csrGpuAddr), cmd->getGpgpuCsrBaseAddress());
}
