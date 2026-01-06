/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_callbacks.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/gmm_callbacks_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

using GmmCallbacksTests = ::Test<GmmCallbacksFixture>;

HWTEST_F(GmmCallbacksTests, whenWriteL3CalledThenWriteTwoMmio) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    UltCommandStreamReceiver<FamilyType> csr(*executionEnvironment, 0, 1);
    uint8_t buffer[128] = {};
    csr.commandStream.replaceBuffer(buffer, 128);

    uint64_t address = 0x00234564002BCDEC;
    uint64_t value = 0xFEDCBA987654321C;

    auto res = GmmCallbacks<FamilyType>::writeL3Address(&csr, value, address);
    EXPECT_EQ(1, res);
    EXPECT_EQ(2 * sizeof(MI_LOAD_REGISTER_IMM), csr.commandStream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(csr.commandStream, 0);
    EXPECT_EQ(2u, hwParse.cmdList.size());

    auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*hwParse.cmdList.begin());
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(address & 0xFFFFFFFF, cmd->getRegisterOffset());
    EXPECT_EQ(value & 0xFFFFFFFF, cmd->getDataDword());

    cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*(++hwParse.cmdList.begin()));
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(address >> 32, cmd->getRegisterOffset());
    EXPECT_EQ(value >> 32, cmd->getDataDword());
}

HWTEST_F(GmmCallbacksTests, givenCcsEnabledhenWriteL3CalledThenSetRemapBit) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.featureTable.flags.ftrCCSNode = true;
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1u);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&localHwInfo);
    executionEnvironment.initializeMemoryManager();
    UltCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, 1);
    uint8_t buffer[128] = {};
    csr.commandStream.replaceBuffer(buffer, 128);

    auto res = GmmCallbacks<FamilyType>::writeL3Address(&csr, 1, 1);
    EXPECT_EQ(1, res);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(csr.commandStream, 0);
    EXPECT_EQ(2u, hwParse.cmdList.size());

    auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*hwParse.cmdList.begin());
    ASSERT_NE(nullptr, cmd);
    EXPECT_TRUE(cmd->getMmioRemapEnable());

    cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*(++hwParse.cmdList.begin()));
    ASSERT_NE(nullptr, cmd);
    EXPECT_TRUE(cmd->getMmioRemapEnable());
}

HWTEST_F(GmmCallbacksTests, givenCcsDisabledhenWriteL3CalledThenSetRemapBitToTrue) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.featureTable.flags.ftrCCSNode = false;
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1u);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&localHwInfo);
    executionEnvironment.initializeMemoryManager();
    UltCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0, 1);
    uint8_t buffer[128] = {};
    csr.commandStream.replaceBuffer(buffer, 128);

    auto res = GmmCallbacks<FamilyType>::writeL3Address(&csr, 1, 1);
    EXPECT_EQ(1, res);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(csr.commandStream, 0);
    EXPECT_EQ(2u, hwParse.cmdList.size());

    auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*hwParse.cmdList.begin());
    ASSERT_NE(nullptr, cmd);
    EXPECT_TRUE(cmd->getMmioRemapEnable());

    cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*(++hwParse.cmdList.begin()));
    ASSERT_NE(nullptr, cmd);
    EXPECT_TRUE(cmd->getMmioRemapEnable());
}