/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/gen9/hw_cmds_base.h"
#include "shared/source/gen9/reg_configs.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using Gen9Slm = PreambleFixture;

GEN9TEST_F(Gen9Slm, WhenL3ConfigIsDispatchedThenProperRegisterAddressAndValueAreProgrammed) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true);

    PreambleHelper<FamilyType>::programL3(&cs, l3Config);

    parseCommands<FamilyType>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto registerOffset = L3CNTLRegisterOffset<FamilyType>::registerOffset;
    EXPECT_EQ(registerOffset, lri.getRegisterOffset());
    EXPECT_EQ(1u, lri.getDataDword() & 1);
}

using Gen9L3Config = PreambleFixture;

GEN9TEST_F(Gen9L3Config, GivenNoSlmWhenProgrammingL3ThenProgrammingIsCorrect) {
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

GEN9TEST_F(Gen9L3Config, GivenSlmWhenProgrammingL3ThenProgrammingIsCorrect) {
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

using ThreadArbitration = PreambleFixture;
GEN9TEST_F(ThreadArbitration, GivenDefaultWhenProgrammingPreambleThenArbitrationPolicyIsRoundRobin) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobin, gfxCoreHelper.getDefaultThreadArbitrationPolicy());
}

GEN9TEST_F(ThreadArbitration, whenGetSupportedThreadArbitrationPoliciesIsCalledThenAgeBasedAndRoundRobinAreReturned) {
    auto supportedPolicies = PreambleHelper<FamilyType>::getSupportedThreadArbitrationPolicies();

    EXPECT_EQ(2u, supportedPolicies.size());
    EXPECT_NE(supportedPolicies.end(), std::find(supportedPolicies.begin(),
                                                 supportedPolicies.end(),
                                                 ThreadArbitrationPolicy::AgeBased));
    EXPECT_NE(supportedPolicies.end(), std::find(supportedPolicies.begin(),
                                                 supportedPolicies.end(),
                                                 ThreadArbitrationPolicy::RoundRobin));
}

GEN9TEST_F(PreambleVfeState, GivenWaOffWhenProgrammingVfeStateThenProgrammingIsCorrect) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 0;
    LinearStream &cs = linearStream;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::renderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 168u, emptyProperties);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_FALSE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

GEN9TEST_F(PreambleVfeState, GivenWaOnWhenProgrammingVfeStateThenProgrammingIsCorrect) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    testWaTable->flags.waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, pDevice->getHardwareInfo(), EngineGroupType::renderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 168u, emptyProperties);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pc.getDepthCacheFlushEnable());
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

using Gen9PreamblePipelineSelect = PreambleFixture;
GEN9TEST_F(Gen9PreamblePipelineSelect, WhenPreambleRetrievesPipelineSelectSizeThenValueIsCorrect) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    size_t actualVal = PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(sizeof(PIPELINE_SELECT), actualVal);
}
