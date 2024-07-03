/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gen11/hw_cmds_base.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

typedef PreambleFixture IclSlm;

GEN11TEST_F(IclSlm, WhenL3ConfigIsDispatchedThenProperRegisterAddressAndValueAreProgrammed) {
    typedef Gen11Family::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true);
    PreambleHelper<FamilyType>::programL3(&cs, l3Config, false);

    parseCommands<Gen11Family>(cs);

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

typedef PreambleFixture Gen11UrbEntryAllocationSize;
GEN11TEST_F(Gen11UrbEntryAllocationSize, WhenPreambleRetrievesUrbEntryAllocationSizeThenValueIsCorrect) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(0x782u, actualVal);
}

typedef PreambleVfeState Gen11PreambleVfeState;
GEN11TEST_F(Gen11PreambleVfeState, GivenWaOffWhenProgrammingVfeStateThenProgrammingIsCorrect) {
    typedef typename Gen11Family::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 0;
    LinearStream &cs = linearStream;
    auto pVfeCmd = PreambleHelper<Gen11Family>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::renderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<Gen11Family>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 168u, emptyProperties);

    parseCommands<Gen11Family>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_FALSE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

GEN11TEST_F(Gen11PreambleVfeState, GivenWaOnWhenProgrammingVfeStateThenProgrammingIsCorrect) {
    typedef typename Gen11Family::PIPE_CONTROL PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;
    auto pVfeCmd = PreambleHelper<Gen11Family>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::renderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<Gen11Family>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 168u, emptyProperties);

    parseCommands<Gen11Family>(cs);

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
    size_t expectedSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(mockDevice);
    EXPECT_EQ(expectedSize, PreambleHelper<FamilyType>::getAdditionalCommandsSize(mockDevice));

    mockDevice.executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(&mockDevice);
    expectedSize += PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(mockDevice.getDebugger() != nullptr);
    EXPECT_EQ(expectedSize, PreambleHelper<FamilyType>::getAdditionalCommandsSize(mockDevice));
}

typedef PreambleFixture ThreadArbitrationGen11;
GEN11TEST_F(ThreadArbitrationGen11, givenPreambleWhenItIsProgrammedThenThreadArbitrationIsNotSet) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef Gen11Family::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef Gen11Family::PIPE_CONTROL PIPE_CONTROL;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true);
    MockDevice mockDevice;
    PreambleHelper<FamilyType>::programPreamble(&linearStream, mockDevice, l3Config, nullptr, false);

    parseCommands<FamilyType>(cs);

    auto ppC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), ppC);

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), RowChickenReg4::address);
    ASSERT_EQ(nullptr, cmd);

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<Gen11Family>::getAdditionalCommandsSize(device));
}

GEN11TEST_F(ThreadArbitrationGen11, whenThreadArbitrationPolicyIsProgrammedThenCorrectValuesAreSet) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef Gen11Family::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef Gen11Family::PIPE_CONTROL PIPE_CONTROL;
    LinearStream &cs = linearStream;
    MockDevice mockDevice;
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.threadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, mockDevice.getRootDeviceEnvironment());

    parseCommands<FamilyType>(cs);

    auto ppC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(ppC, cmdList.end());

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), RowChickenReg4::address);
    ASSERT_NE(nullptr, cmd);

    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[ThreadArbitrationPolicy::RoundRobin], cmd->getDataDword());

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<Gen11Family>::getAdditionalCommandsSize(device));
}

GEN11TEST_F(ThreadArbitrationGen11, GivenDefaultWhenProgrammingPreambleThenArbitrationPolicyIsRoundRobin) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobinAfterDependency, gfxCoreHelper.getDefaultThreadArbitrationPolicy());
}

GEN11TEST_F(ThreadArbitrationGen11, whenGetSupportThreadArbitrationPoliciesIsCalledThenAllPoliciesAreReturned) {
    auto supportedPolicies = PreambleHelper<Gen11Family>::getSupportedThreadArbitrationPolicies();

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

using Gen11PreamblePipelineSelect = PreambleFixture;
GEN11TEST_F(Gen11PreamblePipelineSelect, WhenPreambleRetrievesPipelineSelectSizeThenValueIsCorrect) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    size_t actualVal = PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(sizeof(PIPELINE_SELECT), actualVal);
}
