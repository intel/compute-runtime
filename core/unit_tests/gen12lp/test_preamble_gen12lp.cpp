/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/preamble/preamble_fixture.h"
#include "runtime/command_stream/preemption.h"

#include "reg_configs_common.h"

using namespace NEO;

typedef PreambleFixture TglLpSlm;

TGLLPTEST_F(TglLpSlm, givenTglLpWhenPreambleIsBeingProgrammedThenThreadArbitrationPolicyIsIgnored) {
    typedef TGLLPFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<TGLLPFamily>::getL3Config(pDevice->getHardwareInfo(), true);
    MockDevice mockDevice;
    PreambleHelper<TGLLPFamily>::programPreamble(&linearStream, mockDevice, l3Config,
                                                 ThreadArbitrationPolicy::RoundRobin,
                                                 nullptr, nullptr);

    parseCommands<TGLLPFamily>(cs);

    // parse through commands and ensure that 0xE404 is not being programmed
    EXPECT_EQ(0U, countMmio<FamilyType>(cmdList.begin(), cmdList.end(), 0xE404));
}

TGLLPTEST_F(TglLpSlm, givenTglLpIsL3Programing) {
    bool isL3Programmable =
        PreambleHelper<TGLLPFamily>::isL3Configurable(**platformDevices);

    EXPECT_FALSE(isL3Programmable);
}

TGLLPTEST_F(TglLpSlm, shouldNotBeEnabledOnGen12) {
    typedef TGLLPFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(pDevice->getHardwareInfo(), true);
    PreambleHelper<FamilyType>::programL3(&cs, l3Config);

    parseCommands<TGLLPFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorLRI);
}

typedef PreambleFixture Gen12LpUrbEntryAllocationSize;
TGLLPTEST_F(Gen12LpUrbEntryAllocationSize, getUrbEntryAllocationSize) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(1024u, actualVal);
}

typedef PreambleVfeState Gen12LpPreambleVfeState;
TGLLPTEST_F(Gen12LpPreambleVfeState, WaOff) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->waSendMIFLUSHBeforeVFE = 0;
    LinearStream &cs = linearStream;
    PreambleHelper<FamilyType>::programVFEState(&linearStream, *pPlatform->peekExecutionEnvironment()->getHardwareInfo(), 0, 0, 672u);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_FALSE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

TGLLPTEST_F(Gen12LpPreambleVfeState, givenCcsEngineWhenWaIsSetThenAppropriatePipeControlFlushesAreSet) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;

    EXPECT_EQ(aub_stream::ENGINE_CCS, platformDevices[0]->capabilityTable.defaultEngineType);

    PreambleHelper<FamilyType>::programVFEState(&linearStream, *pPlatform->peekExecutionEnvironment()->getHardwareInfo(), 0, 0, 672u);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

TGLLPTEST_F(Gen12LpPreambleVfeState, givenRcsEngineWhenWaIsSetThenAppropriatePipeControlFlushesAreSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    testWaTable->waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;

    HardwareInfo hwInfo = const_cast<HardwareInfo &>(pPlatform->getDevice(0)->getHardwareInfo());
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;

    PreambleHelper<FamilyType>::programVFEState(&linearStream, hwInfo, 0, 0, 672u);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pc.getDepthCacheFlushEnable());
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

TGLLPTEST_F(Gen12LpPreambleVfeState, givenDefaultPipeControlWhenItIsProgrammedThenCsStallBitIsSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    PIPE_CONTROL *pipeControl = static_cast<PIPE_CONTROL *>(linearStream.getSpace(sizeof(PIPE_CONTROL)));
    *pipeControl = FamilyType::cmdInitPipeControl;

    EXPECT_EQ(1u, pipeControl->getCommandStreamerStallEnable());
}

TGLLPTEST_F(Gen12LpPreambleVfeState, givenCfeFusedEuDispatchFlagsWhenprogramAdditionalFieldsInVfeStateIsCalledThenGetDisableSlice0Subslice2ReturnsCorrectValues) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    DebugManagerStateRestore restorer;
    auto pHwInfo = pPlatform->getDevice(0)->getExecutionEnvironment()->getMutableHardwareInfo();
    auto pMediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(linearStream.getSpace(sizeof(MEDIA_VFE_STATE)));
    *pMediaVfeState = FamilyType::cmdInitMediaVfeState;
    auto &waTable = pHwInfo->workaroundTable;

    const std::array<std::tuple<bool, bool, int32_t>, 6> testParams{{std::make_tuple(false, false, 0),
                                                                    std::make_tuple(false, true, 0),
                                                                    std::make_tuple(false, false, -1),
                                                                    std::make_tuple(true, false, 1),
                                                                    std::make_tuple(true, true, -1),
                                                                    std::make_tuple(true, true, 1)}};

    for (const auto &params : testParams) {
        bool expectedValue, waDisableFusedThreadScheduling;
        int32_t debugKeyValue;
        std::tie(expectedValue, waDisableFusedThreadScheduling, debugKeyValue) = params;

        waTable.waDisableFusedThreadScheduling = waDisableFusedThreadScheduling;
        ::DebugManager.flags.CFEFusedEUDispatch.set(debugKeyValue);
        PreambleHelper<FamilyType>::programAdditionalFieldsInVfeState(pMediaVfeState, *pHwInfo);
        EXPECT_EQ(expectedValue, pMediaVfeState->getDisableSlice0Subslice2());
    }
}

typedef PreambleFixture ThreadArbitrationGen12Lp;
GEN12LPTEST_F(ThreadArbitrationGen12Lp, givenPolicyWhenThreadArbitrationProgrammedThenDoNothing) {
    LinearStream &cs = linearStream;

    PreambleHelper<FamilyType>::programThreadArbitration(&cs, ThreadArbitrationPolicy::RoundRobin);

    EXPECT_EQ(0u, cs.getUsed());
    EXPECT_EQ(0u, PreambleHelper<FamilyType>::getDefaultThreadArbitrationPolicy());
}

typedef PreambleFixture PreemptionWatermarkGen12LP;
GEN12LPTEST_F(PreemptionWatermarkGen12LP, givenPreambleThenPreambleWorkAroundsIsNotProgrammed) {
    MockDevice mockDevice;
    PreambleHelper<FamilyType>::programGenSpecificPreambleWorkArounds(&linearStream, pDevice->getHardwareInfo());

    parseCommands<FamilyType>(linearStream);

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), FfSliceCsChknReg2::address);
    ASSERT_EQ(nullptr, cmd);

    size_t expectedSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(MockDevice());
    EXPECT_EQ(expectedSize, PreambleHelper<FamilyType>::getAdditionalCommandsSize(MockDevice()));
}
