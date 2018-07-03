/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "reg_configs_common.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/gen10/reg_configs.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/preamble/preamble_fixture.h"
#include "runtime/command_stream/preemption.h"

namespace OCLRT {
struct HardwareInfo;
extern const HardwareInfo **platformDevices;
} // namespace OCLRT

using namespace OCLRT;

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
        pHwInfo = const_cast<HardwareInfo *>(*OCLRT::platformDevices);
        pOldWaTable = pHwInfo->pWaTable;
        pHwInfo->pWaTable = &waTable;

        DeviceFixture::SetUpImpl(pHwInfo);
        HardwareParse::SetUp();
        if (pDevice->getPreemptionMode() == PreemptionMode::MidThread) {
            preemptionLocation.reset(new GraphicsAllocation(nullptr, 0));
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

    uint32_t validCNLNoSLMConfigs[] = {0x80000180, 0x00418180, 0x00420160, 0x00030140, 0xc0000140, 0x00428140};
    uint32_t validCNLSLMConfigs[] = {0, 0xa0000121, 0x01008121, 0xc0000101};

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

typedef PreambleFixture PreemptionWatermarkGen10;
GEN10TEST_F(PreemptionWatermarkGen10, givenPreambleThenPreambleWorkAroundsIsNotProgrammed) {
    typedef CNLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef CNLFamily::PIPE_CONTROL PIPE_CONTROL;
    PreambleHelper<FamilyType>::programGenSpecificPreambleWorkArounds(&linearStream, **platformDevices);

    parseCommands<FamilyType>(linearStream);

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), FfSliceCsChknReg2::address);
    ASSERT_EQ(nullptr, cmd);

    size_t expectedSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(MockDevice(*platformDevices[0]));
    EXPECT_EQ(expectedSize, PreambleHelper<FamilyType>::getAdditionalCommandsSize(MockDevice(*platformDevices[0])));
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

using PreambleTestGen10 = ::testing::Test;

GEN10TEST_F(PreambleTestGen10, givenProgrammingPreambleWhenPreemptionIsTakenIntoAccountThenCSRBaseAddressIsEqualCSRGpuAddress) {
    using GPGPU_CSR_BASE_ADDRESS = typename FamilyType::GPGPU_CSR_BASE_ADDRESS;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*mockDevice);
    std::array<char, 8192> preambleBuffer{};
    LinearStream preambleStream(&preambleBuffer, preambleBuffer.size());
    StackVec<char, 4096> preemptionBuffer;
    preemptionBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preemptionStream(&*preemptionBuffer.begin(), preemptionBuffer.size());

    uintptr_t csrGpuAddr = 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface(reinterpret_cast<void *>(csrGpuAddr), 1024);

    PreambleHelper<FamilyType>::programPreamble(&preambleStream, *mockDevice, 0U,
                                                ThreadArbitrationPolicy::RoundRobin, &csrSurface);

    PreemptionHelper::programPreamble<FamilyType>(preemptionStream, *mockDevice, &csrSurface);

    HardwareParse hwParserFullPreamble;
    hwParserFullPreamble.parseCommands<FamilyType>(preambleStream, 0);
    auto cmd = hwParserFullPreamble.getCommand<GPGPU_CSR_BASE_ADDRESS>();
    EXPECT_NE(nullptr, cmd);
    EXPECT_EQ(static_cast<uint64_t>(csrGpuAddr), cmd->getGpgpuCsrBaseAddress());

    HardwareParse hwParserOnlyPreemption;
    hwParserOnlyPreemption.parseCommands<FamilyType>(preemptionStream, 0);
    cmd = hwParserOnlyPreemption.getCommand<GPGPU_CSR_BASE_ADDRESS>();
    EXPECT_NE(nullptr, cmd);
    EXPECT_EQ(static_cast<uint64_t>(csrGpuAddr), cmd->getGpgpuCsrBaseAddress());
}
