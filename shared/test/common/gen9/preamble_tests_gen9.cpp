/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/source_level_debugger/source_level_debugger_preamble_test.h"

#include "gtest/gtest.h"

using namespace NEO;
typedef SKLFamily GfxFamily;

#include "shared/test/unit_test/source_level_debugger/source_level_debugger_preamble_test.inl"

using PreambleTestGen9 = ::testing::Test;

GEN9TEST_F(PreambleTestGen9, givenMidThreadPreemptionAndDebuggingActiveWhenStateSipIsProgrammedThenCorrectSipKernelIsUsed) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDebuggingActiveWhenStateSipIsProgrammedThenCorrectSipKernelIsUsedTest();
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

GEN9TEST_F(PreambleTestGen9, givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleIsProgrammedThenCorrectSipKernelIsUsed) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleIsProgrammedThenCorrectSipKernelIsUsedTest();
}

GEN9TEST_F(PreambleTestGen9, givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN9TEST_F(PreambleTestGen9, givenDisabledPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturned) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenDisabledPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest();
}

GEN9TEST_F(PreambleTestGen9, givenKernelDebuggingActiveAndDisabledPreemptionWhenGetAdditionalCommandsSizeIsCalledThen2MiLoadRegisterImmCmdsAreInlcuded) {
    SourceLevelDebuggerPreambleTest<FamilyType>::givenKernelDebuggingActiveAndDisabledPreemptionWhenGetAdditionalCommandsSizeIsCalledThen2MiLoadRegisterImmCmdsAreInlcudedTest();
}

GEN9TEST_F(PreambleTestGen9, givenGen9ThenL3IsProgrammed) {
    bool l3ConfigDifference;
    bool isL3Programmable;

    l3ConfigDifference =
        PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true) !=
        PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, false);
    isL3Programmable =
        PreambleHelper<FamilyType>::isL3Configurable(*defaultHwInfo);

    EXPECT_EQ(l3ConfigDifference, isL3Programmable);
}
