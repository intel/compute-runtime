/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "per_product_test_definitions.h"

using namespace NEO;

template <>
PreemptionTestHwDetails getPreemptionTestHwDetails<Xe3pCoreFamily>() {
    PreemptionTestHwDetails ret;
    return ret;
}

using Xe3pPreemptionTests = DevicePreemptionTests;

XE3P_CORETEST_F(Xe3pPreemptionTests, givenMidThreadPreemptionAndDebugEnabledWhenQueryingRequiredPreambleSizeThenExpectCorrectSize) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;

    // Mid thread preemption is forced And debugger not enabled
    size_t cmdSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(sizeof(STATE_CONTEXT_DATA_BASE_ADDRESS), cmdSize);

    // Mid thread preemption and debugger enabled
    device->getExecutionEnvironment()->rootDeviceEnvironments[0]->initDebuggerL0(device.get());
    cmdSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(sizeof(STATE_CONTEXT_DATA_BASE_ADDRESS), cmdSize);

    // No mid thread preemption and debugger enabled
    device->overridePreemptionMode(PreemptionMode::Initial);
    cmdSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(sizeof(STATE_CONTEXT_DATA_BASE_ADDRESS), cmdSize);
}
XE3P_CORETEST_F(Xe3pPreemptionTests, givenDebuggerInitializedAndMidThreadPreemptionWhenGetAdditionalCommandsSizeIsCalledThen2MiLoadRegisterImmCmdsAreAddedInsteadOfBasePreambleProgramming) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);

    size_t withoutDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);

    mockDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(mockDevice.get());
    size_t withDebugging = PreambleHelper<FamilyType>::getAdditionalCommandsSize(*mockDevice);
    EXPECT_LT(withoutDebugging, withDebugging);

    auto expectedProgrammedCmdsCount = UnitTestHelper<FamilyType>::getMiLoadRegisterImmProgrammedCmdsCount(true);
    size_t sizeExpected = withoutDebugging + expectedProgrammedCmdsCount * sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(sizeExpected, withDebugging);
}

XE3P_CORETEST_F(Xe3pPreemptionTests, givenXe3pInterfaceDescriptorDataWhenPreemptionModeIsMidThreadThenThreadPreemptionBitIsEnabled) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::Disabled);
    EXPECT_FALSE(iddArg.getThreadPreemption());

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidBatch);
    EXPECT_FALSE(iddArg.getThreadPreemption());

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::ThreadGroup);
    EXPECT_FALSE(iddArg.getThreadPreemption());

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    EXPECT_TRUE(iddArg.getThreadPreemption());
}

XE3P_CORETEST_F(Xe3pPreemptionTests, givenXe3pAndMidThreadPreemptionWhenProgramStateSipThenSystemInstructionPointerIsCorrect) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    VariableBackup<bool> mockSipBackup{&MockSipData::useMockSip, false};
    DebugManagerStateRestore restorer;

    for (bool enableHeapless : {true, false}) {

        debugManager.flags.Enable64BitAddressing.set(enableHeapless);

        auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
        mockDevice->setPreemptionMode(PreemptionMode::MidThread);
        auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice, false);

        StackVec<char, 64> preambleBuffer;
        preambleBuffer.resize(cmdSizePreemptionMidThread);
        LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

        PreemptionHelper::programStateSip<FamilyType>(preambleStream, *mockDevice, nullptr);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(preambleStream);
        auto itStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), itStateSip);
        auto stateSipCmd = static_cast<STATE_SIP *>(*itStateSip);

        auto sipAddress = stateSipCmd->getSystemInstructionPointer();
        auto fullAddress = SipKernel::getSipKernel(*mockDevice, nullptr).getSipAllocation()->getGpuAddress();
        auto offsetToHeap = SipKernel::getSipKernel(*mockDevice, nullptr).getSipAllocation()->getGpuAddressToPatch();
        EXPECT_NE(fullAddress, offsetToHeap);

        auto &compilerProductHelper = mockDevice->getCompilerProductHelper();

        if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
            EXPECT_EQ(fullAddress, sipAddress);
        } else {
            EXPECT_EQ(offsetToHeap, sipAddress);
        }
    }
}
