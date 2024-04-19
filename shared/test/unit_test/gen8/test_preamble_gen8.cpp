/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/gen8/hw_cmds_base.h"
#include "shared/source/gen8/reg_configs.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

typedef PreambleFixture BdwSlm;

BDWTEST_F(BdwSlm, WhenL3ConfigIsDispatchedThenProperRegisterAddressAndValueAreProgrammed) {
    typedef Gen8Family::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<Gen8Family>::getL3Config(*defaultHwInfo, true);
    PreambleHelper<Gen8Family>::programL3(&cs, l3Config, false);

    parseCommands<Gen8Family>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto registerOffset = L3CNTLRegisterOffset<Gen8Family>::registerOffset;
    EXPECT_EQ(registerOffset, lri.getRegisterOffset());
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

typedef PreambleFixture ThreadArbitrationGen8;
BDWTEST_F(ThreadArbitrationGen8, givenPolicyWhenThreadArbitrationProgrammedThenDoNothing) {
    typedef Gen8Family::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.threadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

    EXPECT_EQ(0u, cs.getUsed());

    MockDevice device;
    EXPECT_EQ(0u, PreambleHelper<Gen8Family>::getAdditionalCommandsSize(device));
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(ThreadArbitrationPolicy::AgeBased, gfxCoreHelper.getDefaultThreadArbitrationPolicy());
}

BDWTEST_F(ThreadArbitrationGen8, whenGetSupportThreadArbitrationPoliciesIsCalledThenEmptyVectorIsReturned) {
    auto supportedPolicies = PreambleHelper<Gen8Family>::getSupportedThreadArbitrationPolicies();

    EXPECT_EQ(0u, supportedPolicies.size());
}

typedef PreambleFixture Gen8UrbEntryAllocationSize;
BDWTEST_F(Gen8UrbEntryAllocationSize, WhenPreambleIsCreatedThenUrbEntryAllocationSizeIsCorrect) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(0x782u, actualVal);
}

BDWTEST_F(PreambleVfeState, WhenProgrammingVfeStateThenProgrammingIsCorrect) {
    typedef Gen8Family::PIPE_CONTROL PIPE_CONTROL;

    LinearStream &cs = linearStream;
    auto pVfeCmd = PreambleHelper<Gen8Family>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::renderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<Gen8Family>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, 0, 168u, emptyProperties);

    parseCommands<Gen8Family>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

using Gen8PreamblePipelineSelect = PreambleFixture;
BDWTEST_F(Gen8PreamblePipelineSelect, WhenPreambleRetrievesPipelineSelectSizeThenValueIsCorrect) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    size_t actualVal = PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(sizeof(PIPELINE_SELECT), actualVal);
}
