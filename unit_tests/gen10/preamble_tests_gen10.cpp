/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/source_level_debugger/source_level_debugger_preamble_test.h"
#include "gtest/gtest.h"

using namespace OCLRT;
typedef CNLFamily GfxFamily;

#include "unit_tests/source_level_debugger/source_level_debugger_preamble_test.inl"

using PreambleTestGen10 = ::testing::Test;

GEN10TEST_F(PreambleTestGen10, givenMidThreadPreemptionAndDebuggingActiveWhenPreambleIsPrograamedThenCorrectSipKernelIsUsed) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDebuggingActiveWhenPreambleIsPrograamedThenCorrectSipKernelIsUsedTest();
}

GEN10TEST_F(PreambleTestGen10, givenMidThreadPreemptionAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN10TEST_F(PreambleTestGen10, givenPreemptionDisabledAndDebuggingActiveWhenPreambleIsProgrammedThenCorrectSipKernelIsUsed) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenPreemptionDisabledAndDebuggingActiveWhenPreambleIsProgrammedThenCorrectSipKernelIsUsedTest();
}

GEN10TEST_F(PreambleTestGen10, givenPreemptionDisabledAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenPreemptionDisabledAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN10TEST_F(PreambleTestGen10, givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleIsPrograamedThenCorrectSipKernelIsUsed) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleIsPrograamedThenCorrectSipKernelIsUsedTest();
}

GEN10TEST_F(PreambleTestGen10, givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN10TEST_F(PreambleTestGen10, givenDisabledPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenDisabledPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN10TEST_F(PreambleTestGen10, givenKernelDebuggingActiveAndDisabledPreemptionWhenGetAdditionalCommandsSizeIsCalledThen2MiLoadRegisterImmCmdsAndStateSipAreInlcuded) {
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
