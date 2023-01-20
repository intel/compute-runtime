/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/gen11/hw_cmds_icllp.h"
#include "shared/source/gen11/reg_configs.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct Gen11MediaSamplerProgramingTest : public ::testing::Test {
    typedef typename Gen11Family::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename Gen11Family::PIPE_CONTROL PIPE_CONTROL;

    struct myCsr : public CommandStreamReceiverHw<Gen11Family> {
        using CommandStreamReceiver::commandStream;
        using CommandStreamReceiverHw<Gen11Family>::programMediaSampler;
        myCsr(ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverHw<Gen11Family>(executionEnvironment, 0, 1){};
        void overrideLastVmeSubliceConfig(bool value) {
            lastVmeSubslicesConfig = value;
        }
    };

    void overrideMediaRequest(bool lastVmeConfig, bool mediaSamplerRequired) {
        csr->overrideLastVmeSubliceConfig(lastVmeConfig);
        flags.pipelineSelectArgs.mediaSamplerRequired = mediaSamplerRequired;
    }

    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        csr = new myCsr(*device->executionEnvironment);
        device->resetCommandStreamReceiver(csr);

        stream.reset(new LinearStream(buff, MemoryConstants::pageSize));
    }

    void programMediaSampler() {
        csr->programMediaSampler(*stream, flags);
    }

    size_t getCmdSize() {
        return csr->getCmdSizeForMediaSampler(flags.pipelineSelectArgs.mediaSamplerRequired);
    }

    myCsr *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    char buff[MemoryConstants::pageSize];
    std::unique_ptr<LinearStream> stream;
};

template <typename PIPE_CONTROL>
void setFlushAllCaches(PIPE_CONTROL &pc) {
    pc.setDcFlushEnable(true);
    pc.setRenderTargetCacheFlushEnable(true);
    pc.setInstructionCacheInvalidateEnable(true);
    pc.setTextureCacheInvalidationEnable(true);
    pc.setPipeControlFlushEnable(true);
    pc.setVfCacheInvalidationEnable(true);
    pc.setConstantCacheInvalidationEnable(true);
    pc.setStateCacheInvalidationEnable(true);
}

ICLLPTEST_F(Gen11MediaSamplerProgramingTest, givenVmeEnableSubsliceDisabledWhenPowerClockStateRegisterEnableThenExpectCorrectCmdValues) {
    uint32_t programVmeCmdSize = sizeof(MI_LOAD_REGISTER_IMM) + 2 * sizeof(PIPE_CONTROL);

    overrideMediaRequest(false, true);

    size_t estimatedCmdSize = getCmdSize();
    EXPECT_EQ(programVmeCmdSize, estimatedCmdSize);

    auto expectedMiLrCmd = FamilyType::cmdInitLoadRegisterImm;
    expectedMiLrCmd.setRegisterOffset(gen11PowerClockStateRegister::address);
    auto expectedRegValue = (device->getHardwareInfo().gtSystemInfo.SubSliceCount / 2) << gen11PowerClockStateRegister::subSliceCountShift;
    expectedRegValue |= (gen11PowerClockStateRegister::vmeSliceCount << gen11PowerClockStateRegister::sliceCountShift);
    expectedRegValue |= (device->getHardwareInfo().gtSystemInfo.MaxEuPerSubSlice << gen11PowerClockStateRegister::minEuCountShift);
    expectedRegValue |= (device->getHardwareInfo().gtSystemInfo.MaxEuPerSubSlice << gen11PowerClockStateRegister::maxEuCountShift);
    expectedRegValue |= gen11PowerClockStateRegister::enabledValue;
    expectedMiLrCmd.setDataDword(expectedRegValue);

    programMediaSampler();
    ASSERT_EQ(programVmeCmdSize, stream->getUsed());

    auto expectedPipeControlCmd = FamilyType::cmdInitPipeControl;
    expectedPipeControlCmd.setCommandStreamerStallEnable(0x1);
    setFlushAllCaches(expectedPipeControlCmd);
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(stream->getCpuBase());
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(0, memcmp(&expectedPipeControlCmd, pipeControlCmd, sizeof(PIPE_CONTROL)));

    size_t cmdOffset = sizeof(PIPE_CONTROL);
    auto miLrCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(ptrOffset(stream->getCpuBase(), cmdOffset));
    ASSERT_NE(nullptr, miLrCmd);
    EXPECT_EQ(0, memcmp(&expectedMiLrCmd, miLrCmd, sizeof(MI_LOAD_REGISTER_IMM)));

    cmdOffset += sizeof(MI_LOAD_REGISTER_IMM);
    expectedPipeControlCmd = FamilyType::cmdInitPipeControl;
    expectedPipeControlCmd.setCommandStreamerStallEnable(0x1);
    pipeControlCmd = genCmdCast<PIPE_CONTROL *>(ptrOffset(stream->getCpuBase(), cmdOffset));
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(0, memcmp(&expectedPipeControlCmd, pipeControlCmd, sizeof(PIPE_CONTROL)));
}

ICLLPTEST_F(Gen11MediaSamplerProgramingTest, givenVmeEnableSubsliceEnabledWhenPowerClockStateRegisterDisableThenExpectCorrectCmdValues) {
    constexpr uint32_t programVmeCmdSize = sizeof(MI_LOAD_REGISTER_IMM) + 3 * sizeof(PIPE_CONTROL);

    overrideMediaRequest(true, false);

    size_t estimatedCmdSize = getCmdSize();
    EXPECT_EQ(programVmeCmdSize, estimatedCmdSize);

    programMediaSampler();

    auto expectedMiLrCmd = FamilyType::cmdInitLoadRegisterImm;
    expectedMiLrCmd.setRegisterOffset(gen11PowerClockStateRegister::address);
    auto expectedRegValue = (device->getHardwareInfo().gtSystemInfo.SubSliceCount / 2) << gen11PowerClockStateRegister::subSliceCountShift;
    expectedRegValue |= ((device->getHardwareInfo().gtSystemInfo.SliceCount * 2) << gen11PowerClockStateRegister::sliceCountShift);
    expectedRegValue |= (device->getHardwareInfo().gtSystemInfo.MaxEuPerSubSlice << gen11PowerClockStateRegister::minEuCountShift);
    expectedRegValue |= (device->getHardwareInfo().gtSystemInfo.MaxEuPerSubSlice << gen11PowerClockStateRegister::maxEuCountShift);
    expectedRegValue |= gen11PowerClockStateRegister::disabledValue;
    expectedMiLrCmd.setDataDword(expectedRegValue);

    ASSERT_EQ(programVmeCmdSize, stream->getUsed());

    auto expectedPipeControlCmd = FamilyType::cmdInitPipeControl;
    expectedPipeControlCmd.setCommandStreamerStallEnable(0x1);
    setFlushAllCaches(expectedPipeControlCmd);
    expectedPipeControlCmd.setGenericMediaStateClear(true);
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(stream->getCpuBase());
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(0, memcmp(&expectedPipeControlCmd, pipeControlCmd, sizeof(PIPE_CONTROL)));

    size_t cmdOffset = sizeof(PIPE_CONTROL);
    pipeControlCmd = genCmdCast<PIPE_CONTROL *>(ptrOffset(stream->getCpuBase(), cmdOffset));
    ASSERT_NE(nullptr, pipeControlCmd);
    expectedPipeControlCmd = FamilyType::cmdInitPipeControl;
    expectedPipeControlCmd.setCommandStreamerStallEnable(0x1);
    EXPECT_EQ(0, memcmp(&expectedPipeControlCmd, pipeControlCmd, sizeof(PIPE_CONTROL)));

    cmdOffset += sizeof(PIPE_CONTROL);
    auto miLrCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(ptrOffset(stream->getCpuBase(), cmdOffset));
    ASSERT_NE(nullptr, miLrCmd);
    EXPECT_EQ(0, memcmp(&expectedMiLrCmd, miLrCmd, sizeof(MI_LOAD_REGISTER_IMM)));

    cmdOffset += sizeof(MI_LOAD_REGISTER_IMM);
    pipeControlCmd = genCmdCast<PIPE_CONTROL *>(ptrOffset(stream->getCpuBase(), cmdOffset));
    ASSERT_NE(nullptr, pipeControlCmd);
    EXPECT_EQ(0, memcmp(&expectedPipeControlCmd, pipeControlCmd, sizeof(PIPE_CONTROL)));
}

ICLLPTEST_F(Gen11MediaSamplerProgramingTest, givenVmeEnableSubsliceEnabledWhenPowerClockStateRegisterEnabledThenExpectNoCmds) {
    constexpr uint32_t programVmeCmdSize = 0;

    overrideMediaRequest(true, true);

    size_t estimatedCmdSize = getCmdSize();
    EXPECT_EQ(programVmeCmdSize, estimatedCmdSize);

    programMediaSampler();
    EXPECT_EQ(programVmeCmdSize, stream->getUsed());
}

ICLLPTEST_F(Gen11MediaSamplerProgramingTest, givenVmeEnableSubsliceDisabledWhenPowerClockStateRegisterDisableThenExpectNoCmds) {
    constexpr uint32_t programVmeCmdSize = 0;

    overrideMediaRequest(false, false);

    size_t estimatedCmdSize = getCmdSize();
    EXPECT_EQ(programVmeCmdSize, estimatedCmdSize);

    programMediaSampler();
    EXPECT_EQ(programVmeCmdSize, stream->getUsed());
}
