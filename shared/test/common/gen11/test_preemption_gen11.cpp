/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/preemption_fixture.h"
#include "shared/test/common/mocks/mock_device.h"

#include "patch_shared.h"

using namespace NEO;

template <>
PreemptionTestHwDetails GetPreemptionTestHwDetails<ICLFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = DwordBuilder::build(1, true) | DwordBuilder::build(2, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = DwordBuilder::build(2, true) | DwordBuilder::build(1, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidThread] = DwordBuilder::build(2, true, false) | DwordBuilder::build(1, true, false);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2580u;
    return ret;
}

using Gen11PreemptionTests = DevicePreemptionTests;

GEN11TEST_F(Gen11PreemptionTests, whenMidThreadPreemptionIsNotAvailableThenDoesNotProgramStateSip) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);

    size_t requiredSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    EXPECT_EQ(0U, cmdStream.getUsed());
}

GEN11TEST_F(Gen11PreemptionTests, whenMidThreadPreemptionIsAvailableThenStateSipIsProgrammed) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment->DisableMidThreadPreemption = 0;

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

GEN11TEST_F(Gen11PreemptionTests, WhenGettingPreemptionWaCsSizeThenZeroIsReturned) {
    size_t expectedSize = 0;
    EXPECT_EQ(expectedSize, PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device));
}

GEN11TEST_F(Gen11PreemptionTests, WhenApplyingPreemptionWaCmdsThenNothingIsAdded) {
    size_t usedSize = 0;
    StackVec<char, 1024> streamStorage(1024);
    LinearStream cmdStream{streamStorage.begin(), streamStorage.size()};

    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdStream, *device);
    EXPECT_EQ(usedSize, cmdStream.getUsed());
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdStream, *device);
    EXPECT_EQ(usedSize, cmdStream.getUsed());
}

GEN11TEST_F(Gen11PreemptionTests, givenInterfaceDescriptorDataWhenMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToDisable) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE, iddArg.getThreadPreemptionDisable());
}

GEN11TEST_F(Gen11PreemptionTests, givenInterfaceDescriptorDataWhenNoMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToEnable) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::Disabled);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE, iddArg.getThreadPreemptionDisable());

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidBatch);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE, iddArg.getThreadPreemptionDisable());

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::ThreadGroup);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE, iddArg.getThreadPreemptionDisable());
}
