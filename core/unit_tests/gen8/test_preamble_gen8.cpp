/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/options.h"
#include "core/helpers/preamble.h"
#include "core/unit_tests/preamble/preamble_fixture.h"
#include "runtime/command_stream/thread_arbitration_policy.h"
#include "runtime/gen8/reg_configs.h"
#include "unit_tests/fixtures/platform_fixture.h"

using namespace NEO;

typedef PreambleFixture BdwSlm;

BDWTEST_F(BdwSlm, shouldBeEnabledOnGen8) {
    typedef BDWFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<BDWFamily>::getL3Config(**platformDevices, true);
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

BDWTEST_F(Gen8L3Config, checkNoSLM) {
    bool slmUsed = false;
    uint32_t l3Config = 0;

    l3Config = getL3ConfigHelper<IGFX_BROADWELL>(slmUsed);
    EXPECT_EQ(0x80000340u, l3Config);

    uint32_t errorDetectionBehaviorControlBit = 1 << 9;
    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);
}

BDWTEST_F(Gen8L3Config, checkSLM) {
    bool slmUsed = true;
    uint32_t l3Config = 0;

    l3Config = getL3ConfigHelper<IGFX_BROADWELL>(slmUsed);
    EXPECT_EQ(0x60000321u, l3Config);

    uint32_t errorDetectionBehaviorControlBit = 1 << 9;
    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);
}

BDWTEST_F(Gen8L3Config, givenGen8IsL3Programing) {
    bool l3ConfigDifference;
    bool isL3Programmable;

    l3ConfigDifference =
        PreambleHelper<BDWFamily>::getL3Config(**platformDevices, true) !=
        PreambleHelper<BDWFamily>::getL3Config(**platformDevices, false);
    isL3Programmable =
        PreambleHelper<BDWFamily>::isL3Configurable(**platformDevices);

    EXPECT_EQ(l3ConfigDifference, isL3Programmable);
}

typedef PreambleFixture ThreadArbitrationGen8;
BDWTEST_F(ThreadArbitrationGen8, givenPolicyWhenThreadArbitrationProgrammedThenDoNothing) {
    typedef BDWFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;

    PreambleHelper<BDWFamily>::programThreadArbitration(&cs, ThreadArbitrationPolicy::RoundRobin);

    EXPECT_EQ(0u, cs.getUsed());

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<BDWFamily>::getAdditionalCommandsSize(device));
    EXPECT_EQ(0u, PreambleHelper<BDWFamily>::getThreadArbitrationCommandsSize());
    EXPECT_EQ(0u, PreambleHelper<BDWFamily>::getDefaultThreadArbitrationPolicy());
}

typedef PreambleFixture Gen8UrbEntryAllocationSize;
BDWTEST_F(Gen8UrbEntryAllocationSize, getUrbEntryAllocationSize) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(0x782u, actualVal);
}

BDWTEST_F(PreambleVfeState, basic) {
    typedef BDWFamily::PIPE_CONTROL PIPE_CONTROL;

    LinearStream &cs = linearStream;
    PreambleHelper<BDWFamily>::programVFEState(&linearStream, **platformDevices, 0, 0, 168u);

    parseCommands<BDWFamily>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}
