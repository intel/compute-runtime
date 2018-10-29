/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "reg_configs_common.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/event/user_event.h"

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/ult_command_stream_receiver_fixture.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"

#include "test.h"
#include "gtest/gtest.h"
using namespace OCLRT;

#include "unit_tests/command_stream/command_stream_receiver_hw_tests.inl"

using CommandStreamReceiverHwTestGen9 = CommandStreamReceiverHwTest<SKLFamily>;

GEN9TEST_F(UltCommandStreamReceiverTest, givenNotSentPreambleAndMidThreadPreemptionWhenPreambleIsProgrammedThenCorrectSipKernelGpuAddressIsProgrammed) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;

    size_t minCsrSize = pDevice->getHardwareInfo().pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);
    commandStreamReceiver.setPreemptionCsrAllocation(&csrSurface);

    pDevice->setPreemptionMode(PreemptionMode::MidThread);
    uint32_t newL3Config;
    DispatchFlags dispatchFlags;

    auto cmdSizePreambleMidThread = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    StackVec<char, 4096> preemptionBuffer;
    preemptionBuffer.resize(cmdSizePreambleMidThread);
    LinearStream preambleStream(&*preemptionBuffer.begin(), preemptionBuffer.size());
    auto sipAllocation = pDevice->getExecutionEnvironment()->getBuiltIns()->getSipKernel(SipKernelType::Csr, *pDevice).getSipAllocation();
    commandStreamReceiver.programPreamble(preambleStream, *pDevice, dispatchFlags, newL3Config);

    this->parseCommands<FamilyType>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(this->cmdList.begin(), this->cmdList.end());
    ASSERT_NE(this->cmdList.end(), itorStateSip);

    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();
    EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress);
}

GEN9TEST_F(UltCommandStreamReceiverTest, givenNotSentPreambleAndKernelDebuggingActiveWhenPreambleIsProgrammedThenCorrectSipKernelGpuAddressIsProgrammed) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    auto &builtIns = *pDevice->getExecutionEnvironment()->getBuiltIns();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;
    size_t minCsrSize = pDevice->getHardwareInfo().pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);
    commandStreamReceiver.setPreemptionCsrAllocation(&csrSurface);

    pDevice->setPreemptionMode(PreemptionMode::Disabled);
    pDevice->setSourceLevelDebuggerActive(true);
    uint32_t newL3Config;
    DispatchFlags dispatchFlags;

    auto cmdSizePreambleMidThread = commandStreamReceiver.getRequiredCmdSizeForPreamble(*pDevice);
    StackVec<char, 4096> preemptionBuffer;
    preemptionBuffer.resize(cmdSizePreambleMidThread);
    LinearStream preambleStream(&*preemptionBuffer.begin(), preemptionBuffer.size());
    auto dbgLocalSipAllocation = builtIns.getSipKernel(SipKernelType::DbgCsrLocal, *pDevice).getSipAllocation();
    auto sipAllocation = builtIns.getSipKernel(SipKernelType::Csr, *pDevice).getSipAllocation();

    ASSERT_NE(builtIns.getSipKernel(SipKernelType::DbgCsrLocal, *pDevice).getType(), builtIns.getSipKernel(SipKernelType::Csr, *pDevice).getType());
    ASSERT_NE(dbgLocalSipAllocation, nullptr);
    ASSERT_NE(sipAllocation, nullptr);

    commandStreamReceiver.programPreamble(preambleStream, *pDevice, dispatchFlags, newL3Config);

    this->parseCommands<FamilyType>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(this->cmdList.begin(), this->cmdList.end());
    ASSERT_NE(this->cmdList.end(), itorStateSip);

    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();
    EXPECT_EQ(dbgLocalSipAllocation->getGpuAddressToPatch(), sipAddress);
}

GEN9TEST_F(CommandStreamReceiverHwTestGen9, GivenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3Config) {
    givenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigImpl();
}

GEN9TEST_F(CommandStreamReceiverHwTestGen9, GivenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentOnThenProgramL3WithSLML3ConfigAfterUnblocking) {
    givenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblockingImpl();
}
