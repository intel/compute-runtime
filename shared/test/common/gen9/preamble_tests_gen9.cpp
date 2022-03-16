/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/preamble/preamble_fixture.h"
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

using ThreadArbitrationGen9 = PreambleFixture;
GEN9TEST_F(ThreadArbitrationGen9, givenPreambleWhenItIsProgrammedThenThreadArbitrationIsNotSet) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef SKLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef SKLFamily::PIPE_CONTROL PIPE_CONTROL;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true);
    MockDevice mockDevice;
    PreambleHelper<SKLFamily>::programPreamble(&linearStream, mockDevice, l3Config,
                                               nullptr);

    parseCommands<SKLFamily>(cs);

    auto ppC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), ppC);

    auto itorLRI = reverse_find<MI_LOAD_REGISTER_IMM *>(cmdList.rbegin(), cmdList.rend());
    ASSERT_NE(cmdList.rend(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    EXPECT_NE(0xE404u, lri.getRegisterOffset());
    EXPECT_NE(0x100u, lri.getDataDword());

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<SKLFamily>::getAdditionalCommandsSize(device));
}

GEN9TEST_F(ThreadArbitrationGen9, whenThreadArbitrationPolicyIsProgrammedThenCorrectValuesAreSet) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef SKLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef SKLFamily::PIPE_CONTROL PIPE_CONTROL;
    LinearStream &cs = linearStream;
    MockDevice mockDevice;
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.threadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, *defaultHwInfo);

    parseCommands<SKLFamily>(cs);

    auto ppC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(ppC, cmdList.end());

    auto itorLRI = reverse_find<MI_LOAD_REGISTER_IMM *>(cmdList.rbegin(), cmdList.rend());
    ASSERT_NE(cmdList.rend(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    EXPECT_EQ(0xE404u, lri.getRegisterOffset());
    EXPECT_EQ(0x100u, lri.getDataDword());

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<SKLFamily>::getAdditionalCommandsSize(device));
}
