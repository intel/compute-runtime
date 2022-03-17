/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/preamble/preamble_fixture.h"

#include "reg_configs_common.h"

using namespace NEO;

typedef PreambleFixture TglLpSlm;

HWTEST2_F(TglLpSlm, givenTglLpWhenPreambleIsBeingProgrammedThenThreadArbitrationPolicyIsIgnored, IsTGLLP) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef TGLLPFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<TGLLPFamily>::getL3Config(pDevice->getHardwareInfo(), true);
    MockDevice mockDevice;
    PreambleHelper<TGLLPFamily>::programPreamble(&linearStream, mockDevice, l3Config,
                                                 nullptr);

    parseCommands<TGLLPFamily>(cs);

    // parse through commands and ensure that 0xE404 is not being programmed
    EXPECT_EQ(0U, countMmio<FamilyType>(cmdList.begin(), cmdList.end(), 0xE404));
}

HWTEST2_F(TglLpSlm, WhenCheckingL3IsConfigurableThenExpectItToBeFalse, IsTGLLP) {
    bool isL3Programmable =
        PreambleHelper<TGLLPFamily>::isL3Configurable(*defaultHwInfo);

    EXPECT_FALSE(isL3Programmable);
}

HWTEST2_F(TglLpSlm, WhenPreambleIsCreatedThenSlmIsDisabled, IsTGLLP) {
    typedef TGLLPFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(pDevice->getHardwareInfo(), true);
    PreambleHelper<FamilyType>::programL3(&cs, l3Config);

    parseCommands<TGLLPFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorLRI);
}

typedef PreambleFixture Gen12LpUrbEntryAllocationSize;
HWTEST2_F(Gen12LpUrbEntryAllocationSize, WhenPreambleIsCreatedThenUrbEntryAllocationSizeIsCorrect, IsTGLLP) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(1024u, actualVal);
}

typedef PreambleVfeState Gen12LpPreambleVfeState;
HWTEST2_F(Gen12LpPreambleVfeState, GivenWaOffWhenProgrammingVfeStateThenProgrammingIsCorrect, IsTGLLP) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 0;
    LinearStream &cs = linearStream;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getHardwareInfo(), 0u, 0, 672u, emptyProperties);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthStallEnable());
    EXPECT_FALSE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

HWTEST2_F(Gen12LpPreambleVfeState, givenCcsEngineWhenWaIsSetThenAppropriatePipeControlFlushesAreSet, IsTGLLP) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::Compute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getHardwareInfo(), 0u, 0, 672u, emptyProperties);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

HWTEST2_F(Gen12LpPreambleVfeState, givenRcsEngineWhenWaIsSetThenAppropriatePipeControlFlushesAreSet, IsTGLLP) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getHardwareInfo(), 0u, 0, 672u, emptyProperties);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pc.getDepthCacheFlushEnable());
    EXPECT_TRUE(pc.getDepthStallEnable());
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

HWTEST2_F(Gen12LpPreambleVfeState, givenDefaultPipeControlWhenItIsProgrammedThenCsStallBitIsSet, IsTGLLP) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    PIPE_CONTROL *pipeControl = static_cast<PIPE_CONTROL *>(linearStream.getSpace(sizeof(PIPE_CONTROL)));
    *pipeControl = FamilyType::cmdInitPipeControl;

    EXPECT_EQ(1u, pipeControl->getCommandStreamerStallEnable());
}

HWTEST2_F(Gen12LpPreambleVfeState, givenCfeFusedEuDispatchFlagsWhenprogramAdditionalFieldsInVfeStateIsCalledThenGetDisableSlice0Subslice2ReturnsCorrectValues, IsTGLLP) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    DebugManagerStateRestore restorer;
    auto pHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto pMediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(linearStream.getSpace(sizeof(MEDIA_VFE_STATE)));
    *pMediaVfeState = FamilyType::cmdInitMediaVfeState;
    auto &waTable = pHwInfo->workaroundTable;

    std::tuple<bool, bool, int32_t> testParams[]{
        {false, false, 0},
        {false, true, 0},
        {false, false, -1},
        {true, false, 1},
        {true, true, -1},
        {true, true, 1}};

    for (auto &[expectedValue, waDisableFusedThreadScheduling, debugKeyValue] : testParams) {
        waTable.flags.waDisableFusedThreadScheduling = waDisableFusedThreadScheduling;
        ::DebugManager.flags.CFEFusedEUDispatch.set(debugKeyValue);
        PreambleHelper<FamilyType>::programAdditionalFieldsInVfeState(pMediaVfeState, *pHwInfo, false);
        EXPECT_EQ(expectedValue, pMediaVfeState->getDisableSlice0Subslice2());
    }
}

HWTEST2_F(Gen12LpPreambleVfeState, givenMaxNumberOfDssDebugVariableWhenMediaVfeStateIsProgrammedThenFieldIsSet, IsTGLLP) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    DebugManagerStateRestore restorer;
    DebugManager.flags.MediaVfeStateMaxSubSlices.set(2);
    auto pHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto pMediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(linearStream.getSpace(sizeof(MEDIA_VFE_STATE)));
    *pMediaVfeState = FamilyType::cmdInitMediaVfeState;
    PreambleHelper<FamilyType>::programAdditionalFieldsInVfeState(pMediaVfeState, *pHwInfo, false);
    EXPECT_EQ(2u, pMediaVfeState->getMaximumNumberOfDualSubslices());
}

HWTEST2_F(Gen12LpPreambleVfeState, givenDisableEUFusionWhenProgramAdditionalFieldsInVfeStateThenCorrectFieldIsSet, IsTGLLP) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    auto pHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto pMediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(linearStream.getSpace(sizeof(MEDIA_VFE_STATE)));
    *pMediaVfeState = FamilyType::cmdInitMediaVfeState;
    PreambleHelper<FamilyType>::programAdditionalFieldsInVfeState(pMediaVfeState, *pHwInfo, true);
    EXPECT_TRUE(pMediaVfeState->getDisableSlice0Subslice2());
}

HWTEST2_F(Gen12LpPreambleVfeState, givenDisableEUFusionAndCFEFusedEUDispatchWhenProgramAdditionalFieldsInVfeStateThenCorrectFieldIsSet, IsTGLLP) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.CFEFusedEUDispatch.set(0);

    auto pHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto pMediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(linearStream.getSpace(sizeof(MEDIA_VFE_STATE)));
    *pMediaVfeState = FamilyType::cmdInitMediaVfeState;
    PreambleHelper<FamilyType>::programAdditionalFieldsInVfeState(pMediaVfeState, *pHwInfo, true);
    EXPECT_FALSE(pMediaVfeState->getDisableSlice0Subslice2());
}

typedef PreambleFixture ThreadArbitrationGen12Lp;
GEN12LPTEST_F(ThreadArbitrationGen12Lp, whenGetDefaultThreadArbitrationPolicyIsCalledThenCorrectPolicyIsReturned) {
    EXPECT_EQ(ThreadArbitrationPolicy::AgeBased, HwHelperHw<FamilyType>::get().getDefaultThreadArbitrationPolicy());
}

GEN12LPTEST_F(ThreadArbitrationGen12Lp, whenGetSupportThreadArbitrationPoliciesIsCalledThenEmptyVectorIsReturned) {
    auto supportedPolicies = PreambleHelper<FamilyType>::getSupportedThreadArbitrationPolicies();

    EXPECT_EQ(0u, supportedPolicies.size());
}

typedef PreambleFixture PreemptionWatermarkGen12LP;
GEN12LPTEST_F(PreemptionWatermarkGen12LP, WhenPreambleIsCreatedThenPreambleWorkAroundsIsNotProgrammed) {
    PreambleHelper<FamilyType>::programGenSpecificPreambleWorkArounds(&linearStream, pDevice->getHardwareInfo());

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

using PreambleFixtureGen12lp = PreambleFixture;
GEN12LPTEST_F(PreambleFixtureGen12lp, whenKernelDebuggingCommandsAreProgrammedThenCorrectRegisterAddressesAndValuesAreSet) {
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
    EXPECT_EQ(0x20d8u, pCmd->getRegisterOffset());
    EXPECT_EQ((1u << 5) | (1u << 21), pCmd->getDataDword());
    it++;

    pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*it);
    EXPECT_EQ(0xe400u, pCmd->getRegisterOffset());
    EXPECT_EQ((1u << 7) | (1u << 4), pCmd->getDataDword());
}