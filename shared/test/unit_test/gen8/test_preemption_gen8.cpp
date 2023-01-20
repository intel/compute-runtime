/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

using namespace NEO;

template <>
PreemptionTestHwDetails getPreemptionTestHwDetails<Gen8Family>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = 0;
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = (1 << 2);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2248u;
    return ret;
}

using Gen8PreemptionTests = DevicePreemptionTests;

GEN8TEST_F(Gen8PreemptionTests, whenProgramStateSipIsCalledThenNoCmdsAreProgrammed) {
    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device, nullptr);
    EXPECT_EQ(0U, cmdStream.getUsed());
}

GEN8TEST_F(Gen8PreemptionTests, GivenMidBatchWhenGettingPreemptionWaCsSizeThenSizeIsZero) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidBatch);
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, GivenMidBatchAndNoWaWhenGettingPreemptionWaCsSizeThenSizeIsZero) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, GivenMidBatchAndWaWhenGettingPreemptionWaCsSizeThenSizeIsNonZero) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, GivenMidThreadAndNoWaWhenGettingPreemptionWaCsSizeThenSizeIsZero) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, GivenMidThreadAndWaWhenGettingPreemptionWaCsSizeThenSizeNonIsZero) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, givenInterfaceDescriptorDataWhenAnyPreemptionModeThenNoChange) {
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

struct Gen8PreemptionTestsLinearStream : public Gen8PreemptionTests {
    void SetUp() override {
        Gen8PreemptionTests::SetUp();
        cmdBufferAllocation = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
        cmdBuffer.replaceBuffer(cmdBufferAllocation, MemoryConstants::pageSize);
    }

    void TearDown() override {
        alignedFree(cmdBufferAllocation);
        Gen8PreemptionTests::TearDown();
    }

    LinearStream cmdBuffer;
    void *cmdBufferAllocation;
    HardwareParse cmdBufferParser;
};

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenMidBatchPreemptionWhenProgrammingWaCmdsBeginThenExpectNoCmds) {
    device->setPreemptionMode(PreemptionMode::MidBatch);
    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdBuffer, *device);
    EXPECT_EQ(0u, cmdBuffer.getUsed());
}

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenMidBatchPreemptionWhenProgrammingWaCmdsEndThenExpectNoCmds) {
    device->setPreemptionMode(PreemptionMode::MidBatch);
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdBuffer, *device);
    EXPECT_EQ(0u, cmdBuffer.getUsed());
}

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenThreadGroupPreemptionNoWaSetWhenProgrammingWaCmdsBeginThenExpectNoCmd) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;
    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdBuffer, *device);
    EXPECT_EQ(0u, cmdBuffer.getUsed());
}

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenThreadGroupPreemptionNoWaSetWhenProgrammingWaCmdsEndThenExpectNoCmd) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdBuffer, *device);
    EXPECT_EQ(0u, cmdBuffer.getUsed());
}

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenThreadGroupPreemptionWaSetWhenProgrammingWaCmdsBeginThenExpectMmioCmd) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdBuffer, *device);

    cmdBufferParser.parseCommands<FamilyType>(cmdBuffer);
    cmdBufferParser.findHardwareCommands<FamilyType>();
    GenCmdList::iterator itMmioCmd = cmdBufferParser.lriList.begin();
    ASSERT_NE(cmdBufferParser.lriList.end(), itMmioCmd);
    MI_LOAD_REGISTER_IMM *mmioCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itMmioCmd);
    EXPECT_EQ(0x2600u, mmioCmd->getRegisterOffset());
    EXPECT_EQ(0xFFFFFFFFu, mmioCmd->getDataDword());
}

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenThreadGroupPreemptionWaSetWhenProgrammingWaCmdsEndThenExpectMmioCmd) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdBuffer, *device);

    cmdBufferParser.parseCommands<FamilyType>(cmdBuffer);
    cmdBufferParser.findHardwareCommands<FamilyType>();
    GenCmdList::iterator itMmioCmd = cmdBufferParser.lriList.begin();
    ASSERT_NE(cmdBufferParser.lriList.end(), itMmioCmd);
    MI_LOAD_REGISTER_IMM *mmioCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itMmioCmd);
    EXPECT_EQ(0x2600u, mmioCmd->getRegisterOffset());
    EXPECT_EQ(0x00000000u, mmioCmd->getDataDword());
}

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenMidThreadPreemptionNoWaSetWhenProgrammingWaCmdsBeginThenExpectNoCmd) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;
    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdBuffer, *device);
    EXPECT_EQ(0u, cmdBuffer.getUsed());
}

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenMidThreadPreemptionNoWaSetWhenProgrammingWaCmdsEndThenExpectNoCmd) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdBuffer, *device);
    EXPECT_EQ(0u, cmdBuffer.getUsed());
}

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenMidThreadPreemptionWaSetWhenProgrammingWaCmdsBeginThenExpectMmioCmd) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdBuffer, *device);

    cmdBufferParser.parseCommands<FamilyType>(cmdBuffer);
    cmdBufferParser.findHardwareCommands<FamilyType>();
    GenCmdList::iterator itMmioCmd = cmdBufferParser.lriList.begin();
    ASSERT_NE(cmdBufferParser.lriList.end(), itMmioCmd);
    MI_LOAD_REGISTER_IMM *mmioCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itMmioCmd);
    EXPECT_EQ(0x2600u, mmioCmd->getRegisterOffset());
    EXPECT_EQ(0xFFFFFFFFu, mmioCmd->getDataDword());
}

GEN8TEST_F(Gen8PreemptionTestsLinearStream, givenMidThreadPreemptionWaSetWhenProgrammingWaCmdsEndThenExpectMmioCmd) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdBuffer, *device);

    cmdBufferParser.parseCommands<FamilyType>(cmdBuffer);
    cmdBufferParser.findHardwareCommands<FamilyType>();
    GenCmdList::iterator itMmioCmd = cmdBufferParser.lriList.begin();
    ASSERT_NE(cmdBufferParser.lriList.end(), itMmioCmd);
    MI_LOAD_REGISTER_IMM *mmioCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itMmioCmd);
    EXPECT_EQ(0x2600u, mmioCmd->getRegisterOffset());
    EXPECT_EQ(0x00000000u, mmioCmd->getDataDword());
}
