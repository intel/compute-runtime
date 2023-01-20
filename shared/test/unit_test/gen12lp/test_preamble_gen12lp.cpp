/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "reg_configs_common.h"

using namespace NEO;

using TglLpSlm = PreambleFixture;

HWTEST2_F(TglLpSlm, givenTglLpWhenPreambleIsBeingProgrammedThenThreadArbitrationPolicyIsIgnored, IsTGLLP) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef Gen12LpFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<Gen12LpFamily>::getL3Config(pDevice->getHardwareInfo(), true);
    MockDevice mockDevice;
    PreambleHelper<Gen12LpFamily>::programPreamble(&linearStream, mockDevice, l3Config,
                                                   nullptr, nullptr);

    parseCommands<Gen12LpFamily>(cs);

    // parse through commands and ensure that 0xE404 is not being programmed
    EXPECT_EQ(0U, countMmio<FamilyType>(cmdList.begin(), cmdList.end(), 0xE404));
}

HWTEST2_F(TglLpSlm, WhenPreambleIsCreatedThenSlmIsDisabled, IsTGLLP) {
    typedef Gen12LpFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(pDevice->getHardwareInfo(), true);
    PreambleHelper<FamilyType>::programL3(&cs, l3Config);

    parseCommands<Gen12LpFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorLRI);
}

using Gen12LpUrbEntryAllocationSize = PreambleFixture;
HWTEST2_F(Gen12LpUrbEntryAllocationSize, WhenPreambleIsCreatedThenUrbEntryAllocationSizeIsCorrect, IsTGLLP) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(1024u, actualVal);
}

using Gen12LpPreambleVfeState = PreambleVfeState;
HWTEST2_F(Gen12LpPreambleVfeState, GivenWaOffWhenProgrammingVfeStateThenProgrammingIsCorrect, IsTGLLP) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 0;
    LinearStream &cs = linearStream;
    auto vfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(vfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 672u, emptyProperties, nullptr);

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

    auto vfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::Compute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(vfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 672u, emptyProperties, nullptr);

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

    auto vfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(vfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 672u, emptyProperties, nullptr);

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

    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &waTable = hwInfo->workaroundTable;

    void *cmdSpace = linearStream.getSpace(sizeof(MEDIA_VFE_STATE));
    auto mediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(cmdSpace);
    *mediaVfeState = FamilyType::cmdInitMediaVfeState;

    std::tuple<bool, bool, int32_t> testParams[]{
        {false, false, 0},
        {false, true, 0},
        {false, false, -1},
        {true, false, 1},
        {true, true, -1},
        {true, true, 1}};

    StreamProperties streamProperties = {};
    streamProperties.frontEndState.disableEUFusion.value = 0;

    for (auto &[expectedValue, waDisableFusedThreadScheduling, debugKeyValue] : testParams) {
        waTable.flags.waDisableFusedThreadScheduling = waDisableFusedThreadScheduling;

        DebugManager.flags.CFEFusedEUDispatch.set(debugKeyValue);

        PreambleHelper<FamilyType>::appendProgramVFEState(pDevice->getRootDeviceEnvironment(), streamProperties, cmdSpace);
        EXPECT_EQ(expectedValue, mediaVfeState->getDisableSlice0Subslice2());
    }
}

HWTEST2_F(Gen12LpPreambleVfeState, givenMaxNumberOfDssDebugVariableWhenMediaVfeStateIsProgrammedThenFieldIsSet, IsTGLLP) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    DebugManagerStateRestore restorer;
    DebugManager.flags.MediaVfeStateMaxSubSlices.set(2);

    void *cmdSpace = linearStream.getSpace(sizeof(MEDIA_VFE_STATE));
    auto mediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(cmdSpace);
    *mediaVfeState = FamilyType::cmdInitMediaVfeState;

    StreamProperties streamProperties = {};
    streamProperties.frontEndState.disableEUFusion.value = 0;

    PreambleHelper<FamilyType>::appendProgramVFEState(pDevice->getRootDeviceEnvironment(), streamProperties, cmdSpace);
    EXPECT_EQ(2u, mediaVfeState->getMaximumNumberOfDualSubslices());
}

HWTEST2_F(Gen12LpPreambleVfeState, givenDisableEUFusionWhenProgramAdditionalFieldsInVfeStateThenCorrectFieldIsSet, IsTGLLP) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    void *cmdSpace = linearStream.getSpace(sizeof(MEDIA_VFE_STATE));
    auto mediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(cmdSpace);
    *mediaVfeState = FamilyType::cmdInitMediaVfeState;

    StreamProperties streamProperties = {};
    streamProperties.frontEndState.disableEUFusion.value = 1;

    PreambleHelper<FamilyType>::appendProgramVFEState(pDevice->getRootDeviceEnvironment(), streamProperties, cmdSpace);
    EXPECT_TRUE(mediaVfeState->getDisableSlice0Subslice2());
}

HWTEST2_F(Gen12LpPreambleVfeState, givenDisableEUFusionAndCFEFusedEUDispatchWhenProgramAdditionalFieldsInVfeStateThenCorrectFieldIsSet, IsTGLLP) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.CFEFusedEUDispatch.set(0);

    void *cmdSpace = linearStream.getSpace(sizeof(MEDIA_VFE_STATE));
    auto mediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(cmdSpace);
    *mediaVfeState = FamilyType::cmdInitMediaVfeState;

    StreamProperties streamProperties = {};
    streamProperties.frontEndState.disableEUFusion.value = 1;

    PreambleHelper<FamilyType>::appendProgramVFEState(pDevice->getRootDeviceEnvironment(), streamProperties, cmdSpace);
    EXPECT_FALSE(mediaVfeState->getDisableSlice0Subslice2());
}

using ThreadArbitrationGen12Lp = PreambleFixture;
GEN12LPTEST_F(ThreadArbitrationGen12Lp, whenGetDefaultThreadArbitrationPolicyIsCalledThenCorrectPolicyIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(ThreadArbitrationPolicy::AgeBased, gfxCoreHelper.getDefaultThreadArbitrationPolicy());
}

GEN12LPTEST_F(ThreadArbitrationGen12Lp, whenGetSupportThreadArbitrationPoliciesIsCalledThenEmptyVectorIsReturned) {
    auto supportedPolicies = PreambleHelper<FamilyType>::getSupportedThreadArbitrationPolicies();

    EXPECT_EQ(0u, supportedPolicies.size());
}

using PreemptionWatermarkGen12Lp = PreambleFixture;
GEN12LPTEST_F(PreemptionWatermarkGen12Lp, WhenPreambleIsCreatedThenPreambleWorkAroundsIsNotProgrammed) {
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

    MI_LOAD_REGISTER_IMM *cmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*it);
    EXPECT_EQ(0x20d8u, cmd->getRegisterOffset());
    EXPECT_EQ((1u << 5) | (1u << 21), cmd->getDataDword());
    it++;

    cmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*it);
    EXPECT_EQ(0xe400u, cmd->getRegisterOffset());
    EXPECT_EQ((1u << 7) | (1u << 4), cmd->getDataDword());
}
