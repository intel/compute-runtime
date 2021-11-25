/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/preemption_fixture.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "patch_shared.h"

using namespace NEO;

template <>
PreemptionTestHwDetails GetPreemptionTestHwDetails<SKLFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = DwordBuilder::build(1, true) | DwordBuilder::build(2, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = DwordBuilder::build(2, true) | DwordBuilder::build(1, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidThread] = DwordBuilder::build(2, true, false) | DwordBuilder::build(1, true, false);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2580u;
    return ret;
}

using Gen9PreemptionTests = DevicePreemptionTests;

GEN9TEST_F(Gen9PreemptionTests, whenMidThreadPreemptionIsNotAvailableThenDoesNotProgramPreamble) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);

    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device);
    EXPECT_EQ(0U, cmdStream.getUsed());
}

GEN9TEST_F(Gen9PreemptionTests, whenMidThreadPreemptionIsAvailableThenStateSipIsProgrammed) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment->DisableMidThreadPreemption = 0;

    size_t minCsrSize = device->getHardwareInfo().gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);

    size_t requiredCmdStreamSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    size_t expectedPreambleSize = sizeof(STATE_SIP);
    EXPECT_EQ(expectedPreambleSize, requiredCmdStreamSize);

    StackVec<char, 8192> streamStorage(requiredCmdStreamSize);
    ASSERT_LE(requiredCmdStreamSize, streamStorage.size());

    LinearStream cmdStream{streamStorage.begin(), streamStorage.size()};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device);

    HardwareParse hwParsePreamble;
    hwParsePreamble.parseCommands<FamilyType>(cmdStream);

    auto stateSipCmd = hwParsePreamble.getCommand<STATE_SIP>();
    ASSERT_NE(nullptr, stateSipCmd);
    EXPECT_EQ(SipKernel::getSipKernel(*device).getSipAllocation()->getGpuAddressToPatch(), stateSipCmd->getSystemInstructionPointer());
}

GEN9TEST_F(Gen9PreemptionTests, givenMidBatchPreemptionWhenProgrammingWaCmdsBeginThenExpectNoCmds) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidBatch);
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, givenThreadGroupPreemptionNoWaSetWhenProgrammingWaCmdsBeginThenExpectNoCmd) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, givenThreadGroupPreemptionWaSetWhenProgrammingWaCmdsBeginThenExpectCmd) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, givenMidThreadPreemptionNoWaSetWhenProgrammingWaCmdsBeginThenExpectNoCmd) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, givenMidThreadPreemptionWaSetWhenProgrammingWaCmdsBeginThenExpectCmd) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, givenInterfaceDescriptorDataWhenAnyPreemptionModeThenNoChange) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA idd;
    INTERFACE_DESCRIPTOR_DATA iddArg;
    int ret;

    idd = FamilyType::cmdInitInterfaceDescriptorData;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::Disabled);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidBatch);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::ThreadGroup);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);
}

GEN9TEST_F(Gen9PreemptionTests, givenMidThreadPreemptionModeWhenStateSipIsProgrammedThenSipEqualsSipAllocationGpuAddressToPatch) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice, false);

    StackVec<char, 4096> preemptionBuffer;
    preemptionBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preemptionStream(&*preemptionBuffer.begin(), preemptionBuffer.size());

    PreemptionHelper::programStateSip<FamilyType>(preemptionStream, *mockDevice);

    HardwareParse hwParserOnlyPreemption;
    hwParserOnlyPreemption.parseCommands<FamilyType>(preemptionStream, 0);
    auto cmd = hwParserOnlyPreemption.getCommand<STATE_SIP>();
    EXPECT_NE(nullptr, cmd);

    EXPECT_EQ(SipKernel::getSipKernel(*mockDevice).getSipAllocation()->getGpuAddressToPatch(), cmd->getSystemInstructionPointer());
}
