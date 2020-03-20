/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/preamble/preamble_fixture.h"

#include "reg_configs_common.h"

using namespace NEO;

typedef PreambleFixture IclSlm;

GEN11TEST_F(IclSlm, shouldBeEnabledOnGen11) {
    typedef ICLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(**platformDevices, true);
    PreambleHelper<FamilyType>::programL3(&cs, l3Config);

    parseCommands<ICLFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto RegisterOffset = L3CNTLRegisterOffset<FamilyType>::registerOffset;
    EXPECT_EQ(RegisterOffset, lri.getRegisterOffset());
    EXPECT_EQ(0u, lri.getDataDword() & 1);
}

GEN11TEST_F(IclSlm, givenGen11WhenProgramingL3ThenErrorDetectionBehaviorControlBitSet) {
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(**platformDevices, true);

    uint32_t errorDetectionBehaviorControlBit = 1 << 9;

    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);
}

GEN11TEST_F(IclSlm, givenGen11IsL3Programing) {
    bool isL3Programmable =
        PreambleHelper<FamilyType>::isL3Configurable(**platformDevices);

    EXPECT_FALSE(isL3Programmable);
}

typedef PreambleFixture Gen11UrbEntryAllocationSize;
GEN11TEST_F(Gen11UrbEntryAllocationSize, getUrbEntryAllocationSize) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(0x782u, actualVal);
}

typedef PreambleVfeState Gen11PreambleVfeState;
GEN11TEST_F(Gen11PreambleVfeState, WaOff) {
    typedef typename ICLFamily::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->waSendMIFLUSHBeforeVFE = 0;
    LinearStream &cs = linearStream;
    PreambleHelper<ICLFamily>::programVFEState(&linearStream, pPlatform->getClDevice(0)->getHardwareInfo(), 0, 0, 168u, aub_stream::EngineType::ENGINE_RCS);

    parseCommands<ICLFamily>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_FALSE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

GEN11TEST_F(Gen11PreambleVfeState, WaOn) {
    typedef typename ICLFamily::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;
    PreambleHelper<ICLFamily>::programVFEState(&linearStream, pPlatform->getClDevice(0)->getHardwareInfo(), 0, 0, 168u, aub_stream::EngineType::ENGINE_RCS);

    parseCommands<ICLFamily>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pc.getDepthCacheFlushEnable());
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

typedef PreambleFixture PreemptionWatermarkGen11;
GEN11TEST_F(PreemptionWatermarkGen11, givenPreambleThenPreambleWorkAroundsIsNotProgrammed) {
    PreambleHelper<FamilyType>::programGenSpecificPreambleWorkArounds(&linearStream, **platformDevices);

    parseCommands<FamilyType>(linearStream);

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), FfSliceCsChknReg2::address);
    ASSERT_EQ(nullptr, cmd);

    MockDevice mockDevice;
    mockDevice.setDebuggerActive(false);
    size_t expectedSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(mockDevice);
    EXPECT_EQ(expectedSize, PreambleHelper<FamilyType>::getAdditionalCommandsSize(mockDevice));

    mockDevice.setDebuggerActive(true);
    expectedSize += PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(mockDevice.isDebuggerActive());
    EXPECT_EQ(expectedSize, PreambleHelper<FamilyType>::getAdditionalCommandsSize(mockDevice));
}

typedef PreambleFixture ThreadArbitrationGen11;
GEN11TEST_F(ThreadArbitrationGen11, givenPreambleWhenItIsProgrammedThenThreadArbitrationIsSet) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef ICLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef ICLFamily::PIPE_CONTROL PIPE_CONTROL;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(**platformDevices, true);
    MockDevice mockDevice;
    PreambleHelper<FamilyType>::programPreamble(&linearStream, mockDevice, l3Config,
                                                ThreadArbitrationPolicy::RoundRobin,
                                                nullptr, nullptr);

    parseCommands<FamilyType>(cs);

    auto ppC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(ppC, cmdList.end());

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), RowChickenReg4::address);
    ASSERT_NE(nullptr, cmd);

    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[ThreadArbitrationPolicy::RoundRobin], cmd->getDataDword());

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<ICLFamily>::getAdditionalCommandsSize(device));
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL), PreambleHelper<ICLFamily>::getThreadArbitrationCommandsSize());
}

GEN11TEST_F(ThreadArbitrationGen11, defaultArbitrationPolicy) {
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobinAfterDependency, PreambleHelper<ICLFamily>::getDefaultThreadArbitrationPolicy());
}
