/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/gen10/reg_configs.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/preamble/preamble_fixture.h"

#include "reg_configs_common.h"

namespace NEO {
struct HardwareInfo;
extern const HardwareInfo **platformDevices;
} // namespace NEO

using namespace NEO;

typedef PreambleFixture CnlSlm;

CNLTEST_F(CnlSlm, shouldBeEnabledOnGen10) {
    typedef CNLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(**platformDevices, true);
    PreambleHelper<CNLFamily>::programL3(&cs, l3Config);

    parseCommands<CNLFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto RegisterOffset = L3CNTLRegisterOffset<FamilyType>::registerOffset;
    EXPECT_EQ(RegisterOffset, lri.getRegisterOffset());
    EXPECT_EQ(1u, lri.getDataDword() & 1);
}

struct CnlPreambleWaCmds : public PreambleFixture {
    CnlPreambleWaCmds() {
        memset(reinterpret_cast<void *>(&waTable), 0, sizeof(waTable));
    }
    void SetUp() override {
        pHwInfo = const_cast<HardwareInfo *>(*NEO::platformDevices);
        pOldWaTable = pHwInfo->pWaTable;
        pHwInfo->pWaTable = &waTable;

        DeviceFixture::SetUpImpl(pHwInfo);
        HardwareParse::SetUp();
        if (pDevice->getPreemptionMode() == PreemptionMode::MidThread) {
            preemptionLocation.reset(new MockGraphicsAllocation);
        }
    }

    void TearDown() override {
        preemptionLocation.reset();
        pHwInfo->pWaTable = pOldWaTable;
        HardwareParse::TearDown();
        DeviceFixture::TearDown();
    }

    WorkaroundTable waTable;
    HardwareInfo *pHwInfo;
    const WorkaroundTable *pOldWaTable;
    std::unique_ptr<GraphicsAllocation> preemptionLocation;
};

typedef PreambleFixture Gen10UrbEntryAllocationSize;
CNLTEST_F(Gen10UrbEntryAllocationSize, getUrbEntryAllocationSize) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(1024u, actualVal);
}

typedef PreambleVfeState Gen10PreambleVfeState;
CNLTEST_F(Gen10PreambleVfeState, WaOff) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    testWaTable.waSendMIFLUSHBeforeVFE = 0;
    LinearStream &cs = linearStream;
    PreambleHelper<FamilyType>::programVFEState(&linearStream, **platformDevices, 0, 0);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_FALSE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pc.getDepthCacheFlushEnable());
    EXPECT_FALSE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

CNLTEST_F(Gen10PreambleVfeState, WaOn) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    testWaTable.waSendMIFLUSHBeforeVFE = 1;
    LinearStream &cs = linearStream;
    PreambleHelper<FamilyType>::programVFEState(&linearStream, **platformDevices, 0, 0);

    parseCommands<FamilyType>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pc.getDepthCacheFlushEnable());
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}

TEST(L3CNTLREGConfig, checkValidValues) {

    uint32_t validCNLNoSLMConfigs[] = {0x80000180, 0x00418180, 0x00420160, 0x00030140, 0xc0000340, 0x00428140};
    uint32_t validCNLSLMConfigs[] = {0, 0xa0000321, 0x01008121, 0xc0000101};

    bool noSLMConfigValid = false;
    bool SLMConfigValid = false;

    for (uint32_t i = 0; i < sizeof(validCNLNoSLMConfigs) / sizeof(validCNLNoSLMConfigs[0]); i++) {
        if (L3CNTLREGConfig<IGFX_CANNONLAKE>::valueForNoSLM == validCNLNoSLMConfigs[i]) {
            noSLMConfigValid = true;
            break;
        }
    }

    for (uint32_t i = 1; i < sizeof(validCNLSLMConfigs) / sizeof(validCNLSLMConfigs[0]); i++) {
        if (L3CNTLREGConfig<IGFX_CANNONLAKE>::valueForSLM == validCNLSLMConfigs[i]) {
            SLMConfigValid = true;
            break;
        }
    }

    EXPECT_TRUE(SLMConfigValid);
    EXPECT_TRUE(noSLMConfigValid);
}

typedef PreambleFixture L3ErrorDetectionBit;
GEN10TEST_F(L3ErrorDetectionBit, GivenGen10WhenProgrammingL3ThenErrorDetectionBehaviorControlBitSet) {
    uint32_t l3Config = 0;

    l3Config = getL3ConfigHelper<IGFX_CANNONLAKE>(true);

    uint32_t errorDetectionBehaviorControlBit = 1 << 9;
    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);

    l3Config = getL3ConfigHelper<IGFX_CANNONLAKE>(false);
    EXPECT_TRUE((l3Config & errorDetectionBehaviorControlBit) != 0);
}

typedef PreambleFixture PreemptionWatermarkGen10;
GEN10TEST_F(PreemptionWatermarkGen10, givenPreambleThenPreambleWorkAroundsIsNotProgrammed) {
    typedef CNLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef CNLFamily::PIPE_CONTROL PIPE_CONTROL;
    PreambleHelper<FamilyType>::programGenSpecificPreambleWorkArounds(&linearStream, **platformDevices);

    parseCommands<FamilyType>(linearStream);

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), FfSliceCsChknReg2::address);
    EXPECT_EQ(nullptr, cmd);
}

typedef PreambleFixture ThreadArbitrationGen10;
GEN10TEST_F(ThreadArbitrationGen10, givenPreambleWhenItIsProgrammedThenThreadArbitrationIsSet) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    typedef CNLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef CNLFamily::PIPE_CONTROL PIPE_CONTROL;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(**platformDevices, true);
    MockDevice mockDevice(**platformDevices);
    PreambleHelper<FamilyType>::programPreamble(&linearStream, mockDevice, l3Config,
                                                ThreadArbitrationPolicy::RoundRobin,
                                                nullptr);

    parseCommands<FamilyType>(cs);

    auto ppC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(ppC, cmdList.end());

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), RowChickenReg4::address);
    ASSERT_NE(nullptr, cmd);

    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[ThreadArbitrationPolicy::RoundRobin], cmd->getDataDword());

    EXPECT_EQ(0u, PreambleHelper<CNLFamily>::getAdditionalCommandsSize(MockDevice(*platformDevices[0])));
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL), PreambleHelper<CNLFamily>::getThreadArbitrationCommandsSize());
}

GEN10TEST_F(ThreadArbitrationGen10, defaultArbitrationPolicy) {
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobinAfterDependency, PreambleHelper<CNLFamily>::getDefaultThreadArbitrationPolicy());
}
