/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"

using namespace NEO;

using Gen12LpPreemptionTests = DevicePreemptionTests;

template <>
PreemptionTestHwDetails GetPreemptionTestHwDetails<TGLLPFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = DwordBuilder::build(1, true) | DwordBuilder::build(2, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = DwordBuilder::build(2, true) | DwordBuilder::build(1, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidThread] = DwordBuilder::build(2, true, false) | DwordBuilder::build(1, true, false);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2580u;
    return ret;
}

GEN12LPTEST_F(Gen12LpPreemptionTests, whenProgramStateSipIsCalledThenStateSipCmdIsNotAddedToStream) {
    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(device->getDevice());
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, device->getDevice());
    EXPECT_EQ(0U, cmdStream.getUsed());
}

GEN12LPTEST_F(Gen12LpPreemptionTests, getRequiredCmdQSize) {
    size_t expectedSize = 0;
    EXPECT_EQ(expectedSize, PreemptionHelper::getPreemptionWaCsSize<FamilyType>(device->getDevice()));
}

GEN12LPTEST_F(Gen12LpPreemptionTests, applyPreemptionWaCmds) {
    size_t usedSize = 0;
    auto &cmdStream = cmdQ->getCS(0);

    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdStream, device->getDevice());
    EXPECT_EQ(usedSize, cmdStream.getUsed());
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdStream, device->getDevice());
    EXPECT_EQ(usedSize, cmdStream.getUsed());
}

GEN12LPTEST_F(Gen12LpPreemptionTests, givenInterfaceDescriptorDataWhenMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToDisable) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE, iddArg.getThreadPreemptionDisable());
}

GEN12LPTEST_F(Gen12LpPreemptionTests, givenInterfaceDescriptorDataWhenNoMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToEnable) {
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
