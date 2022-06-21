/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/preemption_fixture.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_device.h"

#include "test_traits_common.h"

using namespace NEO;

using XeHPAndLaterPreemptionTests = DevicePreemptionTests;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterPreemptionTests, whenProgramStateSipIsCalledThenStateSipCmdIsNotAddedToStream) {
    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device, nullptr);
    EXPECT_EQ(0U, cmdStream.getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterPreemptionTests, WhenProgrammingThenWaHasExpectedSize) {
    size_t expectedSize = 0;
    EXPECT_EQ(expectedSize, PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterPreemptionTests, WhenProgrammingThenWaNotApplied) {
    size_t usedSize = 0;

    auto requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    StackVec<char, 4096> buff(requiredSize);
    LinearStream cmdStream(buff.begin(), buff.size());

    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdStream, *device);
    EXPECT_EQ(usedSize, cmdStream.getUsed());
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdStream, *device);
    EXPECT_EQ(usedSize, cmdStream.getUsed());
}

struct ThreadPreemptionDisableBitMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::threadPreemptionDisableBitMatcher;
        }
        return false;
    }
};

HWTEST2_F(XeHPAndLaterPreemptionTests, givenInterfaceDescriptorDataWhenMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToDisable, ThreadPreemptionDisableBitMatcher) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE, iddArg.getThreadPreemptionDisable());
}

HWTEST2_F(XeHPAndLaterPreemptionTests, givenInterfaceDescriptorDataWhenNoMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToEnable, ThreadPreemptionDisableBitMatcher) {
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

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterPreemptionTests, WhenProgrammingPreemptionThenExpectLoadRegisterCommandRemapFlagEnabled) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    const size_t bufferSize = 128;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    PreemptionHelper::programCmdStream<FamilyType>(cmdStream, PreemptionMode::ThreadGroup, PreemptionMode::Initial, nullptr);
    auto lriCommand = genCmdCast<MI_LOAD_REGISTER_IMM *>(cmdStream.getCpuBase());
    ASSERT_NE(nullptr, lriCommand);
    EXPECT_TRUE(lriCommand->getMmioRemapEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterPreemptionTests, GivenDebuggerUsedWhenProgrammingStateSipThenStateSipIsAdded) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    device->executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockDebugger);

    auto sipType = SipKernel::getSipKernelType(*device.get());
    SipKernel::initSipKernel(sipType, *device.get());

    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    EXPECT_EQ(sizeof(STATE_SIP), requiredSize);

    const size_t bufferSize = 128;
    uint64_t buffer[bufferSize];

    LinearStream cmdStream{buffer, bufferSize * sizeof(uint64_t)};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device, nullptr);
    EXPECT_EQ(sizeof(STATE_SIP), cmdStream.getUsed());

    auto sipAllocation = SipKernel::getSipKernel(*device).getSipAllocation();
    auto sipCommand = genCmdCast<STATE_SIP *>(cmdStream.getCpuBase());
    auto sipAddress = sipCommand->getSystemInstructionPointer();

    EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress);
}
