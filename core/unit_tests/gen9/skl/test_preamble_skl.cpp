/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/preemption.h"
#include "core/helpers/preamble.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/preamble/preamble_fixture.h"
#include "runtime/command_stream/thread_arbitration_policy.h"
#include "runtime/gen9/reg_configs.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"

using namespace NEO;

typedef PreambleFixture SklSlm;

SKLTEST_F(SklSlm, shouldBeEnabledOnGen9) {
    typedef SKLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(**platformDevices, true);

    PreambleHelper<SKLFamily>::programL3(&cs, l3Config);

    parseCommands<SKLFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto RegisterOffset = L3CNTLRegisterOffset<FamilyType>::registerOffset;
    EXPECT_EQ(RegisterOffset, lri.getRegisterOffset());
    EXPECT_EQ(1u, lri.getDataDword() & 1);
}

typedef PreambleFixture Gen9L3Config;

SKLTEST_F(Gen9L3Config, checkNoSLM) {
    bool slmUsed = false;
    uint32_t l3Config = 0;

    l3Config = getL3ConfigHelper<IGFX_SKYLAKE>(slmUsed);
    EXPECT_EQ(0x80000340u, l3Config);

    uint32_t errorDetectionBehaviorControlBit = 1 << 9;
    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);

    l3Config = getL3ConfigHelper<IGFX_BROXTON>(slmUsed);
    EXPECT_EQ(0x80000340u, l3Config);

    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);
}

SKLTEST_F(Gen9L3Config, checkSLM) {
    bool slmUsed = true;
    uint32_t l3Config = 0;

    l3Config = getL3ConfigHelper<IGFX_SKYLAKE>(slmUsed);
    EXPECT_EQ(0x60000321u, l3Config);

    uint32_t errorDetectionBehaviorControlBit = 1 << 9;
    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);

    l3Config = getL3ConfigHelper<IGFX_BROXTON>(slmUsed);
    EXPECT_EQ(0x60000321u, l3Config);

    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);
}

typedef PreambleFixture ThreadArbitration;
SKLTEST_F(ThreadArbitration, givenPreambleWhenItIsProgrammedThenThreadArbitrationIsSetToRoundRobin) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef SKLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef SKLFamily::PIPE_CONTROL PIPE_CONTROL;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(**platformDevices, true);
    MockDevice mockDevice;
    PreambleHelper<SKLFamily>::programPreamble(&linearStream, mockDevice, l3Config,
                                               ThreadArbitrationPolicy::RoundRobin,
                                               nullptr, nullptr);

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
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL), PreambleHelper<SKLFamily>::getThreadArbitrationCommandsSize());
}

SKLTEST_F(ThreadArbitration, defaultArbitrationPolicy) {
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobin, PreambleHelper<SKLFamily>::getDefaultThreadArbitrationPolicy());
}

GEN9TEST_F(PreambleVfeState, WaOff) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->waSendMIFLUSHBeforeVFE = 0;
    LinearStream &cs = linearStream;
    PreambleHelper<FamilyType>::programVFEState(&linearStream, pPlatform->getDevice(0)->getHardwareInfo(), 0, 0, 168u);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_FALSE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

GEN9TEST_F(PreambleVfeState, WaOn) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;
    PreambleHelper<FamilyType>::programVFEState(&linearStream, pPlatform->getDevice(0)->getHardwareInfo(), 0, 0, 168u);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pc.getDepthCacheFlushEnable());
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}
