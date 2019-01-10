/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/gen11/reg_configs.h"
#include "runtime/helpers/hw_helper.h"
#include "test.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device.h"

using namespace NEO;

struct Gen11MediaSamplerProgramingTest : public ::testing::Test {
    typedef typename ICLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename ICLFamily::PIPE_CONTROL PIPE_CONTROL;

    struct myCsr : public CommandStreamReceiverHw<ICLFamily> {
        using CommandStreamReceiver::commandStream;
        using CommandStreamReceiverHw<ICLFamily>::programMediaSampler;
        myCsr(ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverHw<ICLFamily>(executionEnvironment){};
        void overrideLastVmeSubliceConfig(bool value) {
            lastVmeSubslicesConfig = value;
        }
    };

    void overrideMediaRequest(bool lastVmeConfig, bool mediaSamplerRequired) {
        csr->overrideLastVmeSubliceConfig(lastVmeConfig);
        flags.mediaSamplerRequired = mediaSamplerRequired;
    }

    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        csr = new myCsr(*device->executionEnvironment);
        device->resetCommandStreamReceiver(csr);

        stream.reset(new LinearStream(buff, MemoryConstants::pageSize));
    }

    void programMediaSampler() {
        csr->programMediaSampler(*stream, flags);
    }

    size_t getCmdSize() {
        return csr->getCmdSizeForMediaSampler(flags.mediaSamplerRequired);
    }

    myCsr *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags = {};

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
    auto expectedRegValue = (device->getHardwareInfo().pSysInfo->SubSliceCount / 2) << gen11PowerClockStateRegister::subSliceCountShift;
    expectedRegValue |= (gen11PowerClockStateRegister::vmeSliceCount << gen11PowerClockStateRegister::sliceCountShift);
    expectedRegValue |= (device->getHardwareInfo().pSysInfo->MaxEuPerSubSlice << gen11PowerClockStateRegister::minEuCountShift);
    expectedRegValue |= (device->getHardwareInfo().pSysInfo->MaxEuPerSubSlice << gen11PowerClockStateRegister::maxEuCountShift);
    expectedRegValue |= gen11PowerClockStateRegister::enabledValue;
    expectedMiLrCmd.setDataDword(expectedRegValue);

    programMediaSampler();
    ASSERT_EQ(programVmeCmdSize, stream->getUsed());

    auto expectedPipeControlCmd = FamilyType::cmdInitPipeControl;
    expectedPipeControlCmd.setCommandStreamerStallEnable(0x1);
    setFlushAllCaches(expectedPipeControlCmd);
    auto pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(stream->getCpuBase());
    EXPECT_EQ(0, memcmp(&expectedPipeControlCmd, pipeControlCmd, sizeof(PIPE_CONTROL)));

    size_t cmdOffset = sizeof(PIPE_CONTROL);
    auto miLrCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(stream->getCpuBase(), cmdOffset));
    EXPECT_EQ(0, memcmp(&expectedMiLrCmd, miLrCmd, sizeof(MI_LOAD_REGISTER_IMM)));

    cmdOffset += sizeof(MI_LOAD_REGISTER_IMM);
    expectedPipeControlCmd = FamilyType::cmdInitPipeControl;
    expectedPipeControlCmd.setCommandStreamerStallEnable(0x1);
    pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream->getCpuBase(), cmdOffset));
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
    auto expectedRegValue = (device->getHardwareInfo().pSysInfo->SubSliceCount / 2) << gen11PowerClockStateRegister::subSliceCountShift;
    expectedRegValue |= ((device->getHardwareInfo().pSysInfo->SliceCount * 2) << gen11PowerClockStateRegister::sliceCountShift);
    expectedRegValue |= (device->getHardwareInfo().pSysInfo->MaxEuPerSubSlice << gen11PowerClockStateRegister::minEuCountShift);
    expectedRegValue |= (device->getHardwareInfo().pSysInfo->MaxEuPerSubSlice << gen11PowerClockStateRegister::maxEuCountShift);
    expectedRegValue |= gen11PowerClockStateRegister::disabledValue;
    expectedMiLrCmd.setDataDword(expectedRegValue);

    ASSERT_EQ(programVmeCmdSize, stream->getUsed());

    auto expectedPipeControlCmd = FamilyType::cmdInitPipeControl;
    expectedPipeControlCmd.setCommandStreamerStallEnable(0x1);
    setFlushAllCaches(expectedPipeControlCmd);
    expectedPipeControlCmd.setGenericMediaStateClear(true);
    auto pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(stream->getCpuBase());
    EXPECT_EQ(0, memcmp(&expectedPipeControlCmd, pipeControlCmd, sizeof(PIPE_CONTROL)));

    size_t cmdOffset = sizeof(PIPE_CONTROL);
    pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream->getCpuBase(), cmdOffset));
    expectedPipeControlCmd = FamilyType::cmdInitPipeControl;
    expectedPipeControlCmd.setCommandStreamerStallEnable(0x1);
    EXPECT_EQ(0, memcmp(&expectedPipeControlCmd, pipeControlCmd, sizeof(PIPE_CONTROL)));

    cmdOffset += sizeof(PIPE_CONTROL);
    auto miLrCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(stream->getCpuBase(), cmdOffset));
    EXPECT_EQ(0, memcmp(&expectedMiLrCmd, miLrCmd, sizeof(MI_LOAD_REGISTER_IMM)));

    cmdOffset += sizeof(MI_LOAD_REGISTER_IMM);
    pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream->getCpuBase(), cmdOffset));
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
