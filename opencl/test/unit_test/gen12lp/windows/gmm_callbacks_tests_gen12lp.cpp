/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/source/command_stream/aub_command_stream_receiver_hw.h"
#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "opencl/source/os_interface/windows/wddm_device_command_stream.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "test.h"

using namespace NEO;

using Gen12LpGmmCallbacksTests = ::testing::Test;

template <typename GfxFamily>
struct MockAubCsrToTestNotifyAubCapture : public AUBCommandStreamReceiverHw<GfxFamily> {
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;
    using AUBCommandStreamReceiverHw<GfxFamily>::externalAllocations;
};

GEN12LPTEST_F(Gen12LpGmmCallbacksTests, givenCsrWithoutAubDumpWhenNotifyAubCaptureCallbackIsCalledThenDoNothing) {
    HardwareInfo *hwInfo = nullptr;
    ExecutionEnvironment *executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    executionEnvironment->initializeMemoryManager();
    auto csr = std::make_unique<WddmCommandStreamReceiver<FamilyType>>(*executionEnvironment, 0);
    uint64_t address = 0xFEDCBA9876543210;
    size_t size = 1024;

    auto res = DeviceCallbacks<FamilyType>::notifyAubCapture(csr.get(), address, size, true);

    EXPECT_EQ(1, res);
}

GEN12LPTEST_F(Gen12LpGmmCallbacksTests, givenWddmCsrWhenWriteL3CalledThenWriteTwoMmio) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    UltCommandStreamReceiver<FamilyType> csr(*executionEnvironment, 0);
    uint8_t buffer[128] = {};
    csr.commandStream.replaceBuffer(buffer, 128);

    uint64_t address = 0x00234564002BCDEC;
    uint64_t value = 0xFEDCBA987654321C;

    auto res = TTCallbacks<FamilyType>::writeL3Address(&csr, value, address);
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

GEN12LPTEST_F(Gen12LpGmmCallbacksTests, givenCcsEnabledhenWriteL3CalledThenSetRemapBit) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.featureTable.ftrCCSNode = true;
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1u);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&localHwInfo);
    executionEnvironment.initializeMemoryManager();
    UltCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0);
    uint8_t buffer[128] = {};
    csr.commandStream.replaceBuffer(buffer, 128);

    auto res = TTCallbacks<FamilyType>::writeL3Address(&csr, 1, 1);
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

GEN12LPTEST_F(Gen12LpGmmCallbacksTests, givenCcsDisabledhenWriteL3CalledThenSetRemapBitToTrue) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.featureTable.ftrCCSNode = false;
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1u);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(&localHwInfo);
    executionEnvironment.initializeMemoryManager();
    UltCommandStreamReceiver<FamilyType> csr(executionEnvironment, 0);
    uint8_t buffer[128] = {};
    csr.commandStream.replaceBuffer(buffer, 128);

    auto res = TTCallbacks<FamilyType>::writeL3Address(&csr, 1, 1);
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
