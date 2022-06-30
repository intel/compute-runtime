/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/preamble/preamble_fixture.h"

#include "reg_configs_common.h"

using namespace NEO;

typedef PreambleFixture IclSlm;

GEN11TEST_F(IclSlm, WhenL3ConfigIsDispatchedThenProperRegisterAddressAndValueAreProgrammed) {
    typedef ICLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true);
    PreambleHelper<FamilyType>::programL3(&cs, l3Config);

    parseCommands<ICLFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto registerOffset = L3CNTLRegisterOffset<FamilyType>::registerOffset;
    EXPECT_EQ(registerOffset, lri.getRegisterOffset());
    EXPECT_EQ(0u, lri.getDataDword() & 1);
}

GEN11TEST_F(IclSlm, givenGen11WhenProgramingL3ThenErrorDetectionBehaviorControlBitSet) {
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true);

    uint32_t errorDetectionBehaviorControlBit = 1 << 9;

    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);
}

GEN11TEST_F(IclSlm, WhenCheckingL3IsConfigurableThenExpectItToBeFalse) {
    bool isL3Programmable =
        PreambleHelper<FamilyType>::isL3Configurable(*defaultHwInfo);

    EXPECT_FALSE(isL3Programmable);
}

typedef PreambleFixture Gen11UrbEntryAllocationSize;
GEN11TEST_F(Gen11UrbEntryAllocationSize, WhenPreambleRetrievesUrbEntryAllocationSizeThenValueIsCorrect) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(0x782u, actualVal);
}

typedef PreambleVfeState Gen11PreambleVfeState;
GEN11TEST_F(Gen11PreambleVfeState, GivenWaOffWhenProgrammingVfeStateThenProgrammingIsCorrect) {
    typedef typename ICLFamily::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 0;
    LinearStream &cs = linearStream;
    auto pVfeCmd = PreambleHelper<ICLFamily>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<ICLFamily>::programVfeState(pVfeCmd, pDevice->getHardwareInfo(), 0u, 0, 168u, emptyProperties, nullptr);

    parseCommands<ICLFamily>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_FALSE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

GEN11TEST_F(Gen11PreambleVfeState, GivenWaOnWhenProgrammingVfeStateThenProgrammingIsCorrect) {
    typedef typename ICLFamily::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;
    auto pVfeCmd = PreambleHelper<ICLFamily>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<ICLFamily>::programVfeState(pVfeCmd, pDevice->getHardwareInfo(), 0u, 0, 168u, emptyProperties, nullptr);

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
GEN11TEST_F(PreemptionWatermarkGen11, WhenPreambleIsCreatedThenWorkAroundsIsNotProgrammed) {
    PreambleHelper<FamilyType>::programGenSpecificPreambleWorkArounds(&linearStream, *defaultHwInfo);

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
GEN11TEST_F(ThreadArbitrationGen11, givenPreambleWhenItIsProgrammedThenThreadArbitrationIsNotSet) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef ICLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef ICLFamily::PIPE_CONTROL PIPE_CONTROL;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true);
    MockDevice mockDevice;
    PreambleHelper<FamilyType>::programPreamble(&linearStream, mockDevice, l3Config,
                                                nullptr, nullptr);

    parseCommands<FamilyType>(cs);

    auto ppC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), ppC);

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), RowChickenReg4::address);
    ASSERT_EQ(nullptr, cmd);

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<ICLFamily>::getAdditionalCommandsSize(device));
}

GEN11TEST_F(ThreadArbitrationGen11, whenThreadArbitrationPolicyIsProgrammedThenCorrectValuesAreSet) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef ICLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef ICLFamily::PIPE_CONTROL PIPE_CONTROL;
    LinearStream &cs = linearStream;
    MockDevice mockDevice;
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.threadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, *defaultHwInfo, nullptr);

    parseCommands<FamilyType>(cs);

    auto ppC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(ppC, cmdList.end());

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), RowChickenReg4::address);
    ASSERT_NE(nullptr, cmd);

    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[ThreadArbitrationPolicy::RoundRobin], cmd->getDataDword());

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<ICLFamily>::getAdditionalCommandsSize(device));
}

GEN11TEST_F(ThreadArbitrationGen11, GivenDefaultWhenProgrammingPreambleThenArbitrationPolicyIsRoundRobin) {
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobinAfterDependency, HwHelperHw<ICLFamily>::get().getDefaultThreadArbitrationPolicy());
}

GEN11TEST_F(ThreadArbitrationGen11, whenGetSupportThreadArbitrationPoliciesIsCalledThenAllPoliciesAreReturned) {
    auto supportedPolicies = PreambleHelper<ICLFamily>::getSupportedThreadArbitrationPolicies();

    EXPECT_EQ(3u, supportedPolicies.size());
    EXPECT_NE(supportedPolicies.end(), std::find(supportedPolicies.begin(),
                                                 supportedPolicies.end(),
                                                 ThreadArbitrationPolicy::AgeBased));
    EXPECT_NE(supportedPolicies.end(), std::find(supportedPolicies.begin(),
                                                 supportedPolicies.end(),
                                                 ThreadArbitrationPolicy::RoundRobin));
    EXPECT_NE(supportedPolicies.end(), std::find(supportedPolicies.begin(),
                                                 supportedPolicies.end(),
                                                 ThreadArbitrationPolicy::RoundRobinAfterDependency));
}
using PreambleFixtureGen11 = PreambleFixture;
GEN11TEST_F(PreambleFixtureGen11, whenKernelDebuggingCommandsAreProgrammedThenCorrectRegisterAddressesAndValuesAreSet) {
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
