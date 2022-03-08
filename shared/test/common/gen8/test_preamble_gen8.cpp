/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/gen8/reg_configs.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/unit_test/preamble/preamble_fixture.h"

using namespace NEO;

typedef PreambleFixture BdwSlm;

BDWTEST_F(BdwSlm, WhenL3ConfigIsDispatchedThenProperRegisterAddressAndValueAreProgrammed) {
    typedef BDWFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<BDWFamily>::getL3Config(*defaultHwInfo, true);
    PreambleHelper<BDWFamily>::programL3(&cs, l3Config);

    parseCommands<BDWFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto RegisterOffset = L3CNTLRegisterOffset<BDWFamily>::registerOffset;
    EXPECT_EQ(RegisterOffset, lri.getRegisterOffset());
    EXPECT_EQ(1u, lri.getDataDword() & 1);
}

typedef PreambleFixture Gen8L3Config;

BDWTEST_F(Gen8L3Config, GivenNoSlmWhenProgrammingL3ThenProgrammingIsCorrect) {
    bool slmUsed = false;
    uint32_t l3Config = 0;

    l3Config = getL3ConfigHelper<IGFX_BROADWELL>(slmUsed);
    EXPECT_EQ(0x80000340u, l3Config);

    uint32_t errorDetectionBehaviorControlBit = 1 << 9;
    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);
}

BDWTEST_F(Gen8L3Config, GivenlmWhenProgrammingL3ThenProgrammingIsCorrect) {
    bool slmUsed = true;
    uint32_t l3Config = 0;

    l3Config = getL3ConfigHelper<IGFX_BROADWELL>(slmUsed);
    EXPECT_EQ(0x60000321u, l3Config);

    uint32_t errorDetectionBehaviorControlBit = 1 << 9;
    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);
}

BDWTEST_F(Gen8L3Config, WhenPreambleIsCreatedThenL3ProgrammingIsCorrect) {
    bool l3ConfigDifference;
    bool isL3Programmable;

    l3ConfigDifference =
        PreambleHelper<BDWFamily>::getL3Config(*defaultHwInfo, true) !=
        PreambleHelper<BDWFamily>::getL3Config(*defaultHwInfo, false);
    isL3Programmable =
        PreambleHelper<BDWFamily>::isL3Configurable(*defaultHwInfo);

    EXPECT_EQ(l3ConfigDifference, isL3Programmable);
}

typedef PreambleFixture ThreadArbitrationGen8;
BDWTEST_F(ThreadArbitrationGen8, givenPolicyWhenThreadArbitrationProgrammedThenDoNothing) {
    typedef BDWFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;

    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.threadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, *defaultHwInfo);

    EXPECT_EQ(0u, cs.getUsed());

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<BDWFamily>::getAdditionalCommandsSize(device));
    EXPECT_EQ(ThreadArbitrationPolicy::AgeBased, HwHelperHw<BDWFamily>::get().getDefaultThreadArbitrationPolicy());
}

BDWTEST_F(ThreadArbitrationGen8, whenGetSupportThreadArbitrationPoliciesIsCalledThenEmptyVectorIsReturned) {
    auto supportedPolicies = PreambleHelper<BDWFamily>::getSupportedThreadArbitrationPolicies();

    EXPECT_EQ(0u, supportedPolicies.size());
}

typedef PreambleFixture Gen8UrbEntryAllocationSize;
BDWTEST_F(Gen8UrbEntryAllocationSize, WhenPreambleIsCreatedThenUrbEntryAllocationSizeIsCorrect) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(0x782u, actualVal);
}

BDWTEST_F(PreambleVfeState, WhenProgrammingVfeStateThenProgrammingIsCorrect) {
    typedef BDWFamily::PIPE_CONTROL PIPE_CONTROL;

    LinearStream &cs = linearStream;
    auto pVfeCmd = PreambleHelper<BDWFamily>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<BDWFamily>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, 0, 168u, emptyProperties);

    parseCommands<BDWFamily>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}
